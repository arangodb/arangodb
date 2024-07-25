#pragma once

#include "Basics/async/promise_registry.hpp"
#include "Basics/async/feature.hpp"
#include "Basics/expected.h"
#include <coroutine>
#include <atomic>
#include <stdexcept>
#include <utility>
#include <source_location>

namespace arangodb {

template<typename T>
struct async_promise;
template<typename T>
struct async;

template<typename T>
struct async_promise_base : coroutine::PromiseInList {
  using promise_type = async_promise<T>;

  async_promise_base(std::source_location loc)
      : PromiseInList(std::move(loc)) {}

  std::suspend_never initial_suspend() noexcept { return {}; }
  auto final_suspend() noexcept {
    struct awaitable {
      bool await_ready() noexcept { return false; }
      std::coroutine_handle<> await_suspend(
          std::coroutine_handle<> self) noexcept {
        auto addr = _promise->_continuation.exchange(self.address(),
                                                     std::memory_order_acq_rel);
        if (addr == nullptr) {
          return std::noop_coroutine();
        } else if (addr == self.address()) {
          self.destroy();
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
    coroutine::promise_registry->add(this);
    return async<T>{std::coroutine_handle<promise_type>::from_promise(
        *static_cast<promise_type*>(this))};
  }

  std::atomic<void*> _continuation = nullptr;
  expected<T> _value;
};

template<typename T>
struct async_promise : async_promise_base<T> {
  async_promise(std::source_location loc = std::source_location::current())
      : async_promise_base<T>(std::move(loc)) {}
  template<typename V = T>
  void return_value(V&& v) {
    async_promise_base<T>::_value.emplace(std::forward<V>(v));
  }
};

template<>
struct async_promise<void> : async_promise_base<void> {
  async_promise(std::source_location loc = std::source_location::current())
      : async_promise_base<void>(std::move(loc)) {}
  void return_void() { async_promise_base<void>::_value.emplace(); }
};

template<typename T>
struct async {
  using promise_type = async_promise<T>;

  auto operator co_await() && {
    struct awaitable {
      bool await_ready() noexcept {
        return _handle.promise()._continuation.load(
                   std::memory_order_acquire) != nullptr;
      }
      bool await_suspend(std::coroutine_handle<> c) noexcept {
        return _handle.promise()._continuation.exchange(
                   c.address(), std::memory_order_acq_rel) == nullptr;
      }
      auto await_resume() {
        expected<T> r = std::move(_handle.promise()._value);
        _handle.destroy();
        return std::move(r).get();
      }
      explicit awaitable(std::coroutine_handle<promise_type> handle)
          : _handle(handle) {}

     private:
      std::coroutine_handle<promise_type> _handle;
    };

    auto r = awaitable{_handle};
    _handle = nullptr;
    return r;
  }

  void reset() {
    if (_handle) {
      if (_handle.promise()._continuation.exchange(
              _handle.address(), std::memory_order_release) != nullptr) {
        _handle.destroy();
      }
      _handle = nullptr;
    }
  }

  bool valid() const noexcept { return _handle != nullptr; }
  operator bool() const noexcept { return valid(); }

  ~async() {
    // TODO add to free list
    reset();
  }

  explicit async(std::coroutine_handle<promise_type> h) : _handle(h) {}

  async(async const&) = delete;
  async& operator=(async const&) = delete;

  async(async&& o) noexcept : _handle(std::move(o._handle)) {
    o._handle = nullptr;
  }

  async& operator=(async&& o) noexcept {
    reset();
    std::swap(o._handle, _handle);
    return *this;
  }

 private:
  std::coroutine_handle<promise_type> _handle;
};

}  // namespace arangodb
