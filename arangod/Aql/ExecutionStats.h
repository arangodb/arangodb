////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTION_STATS_H
#define ARANGOD_AQL_EXECUTION_STATS_H 1

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace velocypack {
class Builder;
}
namespace aql {

struct ExecutionStats {
  ExecutionStats();

  /// @brief instantiate the statistics from VelocyPack
  explicit ExecutionStats(arangodb::velocypack::Slice const& slice);

  /// @brief convert the statistics to VelocyPack
  void toVelocyPack(arangodb::velocypack::Builder&, bool reportFullCount) const;

  /// @brief create empty statistics for VelocyPack
  static void toVelocyPackStatic(arangodb::velocypack::Builder&);

  /// @brief sets query execution time from the outside
  void setExecutionTime(double value) { executionTime = value; }

  /// @brief sumarize two sets of ExecutionStats
  void add(ExecutionStats const& summand) {
    writesExecuted += summand.writesExecuted;
    writesIgnored += summand.writesIgnored;
    scannedFull += summand.scannedFull;
    scannedIndex += summand.scannedIndex;
    filtered += summand.filtered;
    httpRequests += summand.httpRequests;
    if (summand.fullCount > 0) {
      fullCount += summand.fullCount;
    }
    // intentionally no modification of executionTime
  }

  void clear() {
    writesExecuted = 0;
    writesIgnored = 0;
    scannedFull = 0;
    scannedIndex = 0;
    filtered = 0;
    httpRequests = 0;
    fullCount = 0;
    executionTime = 0.0;
  }

  /// @brief number of successfully executed write operations
  int64_t writesExecuted;

  /// @brief number of ignored write operations (ignored due to errors)
  int64_t writesIgnored;

  /// @brief number of documents scanned (full-collection scan)
  int64_t scannedFull;

  /// @brief number of documents scanned (using indexes scan)
  int64_t scannedIndex;

  /// @brief number of documents filtered away
  int64_t filtered;
  /// @brief total number of HTTP requests made

  int64_t httpRequests;

  /// @brief total number of results, before applying last limit
  int64_t fullCount;

  /// @brief query execution time (wall-clock time). value will be set from
  /// the outside
  double executionTime;
};
}  // namespace aql
}  // namespace arangodb

#endif
