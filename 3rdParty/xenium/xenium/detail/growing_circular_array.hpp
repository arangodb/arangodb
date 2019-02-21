//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_GROWING_CIRCULAR_ARRAY_HPP
#define XENIUM_GROWING_CIRCULAR_ARRAY_HPP

#include <xenium/utils.hpp>

#include <atomic>
#include <cassert>
#include <cstdint>

namespace xenium { namespace  detail {
  template <class T, std::size_t MinCapacity = 64, std::size_t MaxCapacity = 1u << 31>
  struct growing_circular_array {
    static constexpr std::size_t min_capacity = MinCapacity;
    static constexpr std::size_t max_capacity = MaxCapacity;
    static constexpr std::size_t num_buckets = utils::find_last_bit_set(max_capacity);

    static_assert(utils::is_power_of_two(min_capacity), "MinCapacity must be a power of two");
    static_assert(utils::is_power_of_two(max_capacity), "MaxCapacity must be a power of two");
    static_assert(min_capacity < max_capacity, "MaxCapacity must be greater than MinCapacity");

    growing_circular_array();
    ~growing_circular_array();

    std::size_t capacity() const { return _capacity.load(std::memory_order_relaxed); }

    T* get(std::size_t idx, std::memory_order order) {
      // (1) - this acquire-load synchronizes-with the release-store (2)
      auto capacitiy = _capacity.load(std::memory_order_acquire);
      return get_entry(idx, capacitiy).load(order);
    }

    void put(std::size_t idx, T* value, std::memory_order order) {
      auto capacitiy = _capacity.load(std::memory_order_relaxed);
      get_entry(idx, capacitiy).store(value, order);
    }

    bool can_grow() { return capacity() < max_capacity; }

    void grow(std::size_t bottom, std::size_t top);
  private:
    using entry = std::atomic<T*>;

    entry& get_entry(std::size_t idx, std::size_t capacity) {
      idx = idx & (capacity - 1);
      std::size_t bucket = utils::find_last_bit_set(idx);
      assert(bucket < num_buckets);
      // bucket can be zero, so we have to use two shifts here.
      idx ^= (1 << bucket) >> 1;
      return _data[bucket][idx];
    }

    static constexpr std::size_t initial_buckets = utils::find_last_bit_set(MinCapacity);

    std::size_t _buckets;
    std::atomic<std::size_t> _capacity;
    entry* _data[num_buckets];
  };

  template <class T, std::size_t MinCapacity, std::size_t Buckets>
  growing_circular_array<T, MinCapacity, Buckets>::growing_circular_array() :
    _buckets(initial_buckets),
    _capacity(MinCapacity),
    _data()
  {
    entry* ptr = new entry[MinCapacity];
    _data[0] = ptr++;
    for(std::size_t i = 1; i < _buckets; ++i) {
      _data[i] = ptr;
      ptr += 1 << (i - 1);
    }
  }

  template <class T, std::size_t MinCapacity, std::size_t Buckets>
  growing_circular_array<T, MinCapacity, Buckets>::~growing_circular_array() {
    delete[](_data[0]);
    for(std::size_t i = initial_buckets; i < num_buckets; ++i)
      delete[](_data[i]);
  }

  template <class T, std::size_t MinCapacity, std::size_t Buckets>
  void growing_circular_array<T, MinCapacity, Buckets>::grow(std::size_t bottom, std::size_t top) {
    assert(can_grow());

    auto capacity = this->capacity();
    auto mod_mask = capacity - 1;
    assert((capacity & mod_mask) == 0);

    _data[_buckets] = new entry[capacity];
    _buckets++;
    auto new_capacity = capacity * 2;
    auto new_mod_mask = new_capacity - 1;

    auto start = top;
    auto start_mod = top & mod_mask;
    if(start_mod == (top & new_mod_mask)) {
      // Make sure we don't iterate through useless indices
      start += capacity - start_mod;
    }

    for(std::size_t i = start; i < bottom; i++) {
      auto oldI = i & mod_mask;
      auto newI = i % new_mod_mask;
      if(oldI != newI) {
        auto oldBit = utils::find_last_bit_set(oldI);
        auto newBit = utils::find_last_bit_set(newI);
        auto v = _data[oldBit][oldI ^ ((1 << (oldBit)) >> 1)].load(std::memory_order_relaxed);
        _data[newBit][newI ^ ((1 << (newBit)) >> 1)].store(v, std::memory_order_relaxed);
      } else {
        // Make sure we don't iterate through useless indices
        break;
      }
    }

    // (2) - this release-store synchronizes-with the acquire-load (1)
    _capacity.store(new_capacity, std::memory_order_release);
  }
}}

#endif
