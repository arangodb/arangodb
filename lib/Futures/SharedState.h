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
#include <function2.hpp>

#include "Basics/debugging.h"
#include "Futures/Try.h"
#include "Logger/LogMacros.h"

namespace arangodb {
namespace futures {
namespace detail {

/// The FSM to manage the primary producer-to-consumer info-flow has these
///   allowed (atomic) transitions:
///
///   +-------------------------------------------------------------+
///   |                    ---> OnlyResult -----                    |
///   |                  /                       \                  |
///   |               (setResult())             (setCallback())     |
///   |                /                           \                |
///   |   Start ----->                               ------> Done   |
///   |                \                           /                |
///   |               (setCallback())           (setResult())       |
///   |                  \                       /                  |
///   |                    ---> OnlyCallback ---                    |
///   +-------------------------------------------------------------+
template <typename T>
class SharedState {
  enum class State : uint8_t {
    Start = 1 << 0,
    OnlyResult = 1 << 1,
    OnlyCallback = 1 << 2,
    Done = 1 << 3,
  };

  /// Allow us to safely pass a core pointer to the Scheduler
  struct SharedStateScope {
    explicit SharedStateScope(SharedState* state) noexcept : _state(state) {}
    SharedStateScope(SharedStateScope const&) = delete;
    SharedStateScope& operator=(SharedStateScope const&) = delete;
    SharedStateScope(SharedStateScope&& o) noexcept : _state(o._state) {
      o._state = nullptr;
    }

    ~SharedStateScope() {
      if (_state) {
        _state->_callback = nullptr;
        _state->detachOne();
      }
    }

    SharedState* _state;
  };

 public:
  /// State will be Start
  static SharedState* make() { return new SharedState(); }

  /// State will be OnlyResult
  /// Result held will be move-constructed from `t`
  static SharedState* make(Try<T>&& t) { return new SharedState(std::move(t)); }

  /// State will be OnlyResult
  /// Result held will be the `T` constructed from forwarded `args`
  template <typename... Args>
  static SharedState<T>* make(std::in_place_t, Args&&... args) {
    return new SharedState<T>(std::in_place, std::forward<Args>(args)...);
  }

  // not copyable
  SharedState(SharedState const&) = delete;
  SharedState& operator=(SharedState const&) = delete;

  // not movable (see comment in the implementation of Future::then)
  SharedState(SharedState&&) noexcept = delete;
  SharedState& operator=(SharedState&&) = delete;

  /// True if state is OnlyCallback or Done.
  /// May call from any thread
  bool hasCallback() const noexcept {
    auto const state = _state.load(std::memory_order_acquire);
    return state == State::OnlyCallback || state == State::Done;
  }

  /// True if state is OnlyResult or Done.
  /// May call from any thread
  bool hasResult() const noexcept {
    auto const state = _state.load(std::memory_order_acquire);
    return state == State::OnlyResult || state == State::Done;
  }

  /// Call only from consumer thread (since the consumer thread can modify the
  ///   referenced Try object; see non-const overloads of `future.result()`,
  ///   etc., and certain Future-provided callbacks which move-out the result).
  ///
  /// Unconditionally returns a reference to the result.
  ///
  /// State dependent preconditions:
  ///
  /// - Start or OnlyCallback: Never safe - do not call. (Access in those states
  ///   would be undefined behavior since the producer thread can, in those
  ///   states, asynchronously set the referenced Try object.)
  /// - OnlyResult: Always safe. (Though the consumer thread should not use the
  ///   returned reference after it attaches a callback unless it knows that
  ///   the callback does not move-out the referenced result.)
  /// - Done: Safe but sometimes unusable. (Always returns a valid reference,
  ///   but the referenced result may or may not have been modified, including
  ///   possibly moved-out, depending on what the callback did; some but not
  ///   all callbacks modify (possibly move-out) the result.)
  Try<T>& getTry() noexcept {
    TRI_ASSERT(hasResult());
    return _result;
  }
  Try<T> const& getTry() const noexcept {
    TRI_ASSERT(hasResult());
    return _result;
  }

  /// Call only from consumer thread.
  /// Call only once - else undefined behavior.
  ///
  /// See FSM graph for allowed transitions.
  ///
  /// If it transitions to Done, synchronously initiates a call to the callback,
  /// and might also synchronously execute that callback (e.g., if there is no
  /// executor or if the executor is inline).
  template <typename F>
  void setCallback(F&& func) {
    TRI_ASSERT(!hasCallback());

    // construct _callback first; TODO maybe try to avoid this?
    _callback = std::forward<F>(func);

    auto state = _state.load(std::memory_order_acquire);
    switch (state) {
      case State::Start:
        if (_state.compare_exchange_strong(state, State::OnlyCallback,
                                           std::memory_order_release)) {
          return;
        }
        TRI_ASSERT(state == State::OnlyResult);  // race with setResult
        [[fallthrough]];
      case State::OnlyResult:
        // acquire is actually correct here
        if (_state.compare_exchange_strong(state, State::Done, std::memory_order_acquire)) {
          doCallback();
          return;
        }
        [[fallthrough]];
      default:
        TRI_ASSERT(false);  // unexpected state
    }
  }

  /// Call only from producer thread.
  /// Call only once - else undefined behavior.
  ///
  /// See FSM graph for allowed transitions.
  ///
  /// If it transitions to Done, synchronously initiates a call to the callback,
  /// and might also synchronously execute that callback (e.g., if there is no
  /// executor or if the executor is inline).
  void setResult(Try<T>&& t) {
    TRI_ASSERT(!hasResult());
    // call move constructor of content
    ::new (&_result) Try<T>(std::move(t));

    auto state = _state.load(std::memory_order_acquire);

    switch (state) {
      case State::Start:
        if (_state.compare_exchange_strong(state, State::OnlyResult, std::memory_order_release)) {
          return;
        }
        TRI_ASSERT(state == State::OnlyCallback);  // race with setCallback
        [[fallthrough]];
      case State::OnlyCallback:
        // acquire is actually correct here
        if (_state.compare_exchange_strong(state, State::Done, std::memory_order_acquire)) {
          doCallback();
          return;
        }
        [[fallthrough]];

      default:
        TRI_ASSERT(false);  // unexpected state
    }
  }

  /// Called by a destructing Future (in the consumer thread, by definition).
  /// Calls `delete this` if there are no more references to `this`
  /// (including if `detachPromise()` is called previously or concurrently).
  void detachFuture() noexcept { detachOne(); }

  /// Called by a destructing Promise (in the producer thread, by definition).
  /// Calls `delete this` if there are no more references to `this`
  /// (including if `detachFuture()` is called previously or concurrently).
  void detachPromise() noexcept {
    TRI_ASSERT(hasResult());
    detachOne();
  }

 private:
  /// empty shared state
  SharedState() : _state(State::Start), _attached(2) {}

  /// use to construct a ready future
  explicit SharedState(Try<T>&& t)
      : _result(std::move(t)), _state(State::OnlyResult), _attached(1) {}

  /// use to construct a ready future
  template <typename... Args>
  explicit SharedState(std::in_place_t,
                       Args&&... args) noexcept(std::is_nothrow_constructible<T, Args&&...>::value)
      : _result(std::in_place, std::forward<Args>(args)...),
        _state(State::OnlyResult),
        _attached(1) {}

  ~SharedState() {
    TRI_ASSERT(_attached == 0);
    TRI_ASSERT(hasResult());
    _result.~Try<T>();
  }

  /// detach promise or future from shared state
  void detachOne() noexcept {
    auto a = _attached.fetch_sub(1, std::memory_order_acq_rel);
    TRI_ASSERT(a >= 1);
    if (a == 1) {
      _callback = nullptr;
      delete this;
    }
  }

  void doCallback() {
    TRI_ASSERT(_state == State::Done);
    TRI_ASSERT(_callback);

    _attached.fetch_add(1);
    // SharedStateScope makes this exception safe
    SharedStateScope scope(this); // will call detachOne()
    _callback(std::move(_result));
  }

 private:
  using Callback = fu2::unique_function<void(Try<T>&&)>;
  Callback _callback;
  union {  // avoids having to construct the result
    Try<T> _result;
  };
  std::atomic<State> _state;
  std::atomic<uint8_t> _attached;
};

}  // namespace detail
}  // namespace futures
}  // namespace arangodb

#endif  // ARANGOD_FUTURES_SHARED_STATE_H
