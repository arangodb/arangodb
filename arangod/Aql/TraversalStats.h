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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_TRAVERSAL_STATS_H
#define ARANGOD_AQL_TRAVERSAL_STATS_H

#include <cstddef>

#include "ExecutionStats.h"

namespace arangodb {
namespace aql {

class TraversalStats {
 public:
  TraversalStats() noexcept : _filtered(0), _scannedIndex(0), _httpRequests(0) {}

  void setFiltered(std::size_t filtered) noexcept { _filtered = filtered; }

  void addFiltered(std::size_t filtered) noexcept { _filtered += filtered; }

  void incrFiltered() noexcept { _filtered++; }

  std::size_t getFiltered() const noexcept { return _filtered; }

  void addScannedIndex(std::size_t scanned) noexcept {
    _scannedIndex += scanned;
  }

  std::size_t getScannedIndex() const noexcept { return _scannedIndex; }
  
  void addHttpRequests(std::size_t requests) noexcept {
    _httpRequests += requests;
  }
  
  std::size_t getHttpRequests() const noexcept { return _httpRequests; }
  
  void operator+=(TraversalStats const& other) {
    _filtered += other._filtered;
    _scannedIndex += other._scannedIndex;
    _httpRequests += other._httpRequests;
  }

 private:
  std::size_t _filtered;
  std::size_t _scannedIndex;
  std::size_t _httpRequests;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  TraversalStats const& traversalStats) noexcept {
  executionStats.filtered += traversalStats.getFiltered();
  executionStats.scannedIndex += traversalStats.getScannedIndex();
  executionStats.requests += traversalStats.getHttpRequests();
  return executionStats;
}

}  // namespace aql
}  // namespace arangodb

#endif
