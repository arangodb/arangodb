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

#ifndef ARANGOD_FUTURES_FUTURE_H
#define ARANGOD_FUTURES_FUTURE_H 1

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include "Futures/SharedState.h"
#include "Futures/Promise.h"

namespace arangodb {
namespace futures {
  
template<typename T>
class Promise;
  
template<typename T>
class Future;
  
template<typename T>
struct isFuture {
  static constexpr bool value = false;
};

template<typename T>
struct isFuture<Future<T>> {
  static constexpr bool value = true;
  typedef T inner;
};
  
/// Simple Future library based on Facebooks Folly
template<typename T>
class Future {
  friend class Promise<T>;
  template <class T2>
  friend Future<T2> makeFuture(Try<T2>&&);
  
public:
  
  static Future<T> makeEmpty() {
    return Future<T>();
  }
  
  /// @brief Constructs a Future with no shared state.
  /// After construction, valid() == false.
  explicit Future() noexcept : _state(nullptr) {}
  
  /// Construct a Future from a value (perfect forwarding)
  template <
  class T2 = T,
  typename = typename std::enable_if<
  !isFuture<typename std::decay<T2>::type>::value>::type>
  /* implicit */ Future(T2&& val) : _state(detail::SharedState<T>::make(std::forward<T2>(val))) {}
  
  /// Construct a Future from a `T` constructed from `args`
  
  Future(Future const& o) = delete;
  Future(Future<T>&& o) noexcept : _state(std::move(o._state)) {}
  Future& operator=(Future const&) = delete;
  Future& operator=(Future<T>&& o) noexcept {
    _state = std::move(o._state);
  }
  
  ~Future() {
    detach();
  }
  
  /// is there a shared state set
  bool valid() const noexcept { return _state != nullptr; }
  
  // True when the result (or exception) is ready
  bool isReady() const {
    return getState().hasResult();
  }
  
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
  
  T get() & {
    wait();
    return result().get();
  }
  
  /// waits and moves the result out
  T get() && {
    wait();
    return std::move(*this).result().get();
  }
  
  template<class Rep, class Period >
  T get(const std::chrono::duration<Rep,Period>& duration) & {
    wait_for(duration);
    return result().get();
  }
  
  template<class Rep, class Period >
  T get(const std::chrono::duration<Rep,Period>& duration) && {
    wait_for(duration);
    return std::move(*this).result().get();
  }
  
  /// Returns a reference to the result value if it is ready, with a reference
  /// category and const-qualification like those of the future.
  /// Does not `wait()`; see `get()` for that.
  /*T& value() &;
  T const& value() const&;
  T&& value() &&;
  T const&& value() const&&;*/
  
  /// Returns a reference to the result's Try if it is ready, with a reference
  /// category and const-qualification like those of the future.
  /// Does not `wait()`; see `get()` for that.
  Try<T>& result() & {
    return getStateTryChecked();
  }
  Try<T> const& result() const& {
    return getStateTryChecked();
  }
  Try<T>&& result() && {
    return std::move(getStateTryChecked());
  }
  Try<T> const&& result() const&& {
    return std::move(getStateTryChecked());
  }


  /// Blocks until this Future is complete.
  void wait() const {
    while(!isReady()) {
      std::this_thread::yield();
    }
  }
  
  /// waits for the result, returns if it is not available
  /// for the specified timeout duration. Future must be valid
  template<class Rep, class Period >
  std::future_status wait_for(const std::chrono::duration<Rep,Period>& timeout_duration) const {
    return wait_until(std::chrono::steady_clock::now() + timeout_duration);
  }
  
  /// waits for the result, returns if it is not available until
  /// specified time point. Future must be valid
  template<class Clock, class Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock,Duration>& timeout_time) const {
    if (isReady()) {
      return std::future_status::ready;
    }
    std::this_thread::yield();
    while(!isReady()) {
      if (Clock::now() > timeout_time) {
        return std::future_status::timeout;
      }
      std::this_thread::yield();
    }
    return std::future_status::ready;
  }
  
  
  /// Blocks until this Future is complete.
  /*void wait() && {
   TRI_ASSERT(valid());
   if (isReady()) {
   return;
   }
   
   get
   
   while(!isReady()) {
   std::this_thread::yield();
   }
   }*/
  
  /// When this Future has completed, execute func which is a function that
  /// can be called with either `T&&` or `Try<T>&&`.
  template <typename F>
  typename std::enable_if<std::is_same<std::result_of<F>, void>::value>::value
    then(F&& func) & {
    getState().onCallback(std::forward(func));
  }
  
  // Variant: function returns a value
  // e.g. f.then([](Try<T>&& t){ return t.value(); });
  // @return a Future what will get the result of func
  template <typename F,
  typename R = typename std::result_of<F>::type,
  typename std::enable_if<!std::is_same<R, void>::value &&
                          !isFuture<R>::value>::type = 0>
  Future<R> then(F&& func) && {
    //static_assert<>
    Promise<R> promise;
    auto future = promise.get_future();
    getState().onCallback([fn = std::forward<F>(func),
                           pr = std::move(promise)](Try<T>&& t) {
      try {
        pr.set_value(fn(std::move(t)));
      } catch(...) {
        pr.set_exception(std::current_exception);
      }
    });
    return std::move(future);
  }

  template <typename F,
  typename R = typename std::result_of<F>::type,
  typename std::enable_if<!std::is_same<R, void>::value &&
                          isFuture<R>::value>::type = 0>
  Future<typename isFuture<R>::inner> then(F&& func) && {
    typedef typename isFuture<R>::inner B;
    static_assert(!isFuture<B>::value, "");
    
    Promise<B> promise;
    auto future = promise.get_future();
    getState().onCallback([fn = std::forward<F>(func),
                           pr = std::move(promise)](Try<T>&& t) {
      try {
        fn(std::move(t)).then([pr2 = std::move(pr)](Try<B>&& t) {
          pr2.set_value(std::move(t));
        });
      } catch(...) {
        pr.set_exception(std::current_exception);
      }
    });
    return std::move(future);
  }
  
private:
  
  explicit Future(detail::SharedState<T>* state) : _state(state) {}
  
  // convenience method that checks if _state is set
  inline detail::SharedState<T>& getState() {
    if (!_state) {
      throw std::future_error(std::future_errc::no_state);
    }
    return *_state;
  }
  inline detail::SharedState<T> const& getState() const {
    if (!_state) {
      throw std::future_error(std::future_errc::no_state);
    }
    return *_state;
  }
  
  Try<T>& getStateTryChecked() {
    return getStateTryChecked(*this);
  }
  Try<T> const& getStateTryChecked() const {
    return getStateTryChecked(*this);
  }

  template <typename Self>
  static decltype(auto) getStateTryChecked(Self& self) {
    auto& state = self.getState();
    if (!state.hasResult()) {
      throw std::logic_error("future not ready");
    }
    return state.getTry();
  }
  
  void detach() {
    if (_state) {
      _state->detachFuture();
      _state = nullptr;
    }
  }
  
private:
  detail::SharedState<T>* _state;
};

template <class T>
Future<typename std::decay<T>::type> makeFuture(T&& t) {
  return makeFuture(Try<typename std::decay<T>::type>(std::forward<T>(t)));
}

template <class F>
Future<std::result_of<F>> makeFutureWith(F&& f) {
  return makeFuture(std::move(makeTry(std::forward(f))));
}
  
template <class T>
Future<T> makeFuture(Try<T>&& t) {
  return Future<T>(detail::SharedState<T>::make(std::move(t)));
}
  
}}

#endif // ARANGOD_FUTURES_SHARED_STATE_H
