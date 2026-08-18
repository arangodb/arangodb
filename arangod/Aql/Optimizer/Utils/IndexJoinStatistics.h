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

#include "Aql/Optimizer/Utils/JoinStatistics.h"

#include <span>
#include <string>
#include <unordered_map>

namespace arangodb::aql {
class ExecutionPlan;

/// @brief statistics read from whatever indexes happen to exist on the
/// collections. This runs before index selection, so it consults the
/// collection's indexes directly rather than any IndexNode.
///
/// Depends only on the abstract Index interface: no concrete index class is
/// named and no storage engine is assumed.
class IndexJoinStatistics final : public JoinStatistics {
 public:
  explicit IndexJoinStatistics(ExecutionPlan const& plan);

  auto documentCount(JoinGraph::Node const& node) const -> double override;

  auto distinctValues(JoinGraph::Node const& node,
                      std::span<AttributePath const> attributes) const
      -> DistinctEstimate override;

  auto hasIndexCovering(JoinGraph::Node const& node,
                        std::span<AttributePath const> attributes) const
      -> bool override;

 private:
  ExecutionPlan const& _plan;
  // The greedy issues O(n^2) statistics queries and every uncached one walks
  // the collection's index list, so both lookups are memoised. Keyed on the
  // EnumerateCollectionNode -- not the JoinGraph::Node address -- because a
  // JoinGraph is rebuilt per spine run while this object is expected to live
  // for the whole plan, so a freed Node's address could otherwise be reused
  // by an unrelated later graph's node (see SystemRCostEstimator's analogous
  // cache for the same reasoning).
  mutable std::unordered_map<EnumerateCollectionNode const*, double> _counts;
  mutable std::unordered_map<std::string, DistinctEstimate> _distinct;
  mutable std::unordered_map<std::string, bool> _covering;
};

}  // namespace arangodb::aql
