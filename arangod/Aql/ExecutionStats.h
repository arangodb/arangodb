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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionNodeStats.h"

#include <cstdint>
#include <map>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace aql {

struct ExecutionStats {
  ExecutionStats() noexcept;

  /// @brief instantiate the statistics from VelocyPack
  explicit ExecutionStats(arangodb::velocypack::Slice slice);

  /// @brief convert the statistics to VelocyPack
  void toVelocyPack(arangodb::velocypack::Builder&, bool reportFullCount) const;

  /// @brief sets query execution time from the outside
  void setExecutionTime(double value);

  /// @brief sets the peak memory usage from the outside
  void setPeakMemoryUsage(size_t value);

  /// @brief set the number of intermediate commits
  void setIntermediateCommits(uint64_t value);

  /// @brief sumarize two sets of ExecutionStats
  void add(ExecutionStats const& summand);
  void addNode(aql::ExecutionNodeId id, ExecutionNodeStats const&);
  void addAlias(aql::ExecutionNodeId from, aql::ExecutionNodeId to) {
    _nodeAliases.emplace(from, to);
  }
  void setAliases(
      std::map<aql::ExecutionNodeId, aql::ExecutionNodeId>&& aliases) {
    _nodeAliases = std::move(aliases);
  }

  void clear() noexcept;

  /// @brief number of successfully executed write operations
  uint64_t writesExecuted = 0;

  /// @brief number of ignored write operations (ignored due to errors)
  uint64_t writesIgnored = 0;

  /// @brief number of real document document lookups
  uint64_t documentLookups = 0;

  /// @brief number of seeks done by RocksDB iterators. Currently only populated
  /// in JoinExecutor.
  uint64_t seeks = 0;

  /// @brief number of documents scanned (full-collection scan)
  uint64_t scannedFull = 0;

  /// @brief number of documents scanned (using indexes scan)
  uint64_t scannedIndex = 0;

  /// @brief number of cursors created. currently only populated by
  /// IndexExecutor.
  uint64_t cursorsCreated = 0;

  /// @brief number of existing cursors that were rearmed. currently only
  /// populated by IndexExecutor.
  uint64_t cursorsRearmed = 0;

  /// @brief number of (in-memory) cache hits, if cache is enabled
  uint64_t cacheHits = 0;

  /// @brief number of (in-memory) cache misses, if cache is enabled
  uint64_t cacheMisses = 0;

  /// @brief number of documents filtered away
  uint64_t filtered = 0;

  /// @brief total number of requests made
  uint64_t requests = 0;

  /// @brief total number of results, before applying last limit
  uint64_t fullCount = 0;

  /// @brief total number of results
  uint64_t count = 0;

  /// @brief query execution time (wall-clock time). value will be set from
  /// the outside
  double executionTime = 0.0;

  /// @brief peak memory usage of the query
  size_t peakMemoryUsage = 0;

  /// @brief number of commits that happened for the query
  uint64_t intermediateCommits = 0;

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
