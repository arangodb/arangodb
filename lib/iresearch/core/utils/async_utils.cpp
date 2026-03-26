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

#include "utils/async_utils.hpp"

#include <functional>

#include "utils/assert.hpp"
#include "utils/log.hpp"
#include "utils/misc.hpp"
#include "utils/std.hpp"

#include <absl/strings/str_cat.h>

using namespace std::chrono_literals;

namespace irs::async_utils {

void busywait_mutex::lock() noexcept {
  while (!try_lock()) {
    std::this_thread::yield();
  }
}

bool busywait_mutex::try_lock() noexcept {
  bool expected = false;
  return locked_.load(std::memory_order_relaxed) == expected &&
         locked_.compare_exchange_strong(expected, true,
                                         std::memory_order_acquire,
                                         std::memory_order_relaxed);
}

void busywait_mutex::unlock() noexcept {
  locked_.store(false, std::memory_order_release);
}

template<bool UseDelay>
ThreadPool<UseDelay>::ThreadPool(size_t threads, basic_string_view<Char> name) {
  start(threads, name);
}

template<bool UseDelay>
void ThreadPool<UseDelay>::start(size_t threads, basic_string_view<Char> name) {
  std::lock_guard lock{m_};
  IRS_ASSERT(threads_.empty());
  threads_.reserve(threads);
  for (size_t i = 0; i != threads; ++i) {
    threads_.emplace_back([this, name] {
      if (!name.empty()) {
        IRS_ASSERT(std::char_traits<Char>::length(name.data()) == name.size());
        set_thread_name(name.data());
      }
      Work();
    });
  }
}

template<bool UseDelay>
bool ThreadPool<UseDelay>::run(Func&& fn, Clock::duration delay) {
  IRS_ASSERT(fn);
  if constexpr (UseDelay) {
    auto at = Clock::now() + delay;
    std::unique_lock lock{m_};
    if (WasStop()) {
      return false;
    }
    tasks_.emplace(std::move(fn), at);
    // TODO We can skip notify when new element is more delayed than min
  } else {
    std::unique_lock lock{m_};
    if (WasStop()) {
      return false;
    }
    tasks_.push(std::move(fn));
  }
  cv_.notify_one();
  return true;
}

template<bool UseDelay>
void ThreadPool<UseDelay>::stop(bool skip_pending) noexcept {
  std::unique_lock lock{m_};
  if (skip_pending) {
    tasks_ = decltype(tasks_){};
  }
  if (WasStop()) {
    return;
  }
  state_ |= 1;
  auto threads = std::move(threads_);
  lock.unlock();
  cv_.notify_all();
  for (auto& t : threads) {
    t.join();
  }
}

template<bool UseDelay>
void ThreadPool<UseDelay>::Work() {
  std::unique_lock lock{m_};
  while (true) {
    while (!tasks_.empty()) {
      Func fn;
      if constexpr (UseDelay) {
        auto& top = tasks_.top();
        if (top.at > Clock::now()) {
          const auto at = top.at;
          cv_.wait_until(lock, at);
          continue;
        }
        fn = std::move(const_cast<Func&>(top.fn));
      } else {
        fn = std::move(tasks_.front());
      }
      tasks_.pop();
      state_ += 2;
      lock.unlock();
      try {
        fn();
      } catch (...) {
      }
      lock.lock();
      state_ -= 2;
    }
    if (WasStop()) {
      return;
    }
    cv_.wait(lock);
  }
}

template class ThreadPool<true>;
template class ThreadPool<false>;

}  // namespace irs::async_utils
