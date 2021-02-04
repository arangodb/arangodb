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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_FREQUENCY_BUFFER_H
#define ARANGODB_CACHE_FREQUENCY_BUFFER_H

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ApplicationFeatures/SharedPRNGFeature.h"
#include "Basics/debugging.h"

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Lockless structure to calculate approximate relative event
/// frequencies.
///
/// Used to record events and then compute the approximate number of
/// occurrences of each within a certain time-frame. Will write to randomized
/// memory location inside the frequency buffer
////////////////////////////////////////////////////////////////////////////////
template <class T, class Comparator = std::equal_to<T>, class Hasher = std::hash<T>>
class FrequencyBuffer {
 public:
  typedef std::vector<std::pair<T, uint64_t>> stats_t;

  static_assert(sizeof(std::atomic<T>) == sizeof(T), "");

 private:
  SharedPRNGFeature& _sharedPRNG;
  std::size_t _capacity;
  std::size_t _mask;
  std::vector<std::atomic<T>> _buffer;
  Comparator _cmp;
  T _empty;

 private:
  static std::size_t powerOf2(std::size_t capacity) {
    // TODO maybe use
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    std::size_t i = 0;
    for (; (static_cast<size_t>(1) << i) < capacity; i++) {
    }
    return (static_cast<size_t>(1) << i);
  }

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize with the given capacity.
  //////////////////////////////////////////////////////////////////////////////
  explicit FrequencyBuffer(SharedPRNGFeature& sharedPRNG, std::size_t capacity)
      : _sharedPRNG(sharedPRNG),
        _capacity(powerOf2(capacity)),
        _mask(_capacity - 1),
        _buffer(_capacity),
        _cmp(),
        _empty() {
    TRI_ASSERT(_buffer.capacity() == _capacity);
    TRI_ASSERT(_buffer.size() == _capacity);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reports the hidden allocation size (not captured by sizeof).
  //////////////////////////////////////////////////////////////////////////////
  static std::size_t allocationSize(std::size_t capacity) {
    return capacity * sizeof(T);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reports the memory usage in bytes.
  //////////////////////////////////////////////////////////////////////////////
  std::size_t memoryUsage() const {
    return ((_capacity * sizeof(T)) + sizeof(FrequencyBuffer<T>));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Insert an individual event record.
  //////////////////////////////////////////////////////////////////////////////
  void insertRecord(T record) {
    // we do not care about the order in which threads insert their values
    _buffer[_sharedPRNG.rand() & _mask].store(record, std::memory_order_relaxed);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Remove all occurrences of the specified event record.
  //////////////////////////////////////////////////////////////////////////////
  void purgeRecord(T record) {
    for (std::size_t i = 0; i < _capacity; i++) {
      auto tmp = _buffer[i].load(std::memory_order_relaxed);
      if (_cmp(tmp, record)) {
        _buffer[i].compare_exchange_strong(tmp, _empty, std::memory_order_relaxed);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return a list of (event, count) pairs for each recorded event in
  /// ascending order.
  //////////////////////////////////////////////////////////////////////////////
  typename FrequencyBuffer::stats_t getFrequencies() const {
    // calculate frequencies
    std::unordered_map<T, std::size_t, Hasher, Comparator> frequencies;
    for (std::size_t i = 0; i < _capacity; i++) {
      T const entry = _buffer[i].load(std::memory_order_relaxed);
      if (!_cmp(entry, _empty)) {
        frequencies[entry]++;
      }
    }

    // gather and sort frequencies
    stats_t data;
    data.reserve(frequencies.size());
    for (auto f : frequencies) {
      data.emplace_back(std::pair<T, std::size_t>(f.first, f.second));
    }
    std::sort(data.begin(), data.end(),
              [](std::pair<T, std::uint64_t> const& left, std::pair<T, std::size_t> const& right) {
                return left.second < right.second;
              });

    return data;  // RVO moves this out
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Clear the buffer, removing all event records.
  //////////////////////////////////////////////////////////////////////////////
  void clear() {
    for (std::size_t i = 0; i < _capacity; i++) {
      _buffer[i].store(T(), std::memory_order_relaxed);
    }
  }
};

};  // end namespace cache
};  // end namespace arangodb

#endif
