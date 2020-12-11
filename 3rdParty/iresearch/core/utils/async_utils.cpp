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
  : shared_state_(std::make_shared<shared_state>()),
    max_idle_(max_idle),
    max_threads_(max_threads),
    worker_name_(worker_name) {
}

thread_pool::~thread_pool() {
  try {
    stop(true);
  } catch (...) { }
}

size_t thread_pool::max_idle() const {
  auto lock = make_lock_guard(shared_state_->lock);

  return max_idle_;
}

void thread_pool::max_idle(size_t value) {
  auto& state = *shared_state_;

  {
    auto lock = make_lock_guard(state.lock);

    max_idle_ = value;
  }

  state.cond.notify_all(); // wake any idle threads if they need termination
}

void thread_pool::max_idle_delta(int delta) {
  auto& state = *shared_state_;

  {
    auto lock = make_lock_guard(state.lock);
    auto max_idle = max_idle_ + delta;

    if (delta > 0 && max_idle < max_idle_) {
      max_idle_ = std::numeric_limits<size_t>::max();
    } else if (delta < 0 && max_idle > max_idle_) {
      max_idle_ = std::numeric_limits<size_t>::min();
    } else {
      max_idle_ = max_idle;
    }
  }

  state.cond.notify_all(); // wake any idle threads if they need termination
}

size_t thread_pool::max_threads() const {
  auto lock = make_lock_guard(shared_state_->lock);

  return max_threads_;
}

void thread_pool::max_threads(size_t value) {
  auto& state = *shared_state_;

  {
    auto lock = make_lock_guard(state.lock);

    max_threads_ = value;

    if (State::ABORT != state_) {
      maybe_spawn_worker();
    }
  }

  state.cond.notify_all(); // wake any idle threads if they need termination
}

void thread_pool::max_threads_delta(int delta) {
  auto& state = *shared_state_;

  {
    auto lock = make_lock_guard(state.lock);
    auto max_threads = max_threads_ + delta;

    if (delta > 0 && max_threads < max_threads_) {
      max_threads_ = std::numeric_limits<size_t>::max();
    } else if (delta < 0 && max_threads > max_threads_) {
      max_threads_ = std::numeric_limits<size_t>::min();
    } else {
      max_threads_ = max_threads;
    }

    if (State::ABORT != state_) {
      maybe_spawn_worker();
    }
  }

  state.cond.notify_all(); // wake any idle threads if they need termination
}

bool thread_pool::run(std::function<void()>&& fn, clock_t::duration delay /*=0*/) {
  if (!fn) {
    return false;
  }

  auto& state = *shared_state_;

  {
    auto lock = make_lock_guard(state.lock);

    if (State::RUN != state_) {
      return false; // pool not active
    }

    queue_.emplace(std::move(fn), clock_t::now() + delay);

    try {
      maybe_spawn_worker();
    } catch (...) {
      if (0 == threads_) {
        // failed to spawn a thread to execute a task
        queue_.pop();
        throw;
      }
    }
  }

  state.cond.notify_one();

  return true;
}

void thread_pool::stop(bool skip_pending /*= false*/) {
  state_ = skip_pending ? State::ABORT : State::FINISH;

  decltype(queue_) empty;
  {
    auto lock = make_unique_lock(shared_state_->lock);

    // wait for all threads to terminate
    while (threads_.load()) {
      shared_state_->cond.notify_all(); // wake all threads
      shared_state_->cond.wait_for(lock, 100ms);
    }

    queue_.swap(empty);
  }
}

void thread_pool::limits(size_t max_threads, size_t max_idle) {
  auto& state = *shared_state_;

  {
    auto lock = make_lock_guard(state.lock);

    max_threads_ = max_threads;
    max_idle_ = max_idle;

    if (State::ABORT != state_) {
      maybe_spawn_worker();
    }
  }

  state.cond.notify_all(); // wake any idle threads if they need termination
}

bool thread_pool::maybe_spawn_worker() {
  assert(!shared_state_->lock.try_lock()); // lock must be held

  // create extra thread if all threads are busy and can grow pool
  const size_t pool_size = threads_.load();

  if (!queue_.empty() && active_ == pool_size && pool_size < max_threads_) {
    std::thread worker(&thread_pool::worker, this, shared_state_);
    worker.detach();

    ++threads_;

    return true;
  }

  return false;
}

std::pair<size_t, size_t> thread_pool::limits() const {
  auto lock = make_lock_guard(shared_state_->lock);

  return { max_threads_, max_idle_ };
}

std::tuple<size_t, size_t, size_t> thread_pool::stats() const {
  auto lock = make_lock_guard(shared_state_->lock);

  return { active_, queue_.size(), threads_};
}

size_t thread_pool::tasks_active() const {
  auto lock = make_lock_guard(shared_state_->lock);

  return active_;
}

size_t thread_pool::tasks_pending() const {
  auto lock = make_lock_guard(shared_state_->lock);

  return queue_.size();
}

size_t thread_pool::threads() const {
  auto lock = make_lock_guard(shared_state_->lock);

  return threads_;
}

void thread_pool::worker(std::shared_ptr<shared_state> shared_state) noexcept {
  // hold a reference to 'shared_state_' ensure state is still alive
  if (!worker_name_.empty()) {
    set_thread_name(worker_name_.c_str());
  }

  {
    auto lock = make_unique_lock(shared_state->lock, std::defer_lock);

    try {
      worker_impl(lock, shared_state);
    } catch (...) {
      // NOOP
    }

    --threads_;
  }

  if (State::RUN != state_) {
    shared_state->cond.notify_all(); // wake up thread_pool::stop(...)
  }
}

void thread_pool::worker_impl(std::unique_lock<std::mutex>& lock,
                              std::shared_ptr<shared_state> shared_state) {
  lock.lock();

  while (State::ABORT != state_ && threads_ <= max_threads_) {
    if (!queue_.empty()) {
      auto& top = queue_.top();

      if (top.at <= clock_t::now()) {
        func_t fn;

        try {
          // std::function<...> move ctor isn't marked "noexcept" until c++20
          fn = std::move(top.fn);
        } catch (const std::bad_alloc&) {
          fprintf(stderr, "Failed to pop task from queue, skipping it");
          queue_.pop();
          continue;
        } catch (...) {
          IR_FRMT_WARN("Failed to pop task from queue, skipping it");
          queue_.pop();
          continue;
        }

        queue_.pop();
        ++active_;

        {
          auto dec = make_finally([this](){ --active_; });

          // if have more tasks but no idle thread and can grow pool
          try {
            maybe_spawn_worker();
          } catch (const std::bad_alloc&) {
            fprintf(stderr, "Failed to allocate memory while spawning a worker");
          } catch (const std::exception& e) {
            IR_FRMT_ERROR("Failed to grow pool, error '%s'", e.what());
          } catch (...) {
            IR_FRMT_ERROR("Failed to grow pool");
          }

          lock.unlock();

          try {
            fn();
          } catch (const std::bad_alloc&) {
            fprintf(stderr, "Failed to allocate memory while executing task");
          } catch (const std::exception& e) {
            IR_FRMT_ERROR("Failed to execute task, error '%s'", e.what());
          } catch (...) {
            IR_FRMT_ERROR("Failed to execute task");
          }

          lock.lock();
        }

        continue;
      }
    }

    assert(lock.owns_lock());
    assert(active_ <= threads_);

    if (const auto idle = threads_ - active_;
        (idle <= max_idle_ || (!queue_.empty() && threads_ == 1))) {
      if (const auto run_state = state_.load();
          !queue_.empty() && State::ABORT != run_state) {
        const auto at = queue_.top().at; // queue_ might be modified
        shared_state->cond.wait_until(lock, at);
      } else if (State::RUN == run_state) {
        assert(queue_.empty());
        shared_state->cond.wait(lock);
      } else {
        assert(State::RUN != run_state);
        return; // termination requested
      }
    } else {
      return; // too many idle threads
    }

    assert(lock.owns_lock());
  }
}

}
}
