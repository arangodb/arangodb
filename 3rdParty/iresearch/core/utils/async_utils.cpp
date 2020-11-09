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

#include "async_utils.hpp"

#include <cassert>
#include <functional>

#include "utils/log.hpp"
#include "utils/misc.hpp"
#include "utils/std.hpp"

using namespace std::chrono_literals;

namespace {

constexpr auto RW_MUTEX_WAIT_TIMEOUT = 100ms;

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
  auto lock = make_unique_lock(mutex_, std::try_to_lock);
  assert(lock && !concurrent_count_.load() && !exclusive_count_);
#endif
}

void read_write_mutex::lock_read() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return;
  }

  auto lock = make_unique_lock(mutex_);

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

  auto lock = make_unique_lock(mutex_);

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
  VALGRIND_ONLY(auto lock = make_lock_guard(exclusive_owner_mutex_);) // suppress valgrind false-positives related to std::atomic_*
  exclusive_owner_.store(std::this_thread::get_id());
  lock.release(); // disassociate the associated mutex without unlocking it
}

bool read_write_mutex::owns_write() noexcept {
  VALGRIND_ONLY(auto lock = make_lock_guard(exclusive_owner_mutex_);) // suppress valgrind false-positives related to std::atomic_*
  return exclusive_owner_.load() == std::this_thread::get_id();
}

bool read_write_mutex::try_lock_read() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return true;
  }

  auto lock = make_unique_lock(mutex_, std::try_to_lock);

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

  auto lock = make_unique_lock(mutex_, std::try_to_lock);

  if (!lock || concurrent_count_) {
    return false;
  }

  VALGRIND_ONLY(auto lock = make_lock_guard(exclusive_owner_mutex_);) // suppress valgrind false-positives related to std::atomic_*
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

    auto lock = make_unique_lock(mutex_, std::adopt_lock);

    if (exclusive_only) {
      ++concurrent_count_; // acquire the read-lock
    }

    VALGRIND_ONLY(auto lock = make_lock_guard(exclusive_owner_mutex_);) // suppress valgrind false-positives related to std::atomic_*
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

  // try to acquire mutex for use with cond
  auto lock = make_unique_lock(mutex_, std::try_to_lock);

  // wake only writers since this is a reader
  // wake even without lock since writer may be waiting in lock_write() on cond
  // the latter might also indicate a bug if deadlock occurs with SCOPED_LOCK()
  writer_cond_.notify_all();
}

thread_pool::thread_pool(
    size_t max_threads /*= 0*/,
    size_t max_idle /*= 0*/,
    basic_string_ref<native_char_t> worker_name /*= ""*/)
  : max_idle_(max_idle),
    max_threads_(max_threads),
    worker_name_(worker_name) {
}

thread_pool::~thread_pool() {
  try {
    stop(true);
  } catch (...) { }
}

size_t thread_pool::max_idle() const {
  auto lock = make_lock_guard(lock_);

  return max_idle_;
}

void thread_pool::max_idle(size_t value) {
  {
    auto lock = make_lock_guard(lock_);

    max_idle_ = value;
  }
  cond_.notify_all(); // wake any idle threads if they need termination
}

void thread_pool::max_idle_delta(int delta) {
  {
    auto lock = make_lock_guard(lock_);
    auto max_idle = max_idle_ + delta;

    if (delta > 0 && max_idle < max_idle_) {
      max_idle_ = std::numeric_limits<size_t>::max();
    } else if (delta < 0 && max_idle > max_idle_) {
      max_idle_ = std::numeric_limits<size_t>::min();
    } else {
      max_idle_ = max_idle;
    }
  }
  cond_.notify_all(); // wake any idle threads if they need termination
}

size_t thread_pool::max_threads() const {
  auto lock = make_lock_guard(lock_);

  return max_threads_;
}

void thread_pool::max_threads(size_t value) {
  {
    auto lock = make_lock_guard(lock_);

    max_threads_ = value;

    // create extra thread if all threads are busy and can grow pool
    if (State::ABORT != state_ && !queue_.empty() &&
        active_ == pool_.size() && pool_.size() < max_threads_) {
      pool_.emplace_back(&thread_pool::worker, this);
    }
  }

  cond_.notify_all(); // wake any idle threads if they need termination
}

void thread_pool::max_threads_delta(int delta) {
  {
    auto lock = make_lock_guard(lock_);
    auto max_threads = max_threads_ + delta;

    if (delta > 0 && max_threads < max_threads_) {
      max_threads_ = std::numeric_limits<size_t>::max();
    } else if (delta < 0 && max_threads > max_threads_) {
      max_threads_ = std::numeric_limits<size_t>::min();
    } else {
      max_threads_ = max_threads;
    }

    // create extra thread if all threads are busy and can grow pool
    if (State::ABORT != state_ && !queue_.empty() &&
        active_ == pool_.size() && pool_.size() < max_threads_) {
      pool_.emplace_back(&thread_pool::worker, this);
    }
  }

  cond_.notify_all(); // wake any idle threads if they need termination
}

bool thread_pool::run(std::function<void()>&& fn, clock_t::duration delay /*=0*/) {
  {
    auto lock = make_lock_guard(lock_);

    if (State::RUN != state_) {
      return false; // pool not active
    }

    queue_.emplace(std::move(fn), clock_t::now() + delay);

    // create extra thread if all threads are busy and can grow pool
    if (active_ == pool_.size() && pool_.size() < max_threads_) {
      pool_.emplace_back(&thread_pool::worker, this);
    }
  }

  cond_.notify_one();

  return true;
}

void thread_pool::stop(bool skip_pending /*= false*/) {
  auto lock = make_unique_lock(lock_);

  if (State::RUN == state_) {
    state_ = skip_pending ? State::ABORT : State::FINISH;
  }

  // wait for all threads to terminate
  while (!pool_.empty()) {
    cond_.notify_all(); // wake all threads
    cond_.wait_for(lock, 100ms);
  }
}


void thread_pool::set_limits(size_t max_threads, size_t max_idle) {
  {
    auto lock = make_lock_guard(lock_);

    max_threads_ = max_threads;
    max_idle_ = max_idle;

    // create extra thread if all threads are busy and can grow pool
    if (State::ABORT != state_ && !queue_.empty() &&
        active_ == pool_.size() && pool_.size() < max_threads_) {
      pool_.emplace_back(&thread_pool::worker, this);
    }
  }

  cond_.notify_all(); // wake any idle threads if they need termination
}

std::tuple<size_t, size_t, size_t, size_t, size_t> thread_pool::stats() const {
  auto lock = make_lock_guard(lock_);

  return { active_, queue_.size(), pool_.size(), max_threads_, max_idle_ };
}

size_t thread_pool::tasks_active() const {
  auto lock = make_lock_guard(lock_);

  return active_;
}

size_t thread_pool::tasks_pending() const {
  auto lock = make_lock_guard(lock_);

  return queue_.size();
}

size_t thread_pool::threads() const {
  auto lock = make_lock_guard(lock_);

  return pool_.size();
}

void thread_pool::worker() {
  if (!worker_name_.empty()) {
    set_thread_name(worker_name_.c_str());
  }

  auto lock = make_unique_lock(lock_);

  ++active_;

  for (;;) {
    const auto now = clock_t::now();

    // if are allowed to have running threads and have task to process
    if (State::ABORT != state_ && !queue_.empty() && pool_.size() <= max_threads_) {
      auto& top = queue_.top();

      if (top.at <= now) {
        func_t fn;

        try {
          // std::function<...> move ctor isn't marked "noexcept" until c++20
          fn = std::move(top.fn);
        } catch (...) {
          IR_FRMT_WARN("Failed to move task, skipping it");
          queue_.pop();
          continue;
        }

        queue_.pop();

        // if have more tasks but no idle thread and can grow pool
        if (!queue_.empty() && active_ == pool_.size() && pool_.size() < max_threads_) {
          try {
            pool_.emplace_back(&thread_pool::worker, this); // add one thread
          } catch (const std::exception& e) {
            IR_FRMT_ERROR("Failed to grow pool, error '%s'", e.what());
          } catch (...) {
            IR_FRMT_ERROR("Failed to grow pool");
          }
        }

        lock.unlock();

        try {
          fn();
        } catch (const std::exception& e) {
          IR_FRMT_ERROR("Failed to execute task, error '%s'", e.what());
        } catch (...) {
          IR_FRMT_ERROR("Failed to execute task");
        }

        lock.lock();
      } else {
        // we have some tasks pending tasks, let's wait
        --active_;
        const auto sleep_time = std::min(clock_t::duration(50ms), top.at - now);
        try { cond_.wait_for(lock, sleep_time); } catch (...) { }
        ++active_;
      }

      continue;
    }

    assert(lock.owns_lock());
    --active_;

    assert(active_ <= pool_.size());
    if (State::RUN == state_ && // thread pool is still running
        pool_.size() <= max_threads_ && // pool does not exceed requested limit
        pool_.size() - active_ <= max_idle_) { // idle does not exceed requested limit
           IR_FRMT_ERROR("WAITING %d, %d, %d", pool_.size(), active_, max_idle_);

      try { cond_.wait(lock); } catch (...) { }
      ++active_;
      continue;
    }

           IR_FRMT_ERROR("BYEBYE %d, %d, %d", pool_.size(), active_, max_idle_);

    // ...........................................................................
    // too many idle threads
    // ...........................................................................

    assert(lock.owns_lock());

    // swap current thread handle with one at end of pool and remove end
    const auto it = std::find_if(
      std::begin(pool_), std::end(pool_),
      [this_id = std::this_thread::get_id()](const auto& thread) noexcept {
        return thread.get_id() == this_id;
    });

    if (it != std::end(pool_)) {
      it->detach();
      irstd::swap_remove(pool_, it);
    }

    if (State::RUN != state_) {
      lock.unlock();
      cond_.notify_all(); // wake up thread_pool::stop(...)
    }

    return; // terminate thread
  }
}

}
}
