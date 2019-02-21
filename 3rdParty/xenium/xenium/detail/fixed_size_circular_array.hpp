//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_FIXED_SIZE_CIRCULAR_ARRAY_HPP
#define XENIUM_FIXED_SIZE_CIRCULAR_ARRAY_HPP

#include <atomic>
#include <cstdint>
#include <stdexcept>

namespace xenium { namespace detail {
  template <class T, std::size_t Capacity>
  struct fixed_size_circular_array {
    std::size_t capacity() const { return Capacity; }

    T* get(std::size_t idx, std::memory_order order) {
      return items[idx & mask].load(order);
    }

    void put(std::size_t idx, T* value, std::memory_order order) {
      items[idx & mask].store(value, order);
    }

    constexpr bool can_grow() const { return false; }

    void grow(std::size_t bottom, std::size_t top) {
      throw std::runtime_error("cannot grow fixed_size_circular_array");
    }
  private:
    static constexpr std::size_t mask = Capacity - 1;
    static_assert((Capacity & mask) == 0, "capacity has to be a power of two");

    std::atomic<T*> items[Capacity];
  };
}}
#endif
