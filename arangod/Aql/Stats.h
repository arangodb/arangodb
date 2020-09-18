////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_STATS_H
#define ARANGOD_AQL_STATS_H 1

#include "Aql/ExecutionStats.h"

#include <cstddef>

namespace arangodb {
namespace aql {

// no-op statistics for all Executors that don't have custom stats.
class NoStats {
 public:
  void operator+= (NoStats const&) {}
};

inline ExecutionStats& operator+=(ExecutionStats& stats, NoStats const&) {
  return stats;
}

class CountStats {
 public:
  CountStats() noexcept : _counted(0) {}

  void setCounted(std::size_t counted) noexcept { _counted = counted; }

  void addCounted(std::size_t counted) noexcept { _counted += counted; }

  void incrCounted() noexcept { _counted++; }

  std::size_t getCounted() const noexcept { return _counted; }
  
  void operator+= (CountStats const& stats) {
    _counted += stats._counted;
  }

 private:
  std::size_t _counted;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  CountStats const& filterStats) noexcept {
  executionStats.count += filterStats.getCounted();
  return executionStats;
}

class FilterStats {
 public:
  FilterStats() noexcept : _filtered(0) {}

  void setFiltered(std::size_t filtered) noexcept { _filtered = filtered; }

  void addFiltered(std::size_t filtered) noexcept { _filtered += filtered; }

  void incrFiltered() noexcept { _filtered++; }

  std::size_t getFiltered() const noexcept { return _filtered; }
  
  void operator+= (FilterStats const& stats) {
    _filtered += stats._filtered;
  }

 private:
  std::size_t _filtered;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  FilterStats const& filterStats) noexcept {
  executionStats.filtered += filterStats.getFiltered();
  return executionStats;
}

class EnumerateCollectionStats {
 public:
  EnumerateCollectionStats() noexcept 
    : _scannedFull(0), _filtered(0) {}

  void incrScanned(size_t scanned) noexcept { _scannedFull += scanned; }
  void incrFiltered(size_t filtered) noexcept { _filtered += filtered; }

  std::size_t getScanned() const noexcept { return _scannedFull; }
  std::size_t getFiltered() const noexcept { return _filtered; }
  
  void operator+= (EnumerateCollectionStats const& stats) {
    _scannedFull += stats._scannedFull;
    _filtered += stats._filtered;
  }

 private:
  std::size_t _scannedFull;
  std::size_t _filtered;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  EnumerateCollectionStats const& enumerateCollectionStats) noexcept {
  executionStats.scannedFull += enumerateCollectionStats.getScanned();
  executionStats.filtered += enumerateCollectionStats.getFiltered();
  return executionStats;
}

class IndexStats {
 public:
  IndexStats() noexcept : _scannedIndex(0), _filtered(0) {}

  void incrScanned() noexcept { _scannedIndex++; }
  void incrScanned(size_t value) noexcept { _scannedIndex += value; }
  
  void incrFiltered() noexcept { _filtered++; }
  void incrFiltered(size_t value) noexcept { _filtered += value; }

  std::size_t getScanned() const noexcept { return _scannedIndex; }
  std::size_t getFiltered() const noexcept { return _filtered; }
  
  void operator+= (IndexStats const& stats) {
    _scannedIndex += stats._scannedIndex;
    _filtered += stats._filtered;
  }

 private:
  std::size_t _scannedIndex;
  std::size_t _filtered;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  IndexStats const& indexStats) noexcept {
  executionStats.scannedIndex += indexStats.getScanned();
  executionStats.filtered += indexStats.getFiltered();
  return executionStats;
}

class ModificationStats {
 public:
  ModificationStats() noexcept : _writesExecuted(0), _writesIgnored(0) {}

  void setWritesExecuted(std::size_t writesExecuted) noexcept {
    _writesExecuted = writesExecuted;
  }
  void addWritesExecuted(std::size_t writesExecuted) noexcept {
    _writesExecuted += writesExecuted;
  }
  void incrWritesExecuted() noexcept { _writesExecuted++; }
  std::size_t getWritesExecuted() const noexcept { return _writesExecuted; }

  void setWritesIgnored(std::size_t writesIgnored) noexcept {
    _writesIgnored = writesIgnored;
  }
  void addWritesIgnored(std::size_t writesIgnored) noexcept {
    _writesIgnored += writesIgnored;
  }
  void incrWritesIgnored() noexcept { _writesIgnored++; }
  std::size_t getWritesIgnored() const noexcept { return _writesIgnored; }
  
  void operator+= (ModificationStats const& stats) {
    _writesExecuted += stats._writesExecuted;
    _writesIgnored += stats._writesIgnored;
  }

 private:
  std::size_t _writesExecuted;
  std::size_t _writesIgnored;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  ModificationStats const& filterStats) noexcept {
  executionStats.writesExecuted += filterStats.getWritesExecuted();
  executionStats.writesIgnored += filterStats.getWritesIgnored();
  return executionStats;
}

class SingleRemoteModificationStats {
 public:
  SingleRemoteModificationStats() noexcept
      : _writesExecuted(0), _writesIgnored(0), _scannedIndex(0) {}

  void setWritesExecuted(std::size_t writesExecuted) noexcept {
    _writesExecuted = writesExecuted;
  }
  void addWritesExecuted(std::size_t writesExecuted) noexcept {
    _writesExecuted += writesExecuted;
  }
  void incrWritesExecuted() noexcept { _writesExecuted++; }
  std::size_t getWritesExecuted() const noexcept { return _writesExecuted; }

  void setWritesIgnored(std::size_t writesIgnored) noexcept {
    _writesIgnored = writesIgnored;
  }
  void addWritesIgnored(std::size_t writesIgnored) noexcept {
    _writesIgnored += writesIgnored;
  }
  void incrWritesIgnored() noexcept { _writesIgnored++; }
  std::size_t getWritesIgnored() const noexcept { return _writesIgnored; }

  void setScannedIndex(std::size_t scannedIndex) noexcept {
    _scannedIndex = scannedIndex;
  }
  void addScannedIndex(std::size_t scannedIndex) noexcept {
    _scannedIndex += scannedIndex;
  }
  void incrScannedIndex() noexcept { _scannedIndex++; }
  std::size_t getScannedIndex() const noexcept { return _scannedIndex; }
  
  void operator+= (SingleRemoteModificationStats const& stats) {
    _writesExecuted += stats._writesExecuted;
    _writesIgnored += stats._writesIgnored;
    _scannedIndex += stats._scannedIndex;
  }

 private:
  std::size_t _writesExecuted;
  std::size_t _writesIgnored;
  std::size_t _scannedIndex;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  SingleRemoteModificationStats const& filterStats) noexcept {
  executionStats.writesExecuted += filterStats.getWritesExecuted();
  executionStats.writesIgnored += filterStats.getWritesIgnored();
  executionStats.scannedIndex += filterStats.getScannedIndex();
  return executionStats;
}

}  // namespace aql
}  // namespace arangodb
#endif
