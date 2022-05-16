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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionStats.h"

#include <cstddef>
#include <cstdint>

namespace arangodb::aql {

// no-op statistics for all Executors that don't have custom stats.
class NoStats {
 public:
  void operator+=(NoStats const&) noexcept {}
};

inline ExecutionStats& operator+=(ExecutionStats& stats, NoStats const&) {
  return stats;
}

class CountStats {
 public:
  CountStats() noexcept : _counted(0) {}

  void incrCounted(std::uint64_t value = 1) noexcept { _counted += value; }
  [[nodiscard]] std::uint64_t getCounted() const noexcept { return _counted; }

  void operator+=(CountStats const& stats) { _counted += stats._counted; }

 private:
  std::uint64_t _counted = 0;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  CountStats const& filterStats) noexcept {
  executionStats.count += filterStats.getCounted();
  return executionStats;
}

class FilterStats {
 public:
  FilterStats() noexcept : _filtered(0) {}

  void incrFiltered(std::uint64_t value = 1) noexcept { _filtered += value; }
  [[nodiscard]] std::uint64_t getFiltered() const noexcept { return _filtered; }

  void operator+=(FilterStats const& stats) noexcept {
    _filtered += stats._filtered;
  }

 private:
  std::uint64_t _filtered = 0;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  FilterStats const& filterStats) noexcept {
  executionStats.filtered += filterStats.getFiltered();
  return executionStats;
}

// MaterializeExecutor only tracks the number of filtered rows, so we
// can reuse the FilterStats for it.
using MaterializeStats = FilterStats;

class EnumerateCollectionStats {
 public:
  EnumerateCollectionStats() noexcept : _scannedFull(0), _filtered(0) {}

  void incrScanned(std::uint64_t value = 1) noexcept { _scannedFull += value; }
  void incrFiltered(std::uint64_t value = 1) noexcept { _filtered += value; }

  [[nodiscard]] std::uint64_t getScanned() const noexcept {
    return _scannedFull;
  }
  [[nodiscard]] std::uint64_t getFiltered() const noexcept { return _filtered; }

  void operator+=(EnumerateCollectionStats const& stats) noexcept {
    _scannedFull += stats._scannedFull;
    _filtered += stats._filtered;
  }

 private:
  std::uint64_t _scannedFull = 0;
  std::uint64_t _filtered = 0;
};

inline ExecutionStats& operator+=(
    ExecutionStats& executionStats,
    EnumerateCollectionStats const& enumerateCollectionStats) noexcept {
  executionStats.scannedFull += enumerateCollectionStats.getScanned();
  executionStats.filtered += enumerateCollectionStats.getFiltered();
  return executionStats;
}

class IndexStats {
 public:
  IndexStats() noexcept
      : _scannedIndex(0),
        _filtered(0),
        _cursorsCreated(0),
        _cursorsRearmed(0),
        _cacheHits(0),
        _cacheMisses(0) {}

  void incrScanned(std::uint64_t value = 1) noexcept { _scannedIndex += value; }
  void incrFiltered(std::uint64_t value = 1) noexcept { _filtered += value; }
  void incrCursorsCreated(std::uint64_t value = 1) noexcept {
    _cursorsCreated += value;
  }
  void incrCursorsRearmed(std::uint64_t value = 1) noexcept {
    _cursorsRearmed += value;
  }
  void incrCacheHits(std::uint64_t value = 1) noexcept { _cacheHits += value; }
  void incrCacheMisses(std::uint64_t value = 1) noexcept {
    _cacheMisses += value;
  }

  [[nodiscard]] std::uint64_t getScanned() const noexcept {
    return _scannedIndex;
  }
  [[nodiscard]] std::uint64_t getFiltered() const noexcept { return _filtered; }
  [[nodiscard]] std::uint64_t getCursorsCreated() const noexcept {
    return _cursorsCreated;
  }
  [[nodiscard]] std::uint64_t getCursorsRearmed() const noexcept {
    return _cursorsRearmed;
  }
  [[nodiscard]] std::uint64_t getCacheHits() const noexcept {
    return _cacheHits;
  }
  [[nodiscard]] std::uint64_t getCacheMisses() const noexcept {
    return _cacheMisses;
  }

  void operator+=(IndexStats const& stats) noexcept {
    _scannedIndex += stats._scannedIndex;
    _filtered += stats._filtered;
    _cursorsCreated += stats._cursorsCreated;
    _cursorsRearmed += stats._cursorsRearmed;
    _cacheHits += stats._cacheHits;
    _cacheMisses += stats._cacheMisses;
  }

 private:
  std::uint64_t _scannedIndex = 0;
  std::uint64_t _filtered = 0;
  std::uint64_t _cursorsCreated = 0;
  std::uint64_t _cursorsRearmed = 0;
  std::uint64_t _cacheHits = 0;
  std::uint64_t _cacheMisses = 0;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  IndexStats const& indexStats) noexcept {
  executionStats.scannedIndex += indexStats.getScanned();
  executionStats.filtered += indexStats.getFiltered();
  executionStats.cursorsCreated += indexStats.getCursorsCreated();
  executionStats.cursorsRearmed += indexStats.getCursorsRearmed();
  executionStats.cacheHits += indexStats.getCacheHits();
  executionStats.cacheMisses += indexStats.getCacheMisses();
  return executionStats;
}

class ModificationStats {
 public:
  ModificationStats() noexcept : _writesExecuted(0), _writesIgnored(0) {}

  void incrWritesExecuted(std::uint64_t value = 1) noexcept {
    _writesExecuted += value;
  }
  [[nodiscard]] std::uint64_t getWritesExecuted() const noexcept {
    return _writesExecuted;
  }

  void incrWritesIgnored(std::uint64_t value = 1) noexcept {
    _writesIgnored += value;
  }
  [[nodiscard]] std::uint64_t getWritesIgnored() const noexcept {
    return _writesIgnored;
  }

  void operator+=(ModificationStats const& stats) noexcept {
    _writesExecuted += stats._writesExecuted;
    _writesIgnored += stats._writesIgnored;
  }

 private:
  std::uint64_t _writesExecuted = 0;
  std::uint64_t _writesIgnored = 0;
};

inline ExecutionStats& operator+=(
    ExecutionStats& executionStats,
    ModificationStats const& filterStats) noexcept {
  executionStats.writesExecuted += filterStats.getWritesExecuted();
  executionStats.writesIgnored += filterStats.getWritesIgnored();
  return executionStats;
}

class SingleRemoteModificationStats {
 public:
  SingleRemoteModificationStats() noexcept
      : _writesExecuted(0), _writesIgnored(0), _scannedIndex(0) {}

  void incrWritesExecuted(std::uint64_t value = 1) noexcept {
    _writesExecuted += value;
  }
  [[nodiscard]] std::uint64_t getWritesExecuted() const noexcept {
    return _writesExecuted;
  }

  void incrWritesIgnored(std::uint64_t value = 1) noexcept {
    _writesIgnored += value;
  }
  [[nodiscard]] std::uint64_t getWritesIgnored() const noexcept {
    return _writesIgnored;
  }

  void incrScannedIndex(std::uint64_t value = 1) noexcept {
    _scannedIndex += value;
  }
  [[nodiscard]] std::uint64_t getScannedIndex() const noexcept {
    return _scannedIndex;
  }

  void operator+=(SingleRemoteModificationStats const& stats) noexcept {
    _writesExecuted += stats._writesExecuted;
    _writesIgnored += stats._writesIgnored;
    _scannedIndex += stats._scannedIndex;
  }

 private:
  std::uint64_t _writesExecuted = 0;
  std::uint64_t _writesIgnored = 0;
  std::uint64_t _scannedIndex = 0;
};

inline ExecutionStats& operator+=(
    ExecutionStats& executionStats,
    SingleRemoteModificationStats const& filterStats) noexcept {
  executionStats.writesExecuted += filterStats.getWritesExecuted();
  executionStats.writesIgnored += filterStats.getWritesIgnored();
  executionStats.scannedIndex += filterStats.getScannedIndex();
  return executionStats;
}

}  // namespace arangodb::aql
