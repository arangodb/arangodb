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

#ifndef ARANGOD_FUTURES_SHARED_STATE_H
#define ARANGOD_FUTURES_SHARED_STATE_H 1

#include <atomic>
#include <chrono>

#include "Futures/SharedState.h""

namespace arangodb {
namespace futures {
  
/// Simple Future library based on Facebooks Folly
template<typename T>
class Future {
  friend class Promise<T>;
public:
  
  /// @brief Constructs a sFuture with no shared state.
  /// After construction, valid() == false.
  Future() noexcept : _state(nullptr) {}
  Future(Future const& o) = delete;
  Future(Future<T>&& o) noexcept : _state(std::move(o._state)) {}
  
  Future& operator=(Future const&) = delete;
  Future& operator=(Future<T>&& o) noexcept {
    _state = std::move(o._state);
  }
  
  /// is there a shared state set
  bool valid() const noexcept { return _state != nullptr; }
  
  // True when the result (or exception) is ready
  bool isReady() const {
    return getState().hasResult();
  }
  
  /// True if the result is a value (not an exception)
  bool hasValue() const {
    return result().hasValue();
  }
  
  /// True if the result is an exception (not a value) on a future
  bool hasException() const {
    return result().hasException();
  }
  
  template <class T>
  T& get() & {
    wait();
    return getState().get();
  }
  
  template <class T>
  T get() && {
    wait();
    return std::move(getState().get());
  }
  
  /// Returns a reference to the result value if it is ready, with a reference
  /// category and const-qualification like those of the future.
  /// Does not `wait()`; see `get()` for that.
  /*T& value() &;
  T const& value() const&;
  T&& value() &&;
  T const&& value() const&&;
  
  /// Returns a reference to the result's Try if it is ready, with a reference
  /// category and const-qualification like those of the future.
  /// Does not `wait()`; see `get()` for that.
  Try<T>& result() &;
  Try<T> const& result() const&;
  Try<T>&& result() &&;
  Try<T> const&& result() const&&;*/


  /// Blocks until this Future is complete.
  void wait() const {
    while(!isReady()) {
      std::this_thread::yield();
    }
  }
  
  template<class Rep, class Period >
  std::future_status wait_for( const std::chrono::duration<Rep,Period>& timeout_duration ) const {
    return wait_until(std::chrono::steady_clock::now() + timeout_duration);
  }
  
  template<class Clock, class Duration>
  std::future_status wait_until(const std::chrono::time_point<Clock,Duration>& timeout_time) {
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
  void wait() const {
    while(!isReady()) {
      std::this_thread::yield();
    }
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
  
  template <typename F, typename R = std::result_of<F>>
  typename void then(F&& func) && {
    getState.onCallback(std::forward(func));
  }
  
  /*template <typename F, typename R = std::result_of<F>>
  typename R::type then(F&& func) && {
    //static_assert<>
    
    getState.onCallback(std::forward(func));
    
    return this->template thenImplementation<F, R>(
                                                   std::forward<F>(func), typename R::Arg());
  }*/

  
private:
  Future(SharedState<T>* state) : _state(state) {}
  
  ~Future() {
    detach();
  }
  
  // convenience method that checks if _state is set
  inline SharedState<T>& getState() {
    if (!_state) {
      throw std::future_error(std::future_errc::no_state);
    }
    return *_state;
  }
  
  void detach() {
    if (_state) {
      _state->detachFuture();
      _state = nullptr;
    }
  }
  
private:
  SharedState<T>* _state;
};

  
}}

#endif // ARANGOD_FUTURES_SHARED_STATE_H
