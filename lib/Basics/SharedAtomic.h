////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_SHARED_ATOMIC_H
#define ARANGO_SHARED_ATOMIC_H 1

#include "Basics/Common.h"

#include <atomic>

namespace arangodb {
namespace basics {

template <class T>
struct SharedAtomic {
  SharedAtomic() : _data() {}
  SharedAtomic(T t) : _data(t) {}
  SharedAtomic(T& t) : _data(t) {}
  SharedAtomic(SharedAtomic<T>& t) : _data(t.load(std::memory_order_seq_cst)) {}
  SharedAtomic<T>& operator=(SharedAtomic<T> const& other) {
    _data = other.load(std::memory_order_seq_cst);
    return *this;
  }

  T load(std::memory_order order) { return _data.load(order); }

  T operator=(T desired) noexcept {
    _data = desired;
    return desired;
  }

  bool is_lock_free() const noexcept { return _data.is_lock_free(); }

  void store(T desired,
             std::memory_order order = std::memory_order_seq_cst) noexcept {
    _data.store(desired, order);
  }

  T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return _data.load(order);
  }

  operator T() const noexcept { return _data.load(); }

  T exchange(T desired,
             std::memory_order order = std::memory_order_seq_cst) noexcept {
    return _data.exchange(desired, order);
  }

  bool compare_exchange_weak(T& expected, T desired, std::memory_order success,
                             std::memory_order failure) noexcept {
    return _data.compare_exchange_weak(expected, desired, success, failure);
  }

  bool compare_exchange_weak(
      T& expected, T desired,
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    return _data.compare_exchange_weak(expected, desired, order);
  }

  bool compare_exchange_strong(T& expected, T desired,
                               std::memory_order success,
                               std::memory_order failure) noexcept {
    return _data.compare_exchange_strong(expected, desired, success, failure);
  }

  bool compare_exchange_strong(
      T& expected, T desired,
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    return _data.compare_exchange_strong(expected, desired, order);
  }
  
#if __cplusplus > 201402L
  static constexpr bool is_always_lock_free =
      std::atomic<T>::is_always_lock_free;
#endif

  T fetch_add(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return _data.fetch_add(arg, order);
  }

  T fetch_sub(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return _data.fetch_sub(arg, order);
  }

  T fetch_and(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return _data.fetch_and(arg, order);
  }

  T fetch_or(T arg,
             std::memory_order order = std::memory_order_seq_cst) noexcept {
    return _data.fetch_or(arg, order);
  }

  T fetch_xor(T arg,
              std::memory_order order = std::memory_order_seq_cst) noexcept {
    return _data.fetch_xor(arg, order);
  }

  T operator++() noexcept { return _data.fetch_add(1) + 1; }

  T operator++(int)noexcept { return _data.fetch_add(1); }

  T operator--() noexcept { return _data.fetch_sub(1) - 1; }

  T operator--(int)noexcept { return _data.fetch_sub(1); }

  T operator+=(T arg) noexcept { return _data.fetch_add(arg) + arg; }

  T operator-=(T arg) noexcept { return _data.fetch_sub(arg) - arg; }

  T operator&=(T arg) noexcept { return _data.fetch_and(arg) & arg; }

  T operator|=(T arg) noexcept { return _data.fetch_or(arg) | arg; }

  T operator^=(T arg) noexcept { return _data.fetch_xor(arg) ^ arg; }

 private:
  uint8_t frontPadding[64];
  std::atomic<T> _data;
  uint8_t backPadding[64 - sizeof(T)];
};

}  // namespace basics
}  // namespace arangodb

#endif
