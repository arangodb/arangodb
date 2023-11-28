////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 14000
#include <experimental/coroutine>
namespace std_coro = std::experimental;
#else
#include <coroutine>
namespace std_coro = std;
#endif
#include <optional>

#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Promise.h"
#include "Try.h"

namespace arangodb::futures {
template<typename T>
class Future;
template<typename T>
struct FutureAwaitable {
  [[nodiscard]] auto await_ready() const noexcept -> bool { return false; }
  bool await_suspend(std_coro::coroutine_handle<> coro) noexcept {
    // returning false resumes `coro`
    std::move(_future).thenFinal(
        [coro, this](futures::Try<T>&& result) mutable noexcept {
          _result = std::move(result);
          if (_counter.fetch_sub(1) == 1) {
            coro.resume();
          }
        });
    return _counter.fetch_sub(1) != 1;
  }
  auto await_resume() -> T { return std::move(_result.value().get()); }
  explicit FutureAwaitable(Future<T> fut) : _future(std::move(fut)) {}

 private:
  std::atomic_uint8_t _counter{2};
  Future<T> _future;
  std::optional<futures::Try<T>> _result;
};

template<typename T>
auto operator co_await(Future<T>&& f) noexcept {
  return FutureAwaitable<T>{std::move(f)};
}

template<typename T, typename F>
struct FutureTransformAwaitable : F {
  [[nodiscard]] auto await_ready() const noexcept -> bool { return false; }
  bool await_suspend(std_coro::coroutine_handle<> coro) noexcept {
    std::move(_future).thenFinal(
        [coro, this](futures::Try<T>&& result) noexcept {
          _result = F::operator()(std::move(result));
          if (_counter.fetch_sub(1) == 1) {
            coro.resume();
          }
        });
    return _counter.fetch_sub(1) != 1;
  }
  using ResultType = std::invoke_result_t<F, futures::Try<T>&&>;
  auto await_resume() noexcept -> ResultType {
    return std::move(_result.value());
  }
  explicit FutureTransformAwaitable(Future<T> fut, F&& f)
      : F(std::forward<F>(f)), _future(std::move(fut)) {}

 private:
  static_assert(std::is_nothrow_invocable_v<F, futures::Try<T>&&>);
  std::atomic_uint8_t _counter{2};
  Future<T> _future;
  std::optional<ResultType> _result;
};

template<typename T>
auto asTry(Future<T>&& f) noexcept {
  return FutureTransformAwaitable{
      std::move(f),
      [](futures::Try<T>&& res) noexcept { return std::move(res); }};
}

static inline auto asResult(Future<Unit>&& f) noexcept {
  return FutureTransformAwaitable{
      std::move(f), [](futures::Try<Unit>&& res) noexcept -> Result {
        return basics::catchVoidToResult([&] { return res.get(); });
      }};
}

static inline auto asResult(Future<Result>&& f) noexcept {
  return FutureTransformAwaitable{
      std::move(f), [](futures::Try<Result>&& res) noexcept -> Result {
        return basics::catchToResult([&] { return res.get(); });
      }};
}

template<typename T>
auto asResult(Future<ResultT<T>>&& f) noexcept {
  return FutureTransformAwaitable{
      std::move(f), [](futures::Try<ResultT<T>>&& res) noexcept -> ResultT<T> {
        return basics::catchToResult([&] { return res.get(); });
      }};
}

}  // namespace arangodb::futures

template<typename T, typename... Args>
struct std_coro::coroutine_traits<arangodb::futures::Future<T>, Args...> {
  struct promise_type {
    arangodb::futures::Promise<T> promise;
    arangodb::futures::Try<T> result;

    auto initial_suspend() noexcept { return std_coro::suspend_never{}; }
    auto final_suspend() noexcept {
      // TODO use symmetric transfer here
      struct awaitable {
        bool await_ready() noexcept { return false; }
        bool await_suspend(std::coroutine_handle<promise_type> self) noexcept {
          // we have to destroy the coroutine frame before
          // we resolve the promise
          _promise->promise.setTry(std::move(_promise->result));
          return false;
        }
        void await_resume() noexcept {}

        promise_type* _promise;
      };

      return awaitable{this};
    }

    auto get_return_object() -> arangodb::futures::Future<T> {
      return promise.getFuture();
    }

    auto return_value(T const& t) noexcept(
        std::is_nothrow_copy_constructible_v<T>) {
      static_assert(std::is_copy_constructible_v<T>);
      result.emplace(t);
    }

    auto return_value(T&& t) noexcept(std::is_nothrow_move_constructible_v<T>) {
      static_assert(std::is_move_constructible_v<T>);
      result.emplace(std::move(t));
    }

    auto unhandled_exception() noexcept {
      result.set_exception(std::current_exception());
    }
  };
};

template<typename... Args>
struct std_coro::coroutine_traits<
    arangodb::futures::Future<arangodb::futures::Unit>, Args...> {
  struct promise_type {
    arangodb::futures::Promise<arangodb::futures::Unit> promise;
    arangodb::futures::Try<arangodb::futures::Unit> result;

    auto initial_suspend() noexcept { return std_coro::suspend_never{}; }
    auto final_suspend() noexcept {
      // TODO use symmetric transfer here
      struct awaitable {
        bool await_ready() noexcept { return false; }
        bool await_suspend(std::coroutine_handle<promise_type> self) noexcept {
          // we have to destroy the coroutine frame before
          // we resolve the promise
          _promise->promise.setTry(std::move(_promise->result));
          return false;
        }
        void await_resume() noexcept {}

        promise_type* _promise;
      };

      return awaitable{this};
    }

    auto get_return_object()
        -> arangodb::futures::Future<arangodb::futures::Unit> {
      return promise.getFuture();
    }

    auto return_void() noexcept { result.emplace(); }

    auto unhandled_exception() noexcept {
      result.set_exception(std::current_exception());
    }
  };
};
