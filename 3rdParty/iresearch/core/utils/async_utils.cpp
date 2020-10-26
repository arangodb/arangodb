////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <cassert>

#include "log.hpp"
#include "thread_utils.hpp"
#include "async_utils.hpp"

namespace {

const auto RW_MUTEX_WAIT_TIMEOUT = std::chrono::milliseconds(100);

}

namespace iresearch {
namespace async_utils {

busywait_mutex::busywait_mutex(): owner_(std::thread::id()) {}

busywait_mutex::~busywait_mutex() {
  assert(try_lock()); // ensure destroying an unlocked mutex
}

void busywait_mutex::lock() {
  auto this_thread_id = std::this_thread::get_id();

  for (auto expected = std::thread::id();
       !owner_.compare_exchange_strong(expected, this_thread_id);
       expected = std::thread::id()) {
    assert(this_thread_id != expected); // recursive lock acquisition attempted
    std::this_thread::yield();
  }
}

bool busywait_mutex::try_lock() {
  auto this_thread_id = std::this_thread::get_id();
  auto expected = std::thread::id();

  return owner_.compare_exchange_strong(expected, this_thread_id);
}

void busywait_mutex::unlock() {
  auto expected = std::this_thread::get_id();

  if (!owner_.compare_exchange_strong(expected, std::thread::id())) {
    // try again since std::thread::id is garanteed to be '==' but may not be bit equal
    if (expected == std::this_thread::get_id() && owner_.compare_exchange_strong(expected, std::thread::id())) {
      return;
    }

    assert(false); // lock not owned by current thread
  }
}

read_write_mutex::read_write_mutex() noexcept
  : concurrent_count_(0),
    exclusive_count_(0),
    exclusive_owner_(std::thread::id()),
    exclusive_owner_recursion_count_(0) {
}

read_write_mutex::~read_write_mutex() noexcept {
#ifdef IRESEARCH_DEBUG
  // ensure mutex is not locked before destroying it
  TRY_SCOPED_LOCK_NAMED(mutex_, lock);
  assert(lock && !concurrent_count_.load() && !exclusive_count_);
#endif
}

void read_write_mutex::lock_read() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return;
  }

  SCOPED_LOCK_NAMED(mutex_, lock);

  // yield if there is already a writer waiting
  // wait for notification (possibly with writers waiting) or no more writers waiting
  while (exclusive_count_ && std::cv_status::timeout == reader_cond_.wait_for(lock, RW_MUTEX_WAIT_TIMEOUT)) {}

  ++concurrent_count_;
}

void read_write_mutex::lock_write() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return;
  }

  SCOPED_LOCK_NAMED(mutex_, lock);
  ++exclusive_count_; // mark mutex with writer-waiting state

  // wait until lock is held exclusively by the current thread
  while (concurrent_count_) {
    try {
      writer_cond_.wait_for(lock, RW_MUTEX_WAIT_TIMEOUT);
    } catch (...) {
      // 'wait_for' may throw according to specification
    }
  }

  --exclusive_count_;
  VALGRIND_ONLY(SCOPED_LOCK(exclusive_owner_mutex_);) // suppress valgrind false-positives related to std::atomic_*
  exclusive_owner_.store(std::this_thread::get_id());
  lock.release(); // disassociate the associated mutex without unlocking it
}

bool read_write_mutex::owns_write() noexcept {
  VALGRIND_ONLY(SCOPED_LOCK(exclusive_owner_mutex_);) // suppress valgrind false-positives related to std::atomic_*
  return exclusive_owner_.load() == std::this_thread::get_id();
}

bool read_write_mutex::try_lock_read() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return true;
  }

  TRY_SCOPED_LOCK_NAMED(mutex_, lock);

  if (!lock || exclusive_count_) {
    return false;
  }

  ++concurrent_count_;

  return true;
}

bool read_write_mutex::try_lock_write() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return true;
  }

  TRY_SCOPED_LOCK_NAMED(mutex_, lock);

  if (!lock || concurrent_count_) {
    return false;
  }

  VALGRIND_ONLY(SCOPED_LOCK(exclusive_owner_mutex_);) // suppress valgrind false-positives related to std::atomic_*
  exclusive_owner_.store(std::this_thread::get_id());
  lock.release(); // disassociate the associated mutex without unlocking it

  return true;
}

void read_write_mutex::unlock(bool exclusive_only /*= false*/) {
  // if have write lock
  if (owns_write()) {
    if (exclusive_owner_recursion_count_) {
      if (!exclusive_only) { // a recursively locked mutex is always top-level write locked
        --exclusive_owner_recursion_count_; // write recursion unlock one level
      }

      return;
    }

    ADOPT_SCOPED_LOCK_NAMED(mutex_, lock);

    if (exclusive_only) {
      ++concurrent_count_; // acquire the read-lock
    }

    VALGRIND_ONLY(SCOPED_LOCK(exclusive_owner_mutex_);) // suppress valgrind false-positives related to std::atomic_*
    exclusive_owner_.store(std::thread::id());
    reader_cond_.notify_all(); // wake all reader and writers
    writer_cond_.notify_all(); // wake all reader and writers

    return;
  }

  if (exclusive_only) {
    return; // NOOP for readers
  }

  // ...........................................................................
  // after here assume have read lock
  // ...........................................................................

  #ifdef IRESEARCH_DEBUG
    auto count = --concurrent_count_;
    assert(count != size_t(-1)); // ensure decrement was for a positive number (i.e. not --0)
    UNUSED(count);
  #else
    --concurrent_count_;
  #endif // IRESEARCH_DEBUG

  // FIXME: this should be changed to SCOPED_LOCK_NAMED, as right now it is not
  // guaranteed that we can succesfully acquire the mutex here. and if we don't,
  // there is no guarantee that the notify_all will wake up queued waiter.

  TRY_SCOPED_LOCK_NAMED(mutex_, lock); // try to acquire mutex for use with cond

  // wake only writers since this is a reader
  // wake even without lock since writer may be waiting in lock_write() on cond
  // the latter might also indicate a bug if deadlock occurs with SCOPED_LOCK()
  writer_cond_.notify_all();
}

thread_pool::thread_pool(size_t max_threads /*= 0*/, size_t max_idle /*= 0*/):
  active_(0), max_idle_(max_idle), max_threads_(max_threads), state_(State::RUN) {
}

thread_pool::~thread_pool() {
  stop(true);
}

size_t thread_pool::max_idle() {
  std::lock_guard<decltype(lock_)> lock(lock_);

  return max_idle_;
}

void thread_pool::max_idle(size_t value) {
  std::lock_guard<decltype(lock_)> lock(lock_);

  max_idle_ = value;
  cond_.notify_all(); // wake any idle threads if they need termination
}

void thread_pool::max_idle_delta(int delta) {
  std::lock_guard<decltype(lock_)> lock(lock_);
  auto max_idle = max_idle_ + delta;

  if (delta > 0 && max_idle < max_idle_) {
      max_idle_ = std::numeric_limits<size_t>::max();
  }
  else if (delta < 0 && max_idle > max_idle_) {
      max_idle_ = std::numeric_limits<size_t>::min();
  }
  else {
      max_idle_ = max_idle;
  }
}

size_t thread_pool::max_threads() {
  std::lock_guard<decltype(lock_)> lock(lock_);

  return max_threads_;
}

void thread_pool::max_threads(size_t value) {
  std::lock_guard<decltype(lock_)> lock(lock_);

  max_threads_ = value;

  // create extra thread if all threads are busy and can grow pool
  if (State::ABORT != state_ && !queue_.empty() && active_ == pool_.size() && pool_.size() < max_threads_) {
    pool_.emplace_back([](thread_pool* pool)->void{ pool->run(); }, this);
  }

  cond_.notify_all(); // wake any idle threads if they need termination
}

void thread_pool::max_threads_delta(int delta) {
  std::lock_guard<decltype(lock_)> lock(lock_);
  auto max_threads = max_threads_ + delta;

  if (delta > 0 && max_threads < max_threads_) {
      max_threads_ = std::numeric_limits<size_t>::max();
  }
  else if (delta < 0 && max_threads > max_threads_) {
      max_threads_ = std::numeric_limits<size_t>::min();
  }
  else {
      max_threads_ = max_threads;
  }

  // create extra thread if all threads are busy and can grow pool
  if (State::ABORT != state_ && !queue_.empty() && active_ == pool_.size() && pool_.size() < max_threads_) {
    pool_.emplace_back([](thread_pool* pool)->void{ pool->run(); }, this);
  }

  cond_.notify_all(); // wake any idle threads if they need termination
}

bool thread_pool::run(std::function<void()>&& fn) {
  std::lock_guard<decltype(lock_)> lock(lock_);

  if (State::RUN != state_) {
    return false; // pool not active
  }

  queue_.emplace(std::move(fn));
  cond_.notify_one();

  // create extra thread if all threads are busy and can grow pool
  if (active_ == pool_.size() && pool_.size() < max_threads_) {
    pool_.emplace_back([](thread_pool* pool)->void{ pool->run(); }, this);
  }

  return true;
}

void thread_pool::stop(bool skip_pending /*= false*/) {
  std::unique_lock<decltype(lock_)> lock(lock_);

  if (State::RUN == state_) {
    state_ = skip_pending ? State::ABORT : State::FINISH;
  }

  // wait for all threads to terminate
  while(!pool_.empty()) {
    cond_.notify_all(); // wake all threads
    cond_.wait_for(lock, std::chrono::milliseconds(100));
  }
}

size_t thread_pool::tasks_active() {
  std::lock_guard<decltype(lock_)> lock(lock_);

  return active_;
}

size_t thread_pool::tasks_pending() {
  std::lock_guard<decltype(lock_)> lock(lock_);

  return queue_.size();
}

size_t thread_pool::threads() {
  std::lock_guard<decltype(lock_)> lock(lock_);

  return pool_.size();
}

void thread_pool::run() {
  std::unique_lock<decltype(lock_)> lock(lock_);

  ++active_;

  for(;;) {
    // if are allowed to have running threads and have task to process
    if (State::ABORT != state_ && !queue_.empty() && pool_.size() <= max_threads_) {
      auto fn = std::move(queue_.front());

      queue_.pop();

      // if have more tasks but no idle thread and can grow pool
      if (!queue_.empty() && active_ == pool_.size() && pool_.size() < max_threads_) {
        try {
          pool_.emplace_back([](thread_pool* pool)->void{ pool->run(); }, this); // add one thread
        } catch (std::bad_alloc&) {
          IR_LOG_EXCEPTION(); // log and ignore exception, new tasks will start new thread
        }
      }

      lock.unlock();

      try {
        fn();
      } catch (...) {
        IR_LOG_EXCEPTION();
      }

      lock.lock();
      continue;
    }

    --active_;

    if (State::RUN == state_ && // thread pool is still running
        pool_.size() <= max_threads_ && // pool does not exceed requested limit
        pool_.size() - active_ <= max_idle_) { // idle does not exceed requested limit
      cond_.wait(lock);
      ++active_;
      continue;
    }

    // ...........................................................................
    // too many idle threads
    // ...........................................................................

    auto this_id = std::this_thread::get_id();

    // swap current thread handle with one at end of pool and remove end
    for (size_t i = 0, count = pool_.size(); i < count; ++i) {
      if (pool_[i].get_id() == this_id) {
        pool_[i].swap(pool_.back()); // works even if same
        pool_.back().detach();
        pool_.pop_back();
        break;
      }
    }

    if (State::RUN != state_) {
      cond_.notify_all(); // wake up thread_pool::stop(...)
    }

    return; // terminate thread
  }
}

}
}
