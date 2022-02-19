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

#ifndef IRESEARCH_ASYNC_UTILS_H
#define IRESEARCH_ASYNC_UTILS_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>

#include "shared.hpp"
#include "noncopyable.hpp"
#include "string.hpp"
#include "thread_utils.hpp"

namespace iresearch {
namespace async_utils {

//////////////////////////////////////////////////////////////////////////////
/// @brief spinlock implementation for Win32 since std::mutex cannot be used
///        in calls going through dllmain()
//////////////////////////////////////////////////////////////////////////////
class busywait_mutex final {
 public:
  busywait_mutex();
  ~busywait_mutex();

  void lock();
  bool try_lock();
  void unlock();

 private:
  std::atomic<std::thread::id> owner_;
};

class thread_pool {
 public:
  using native_char_t = std::remove_pointer_t<thread_name_t>;
  using clock_t = std::chrono::steady_clock;
  using func_t = std::function<void()>;

  explicit thread_pool(
    size_t max_threads = 0,
    size_t max_idle = 0,
    basic_string_ref<native_char_t> worker_name = basic_string_ref<native_char_t>::EMPTY);
  ~thread_pool();
  size_t max_idle() const;
  void max_idle(size_t value);
  void max_idle_delta(int delta); // change value by delta
  size_t max_threads() const;
  void max_threads(size_t value);
  void max_threads_delta(int delta); // change value by delta

  // 1st - max_threads(), 2nd - max_idle()
  std::pair<size_t, size_t> limits() const;
  void limits(size_t max_threads, size_t max_idle);

  bool run(std::function<void()>&& fn, clock_t::duration delay = {});
  void stop(bool skip_pending = false); // always a blocking call
  size_t tasks_active() const;
  size_t tasks_pending() const;
  size_t threads() const;
  // 1st - tasks active(), 2nd - tasks pending(), 3rd - threads()
  std::tuple<size_t, size_t, size_t> stats() const;


 private:
  enum class State { ABORT, FINISH, RUN };

  struct shared_state {
    std::mutex lock;
    std::condition_variable cond;
    std::atomic<State> state{ State::RUN };
  };

  struct task {
    explicit task(
        std::function<void()>&& fn,
        clock_t::time_point at)
      : at(at), fn(std::move(fn)) {
    }

    clock_t::time_point at;
    func_t fn;

    bool operator<(const task& rhs) const noexcept {
      return rhs.at < at;
    }
  };

  void worker(std::shared_ptr<shared_state> shared_state) noexcept;
  void worker_impl(
    std::unique_lock<std::mutex>& lock,
    std::shared_ptr<shared_state> shared_state);
  bool maybe_spawn_worker();

  std::shared_ptr<shared_state> shared_state_;
  size_t active_{ 0 };
  std::atomic<size_t> threads_{ 0 };
  size_t max_idle_;
  size_t max_threads_;
  std::priority_queue<task> queue_;
  std::basic_string<native_char_t> worker_name_;
}; // thread_pool

} // async_utils
} // namespace iresearch {

#endif
