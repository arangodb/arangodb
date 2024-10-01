////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Basics/debugging.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"

#include "utils/hash_utils.hpp"
#include "utils/memory.hpp"
#include "utils/string.hpp"
#include "utils/thread_utils.hpp"

namespace {

template<typename...>
struct typelist;

}

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief A wrapper to control the lifetime of an object
///        that is used by multiple threads.
///
/// An analogue of shared_ptr and weak_ptr,
/// in fact AsyncValue is both shared_ptr and weak_ptr, before they called
/// AsyncValue::reset, after it only weak_ptr to already destroyed shared_ptr.
////////////////////////////////////////////////////////////////////////////////
template<typename T>
class AsyncValue {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Same semantics as shared_ptr, expect copy ctor and assign
  ///        (easy to add but we don't want it).
  //////////////////////////////////////////////////////////////////////////////
  class Value {
   public:
    constexpr Value() noexcept : _self{nullptr} {}
    constexpr Value(Value&& other) noexcept
        : _self{std::exchange(other._self, nullptr)} {}
    constexpr Value& operator=(Value&& other) noexcept {
      std::swap(_self, other._self);
      return *this;
    }

    T* get() noexcept { return _self ? _self->_resource : nullptr; }
    T const* get() const noexcept { return _self ? _self->_resource : nullptr; }
    T* operator->() noexcept { return get(); }
    T const* operator->() const noexcept { return get(); }

    explicit operator bool() const noexcept { return get() != nullptr; }

    ~Value() {
      if (_self) {
        _self->destroy();
      }
    }

   private:
    friend class AsyncValue;
    constexpr Value(AsyncValue& self) noexcept : _self{&self} {}

    AsyncValue* _self;
  };

  explicit AsyncValue(T* resource) noexcept
      : _resource{resource}, _count{[&] {
          if (!resource) {
            return kReset | kDestroy;
          } else {
            return kRef;
          }
        }()} {}

  ~AsyncValue() { reset(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief If return true then reset was call
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] bool empty() const noexcept {
    return _count.load(std::memory_order_acquire) & kReset;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Has the same semantics as weak_ptr::lock
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] Value lock() noexcept {
    if (empty()) {
      return {};
    }
    if (_count.fetch_add(kRef, std::memory_order_acquire) & kDestroy) {
      return {};
    }
    return {*this};
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Denies access to resource granted by a lock,
  ///        and waits until all locks on resource have been released.
  //////////////////////////////////////////////////////////////////////////////
  void reset() noexcept {
    auto count = _count.fetch_or(kReset, std::memory_order_release);
    if ((count & kDestroy) == kDestroy) {
      return;
    }
    if ((count & kReset) != kReset) {  // handle first reset
      if (destroy()) {                 // handle successfully destroy
        return;
      }
      count -= kRef - kReset;
    }
    do {  // wait destroy
      _count.wait(count, std::memory_order_relaxed);
      count = _count.load(std::memory_order_acquire);
    } while ((count & kDestroy) != kDestroy);
  }

 private:
  static constexpr uint32_t kReset = 1;
  static constexpr uint32_t kDestroy = 2;
  static constexpr uint32_t kRef = 4;

  bool destroy() noexcept {
    auto count = _count.fetch_sub(kRef, std::memory_order_release) - kRef;
    if (count == kReset &&
        _count.compare_exchange_strong(count, kReset | kDestroy,
                                       std::memory_order_acq_rel,
                                       std::memory_order_relaxed)) {
      _count.notify_all();
      return true;
    }
    return false;
  }

  T* _resource;
  mutable std::atomic_uint32_t _count;  // linux futex handle only 32 bit
};

}  // namespace iresearch
}  // namespace arangodb
