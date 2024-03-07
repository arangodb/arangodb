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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "Basics/debugging.h"
#include "Containers/FlatHashMap.h"
#include "RestServer/SharedPRNGFeature.h"

namespace arangodb::cache {

/// @brief Lockless structure to calculate approximate relative event
/// frequencies.
///
/// Used to record events and then compute the approximate number of
/// occurrences of each within a certain time-frame. Will write to randomized
/// memory location inside the frequency buffer
template<class T, class Comparator = std::equal_to<T>,
         class Hasher = std::hash<T>>
class FrequencyBuffer {
 public:
  using stats_t = std::vector<std::pair<T, std::uint64_t>>;

  static_assert(sizeof(std::atomic<T>) == sizeof(T));

  /// @brief Initialize with the given capacity.
  explicit FrequencyBuffer(SharedPRNGFeature& sharedPRNG, std::size_t capacity)
      : _sharedPRNG(sharedPRNG),
        _capacity(powerOf2(capacity)),
        _mask(_capacity - 1),
        _buffer(_capacity),
        _cmp() {
    TRI_ASSERT(_buffer.capacity() == _capacity);
    TRI_ASSERT(_buffer.size() == _capacity);
  }

  /// @brief Reports the hidden allocation size (not captured by sizeof).
  static constexpr std::size_t allocationSize(std::size_t capacity) {
    return capacity * sizeof(T);
  }

  /// @brief Reports the memory usage in bytes.
  std::size_t memoryUsage() const noexcept {
    return allocationSize(_capacity) + sizeof(FrequencyBuffer<T>);
  }

  /// @brief Insert an individual event record.
  void insertRecord(T record) noexcept {
    // we do not care about the order in which threads insert their values
    _buffer[_sharedPRNG.rand() & _mask].store(record,
                                              std::memory_order_relaxed);
  }

  /// @brief Remove all occurrences of the specified event record.
  void purgeRecord(T record) {
    T const empty{};
    for (std::size_t i = 0; i < _capacity; i++) {
      auto tmp = _buffer[i].load(std::memory_order_relaxed);
      if (_cmp(tmp, record)) {
        _buffer[i].compare_exchange_strong(tmp, empty,
                                           std::memory_order_relaxed);
      }
    }
  }

  /// @brief Return a list of (event, count) pairs for each recorded event in
  /// ascending order.
  typename FrequencyBuffer::stats_t getFrequencies() const {
    T const empty{};
    // calculate frequencies
    arangodb::containers::FlatHashMap<T, std::size_t, Hasher, Comparator>
        frequencies;
    for (std::size_t i = 0; i < _capacity; i++) {
      T const entry = _buffer[i].load(std::memory_order_relaxed);
      if (!_cmp(entry, empty)) {
        ++frequencies[entry];
      }
    }

    // gather and sort frequencies
    stats_t data;
    data.reserve(frequencies.size());
    for (auto f : frequencies) {
      data.emplace_back(f.first, f.second);
    }
    std::sort(
        data.begin(), data.end(),
        [](std::pair<T, std::uint64_t> const& left,
           std::pair<T, std::uint64_t> const& right) {
          // in case of equal frequencies, we use the key as an arbiter,
          // so that repeated calls produce the same result for keys
          // with equal values
          return (left.second < right.second) ||
                 (left.second == right.second && left.first < right.first);
        });

    return data;  // RVO moves this out
  }

  /// @brief Clear the buffer, removing all event records.
  void clear() noexcept {
    for (std::size_t i = 0; i < _capacity; i++) {
      _buffer[i].store(T(), std::memory_order_relaxed);
    }
  }

 private:
  static constexpr std::size_t powerOf2(std::size_t capacity) {
    std::size_t bitPos =
        static_cast<std::size_t>(std::ceil(std::log2(capacity)));
    return (static_cast<std::size_t>(1) << bitPos);
  }

  SharedPRNGFeature& _sharedPRNG;
  std::size_t const _capacity;
  std::size_t const _mask;
  std::vector<std::atomic<T>> _buffer;
  Comparator _cmp;
};

};  // end namespace arangodb::cache
