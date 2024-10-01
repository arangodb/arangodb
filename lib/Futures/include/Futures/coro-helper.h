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
#include "Utils/ExecContext.h"

/// This file contains helper classes and tools for coroutines. We use
/// coroutines for asynchronous operations. Every function, method or
/// closure which contains at least one of the keywords
///  - co_await
///  - co_yield
///  - co_return
/// is a coroutine and is thus compiled differently by the C++ compiler
/// than normal. Essentially, the compiler creates a state machine for
/// each such functions. All instances of co_await and co_yield are potential
/// suspension points. The code before and after a co_await/co_yield can
/// be executed by different threads!
/// The return type of a coroutine plays a very special role. For us, it
/// will usually be `Future<T>` for some type T. The code in this file
/// uses this fact and essentially implements the magic for coroutines
/// by providing a few helper classes. See below for details.

/// See below at (*) for an explanation why we need this class
/// `FutureAwaitable` here!

namespace arangodb::futures {
template<typename T>
class Future;

template<typename T>
struct FutureAwaitable {
  [[nodiscard]] auto await_ready() const noexcept -> bool { return false; }
  bool await_suspend(std_coro::coroutine_handle<> coro) noexcept {
    // returning false resumes `coro`
    _execContext = ExecContext::currentAsShared();
    std::move(_future).thenFinal(
        [coro, this](futures::Try<T>&& result) mutable noexcept {
          _result = std::move(result);
          if (_counter.fetch_sub(1) == 1) {
            ExecContextScope exec(_execContext);
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
  std::shared_ptr<ExecContext const> _execContext;
};

/// See below at (*) for an explanation why we need this operator co_await
/// here!

template<typename T>
auto operator co_await(Future<T>&& f) noexcept {
  return FutureAwaitable<T>{std::move(f)};
}

template<typename T, typename F>
struct FutureTransformAwaitable : F {
  [[nodiscard]] auto await_ready() const noexcept -> bool { return false; }
  bool await_suspend(std_coro::coroutine_handle<> coro) noexcept {
    // returning false resumes `coro`
    _execContext = ExecContext::currentAsShared();
    std::move(_future).thenFinal(
        [coro, this](futures::Try<T>&& result) noexcept {
          _result = F::operator()(std::move(result));
          if (_counter.fetch_sub(1) == 1) {
            ExecContextScope exec(_execContext);
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
  std::shared_ptr<ExecContext const> _execContext;
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

/// For every coroutine, there must be a so-called `promise_type`, which
/// is a helper class providing a few methods to configure the behaviour
/// of the coroutine. This can either be a member type called `promise_type`
/// of the return type of the coroutine, or, as in our case, it is determined
/// using the `std_coro::coroutine_traits` template with template parameters
/// using the return type (see
///   https://en.cppreference.com/w/cpp/language/coroutines
/// under "Promise") and then some. Since our return type for coroutines
/// is `arangodb::futures::Future<T>`, we specialize this template here
/// to configure our coroutines (for an explanation see below the class):

template<typename T, typename... Args>
struct std_coro::coroutine_traits<arangodb::futures::Future<T>, Args...> {
  struct promise_type {
    // For some reason, non-maintainer compilation fails with a linker error
    // if these are missing or defaulted.
    promise_type(std::source_location loc = std::source_location::current())
        : promise{std::move(loc)} {}
    ~promise_type() {}

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

/// (*) Explanation for the details:
/// The `promise_type` holds two pieces of data:
///  - first an `arangodb::futures::Promise<T>` (not to be confused with the
///    promise_type!), and
///  - second an `arangodb::futures::Try<T>`
/// After all, we want that the coroutine "returns" an empty `Future<T>`
/// when it suspends, and it is supposed to set the return value (or
/// exception) via the corresponding `Promise<T>` object to trigger
/// potential callbacks which are attached to the Future<T>.
/// So how does this all work?
/// When the coroutine is first called an object of type `promise_type`
/// is contructed, which constructs its member `promise` of type
/// `Promise<T>`. Then, early in the life of the coroutine, the method
/// `get_return_object` is called, which builds an object of type
/// `Future<T>` from the `promise` member, so that it is associated with
/// the `promise` member. This is what will be returned when the coroutine
/// is first suspended.
/// Since `initial_suspend` returns `std_coro::suspend_never{}` no
/// suspension happens before the first code of the coroutine is run.
/// When the coroutine reaches a `co_await`, the expression behind it is
/// first evaluated. It is then the "awaitable" object (unless there is
/// a method `await_transform` in the current coroutines promise object,
/// which we haven't). In most cases, this will be another `Future<T'>`
/// which is returned from another coroutine.
/// The "awaitable" is now transformed to an "awaiter". This is done by
/// means of an `operator co_await` defined earlier in this file. It
/// essentially wraps our `Future<T'>` into a `FutureAwaitable` class
/// also defined above.
/// The C++ coroutine framework will then cal methods on the "awaiter"
/// for events to unfold: First it calls `await_ready` to see if we have
/// to suspend after all. We always return `false` there.
/// Then it calls `await_suspend` to suspend and later `await_resume` to
/// resume. The `FutureAwaitable` class essentially attaches a closure
/// to the `Future<T'>` which resumes the coroutine.

/// The following is the version for return type `Future<Unit>`,
/// corresponding to coroutines which return nothing. The differences
/// are purely technical (`return_void` instead of `return_value`,
/// basically).

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
