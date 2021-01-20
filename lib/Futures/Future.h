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
template<typename T>
using Future = ::mellon::future<Try<T>, arangodb_tag>;
template<typename T>
using Promise = ::mellon::promise<Try<T>, arangodb_tag>;

template<typename T, template<typename> typename Fut, typename Tag>
struct scheduler_addition {};

}  // namespace arangodb::futures

template <>
struct mellon::tag_trait<arangodb::futures::arangodb_tag> {
  struct allocator {
    // TODO is this how you do such things?
    template <typename T>
    static T* allocate() {
      return reinterpret_cast<T*>(::operator new(sizeof(T)));
    }
    template <typename T>
    static T* allocate(std::nothrow_t) noexcept {
      return reinterpret_cast<T*>(::operator new(sizeof(T), std::nothrow));
    }
  };

  /* */
  struct assertion_handler {
    void operator()(bool condition) const noexcept {
      if (!condition) {
        //::arangodb::CrashHandler::assertionFailure();
      }
    }
  };

  struct debug_assertion_handler {
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
  static constexpr bool disable_temporaries = false;

  template<typename T, template<typename> typename Fut>
  struct user_defined_additions;
  template <typename T, template <typename> typename Fut>
  struct user_defined_additions<arangodb::futures::Try<T>, Fut>
      : arangodb::futures::scheduler_addition<T, Fut, arangodb::futures::arangodb_tag> {
    template <typename F>
    auto thenValue(F&& f) && noexcept {
      return std::move(self()).then(std::forward<F>(f));
    }

    template<typename F>
    auto thenFinal(F && f) && noexcept {
      return std::move(self()).finally(std::forward<F>(f));
    }

    template<typename E, typename F>
    auto thenError(F&& f) && noexcept {
      return std::move(self()).template catch_error<E>(std::forward<F>(f));
    }

   private:
    using future_type = Fut<arangodb::futures::Try<T>>;
    future_type& self() { return static_cast<future_type&>(*this); }
  };

  template<typename T>
  struct user_defined_promise_additions {

    void setValue(T t) {
      std::move(self()).fulfill(std::move(t));
    }

   private:
    using promise_type = mellon::promise<T, arangodb::futures::arangodb_tag>;
    promise_type& self() { return static_cast<promise_type&>(*this); }
  };
};

namespace arangodb::futures {

static inline auto makeFuture() -> Future<Unit> {
  return Future<Unit>{std::in_place};
}

template<typename T>
auto makeFuture(T t) -> Future<T> {
  return Future<T>{std::in_place, std::move(t)};
}

template<typename T>
auto makePromise() -> std::pair<Future<T>, Promise<T>> {
  return ::mellon::make_promise<Try<T>, arangodb_tag>();
}

}

#endif  // ARANGOD_FUTURES_FUTURE_H
