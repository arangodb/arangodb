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

#include "Futures/Try.h"

#include <coroutine>
#include <utility>

#include "Logger/LogMacros.h"

namespace arangodb {
namespace runtime {

template<typename Result>
struct Promise;

template<typename Result = void>
struct [[nodiscard]] Task {
  using promise_type = Promise<Result>;
  std::coroutine_handle<promise_type> _handle{};

  explicit Task(std::coroutine_handle<promise_type> handle) : _handle(handle) {}
  explicit Task(Task&& other) : _handle(std::move(other._handle)) {
    other._handle = nullptr;
  }
  explicit Task(Task const& other) = delete;
  auto operator=(Task&& other) -> Task& {
    _handle = other._handle;
    other._handle = nullptr;
    return *this;
  }
  auto operator=(Task const& other) -> Task& = delete;

  ~Task() {
    if (_handle != nullptr) {
      _handle.destroy();
    }
  }

  void resume() { return _handle.resume(); }

  auto& res() & { return _handle.promise()._res; }
  auto&& res() && { return std::move(_handle.promise()._res); }

  auto operator co_await() {
    struct Awaiter {
      std::coroutine_handle<Promise<Result>> _handle;

      explicit Awaiter(std::coroutine_handle<Promise<Result>> handle)
          : _handle(handle) {}
      ~Awaiter() {}

      bool await_ready() { return false; }
      bool await_suspend([[maybe_unused]] std::coroutine_handle<> caller) {
        _handle.promise()._caller = caller;
        return !_handle.promise()._res.valid();
      }
      auto await_resume() { return _handle.promise()._res.get(); }
    };

    return Awaiter{_handle};
  }
};

template<typename Result>
struct Promise {
  futures::Try<Result> _res{};
  std::coroutine_handle<> _caller{};

  Promise() {}
  ~Promise() {}

  std::suspend_never initial_suspend() { return {}; }

  auto final_suspend() noexcept {
    struct FinalAwaiter {
      bool await_ready() noexcept { return false; }
      void await_resume() noexcept {}
      auto await_suspend(std::coroutine_handle<Promise<Result>> h) noexcept
          -> std::coroutine_handle<> {
        if (auto caller = h.promise()._caller; caller != nullptr) {
          return caller;
        } else {
          return std::noop_coroutine();
        }
      }
    };
    return FinalAwaiter{};
  }

  void return_value(Result&& res) { _res.emplace(std::move(res)); }

  auto get_return_object() {
    return Task{std::coroutine_handle<Promise>::from_promise(*this)};
  }

  void unhandled_exception() { _res.set_exception(std::current_exception()); }
};

}  // namespace runtime
}  // namespace arangodb
