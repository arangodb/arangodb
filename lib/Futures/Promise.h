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

#ifndef ARANGOD_FUTURES_PROMISE_H
#define ARANGOD_FUTURES_PROMISE_H 1

#include "Futures/Exceptions.h"
#include "Futures/SharedState.h"
#include "Futures/Unit.h"

namespace arangodb {
namespace futures {

template <typename T>
class Future;

/// producer side of future-promise pair
/// accesses on Promise have to be synchronized externally to
/// be thread-safe
template <typename T>
class Promise {
  static_assert(!std::is_same<void, T>::value, "Promise<Unit> instead of void");

 public:
  /// make invalid promise
  static Promise<T> makeEmpty() { return Promise(nullptr); }

  /// @brief Constructs a Promise with no shared state.
  /// After construction, valid() == true.
  Promise() : _state(detail::SharedState<T>::make()), _retrieved(false) {}

  Promise(Promise const& o) = delete;
  Promise(Promise<T>&& o) noexcept
      : _state(std::move(o._state)), _retrieved(o._retrieved) {
    o._state = nullptr;
  }

  ~Promise() { this->detach(); }

  Promise& operator=(Promise const&) = delete;
  Promise& operator=(Promise<T>&& o) noexcept {
    if (this != &o) {
      detach();
      _state = std::move(o._state);
      _retrieved = o._retrieved;
      o._retrieved = false;
      o._state = nullptr;
    }
    return *this;
  }

  bool valid() const noexcept { return _state != nullptr; }

  bool isFulfilled() const noexcept {
    if (_state) {
      return _state->hasResult();
    }
    return true;
  }

  /// Fulfill the Promise with an exception_ptr.
  void setException(std::exception_ptr ep) { setTry(Try<T>(ep)); }

  /// Fulfill the Promise with exception `e` *as if* by
  ///   `setException(std::make_exception_ptr<E>(e))`.
  template <class E>
  typename std::enable_if<std::is_base_of<std::exception, E>::value>::type setException(E const& e) {
    setException(std::make_exception_ptr<E>(e));
  }

  /// Fulfill the Promise with the specified value using perfect forwarding.
  /// Functionally equivalent to `setTry(Try<T>(std::forward<M>(value)))`
  template <class M>
  void setValue(M&& value) {
    static_assert(!std::is_same<T, void>::value, "Use setValue() instead");
    setTry(Try<T>(std::forward<M>(value)));
  }

  /// set void value
  template <class B = T>
  typename std::enable_if<std::is_same<Unit, B>::value>::type setValue() {
    setTry(Try<Unit>());
  }

  /// Fulfill the Promise with the specified Try (value or exception).
  void setTry(Try<T>&& t) {
    throwIfFulfilled();
    getState().setResult(std::move(t));
  }

  /// Fulfill this Promise with the result of a function that takes no
  ///   arguments and returns something implicitly convertible to T.
  template <class F>
  void setWith(F&& func) {
    throwIfFulfilled();
    getState().setResult(makeTryWith(std::forward<F>(func)));
  }

  arangodb::futures::Future<T> getFuture();

 private:
  explicit Promise(detail::SharedState<T>* state) : _state(state), _retrieved(false) {}

  // convenience method that checks if _state is set
  inline detail::SharedState<T>& getState() {
    if (!_state) {
      throw FutureException(ErrorCode::NoState);
    }
    return *_state;
  }

  inline void throwIfFulfilled() const {
    if (isFulfilled()) {
      throw FutureException(ErrorCode::PromiseAlreadySatisfied);
    }
  }

  void detach() {
    if (_state) {
      if (!_retrieved) {
        _state->detachFuture();
      }
      if (!_state->hasResult()) {
        auto ptr = std::make_exception_ptr(FutureException(ErrorCode::BrokenPromise));
        _state->setResult(Try<T>(std::move(ptr)));
      }
      _state->detachPromise();
      _state = nullptr;
    }
  }

 private:
  detail::SharedState<T>* _state;
  /// Whether the Future has been retrieved (a one-time operation)
  bool _retrieved;
};
}  // namespace futures
}  // namespace arangodb
#endif  // ARANGOD_FUTURES_PROMISE_H

#include "Future.h"
#include "Promise-inl.h"
