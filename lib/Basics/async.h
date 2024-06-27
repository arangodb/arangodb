#pragma once
#include <coroutine>
#include <atomic>
#include <stdexcept>
#include <utility>
#include <source_location>

#include <iostream>

namespace arangodb {

template<typename T>
struct expected {
  expected() {}

  template<typename... Args>
  explicit expected(std::in_place_t, Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>) requires
      std::constructible_from<T, Args...> {
    new (&_value) T(std::forward<Args>(args)...);
    _state = kValue;
  }

  explicit expected(std::exception_ptr ex) {
    _exception = ex;
    _state = kException;
  }

  expected(expected const& other) noexcept(
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

  expected(expected&& other) noexcept(
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

  expected& operator=(expected const& other) noexcept(
      std::is_nothrow_copy_constructible_v<T>&&
          std::is_nothrow_destructible_v<T>&& std::is_nothrow_copy_assignable_v<
              T>) requires(std::copy_constructible<T>&&
                               std::is_copy_assignable_v<T>) {
    if (this != &other) {
      if (other._state == kValue && _state == kValue) {
        _state = other._state;
      } else {
        reset();
        if (other._state == kValue) {
          new (&_value) T(other._value);
          _state = kValue;
        } else if (other._state == kException) {
          static_assert(
              std::is_nothrow_copy_constructible_v<std::exception_ptr>);
          new (&_exception) std::exception_ptr(other._exception);
          _state = kException;
        }
      }
    }
    return *this;
  }

  expected& operator=(expected&& other) noexcept(
      std::is_nothrow_move_constructible_v<T>&&
          std::is_nothrow_destructible_v<T>&& std::is_nothrow_move_assignable_v<
              T>) requires(std::move_constructible<T>&&
                               std::is_move_assignable_v<T>) {
    if (this != &other) {
      if (other._state == kValue && _state == kValue) {
        _state = std::move(other._state);
      } else {
        reset();
        if (other._state == kValue) {
          new (&_value) T(other._value);
          _state = kValue;
        } else if (other._state == kException) {
          static_assert(
              std::is_nothrow_copy_constructible_v<std::exception_ptr>);
          new (&_exception) std::exception_ptr(other._exception);
          _state = kException;
        }
        other.reset();
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

  void set_exception(std::exception_ptr const& ex) noexcept(
      std::is_nothrow_destructible_v<T>) {
    reset();
    new (&_exception) std::exception_ptr(ex);
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

  T const&& get() const&& {
    if (_state == kEmpty) {
      throw std::runtime_error("accessing empty expected");
    } else if (_state == kException) {
      std::rethrow_exception(_exception);
    } else {
      return std::move(_value);
    }
  }

  // It is debatable if -> and * operators should rethrow an exception within
  T* operator->() noexcept { return &_value; }
  T const* operator->() const noexcept { return &_value; }
  T& operator*() noexcept { return &_value; }
  T const& operator*() const noexcept { return &_value; }

  ~expected() { reset(); }

  void reset() {
    if (_state == kValue) {
      _value.~T();
    } else if (_state == kException) {
      _exception.~exception_ptr();
    }
    _state = kEmpty;
  }

 private:
  union {
    T _value;
    std::exception_ptr _exception;
  };

  enum { kEmpty, kValue, kException } _state = kEmpty;
};

template<>
struct expected<void> {
  expected() = default;
  explicit expected(std::exception_ptr ex) : _exception(std::move(ex)) {}
  void reset() { _exception = nullptr; }

  void get() const {
    if (_exception) {
      std::rethrow_exception(_exception);
    }
  }

  void set_exception(std::exception_ptr const& ex) { _exception = ex; }

  void emplace() noexcept { reset(); }

 private:
  std::exception_ptr _exception;
};

template<typename T>
struct async_promise;
template<typename T>
struct async;

template<typename T>
struct async_promise_base {
  using promise_type = async_promise<T>;

  std::suspend_never initial_suspend() noexcept { return {}; }
  auto final_suspend() noexcept {
    struct awaitable {
      bool await_ready() noexcept { return false; }
      std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
        auto addr =
            _promise->_continuation.exchange(std::noop_coroutine().address());
        if (addr == nullptr) {
          return std::noop_coroutine();
        } else {
          return std::coroutine_handle<>::from_address(addr);
        }
      }
      void await_resume() noexcept {}
      async_promise_base* _promise;
    };
    return awaitable{this};
  }
  void unhandled_exception() { _value.set_exception(std::current_exception()); }
  auto get_return_object() {
    return async<T>{std::coroutine_handle<promise_type>::from_promise(
        *static_cast<promise_type*>(this))};
  }

  std::atomic<void*> _continuation = nullptr;
  expected<T> _value;
};

template<typename T>
struct async_promise : async_promise_base<T> {
  void return_value(T v) {
    async_promise_base<T>::_value.emplace(std::move(v));
  }
};

template<>
struct async_promise<void> : async_promise_base<void> {
  void return_void() { async_promise_base<void>::_value.emplace(); }
};

template<typename T>
struct async {
  using promise_type = async_promise<T>;

  auto operator co_await() && {
    struct awaitable {
      bool await_ready() noexcept {
        return _handle.promise()._continuation.load(
                   std::memory_order_relaxed) != nullptr;
      }
      bool await_suspend(std::coroutine_handle<> c) noexcept {
        return _handle.promise()._continuation.exchange(c.address()) == nullptr;
      }
      auto await_resume() {
        expected<T> r = std::move(_handle.promise()._value);
        _handle.destroy();
        return std::move(r).get();
      }
      std::coroutine_handle<promise_type> _handle;
    };

    auto r = awaitable{_handle};
    _handle = nullptr;
    return r;
  }

  void reset() {
    if (_handle) {
      if (_handle.promise()._continuation.exchange(
              std::noop_coroutine().address(), std::memory_order_release) !=
          nullptr) {
        _handle.destroy();
      }
      _handle = nullptr;
    }
  }

  bool valid() const noexcept { return _handle != nullptr; }
  operator bool() const noexcept { return valid(); }

  ~async() { reset(); }

  explicit async(std::coroutine_handle<promise_type> h) : _handle(h) {}

 private:
  std::coroutine_handle<promise_type> _handle;
};

}  // namespace arangodb
