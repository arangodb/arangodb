////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_FUTURES_TRY_H
#define ARANGOD_FUTURES_TRY_H 1

#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "Basics/system-compiler.h"

#include <exception>
#include <type_traits>
#include <utility>

namespace arangodb {
namespace futures {

/// Try<T> is a wrapper that contains either an instance of T, an exception, or
/// nothing. Exceptions are stored as exception_ptrs so that the user can
/// minimize rethrows if so desired. Inspired by Folly's Try
template <typename T>
class Try {
  static_assert(!std::is_reference<T>::value,
                "Do not use with reference types");

  enum class Content { None, Value, Exception };

 public:
  /// Value type of the Try
  typedef T element_type;

  /// Construct an empty Try
  Try() noexcept : _content(Content::None) {}

  /// Construct a Try with a value by copy
  /// @param v The value to copy
  explicit Try(const T& v) noexcept(std::is_nothrow_copy_constructible<T>::value)
      : _value(v), _content(Content::Value) {}

  /// Construct a Try with a value by move
  /// @param v The value to move in
  explicit Try(T&& v) noexcept(std::is_nothrow_move_constructible<T>::value)
      : _value(std::move(v)), _content(Content::Value) {}

  template <typename... Args>
  explicit Try(std::in_place_t,
               Args&&... args) noexcept(std::is_nothrow_constructible<T, Args&&...>::value)
      : _value(static_cast<Args&&>(args)...), _content(Content::Value) {}

  /// Construct a Try with an exception_ptr
  /// @param e The exception_ptr
  explicit Try(std::exception_ptr e) noexcept
      : _exception(std::move(e)), _content(Content::Exception) {}

  // Move constructor
  Try(Try<T>&& t) noexcept(std::is_nothrow_move_constructible<T>::value)
      : _content(t._content) {
    if (_content == Content::Value) {
      new (&_value) T(std::move(t._value));
    } else if (_content == Content::Exception) {
      new (&_exception) std::exception_ptr(std::move(t._exception));
    }
  }

  // Move assigner
  Try& operator=(Try<T>&& t) noexcept(std::is_nothrow_move_constructible<T>::value) {
    if (this == &t) {
      return *this;
    }

    destroy();
    if (t._content == Content::Value) {
      new (&_value) T(std::move(t._value));
    } else if (t._content == Content::Exception) {
      new (&_exception) std::exception_ptr(std::move(t._exception));
    }
    _content = t._content;
    return *this;
  }

  // Copy constructor
  Try(const Try& t) noexcept(std::is_nothrow_copy_constructible<T>::value)
      : _content(t._content) {
    static_assert(std::is_copy_constructible<T>::value,
                  "T must be copyable for Try<T> to be copyable");
    if (_content == Content::Value) {
      new (&_value) T(t._value);
    } else if (_content == Content::Exception) {
      new (&_exception) std::exception_ptr(t._exception);
    }
  }
  // Copy assigner
  Try& operator=(const Try& t) noexcept(std::is_nothrow_copy_constructible<T>::value) {
    static_assert(std::is_copy_constructible<T>::value,
                  "T must be copyable for Try<T> to be copyable");
    if (this == &t) {
      return *this;
    }

    destroy();
    if (t._content == Content::Value) {
      new (&_value) T(std::move(t._value));
    } else if (t._content == Content::Exception) {
      new (&_exception) std::exception_ptr(std::move(t._exception));
    }
    _content = t._content;
    return *this;
  }

  ~Try() {
    if (ADB_LIKELY(_content == Content::Value)) {
      _value.~T();
    } else if (ADB_UNLIKELY(_content == Content::Exception)) {
      _exception.~exception_ptr();
    }
  }

  /// In-place construct the value in the Try object.
  /// Destroys any previous value prior to constructing the new value.
  /// Leaves *this in an empty state if the construction of T throws.
  template <typename... Args>
  T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible<T, Args&&...>::value) {
    this->destroy();
    new (&_value) T(static_cast<Args&&>(args)...);
    _content = Content::Value;
    return _value;
  }

  /// Set an exception value into this Try object.
  /// Destroys any previous value prior to constructing the new value.
  /// Leaves *this empty if throws
  void set_exception(std::exception_ptr e) {
    this->destroy();
    new (&_exception) std::exception_ptr(e);
    _content = Content::Exception;
  }

  /// Set an exception value into this Try object.
  /// Destroys any previous value prior to constructing the new value.
  /// Leaves *this empty if throws
  void set_exception(std::exception_ptr&& e) {
    this->destroy();
    new (&_exception) std::exception_ptr(std::move(e));
    _content = Content::Exception;
  }

  template <typename E>
  void set_exception(E const& e) {
    this->destroy();
    new (&_exception) std::exception_ptr(std::make_exception_ptr(e));
    _content = Content::Exception;
  }

  /// Get a mutable reference to the contained value. If the Try contains an
  /// exception it will be rethrown.
  /// @return mutable reference to the contained value
  T& get() & {
    throwIfFailed();
    return _value;
  }
  /// Get a rvalue reference to the contained value. If the Try contains an
  /// exception it will be rethrown.
  /// @return rvalue reference to the contained value
  T&& get() && {
    throwIfFailed();
    return std::move(_value);
  }
  /// Get a const reference to the contained value. If the Try contains an
  /// exception it will be rethrown.
  /// @return const reference to the contained value
  const T& get() const& {
    throwIfFailed();
    return _value;
  }
  /// Get a const rvalue reference to the contained value. If the Try contains
  /// an exception it will be rethrown.
  /// @return const rvalue reference to the contained value
  const T&& get() const&& {
    throwIfFailed();
    return std::move(_value);
  }

  /// If the Try contains an exception, rethrow it. Otherwise do nothing.
  void throwIfFailed() const {
    switch (_content) {
      case Content::Value:
        return;
      case Content::Exception:
        std::rethrow_exception(_exception);
        return;
      case Content::None:
      default:
        throw std::logic_error("Using uninitialized Try");
    }
  }

  /// Const dereference operator. If the Try contains an exception it will be
  /// rethrown.
  /// @return const reference to the contained value
  const T& operator*() const& { return get(); }
  /*
   * Dereference operator. If the Try contains an exception it will be rethrown.
   *
   * @return mutable reference to the contained value
   */
  T& operator*() & { return get(); }
  /*
   * Mutable rvalue dereference operator.  If the Try contains an exception it
   * will be rethrown.
   *
   * @return rvalue reference to the contained value
   */
  T&& operator*() && { return std::move(get()); }
  /// Const rvalue dereference operator.  If the Try contains an exception it
  /// will be rethrown.
  /// @return rvalue reference to the contained value
  const T&& operator*() const&& { return std::move(get()); }

  /// Const arrow operator. If the Try contains an exception it will be
  /// rethrown.
  /// @return const reference to the contained value
  const T* operator->() const { return &get(); }

  /// Arrow operator. If the Try contains an exception it will be rethrown.
  /// @return mutable reference to the contained value
  T* operator->() { return &get(); }

  /// @return True if the Try contains a value, false otherwise
  bool hasValue() const { return _content == Content::Value; }
  /// @return True if the Try contains an exception, false otherwise
  bool hasException() const { return _content == Content::Exception; }
  /// @return true if the Try contains an exception or a vlaue
  bool valid() const { return _content != Content::None; }

  /// @throws std::logic_error if the Try doesn't contain an exception
  /// @return mutable reference to the exception contained by this Try
  std::exception_ptr& exception() & {
    if (!hasException()) {
      throw std::logic_error("Try does not contain an exception");
    }
    return _exception;
  }

  std::exception_ptr&& exception() && {
    if (!hasException()) {
      throw std::logic_error("Try does not contain an exception");
    }
    return std::move(_exception);
  }

  std::exception_ptr const& exception() const& {
    if (!hasException()) {
      throw std::logic_error("Try does not contain an exception");
    }
    return _exception;
  }

  std::exception_ptr const&& exception() const&& {
    if (!hasException()) {
      throw std::logic_error("Try does not contain an exception");
    }
    return std::move(_exception);
  }

 private:
  void destroy() noexcept {
    auto old = _content;
    _content = Content::None;
    if (ADB_LIKELY(old == Content::Value)) {
      _value.~T();
    } else if (ADB_UNLIKELY(old == Content::Exception)) {
      _exception.~exception_ptr();
    }
  }

 private:
  union {
    T _value;
    std::exception_ptr _exception;
  };
  Content _content;
};

/// Specialization of Try for void value type. Encapsulates either success or an
/// exception.
template <>
class Try<void> {
 public:
  Try() noexcept : _exception() { TRI_ASSERT(!hasException()); }
  explicit Try(std::exception_ptr e) : _exception(std::move(e)) {}
  Try(Try<void>&& o) : _exception(std::move(o._exception)) {}

  /// copy assignment
  Try& operator=(const Try& e) {
    _exception = e._exception;
    return *this;
  }

  bool hasException() const { return _exception != nullptr; }
  bool hasValue() const { return !hasException(); }

  /// Try<void> is never empty
  bool valid() const { return true; }

  /// get the value (will throw if contains exception)
  void get() const { throwIfFailed(); }

  /// If the Try contains an exception, throws it
  inline void throwIfFailed() const {
    if (hasException()) {
      std::rethrow_exception(_exception);
    }
  }

  /// Dereference operator. If the Try contains an exception, throws it
  void operator*() const { return get(); }

  /// @throws std::logic_error if the Try doesn't contain an exception
  /// @return mutable reference to the exception contained by this Try
  std::exception_ptr& exception() & {
    if (!hasException()) {
      throw std::logic_error("Try does not contain an exception");
    }
    return _exception;
  }

  std::exception_ptr&& exception() && {
    if (!hasException()) {
      throw std::logic_error("Try does not contain an exception");
    }
    return std::move(_exception);
  }

  std::exception_ptr const& exception() const& {
    if (!hasException()) {
      throw std::logic_error("Try does not contain an exception");
    }
    return _exception;
  }

  std::exception_ptr const&& exception() const&& {
    if (!hasException()) {
      throw std::logic_error("Try does not contain an exception");
    }
    return std::move(_exception);
  }

  /// In-place construct a 'void' value into this Try object.
  /// This has the effect of clearing any existing exception stored
  void emplace() noexcept { _exception = nullptr; }

  /// Set an exception value into this Try object.
  /// This has the effect of clearing any existing exception stored
  void set_exception(std::exception_ptr e) noexcept { _exception = e; }

  /// Set an exception value into this Try object.
  /// This has the effect of clearing any existing exception stored
  void set_exception(std::exception_ptr const& e) noexcept { _exception = e; }

  /// Set an exception value into this Try object.
  /// This has the effect of clearing any existing exception stored
  void set_exception(std::exception_ptr&& e) noexcept {
    _exception = std::move(e);
  }

  template <typename E>
  void set_exception(E const& e) {
    _exception = std::make_exception_ptr(e);
  }

 private:
  std::exception_ptr _exception;
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
typename std::enable_if<std::is_same<R, void>::value, Try<void>>::type makeTryWith(F&& func) noexcept {
  try {
    func();
    return Try<void>();
  } catch (...) {
    return Try<void>(std::current_exception());
  }
}

/// test for Try parameter in templates
template <typename T>
struct isTry {
  static constexpr bool value = false;
  typedef T inner;
};
template <typename T>
struct isTry<Try<T>> {
  static constexpr bool value = true;
  typedef T inner;
};
template <typename T>
struct isTry<Try<T>&> {
  static constexpr bool value = true;
  typedef T inner;
};
template <typename T>
struct isTry<Try<T>&&> {
  static constexpr bool value = true;
  typedef T inner;
};

}  // namespace futures
}  // namespace arangodb
#endif  // ARANGOD_FUTURES_TRY_H
