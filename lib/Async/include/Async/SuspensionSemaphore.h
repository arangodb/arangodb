////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <memory>
#include <coroutine>
#include <cstdint>

namespace arangodb {

class Scheduler;

// SuspensionSemaphore::await() returns an awaitable that suspends until
// notify() is called, and which returns the number of notifies.
struct SuspensionSemaphore {
  // returns true if still suspended
  bool notify() {
    auto counter = _counter.load();
    do {
      if (counter == -1) {
        auto res = _counter.compare_exchange_weak(counter, 1);
        if (res) {
          // NOTE This doesn't need to be posted on the scheduler, as notify()
          // will be called by SharedQueryState::queueHandler(), which already
          // posted it before calling.
          _c.resume();
          return false;
        }
      } else {
        auto res = _counter.compare_exchange_weak(counter, counter + 1);
        if (res) {
          return true;
        }
      }
    } while (true);
  }

  auto await() {
    struct Awaitable {
      bool await_ready() const noexcept {
        return _semaphore->_counter.load(std::memory_order_relaxed) > 0;
      }
      [[nodiscard]] std::int64_t await_resume() const noexcept {
        return _semaphore->_counter.exchange(0);
      }
      bool await_suspend(std::coroutine_handle<> c) {
        _semaphore->_c = c;
        auto counter = std::int64_t{};
        return _semaphore->_counter.compare_exchange_strong(counter, -1);
      }

      SuspensionSemaphore* _semaphore;
    };

    return Awaitable{this};
  }

  std::atomic<std::int64_t> _counter;
  std::coroutine_handle<> _c;
  Scheduler* _scheduler;
};
}  // namespace arangodb
