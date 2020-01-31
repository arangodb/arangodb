////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_FUTURES_UTILITIES_H
#define ARANGOD_FUTURES_UTILITIES_H 1

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "Futures/Future.h"
#include "Futures/Unit.h"

namespace arangodb {
namespace futures {

template <class T>
Future<T> makeFuture(Try<T>&& t) {
  return Future<T>(detail::SharedState<T>::make(std::move(t)));
}

/// Make a complete void future
Future<Unit> makeFuture();

/// Make a completed Future by moving in a value. e.g.
template <class T>
Future<typename std::decay<T>::type> makeFuture(T&& t) {
  return makeFuture(Try<typename std::decay<T>::type>(std::forward<T>(t)));
}

/// Make a failed Future from an std::exception_ptr.
template <class T>
Future<T> makeFuture(std::exception_ptr const& e) {
  return makeFuture(Try<T>(e));
}

/// Make a Future from an exception type E that can be passed to
/// std::make_exception_ptr().
template <class T, class E>
typename std::enable_if<std::is_base_of<std::exception, E>::value, Future<T>>::type makeFuture(E const& e) {
  return makeFuture(Try<T>(std::make_exception_ptr<E>(e)));
}

// makeFutureWith(Future<T>()) -> Future<T>
template <typename F, typename R = std::result_of_t<F()>>
typename std::enable_if<isFuture<R>::value, R>::type makeFutureWith(F&& func) {
  using InnerType = typename isFuture<R>::inner;
  try {
    return std::forward<F>(func)();
  } catch (...) {
    return makeFuture<InnerType>(std::current_exception());
  }
}

// makeFutureWith(T()) -> Future<T>
// makeFutureWith(void()) -> Future<Unit>
template <typename F, typename R = std::result_of_t<F()>>
typename std::enable_if<!isFuture<R>::value, Future<R>>::type makeFutureWith(F&& func) {
  return makeFuture<R>(
      makeTryWith([&func]() mutable { return std::forward<F>(func)(); }));
}

namespace detail {

template <typename F>
void _foreach(F&&, size_t) {}

template <typename F, typename Arg, typename... Args>
void _foreach(F&& f, size_t i, Arg&& arg, Args&&... args) {
  f(i, std::forward<Arg>(arg));
  _foreach(i + 1, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename F, typename... Args>
void foreach (F&& f, Args && ... args) {
  _foreach(std::forward<F>(f), 0, args...);
}
};  // namespace detail

/// @brief When all the input Futures complete, the returned Future will complete.
/// Errors do not cause early termination; this Future will always succeed
/// after all its Futures have finished (whether successfully or with an
/// error).
/// The Futures are moved in, so your copies are invalid. If you need to
/// chain further from these Futures, use the variant with an output iterator.
/// This function is thread-safe for Futures running on different threads.
/// It will complete in whichever thread the last Future completes in.
/// @return for (Future<T1>, Future<T2>, ...) input is Future<std::tuple<Try<T1>, Try<T2>, ...>>.
//template <typename... Fs>
//Future<std::tuple<Try<typename isFuture<Fs>::inner>...>> collectAll(Fs&&... fs) {
//  using Result = std::tuple<Try<typename isFuture<Fs>::inner>...>;
//  struct Context {
//    ~Context() { p.setValue(std::move(results)); }
//    Promise<Result> p;
//    Result results;
//  };
//  auto ctx = std::make_shared<Context>();
//
//  detail::foreach (
//      [&](auto i, auto&& f) {
//        f.then([i, ctx](auto&& t) { std::get<i>(ctx->results) = std::move(t); });
//      },
//      std::move(fs)...);
//  return ctx->p.getFuture();
//}

/// @brief When all the input Futures complete, the returned Future will
/// complete. Errors do not cause early termination; this Future will always
/// succeed after all its Futures have finished (whether successfully or with an
/// error).
/// The Futures are moved in, so your copies are invalid. If you need to
/// chain further from these Futures, use the variant with an output iterator.
/// This function is thread-safe for Futures running on different threads. But
/// if you are doing anything non-trivial after, you will probably want to
/// follow with `via(executor)` because it will complete in whichever thread the
/// last Future completes in.
/// The return type for Future<T> input is a Future<std::vector<Try<T>>>
template <class InputIterator>
Future<std::vector<Try<typename std::iterator_traits<InputIterator>::value_type::value_type>>> collectAll(
    InputIterator first, InputIterator last) {
  using FT = typename std::iterator_traits<InputIterator>::value_type;
  using T = typename FT::value_type;

  struct Context {
    explicit Context(size_t n) : results(n) {}
    ~Context() { p.setValue(std::move(results)); }
    Promise<std::vector<Try<T>>> p;
    std::vector<Try<T>> results;
  };

  auto ctx = std::make_shared<Context>(size_t(std::distance(first, last)));
  for (size_t i = 0; first != last; ++first, ++i) {
    first->thenFinal([i, ctx](auto&& t) { ctx->results[i] = std::move(t); });
  }

  return ctx->p.getFuture();
}

template <class Collection>
auto collectAll(Collection&& c) -> decltype(collectAll(std::begin(c), std::end(c))) {
  return collectAll(std::begin(c), std::end(c));
}


namespace detail{
namespace gather {

template<typename C, typename... Ts, std::size_t... I>
void thenFinalAll(C& c, std::index_sequence<I...>, Future<Ts>&&... ts) {
  (std::move(ts).thenFinal([&](Try<Ts> &&t) { std::get<I>(c->results) = std::move(t); }),...);
}
}
}

// like collectAll but uses a tuple instead and works with different types
// returns a Future<std::tuple<Try<Ts>>...>
template<typename... Ts>
auto gather(Future<Ts>&&... r) {

  using try_tuple = std::tuple<Try<Ts>...>;

  struct Context {
    ~Context() { p.setValue(std::move(results)); }
    try_tuple results;
    Promise<try_tuple> p;
  };

  auto ctx = std::make_shared<Context>();
  detail::gather::thenFinalAll(ctx, std::index_sequence_for<Ts...>{}, std::forward<Future<Ts>>(r)...);
  return ctx->p.getFuture();
};

template<typename T>
T gather(T t) { return t; };



namespace detail {
namespace collect {

template<typename C, typename... Ts, std::size_t... I>
void thenFinalAll(C& c, std::index_sequence<I...>, Future<Ts>&&... ts) {

  (std::move(ts).thenFinal([c](Try<Ts>&& t) {
    if (t.hasException()) {
      if (c->hadError.exchange(true, std::memory_order_release) == false) {
        c->p.setException(std::move(t).exception());
      }
    } else {
      std::get<I>(c->results) = std::move(t);
    }
  }), ...);
}

template<typename... Ts, std::size_t... I>
auto unpackAll(std::tuple<Try<Ts>...> &c, std::index_sequence<I...>) -> std::tuple<Ts...> {
  return std::make_tuple(std::move(std::get<I>(c)).get()...);
}

template<typename... Ts>
auto unpackAll(std::tuple<Try<Ts>...> &c) -> std::tuple<Ts...> {
  return unpackAll(c, std::index_sequence_for<Ts...>{});
}


}
}

// like collectAll but uses a tuple instead and works with different types
// returns a Future<Try<std::tuple<Ts>>...>
template<typename... Ts>
auto collect(Future<Ts>&&... r) {
  using try_tuple = std::tuple<Try<Ts>...>;
  using value_tuple = std::tuple<Ts...>;

  struct Context {
    Context() : hadError(false) {}
    ~Context() {
      if (hadError.load(std::memory_order_acquire) == false) {
        p.setValue(detail::collect::unpackAll(results));
      }
    }
    try_tuple results;
    Promise<value_tuple> p;
    std::atomic<bool> hadError;
  };

  auto ctx = std::make_shared<Context>();
  detail::collect::thenFinalAll(ctx, std::index_sequence_for<Ts...>{}, std::forward<Future<Ts>>(r)...);
  return ctx->p.getFuture();
}

template<typename T>
T collect(T t) { return t; }


}  // namespace futures
}  // namespace arangodb
#endif  // ARANGOD_FUTURES_UTILITIES_H
