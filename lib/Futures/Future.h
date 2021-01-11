////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_FUTURES_FUTURE_H
#define ARANGOD_FUTURES_FUTURE_H 1

#include <mellon/futures.h>

#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"

namespace arangodb::futures {
struct arangodb_tag {};

using Unit = void;

template <typename T>
using Try = ::expect::expected<T>;

template <typename T, typename future_type = ::mellon::future<Try<T>, arangodb_tag>>
struct Future;
template <typename T, typename G, typename S, typename future_type = ::mellon::future_temporary<Try<T>, G, Try<S>, arangodb_tag>>
struct FutureTemporary;

template <typename T>
auto wrap_future(::mellon::future<Try<T>, arangodb_tag>&& f) -> Future<T>;
template <typename T, typename F, typename R>
auto wrap_future(::mellon::future_temporary<Try<T>, F, Try<R>, arangodb_tag>&& f) -> FutureTemporary<T, F, R>;

template <typename T, typename future_type>
struct Future : future_type {
  using ::mellon::future<Try<T>, arangodb_tag>::future;
  explicit Future(future_type&& o) noexcept : future_type(std::move(o)) {}

  template <typename F, typename U = T, typename R = std::invoke_result_t<F, U&&>>
  auto thenValue(F&& f) && {
    return wrap_future(std::move(self()).then([f = std::forward<F>(f)](T&& t) -> R {
      return std::invoke(f, std::move(t));
    }));
  }

  template <typename F, typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0,
            typename R = std::invoke_result_t<F>>
  auto thenValue(F&& f) && {
    return wrap_future(std::move(self()).then([f = std::forward<F>(f)](T&& t) -> R {
      return std::invoke(f, std::move(t));
    }));
  }

  auto get() {
    return std::move(self()).await_unwrap();
  }

 private:
  future_type& self() { return *this; }
};


template <typename T, typename G, typename S, typename future_type>
struct FutureTemporary : future_type {
  using ::mellon::future_temporary<Try<T>, G, Try<S>, arangodb_tag>::future_temporary;
  explicit FutureTemporary(future_type&& o) noexcept : future_type(std::move(o)) {}

  template <typename F, typename U = T, typename R = std::invoke_result_t<F, U&&>>
  auto thenValue(F&& f) && {
    return wrap_future(std::move(self()).then([f = std::forward<F>(f)](T&& t) -> R {
      return std::invoke(f, std::move(t));
    }));
  }

  template <typename F, typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0,
      typename R = std::invoke_result_t<F>>
  auto thenValue(F&& f) && {
    return wrap_future(std::move(self()).then([f = std::forward<F>(f)](T&& t) -> R {
      return std::invoke(f, std::move(t));
    }));
  }

  /* implicit */ operator Future<S>() && noexcept {
    return Future<S>(std::move(*this).finalize());
  }

 private:
  future_type& self() { return *this; }
};

template <typename T, typename promise_type = ::mellon::promise<Try<T>, arangodb_tag>>
struct Promise : promise_type {
  template <typename... Args>
  auto setValue(Args&&... args) {
    std::move(self()).fulfill(std::in_place, std::forward<Args>(args)...);
  }

  explicit Promise(promise_type&& o) noexcept : promise_type(std::move(o)) {}

 private:
  promise_type& self() { return *this; }
};

template <typename T>
auto makePromise() {
  auto&& [f, p] = ::mellon::make_promise<Try<T>, arangodb_tag>();
  return std::make_pair(Future<T>(std::move(f)), Promise<T>(std::move(p)));
}

template <typename T>
auto makeFuture(T && t) {
  return Future<T>{std::in_place, std::forward<T>(t)};
}
auto makeFuture() {
  return Future<void>{std::in_place};
}

template <typename T>
auto wrap_future(::mellon::future<Try<T>, arangodb_tag>&& f) -> Future<T> {
  return Future<T>(std::move(f));
}
template <typename T, typename F, typename R>
auto wrap_future(::mellon::future_temporary<T, F, Try<R>, arangodb_tag>&& f) {
  return FutureTemporary<T, F, R>(std::move(f));
}

}  // namespace arangodb::futures

template <>
struct mellon::tag_trait<arangodb::futures::arangodb_tag> {
  /* */
  struct assertion_handler {
    void operator()(bool condition) const noexcept { TRI_ASSERT(condition); }
  };

  template <typename T>
  struct abandoned_promise_handler {
    T operator()() const noexcept {
      // call arangodb crash handler for abandoned promise
      std::terminate();
    }
  };

  template <typename T>
  struct abandoned_promise_handler<::expect::expected<T>> {
    ::expect::expected<T> operator()() const noexcept {
      try {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_PROMISE_ABANDONED);
      } catch (...) {
        return std::current_exception();
      }
    }
  };

  template <typename T>
  struct abandoned_future_handler {
    void operator()(T&& t) const noexcept {
      TRI_ASSERT(false);
      if constexpr (::expect::is_expected_v<T>) {
        if (t.has_error()) {
          // LOG UNCAUGHT EXCEPTION AND CRASH
          std::terminate();
        }
      }
      // WARNING ??????
    }
  };

  static constexpr auto small_value_size = 1024;
  /* */
};

template<typename T>
struct mellon::future_trait<arangodb::futures::Future<T>> {
  using value_type = arangodb::futures::Try<T>;
  using tag_type = arangodb::futures::arangodb_tag;
};


template<typename T>
struct mellon::is_future<arangodb::futures::Future<T>> : std::true_type {};

#endif  // ARANGOD_FUTURES_FUTURE_H
