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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_FUTURES_FUTURE_H
#define ARANGOD_FUTURES_FUTURE_H 1

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Futures/Exceptions.h"
#include "Futures/Promise.h"
#include "Futures/SharedState.h"
#include "Futures/Unit.h"

namespace arangodb {
namespace futures {

template <typename T>
class Future;

template <typename T>
class Promise;

template <typename T>
struct isFuture {
  static constexpr bool value = false;
  typedef typename lift_unit<T>::type inner;
};

template <typename T>
struct isFuture<Future<T>> {
  static constexpr bool value = true;
  // typedef T inner;
  typedef typename lift_unit<T>::type inner;
};

/// @brief Specifies state of a future as returned by wait_for and wait_until
enum class FutureStatus : uint8_t {
  Ready,
  Timeout
};

namespace detail {

template <typename...>
struct ArgType;

template <typename Arg, typename... Args>
struct ArgType<Arg, Args...> {
  typedef Arg FirstArg;
};

template <>
struct ArgType<> {
  typedef void FirstArg;
};

template <typename F, typename... Args>
struct argResult {
  using ArgList = ArgType<Args...>;
  using Result = typename std::result_of<F(Args...)>::type;
};

template <typename T, typename F>
struct tryCallableResult {
  typedef detail::argResult<F, Try<T>&&> Arg;
  typedef isFuture<typename Arg::Result> ReturnsFuture;
  typedef typename Arg::ArgList::FirstArg FirstArg;
  typedef Future<typename ReturnsFuture::inner> FutureT;
};

template <typename T, typename F>
struct valueCallableResult {
  typedef detail::argResult<F, T&&> Arg;
  typedef isFuture<typename Arg::Result> ReturnsFuture;
  typedef typename Arg::ArgList::FirstArg FirstArg;
  typedef Future<typename ReturnsFuture::inner> FutureT;
};

template <class F, typename R = typename std::result_of<F()>::type>
typename std::enable_if<!std::is_same<R, void>::value, Try<R>>::type makeTryWith(F&& func) noexcept {
  try {
    return Try<R>(std::in_place, func());
  } catch (...) {
    return Try<R>(std::current_exception());
  }
}

template <class F, typename R = typename std::result_of<F()>::type>
typename std::enable_if<std::is_same<R, void>::value, Try<Unit>>::type makeTryWith(F&& func) noexcept {
  try {
    func();
    return Try<Unit>(unit);
  } catch (...) {
    return Try<Unit>(std::current_exception());
  }
}

// decay variant that does not remove cv-qualifier
template <class T>
struct decay {
 private:
  typedef typename std::remove_reference<T>::type U;

 public:
  typedef
      typename std::conditional<std::is_function<U>::value, typename std::add_pointer<U>::type, T>::type type;
};

template <class T>
using decay_t = typename decay<T>::type;

struct EmptyConstructor {};

// uses a condition_variable to wait
template<typename T>
void waitImpl(Future<T>& f) {
  if (f.isReady()) {
    return; // short-circuit
  }

  std::mutex m;
  std::condition_variable cv;

  Promise<T> p;
  Future<T> ret = p.getFuture();
  f.thenFinal([p(std::move(p)), &cv, &m](Try<T>&& t) mutable {
    // We need to hold this mutex, while sending the notify.
    // Otherwise the future ret may be ready and thereby leaving this function
    // which would free the condtion variable, before sending notify.
    // This is one of the rare cases where notify without lock would cause undefined behaviour.
    std::lock_guard<std::mutex> guard(m);
    p.setTry(std::move(t));
    cv.notify_one();
  });
  std::unique_lock<std::mutex> lock(m);
  cv.wait(lock, [&ret]{ return ret.isReady(); });
  f = std::move(ret);
}

// uses a condition_variable to wait
template<typename T, typename Clock, typename Duration>
void waitImpl(Future<T>& f, std::chrono::time_point<Clock, Duration> const& tp) {
  if (f.isReady()) {
    return; // short-circuit
  }

  std::mutex m;
  std::condition_variable cv;

  Promise<T> p;
  Future<T> ret = p.getFuture();
  f.thenFinal([p(std::move(p)), &cv, &m](Try<T>&& t) mutable {
    // We need to hold this mutex, while sending the notify.
    // Otherwise the future ret may be ready and thereby leaving this function
    // which would free the condtion variable, before sending notify.
    // This is one of the rare cases where notify without lock would cause undefined behaviour.
    std::lock_guard<std::mutex> guard(m);
    p.setTry(std::move(t));
    cv.notify_one();
  });
  std::unique_lock<std::mutex> lock(m);
  cv.wait_until(lock, tp, [&ret]{ return ret.isReady(); });
  f = std::move(ret);
}
}  // namespace detail

/// Simple Future library based on Facebooks Folly
template <typename T>
class Future {
  static_assert(!std::is_same<void, T>::value,
                "use futures::Unit instead of void");

  friend class Promise<T>;
  template <class T2>
  friend Future<T2> makeFuture(Try<T2>&&);
  friend Future<Unit> makeFuture();

 public:
  /// @brief value type of the future
  typedef T value_type;

  /// @brief Constructs a Future with no shared state.
  static Future<T> makeEmpty() { return Future<T>(detail::EmptyConstructor{}); }

  // Construct a Future from a value (perfect forwarding)
  template <class T2 = T, typename = typename std::enable_if<!std::is_same<T2, void>::value &&
                                                             !isFuture<typename std::decay<T2>::type>::value>::type>
  /* implicit */ Future(T2&& val)
      : _state(detail::SharedState<T>::make(Try<T>(std::forward<T2>(val)))) {}

  // Construct a (logical) Future-of-void.
  // cppcheck-ignore noExplicitConstructor
  template <class T2 = T>
  /* implicit */ Future(typename std::enable_if<std::is_same<Unit, T2>::value>::type* p = nullptr)
      : _state(detail::SharedState<T2>::make(Try<Unit>())) {}

  // Construct a Future from a `T` constructed from `args`
  template <class... Args, typename std::enable_if<std::is_constructible<T, Args&&...>::value, int>::type = 0>
  explicit Future(std::in_place_t, Args&&... args)
      : _state(detail::SharedState<T>::make(std::in_place, std::forward<Args>(args)...)) {}

  Future(Future const& o) = delete;
  Future(Future<T>&& o) noexcept : _state(std::move(o._state)) {
    o._state = nullptr;
  }
  Future& operator=(Future const&) = delete;
  Future& operator=(Future<T>&& o) noexcept {
    if (this != &o) {
      detach();
      std::swap(_state, o._state);
      TRI_ASSERT(o._state == nullptr);
    }
    return *this;
  }

  ~Future() { detach(); }

  /// is there a shared state set
  bool valid() const noexcept { return _state != nullptr; }

  // True when the result (or exception) is ready
  bool isReady() const { return getState().hasResult(); }

  /// True if the future already has a callback set
  bool hasCallback() const { return getState().hasCallback(); }

  /// True if the result is a value (not an exception)
  bool hasValue() const {
    TRI_ASSERT(isReady());
    return result().hasValue();
  }

  /// True if the result is an exception (not a value) on a future
  bool hasException() const {
    TRI_ASSERT(isReady());
    return result().hasException();
  }

  // template <bool x = std::is_copy_constructible<T>::value,
  //          std::enable_if_t<x,int> = 0>
  T& get() & {
    wait();
    return result().get();
  }

  /// waits and moves the result out
  T get() && {
    wait();
    return Future<T>(std::move(*this)).result().get();
  }

  template <class Rep, class Period>
  T get(const std::chrono::duration<Rep, Period>& duration) & {
    wait_for(duration);
    return result().get();
  }

  template <class Rep, class Period>
  T get(const std::chrono::duration<Rep, Period>& duration) && {
    wait_for(duration);
    return Future<T>(std::move(*this)).result().get();
  }

  /// Blocks until the future is fulfilled. Returns the Try of the result
  Try<T>& getTry() & {
    wait();
    return getStateTryChecked();
  }

  Try<T>&& getTry() && {
    wait();
    return std::move(getStateTryChecked());
  }

  /// Returns a reference to the result's Try if it is ready, with a reference
  /// category and const-qualification like those of the future.
  /// Does not `wait()`; see `get()` for that.
  Try<T>& result() & { return getStateTryChecked(); }
  Try<T> const& result() const& { return getStateTryChecked(); }
  Try<T>&& result() && { return std::move(getStateTryChecked()); }
  Try<T> const&& result() const&& { return std::move(getStateTryChecked()); }

  /// Blocks until this Future is complete.
  void wait() {
    detail::waitImpl(*this);
  }

  /// waits for the result, returns if it is not available
  /// for the specified timeout duration. Future must be valid
  template <class Rep, class Period>
  FutureStatus wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) {
    return wait_until(std::chrono::steady_clock::now() + timeout_duration);
  }

  /// waits for the result, returns if it is not available until
  /// specified time point. Future must be valid
  template <class Clock, class Duration>
  FutureStatus wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) {
    detail::waitImpl(*this, timeout_time);
    return FutureStatus::Ready;
  }

  /// When this Future has completed, execute func which is a function that
  /// can be called with either `T&&` or `Try<T>&&`.
  ///
  /// Func shall return either another Future or a value.
  ///
  /// A Future for the return type of func is returned.
  ///
  ///   Future<string> f2 = f1.then([](Try<T>&&) { return string("foo"); });
  ///
  /// Preconditions:
  ///
  /// - `valid() == true` (else throws FutureException(ErrorCode::NoState)))
  ///
  /// Postconditions:
  ///
  /// - Calling code should act as if `valid() == false`,
  ///   i.e., as if `*this` was moved into RESULT.
  /// - `RESULT.valid() == true`

  /// Variant: callable accepts T&&, returns value
  ///  e.g. f.then([](T&& t){ return t; });
  template <typename F, typename R = detail::valueCallableResult<T, F>>
  typename std::enable_if<!isTry<typename R::FirstArg>::value && !R::ReturnsFuture::value, typename R::FutureT>::type
  thenValue(F&& fn) && {
    typedef typename R::ReturnsFuture::inner B;
    using DF = detail::decay_t<F>;

    static_assert(!isFuture<B>::value, "");
    static_assert(!std::is_same<B, void>::value, "");
    static_assert(!R::ReturnsFuture::value, "");

    Promise<B> promise;
    auto future = promise.getFuture();
    getState().setCallback(
        [fn = std::forward<DF>(fn), pr = std::move(promise)](Try<T>&& t) mutable {
          if (t.hasException()) {
            pr.setException(std::move(t).exception());
          } else {
            pr.setTry(detail::makeTryWith([&fn, &t] {
              return std::invoke(std::forward<DF>(fn), std::move(t).get());
            }));
          }
        });
    return future;
  }

  /// Variant: callable accepts T&&, returns future
  ///  e.g. f.then([](T&& t){ return makeFuture<T>(t); });
  template <typename F, typename R = detail::valueCallableResult<T, F>>
  typename std::enable_if<!isTry<typename R::FirstArg>::value && R::ReturnsFuture::value, typename R::FutureT>::type
  thenValue(F&& fn) && {
    typedef typename R::ReturnsFuture::inner B;
    using DF = detail::decay_t<F>;

    static_assert(!isFuture<B>::value, "");
    static_assert(std::is_invocable_r<Future<B>, F, T>::value,
                  "Function must be invocable with T");

    Promise<B> promise;
    auto future = promise.getFuture();
    getState().setCallback([fn = std::forward<DF>(fn),
                            pr = std::move(promise)](Try<T>&& t) mutable {
      if (t.hasException()) {
        pr.setException(std::move(t).exception());
      } else {
        try {
          auto f = std::invoke(std::forward<DF>(fn), std::move(t).get());
          std::move(f).then([pr = std::move(pr)](Try<B>&& t) mutable {
            pr.setTry(std::move(t));
          });
        } catch (...) {
          pr.setException(std::current_exception());
        }
      }
    });
    return future;
  }

  /// Variant: callable accepts Try<T&&>, returns value
  ///  e.g. f.then([](T&& t){ return t; });
  template <typename F, typename R = detail::tryCallableResult<T, F>>
  typename std::enable_if<isTry<typename R::FirstArg>::value && !R::ReturnsFuture::value, typename R::FutureT>::type
  then(F&& func) && {
    typedef typename R::ReturnsFuture::inner B;
    using DF = detail::decay_t<F>;

    static_assert(!isFuture<B>::value, "");
    static_assert(!std::is_same<B, void>::value, "");

    Promise<B> promise;
    auto future = promise.getFuture();
    getState().setCallback([fn = std::forward<DF>(func),
                            pr = std::move(promise)](Try<T>&& t) mutable {
      pr.setTry(detail::makeTryWith([&fn, &t] {
        return std::invoke(std::forward<DF>(fn), std::move(t));
      }));
    });
    return future;
  }

  /// Variant: callable accepts Try<T&&>, returns future
  ///  e.g. f.then([](T&& t){ return makeFuture<T>(t); });
  template <typename F, typename R = detail::tryCallableResult<T, F>>
  typename std::enable_if<isTry<typename R::FirstArg>::value && R::ReturnsFuture::value, typename R::FutureT>::type
  then(F&& func) && {
    typedef typename R::ReturnsFuture::inner B;
    static_assert(!isFuture<B>::value, "");

    Promise<B> promise;
    auto future = promise.getFuture();
    getState().setCallback([fn = std::forward<F>(func),
                            pr = std::move(promise)](Try<T>&& t) mutable {
      try {
        auto f = std::invoke(std::forward<F>(fn), std::move(t));
        std::move(f).then([pr = std::move(pr)](Try<B>&& t) mutable {
          pr.setTry(std::move(t));
        });
      } catch (...) {
        pr.setException(std::current_exception());
      }
    });
    return future;
  }

  /// Variant: function returns void and accepts Try<T>&&
  /// When this Future has completed, execute func which is a function that
  /// can be called with either `T&&` or `Try<T>&&`.
  template <typename F, typename R = typename std::result_of<F && (Try<T> &&)>::type>
  typename std::enable_if<std::is_same<R, void>::value>::type thenFinal(F&& fn) {
    getState().setCallback(std::forward<detail::decay_t<F>>(fn));
  }

  /// Set an error continuation for this Future where the continuation can
  /// be called with a known exception type and returns a `T`
  template <typename ExceptionType, typename F, typename R = std::result_of_t<F(ExceptionType)>>
  typename std::enable_if<!isFuture<R>::value, Future<typename lift_unit<R>::type>>::type thenError(
      F&& func) && {
    typedef typename lift_unit<R>::type B;
    typedef std::decay_t<ExceptionType> ET;
    using DF = detail::decay_t<F>;

    Promise<B> promise;
    auto future = promise.getFuture();
    getState().setCallback([fn = std::forward<DF>(func),
                            pr = std::move(promise)](Try<T>&& t) mutable {
      if (t.hasException()) {
        try {
          std::rethrow_exception(std::move(t).exception());
        } catch (ET& e) {
          pr.setTry(detail::makeTryWith([&fn, &e]() mutable {
            return std::invoke(std::forward<DF>(fn), e);
          }));
        } catch (...) {
          pr.setException(std::current_exception());
        }
      } else {
        pr.setTry(std::move(t));
      }
    });
    return future;
  }

  /// Set an error continuation for this Future where the continuation can
  /// be called with a known exception type and returns a `Future<T>`
  template <typename ExceptionType, typename F, typename R = std::result_of_t<F(ExceptionType)>>
  typename std::enable_if<isFuture<R>::value, Future<typename isFuture<R>::inner>>::type thenError(F&& fn) && {
    typedef typename isFuture<R>::inner B;
    typedef std::decay_t<ExceptionType> ET;
    using DF = detail::decay_t<F>;

    Promise<B> promise;
    auto future = promise.getFuture();
    getState().setCallback(
        [fn = std::forward<DF>(fn), pr = std::move(promise)](Try<T>&& t) mutable {
          if (t.hasException()) {
            try {
              std::rethrow_exception(std::move(t).exception());
            } catch (ET& e) {
              try {
                auto f = std::invoke(std::forward<DF>(fn), e);
                std::move(f).then([pr = std::move(pr)](Try<B>&& t) mutable {
                  pr.setTry(std::move(t));
                });
              } catch (...) {
                pr.setException(std::current_exception());
              }
            } catch (...) {
              pr.setException(std::current_exception());
            }
          } else {
            pr.setTry(std::move(t));
          }
        });
    return future;
  }

 private:
  explicit Future(detail::EmptyConstructor) : _state(nullptr) {}
  explicit Future(detail::SharedState<T>* state) : _state(state) {}

  // convenience method that checks if _state is set
  inline detail::SharedState<T>& getState() {
    if (!_state) {
      throw FutureException(ErrorCode::NoState);
    }
    return *_state;
  }
  inline detail::SharedState<T> const& getState() const {
    if (!_state) {
      throw FutureException(ErrorCode::NoState);
    }
    return *_state;
  }

  inline Try<T>& getStateTryChecked() { return getStateTryChecked(*this); }
  inline Try<T> const& getStateTryChecked() const {
    return getStateTryChecked(*this);
  }

  template <typename Self>
  static decltype(auto) getStateTryChecked(Self& self) {
    auto& state = self.getState();
    if (!state.hasResult()) {
      throw FutureException(ErrorCode::FutureNotReady);
    }
    return state.getTry();
  }

  void detach() noexcept {
    detail::SharedState<T>* state = nullptr;
    std::swap(state, _state);
    TRI_ASSERT(_state == nullptr);
    if (state) {
      // may delete the shared state, so must be last action
      state->detachFuture();
    }
  }

 private:
  detail::SharedState<T>* _state;
};

}  // namespace futures
}  // namespace arangodb
#endif  // ARANGOD_FUTURES_FUTURE_H
