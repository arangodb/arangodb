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

#ifndef ARANGODB_CACHE_FREQUENCY_BUFFER_H
#define ARANGODB_CACHE_FREQUENCY_BUFFER_H

#include "Basics/Common.h"
#include "Basics/SharedPRNG.h"

#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Lockless structure to calculate approximate relative event
/// frequencies.
///
/// Used to record events and then compute the number of occurrences of each
/// within a certain time-frame. The underlying structure is a circular buffer
/// which over-writes itself after it fills up (thus only maintaining a recent
/// window on the records).
////////////////////////////////////////////////////////////////////////////////
template <class T, class Comparator = std::equal_to<T>,
          class Hasher = std::hash<T>>
class FrequencyBuffer {
 public:
  typedef std::vector<std::pair<T, uint64_t>> stats_t;

 private:
  size_t _capacity;
  size_t _mask;
  std::unique_ptr<std::vector<T>> _buffer;
  Comparator _cmp;
  T _empty;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize with the given capacity.
  //////////////////////////////////////////////////////////////////////////////
  explicit FrequencyBuffer(size_t capacity)
      : _capacity(0),
        _mask(0),
        _buffer(nullptr),
        _cmp(),
        _empty() {
    size_t i = 0;
    for (; (static_cast<size_t>(1) << i) < capacity; i++) {
    }
    _capacity = (static_cast<size_t>(1) << i);
    _mask = _capacity - 1;
    _buffer.reset(new std::vector<T>(_capacity));
    TRI_ASSERT(_buffer->capacity() == _capacity);
    TRI_ASSERT(_buffer->size() == _capacity);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reports the hidden allocation size (not captured by sizeof).
  //////////////////////////////////////////////////////////////////////////////
  static size_t allocationSize(size_t capacity) {
    return sizeof(std::vector<T>) + (capacity * sizeof(T));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reports the memory usage in bytes.
  //////////////////////////////////////////////////////////////////////////////
  size_t memoryUsage() const {
    return ((_capacity * sizeof(T)) + sizeof(FrequencyBuffer<T>) +
            sizeof(std::vector<T>));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Insert an individual event record.
  //////////////////////////////////////////////////////////////////////////////
  void insertRecord(T record) {
    // we do not care about the order in which threads insert their values
    (*_buffer)[basics::SharedPRNG::rand() & _mask] = record;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Remove all occurrences of the specified event record.
  //////////////////////////////////////////////////////////////////////////////
  void purgeRecord(T record) {
    for (size_t i = 0; i < _capacity; i++) {
      if (_cmp((*_buffer)[i], record)) {
        (*_buffer)[i] = _empty;
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return a list of (event, count) pairs for each recorded event in
  /// ascending order.
  //////////////////////////////////////////////////////////////////////////////
  typename FrequencyBuffer::stats_t getFrequencies() const {
    // calculate frequencies
    std::unordered_map<T, uint64_t, Hasher, Comparator> frequencies;
    for (size_t i = 0; i < _capacity; i++) {
      T const entry = (*_buffer)[i];
      if (!_cmp(entry, _empty)) {
        frequencies[entry]++;
      }
    }

    // gather and sort frequencies
    stats_t data;
    data.reserve(frequencies.size());
    for (auto f : frequencies) {
      data.emplace_back(std::pair<T, uint64_t>(f.first, f.second));
    }
    std::sort(data.begin(), data.end(),
              [](std::pair<T, uint64_t>& left, std::pair<T, uint64_t>& right) {
                return left.second < right.second;
              });

    return data; // RVO moves this out
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Clear the buffer, removing all event records.
  //////////////////////////////////////////////////////////////////////////////
  void clear() {
    for (size_t i = 0; i < _capacity; i++) {
      (*_buffer)[i] = T();
    }
  }
};

};  // end namespace cache
};  // end namespace arangodb

#endif
