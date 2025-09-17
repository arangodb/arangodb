////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2025 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
  [[nodiscard]] bool notify();

  struct Awaitable {
    [[nodiscard]] bool await_ready() const noexcept;

    [[nodiscard]] std::int64_t await_resume() const noexcept;

    [[nodiscard]] bool await_suspend(std::coroutine_handle<> c) noexcept;

    SuspensionCounter* _suspensionCounter;
  };

  auto await() -> Awaitable;

  std::atomic<std::int64_t> _counter{0};
  std::coroutine_handle<> _coroHandle;
};

// waitingFunToCoro() is an adapter to map a WAITING-style function to a
// coroutine. A WAITING-style function achieves asynchronous execution by
// returning some kind of WAITING value to suspend, and is expected to be
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
                                    F&& fun) -> async<T> {
  // Move the function into the coroutine frame to ensure proper lifetime
  auto movedFun = std::move(fun);
  auto res = movedFun();
  while (!res.has_value()) {
    // Get the number of wakeups. We call movedFun() up to that many
    // times before suspending again.
    auto n = co_await suspensionCounter.await();
    for (decltype(n) i = 0; i < n && !res.has_value(); ++i) {
      res = movedFun();
    }
  }
  co_return std::move(res).value();
}

}  // namespace arangodb
