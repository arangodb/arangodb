////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>

#include "ExecutionStats.h"

namespace arangodb::aql {

class TraversalStats {
 public:
  TraversalStats() noexcept
      : _filtered(0),
        _scannedIndex(0),
        _httpRequests(0),
        _cursorsCreated(0),
        _cursorsRearmed(0),
        _cacheHits(0),
        _cacheMisses(0) {}

  void clear() noexcept {
    _filtered = 0;
    _scannedIndex = 0;
    _httpRequests = 0;
    _cursorsCreated = 0;
    _cursorsRearmed = 0;
    _cacheHits = 0;
    _cacheMisses = 0;
  }

  void incrFiltered(std::uint64_t value = 1) noexcept { _filtered += value; }
  [[nodiscard]] std::uint64_t getFiltered() const noexcept { return _filtered; }

  void incrScannedIndex(std::uint64_t value = 1) noexcept {
    _scannedIndex += value;
  }
  [[nodiscard]] std::uint64_t getScannedIndex() const noexcept {
    return _scannedIndex;
  }

  void incrHttpRequests(std::uint64_t value = 1) noexcept {
    _httpRequests += value;
  }
  [[nodiscard]] std::uint64_t getHttpRequests() const noexcept {
    return _httpRequests;
  }

  void incrCursorsCreated(std::uint64_t value = 1) noexcept {
    _cursorsCreated += value;
  }
  void incrCursorsRearmed(std::uint64_t value = 1) noexcept {
    _cursorsRearmed += value;
  }
  [[nodiscard]] std::uint64_t getCursorsCreated() const noexcept {
    return _cursorsCreated;
  }
  [[nodiscard]] std::uint64_t getCursorsRearmed() const noexcept {
    return _cursorsRearmed;
  }

  void incrCacheHits(std::uint64_t value = 1) noexcept { _cacheHits += value; }
  void incrCacheMisses(std::uint64_t value = 1) noexcept {
    _cacheMisses += value;
  }
  [[nodiscard]] std::uint64_t getCacheHits() const noexcept {
    return _cacheHits;
  }
  [[nodiscard]] std::uint64_t getCacheMisses() const noexcept {
    return _cacheMisses;
  }

  void operator+=(TraversalStats const& other) {
    _filtered += other._filtered;
    _scannedIndex += other._scannedIndex;
    _httpRequests += other._httpRequests;
    _cursorsCreated += other._cursorsCreated;
    _cursorsRearmed += other._cursorsRearmed;
    _cacheHits += other._cacheHits;
    _cacheMisses += other._cacheMisses;
  }

 private:
  std::uint64_t _filtered = 0;
  std::uint64_t _scannedIndex = 0;
  std::uint64_t _httpRequests = 0;
  std::uint64_t _cursorsCreated = 0;
  std::uint64_t _cursorsRearmed = 0;
  std::uint64_t _cacheHits = 0;
  std::uint64_t _cacheMisses = 0;
};

inline ExecutionStats& operator+=(
    ExecutionStats& executionStats,
    TraversalStats const& traversalStats) noexcept {
  executionStats.filtered += traversalStats.getFiltered();
  executionStats.scannedIndex += traversalStats.getScannedIndex();
  executionStats.requests += traversalStats.getHttpRequests();
  executionStats.cursorsCreated += traversalStats.getCursorsCreated();
  executionStats.cursorsRearmed += traversalStats.getCursorsRearmed();
  executionStats.cacheHits += traversalStats.getCacheHits();
  executionStats.cacheMisses += traversalStats.getCacheMisses();
  return executionStats;
}

}  // namespace arangodb::aql
