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
#include <concepts>
#include <coroutine>
#include <cstdint>
#include <memory>
#include <optional>

#include "Async/async.h"

namespace arangodb {

// SuspensionCounter::await() returns an awaitable that suspends until
// notify() is called, and which in turn returns the number of notifies that
// happened while it was suspended.
//
// This is useful to connect a callee using WAITING for asynchronous execution
// with a caller that is a coroutine.
// The callee can be instructed to call notify() when it is done, and the caller
// can await the result of notify().
//
// Only one awaitable as returned by await() must exist at a time. In
// particular, concurrent awaits on the same SuspensionCounter are not
// supported.
//
// Note that calling `notify()` may result in a resume of the awaiting
// coroutine, thus executing arbitrary code. Therefore, as a rule, it should
// be posted separately on the Scheduler. This is currently naturally the case
// because it will be called by SharedQueryState::queueHandler().
// If a use case emerges where this isn't so, the SuspensionCounter could be
// made to post just the `_c.resume()` call on the scheduler, to avoid
// unnecessary overhead in the cases where resume isn't called.
//
// The implementation keeps track of its state in
// std::atomic<std::int64_t> _counter;
// . It's initialized to zero.
//
// - `_counter == 0` means no notifies have been counted since the last resume
//   (or since its creation, if it hasn't yet been resumed).
// - `_counter == -1` means that there exists an awaitable, returned by await(),
//   that has been suspended; i.e. it was `co_await`ed and _counter has been
//   zero when await_suspend() was called.
// - `_counter > 0` means there have been notifies while no awaitable was
//   suspended. The value of `_counter` equals the number of notifies.

struct SuspensionCounter {
  // Returns true if a call has resulted in an awaiting coroutine being resumed,
  // and false otherwise.
  [[nodiscard]] bool notify() {
    auto counter = _counter.load(std::memory_order_relaxed);
    do {
      if (counter == -1) {
        // A suspended coroutine is waiting. (Try to) set the notify counter to
        // one and resume the coroutine.

        // (1) This acquire-CAS synchronizes-with the release-CAS (2)
        // This ensures that _c is visible.
        if (_counter.compare_exchange_weak(counter, 1,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed)) {
          // Note that this can throw, in principle, if unhandled_exception()
          // throws.
          _c.resume();
          return true;
        }
        // CAS failed, counter was modified by another thread, retry
      } else {
        // No coroutine is waiting, just increase the counter.
        if (_counter.compare_exchange_weak(counter, counter + 1,
                                           std::memory_order_relaxed)) {
          return false;
        }
        // CAS failed, counter was modified by another thread, retry
      }
    } while (true);
  }

  auto await() {
    struct Awaitable {
      [[nodiscard]] bool await_ready() const noexcept {
        return _suspensionCounter->_counter.load(std::memory_order_relaxed) > 0;
      }

      [[nodiscard]] std::int64_t await_resume() const noexcept {
        return _suspensionCounter->_counter.exchange(0,
                                                     std::memory_order_relaxed);
      }

      [[nodiscard]] bool await_suspend(std::coroutine_handle<> c) noexcept {
        _suspensionCounter->_c = c;
        auto counter = std::int64_t{};
        // Try to transition from 0 to -1 (unsignaled to suspended). If it
        // fails, the coroutine will be resumed because we have been
        // notified since await_ready() was called, and the coroutine will not
        // be suspended.

        // _suspensionCounter->_c needs to be visible when `_c.resume()` is
        // called, therefore:
        // (2) This release-CAS synchronizes-with the acquire-CAS (1)
        return _suspensionCounter->_counter.compare_exchange_strong(
            counter, -1, std::memory_order_release, std::memory_order_relaxed);
      }

      SuspensionCounter* _suspensionCounter;
    };

    return Awaitable{this};
  }

  std::atomic<std::int64_t> _counter{0};
  std::coroutine_handle<> _c;
};

// waitingFunToCoro() is an adapter to map a WAITING-style function to a
// coroutine. A WAITING-style function achieves asynchronous execution by
// returning some kind of WAITING value to suspend, and are expected to be
// called again when a wakeup is triggered by some callback in order to resume.
//
// The caller must make sure to translate every wakeup to a call to `notify()`
// on the provided SuspensionCounter.
//
// The provided function type `F` must return a std::optional, where
// `std::nullopt` corresponds to WAITING, possibly resulting in a suspension of
// the async returned by waitingFunToCoro(). When a value (rather than nullopt)
// is returned, it will be `co_return`ed by waitingFunToCoro().
template<typename F, typename T = std::invoke_result_t<F>::value_type>
requires requires(F f) {
  { f() } -> std::same_as<std::optional<T>>;
}
[[nodiscard]] auto waitingFunToCoro(SuspensionCounter& suspensionCounter,
                                    F&& funArg) -> async<T> {
  auto&& fun = std::forward<F>(funArg);

  auto res = fun();
  while (!res.has_value()) {
    // Get the number of wakeups. We call fun() up to that many
    // times before suspending again.
    auto n = co_await suspensionCounter.await();
    for (decltype(n) i = 0; i < n && !res.has_value(); ++i) {
      res = fun();
    }
  }
  co_return std::move(res).value();
}

}  // namespace arangodb
