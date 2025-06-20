////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Async/Registry/registry_variable.h"
#include "Async/coro-utils.h"
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 14000
#include <experimental/coroutine>
namespace std_coro = std::experimental;
#else
#include <coroutine>
namespace std_coro = std;
#endif
#include <optional>

#include "Async/context.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Promise.h"
#include "Try.h"
#include "Utils/ExecContext.h"

/// This file contains helper classes and tools for coroutines.

template<typename T>
struct future_promise;

/**
   Promise type for a future coroutine

   The type holds two pieces of data:
   - first an `arangodb::futures::Promise<T>` (not to be confused with the
     promise_type!), and
   - second an `arangodb::futures::Try<T>`
   After all, we want that the coroutine "returns" an empty `Future<T>`
   when it suspends, and it is supposed to set the return value (or
   exception) via the corresponding `Promise<T>` object to trigger
   potential callbacks which are attached to the Future<T>.
 */
template<typename T>
struct future_promise_base {
  using promise_type = future_promise<T>;

  future_promise_base(std::source_location loc)
      : promise{std::move(loc)}, context{} {
    *arangodb::async_registry::get_current_coroutine() = {promise.id().value()};
  }
  ~future_promise_base() {}

  auto initial_suspend() noexcept {
    promise.update_state(arangodb::async_registry::State::Running);
    return std_coro::suspend_never{};
  }
  auto final_suspend() noexcept {
    // TODO use symmetric transfer here
    struct awaitable {
      bool await_ready() noexcept { return false; }
      bool await_suspend(std::coroutine_handle<promise_type> self) noexcept {
        _promise->context.set();
        // we have to destroy the coroutine frame before
        // we resolve the promise
        _promise->promise.setTry(std::move(_promise->result));
        return false;
      }
      void await_resume() noexcept {}

      promise_type* _promise;
    };

    return awaitable{static_cast<promise_type*>(this)};
  }

  template<typename U>
  auto await_transform(
      U&& co_awaited_expression,
      std::source_location loc = std::source_location::current()) noexcept {
    using inner_awaitable_type = decltype(arangodb::get_awaitable_object(
        std::forward<U>(co_awaited_expression)));

    struct awaitable {
      bool await_ready() { return inner_awaitable.await_ready(); }
      auto await_suspend(std::coroutine_handle<> handle) {
        outer_promise->promise.update_state(
            arangodb::async_registry::State::Suspended);
        outer_promise->context.set();
        return inner_awaitable.await_suspend(handle);
      }
      auto await_resume() {
        auto old_state = outer_promise->promise.update_state(
            arangodb::async_registry::State::Running);
        if (old_state.has_value() &&
            old_state.value() == arangodb::async_registry::State::Suspended) {
          outer_promise->context.update();
        }
        myContext.set();
        return inner_awaitable.await_resume();
      }

      promise_type* outer_promise;
      inner_awaitable_type inner_awaitable;
      arangodb::Context myContext;
    };

    // update promises in registry
    if constexpr (arangodb::CanUpdateRequester<U>) {
      co_awaited_expression.update_requester(promise.id());
    }
    promise.update_source_location(std::move(loc));

    return awaitable{.outer_promise = static_cast<promise_type*>(this),
                     .inner_awaitable = arangodb::get_awaitable_object(
                         std::forward<U>(co_awaited_expression)),
                     .myContext = arangodb::Context{}};
  }

  auto get_return_object() -> arangodb::futures::Future<T> {
    return promise.getFuture();
  }

  auto unhandled_exception() noexcept {
    result.set_exception(std::current_exception());
    context.set();
  }

  arangodb::futures::Promise<T> promise;
  arangodb::futures::Try<T> result;
  arangodb::Context context;
};

template<typename T>
struct future_promise : future_promise_base<T> {
  future_promise(std::source_location loc = std::source_location::current())
      : future_promise_base<T>(std::move(loc)) {}
  auto return_value(
      T const& t,
      std::source_location loc = std::source_location::
          current()) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    static_assert(std::is_copy_constructible_v<T>);
    future_promise_base<T>::promise.update_state(
        arangodb::async_registry::State::Resolved);
    future_promise_base<T>::promise.update_source_location(std::move(loc));
    future_promise_base<T>::result.emplace(t);
  }

  auto return_value(
      T&& t, std::source_location loc = std::source_location::
                 current()) noexcept(std::is_nothrow_move_constructible_v<T>) {
    static_assert(std::is_move_constructible_v<T>);
    future_promise_base<T>::promise.update_state(
        arangodb::async_registry::State::Resolved);
    future_promise_base<T>::promise.update_source_location(std::move(loc));
    future_promise_base<T>::result.emplace(std::move(t));
  }
};

template<>
struct future_promise<arangodb::futures::Unit>
    : future_promise_base<arangodb::futures::Unit> {
  future_promise(std::source_location loc = std::source_location::current())
      : future_promise_base<arangodb::futures::Unit>(std::move(loc)) {}
  auto return_void(
      std::source_location loc = std::source_location::current()) noexcept {
    promise.update_state(arangodb::async_registry::State::Resolved);
    promise.update_source_location(std::move(loc));
    result.emplace();
  }
};

/**
   With this definition, Future<T> can be used as a coroutine
 */
template<typename T, typename... Args>
struct std_coro::coroutine_traits<arangodb::futures::Future<T>, Args...> {
  using promise_type = future_promise<T>;
};

/**
   With this definition, Future<arangodb::futures::Unit> can be used as a
   coroutine
 */
template<typename... Args>
struct std_coro::coroutine_traits<
    arangodb::futures::Future<arangodb::futures::Unit>, Args...> {
  using promise_type = future_promise<arangodb::futures::Unit>;
};

namespace arangodb::futures {

/**
   Be able to call co_await on a future
 */
template<typename T>
auto operator co_await(Future<T>&& f) noexcept {
  struct FutureAwaitable {
    [[nodiscard]] auto await_ready() const noexcept -> bool { return false; }
    bool await_suspend(std_coro::coroutine_handle<> coro) noexcept {
      std::move(_future).thenFinal(
          [coro, this](futures::Try<T>&& result) mutable noexcept {
            _result = std::move(result);
            if (_counter.fetch_sub(1) == 1) {
              coro.resume();
            }
          });
      // returning false resumes `coro`
      return _counter.fetch_sub(1) != 1;
    }
    auto await_resume() -> T { return std::move(_result.value().get()); }
    explicit FutureAwaitable(Future<T> fut) : _future(std::move(fut)) {}

   private:
    std::atomic_uint8_t _counter{2};
    Future<T> _future;
    std::optional<futures::Try<T>> _result;
  };

  return FutureAwaitable{std::move(f)};
}

/**
   Be able to call co_await on some transformation of a future

   Transformations are defined below
 */
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
    // returning false resumes `coro`
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
