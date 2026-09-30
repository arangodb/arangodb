////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNodeId.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinGraph.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinStatistics.h"

#include <cstddef>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {

struct StatsKey {
  ExecutionNodeId node;
  std::vector<AttributePath> paths;

  auto operator==(StatsKey const&) const -> bool = default;
};

struct StatsKeyHash {
  [[nodiscard]] auto operator()(StatsKey const& key) const noexcept
      -> std::size_t;
};

/// @brief memoises another JoinStatistics. The search queries the same node
/// and attribute set O(n^2) times while the answers cannot change within one
/// rule invocation, so every implementation faces the same repeated calls.
class CachingJoinStatistics final : public JoinStatistics {
 public:
  explicit CachingJoinStatistics(std::unique_ptr<JoinStatistics> inner);

  [[nodiscard]] auto documentCount(JoinGraph::Node const& node) const
      -> double override;

  [[nodiscard]] auto distinctValues(JoinGraph::Node const& node,
                                    std::span<AttributePath const> attributes)
      const -> DistinctEstimate override;

  [[nodiscard]] auto hasIndexCovering(
      JoinGraph::Node const& node,
      std::span<AttributePath const> attributes) const -> bool override;

 private:
  std::unique_ptr<JoinStatistics> _inner;
  mutable std::unordered_map<ExecutionNodeId, double> _counts;
  mutable std::unordered_map<StatsKey, DistinctEstimate, StatsKeyHash>
      _distinct;
  mutable std::unordered_map<StatsKey, bool, StatsKeyHash> _covering;
};

}  // namespace arangodb::aql
