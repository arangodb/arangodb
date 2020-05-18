////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/ExecutionNodeId.h"

#include <cstdint>
#include <map>

#include "Aql/ExecutionNodeStats.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace aql {

struct ExecutionStats {
  ExecutionStats();

  /// @brief instantiate the statistics from VelocyPack
  explicit ExecutionStats(arangodb::velocypack::Slice const& slice);
 
 public:
  /// @brief convert the statistics to VelocyPack
  void toVelocyPack(arangodb::velocypack::Builder&, bool reportFullCount) const;

  /// @brief sets query execution time from the outside
  void setExecutionTime(double value);

  /// @brief sets the peak memory usage from the outside
  void setPeakMemoryUsage(size_t value);

  /// @brief sumarize two sets of ExecutionStats
  void add(ExecutionStats const& summand);
  void addNode(aql::ExecutionNodeId id, ExecutionNodeStats const&);
  void addAlias(aql::ExecutionNodeId from, aql::ExecutionNodeId to) {
    _nodeAliases.emplace(from, to);
  }
  void setAliases(std::map<aql::ExecutionNodeId, aql::ExecutionNodeId>&& aliases) {
    _nodeAliases = std::move(aliases);
  }

  void clear();

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

  /// @brief total number of requests made
  int64_t requests;

  /// @brief total number of results, before applying last limit
  int64_t fullCount;

  /// @brief total number of results
  int64_t count;

  /// @brief query execution time (wall-clock time). value will be set from
  /// the outside
  double executionTime;

  /// @brief peak memory usage of the query
  size_t peakMemoryUsage;

 private:
  /// @brief Node aliases, source => target.
  ///        Every source node in the this aliases list
  ///        will be counted as the target instead
  ///        within nodes.
  std::map<aql::ExecutionNodeId, aql::ExecutionNodeId> _nodeAliases;
  
  ///  @brief statistics per ExecutionNodes
  std::map<aql::ExecutionNodeId, ExecutionNodeStats> _nodes;
};
}  // namespace aql
}  // namespace arangodb

#endif
