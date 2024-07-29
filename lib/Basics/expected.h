#pragma once
#include <stdexcept>
#include <type_traits>
#include "Assertions/Assert.h"
#include <utility>

namespace arangodb {
template<typename T>
struct expected {
  expected() noexcept {}

  template<typename... Args>
  explicit expected(std::in_place_t, Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>) requires
      std::constructible_from<T, Args...> {
    new (&_value) T(std::forward<Args>(args)...);
    _state = kValue;
  }

  explicit expected(std::exception_ptr ex) noexcept
      : _exception(std::move(ex)), _state(kException) {
    static_assert(std::is_nothrow_move_constructible_v<std::exception_ptr>);
  }

  expected(expected const& other) noexcept(
      std::is_nothrow_copy_constructible_v<T>) requires
      std::is_copy_constructible_v<T> {
    copy_from(other);
  }

  expected& operator=(expected const& other) noexcept(
      std::is_nothrow_copy_constructible_v<T>&&
          std::is_nothrow_destructible_v<T>&& std::is_nothrow_copy_assignable_v<
              T>) requires(std::is_copy_constructible_v<T>&&
                               std::is_copy_assignable_v<T>) {
    if (this != &other) {
      if (other._state == kValue && _state == kValue) {
        _value = other._value;
      } else {
        reset();
        copy_from(other);
      }
    }
    return *this;
  }

  expected(expected&& other) noexcept(
      std::is_nothrow_move_constructible_v<T>) requires
      std::move_constructible<T> {
    move_from(std::move(other));
  }

  expected& operator=(expected&& other) noexcept(
      std::is_nothrow_destructible_v<T>&& std::is_nothrow_move_assignable_v<
          T>) requires(std::is_move_assignable_v<T>) {
    if (this != &other) {
      if (other._state == kValue && _state == kValue) {
        _value = std::move(other._value);
      } else {
        reset();
        move_from(std::move(other));
      }
    }
    return *this;
  }

  template<typename... Args>
  T& emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>&&
          std::is_nothrow_destructible_v<T>) requires
      std::constructible_from<T, Args...> {
    reset();
    new (&_value) T(std::forward<Args>(args)...);
    _state = kValue;
    return _value;
  }

  void set_exception(std::exception_ptr ex) noexcept(
      std::is_nothrow_destructible_v<T>) {
    reset();
    new (&_exception) std::exception_ptr(std::move(ex));
    _state = kException;
  }

  T& get() & {
    if (_state == kEmpty) {
      throw std::runtime_error("accessing empty expected");
    } else if (_state == kException) {
      std::rethrow_exception(_exception);
    } else {
      return _value;
    }
  }

  T const& get() const& {
    if (_state == kEmpty) {
      throw std::runtime_error("accessing empty expected");
    } else if (_state == kException) {
      std::rethrow_exception(_exception);
    } else {
      return _value;
    }
  }

  T&& get() && {
    if (_state == kEmpty) {
      throw std::runtime_error("accessing empty expected");
    } else if (_state == kException) {
      std::rethrow_exception(_exception);
    } else {
      return std::move(_value);
    }
  }

  // It is debatable whether -> and * operators should rethrow an exception
  // within or not
  T* operator->() noexcept {
    TRI_ASSERT(_state == kValue);
    return &_value;
  }
  T const* operator->() const noexcept {
    TRI_ASSERT(_state == kValue);
    return &_value;
  }
  T& operator*() noexcept {
    TRI_ASSERT(_state == kValue);
    return &_value;
  }
  T const& operator*() const noexcept {
    TRI_ASSERT(_state == kValue);
    return &_value;
  }

  ~expected() noexcept(std::is_nothrow_destructible_v<T>) { reset(); }

  void reset() noexcept(std::is_nothrow_destructible_v<T>) {
    auto state = _state;
    _state = kEmpty;
    if (state == kValue) {
      _value.~T();
    } else if (state == kException) {
      _exception.~exception_ptr();
    }
  }

  std::exception_ptr exception_ptr() const noexcept {
    TRI_ASSERT(_state == kException);
    return _exception;
  }

  bool ok() const noexcept { return _state == kValue; }
  operator bool() const noexcept { return ok(); }

  enum State { kEmpty, kValue, kException };
  State state() const noexcept { return _state; }

 private:
  auto copy_from(expected const& other) noexcept(
      std::is_nothrow_copy_constructible_v<T>) requires
      std::copy_constructible<T> {
    if (other._state == kValue) {
      new (&_value) T(other._value);
      _state = kValue;
    } else if (other._state == kException) {
      static_assert(std::is_nothrow_copy_constructible_v<std::exception_ptr>);
      new (&_exception) std::exception_ptr(other._exception);
      _state = kException;
    }
  }
  auto move_from(expected&& other) noexcept(
      std::is_nothrow_move_constructible_v<T>) requires
      std::move_constructible<T> {
    if (other._state == kValue) {
      new (&_value) T(std::move(other._value));
      _state = kValue;
    } else if (other._state == kException) {
      static_assert(std::is_nothrow_move_constructible_v<std::exception_ptr>);
      new (&_exception) std::exception_ptr(std::move(other._exception));
      _state = kException;
    }
  }

  union {
    T _value;
    std::exception_ptr _exception;
  };

  State _state = kEmpty;
};

template<>
struct expected<void> {
  expected() = default;
  explicit expected(std::exception_ptr ex) noexcept
      : _exception(std::move(ex)) {
    static_assert(std::is_nothrow_move_constructible_v<std::exception_ptr>);
  }
  void reset() noexcept { _exception = nullptr; }

  void get() const {
    if (_exception) {
      std::rethrow_exception(_exception);
    }
  }

  void set_exception(std::exception_ptr ex) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<std::exception_ptr>);
    _exception = std::move(ex);
  }

  void emplace() noexcept { reset(); }

 private:
  std::exception_ptr _exception;
};
}  // namespace arangodb
