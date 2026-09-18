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

#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinGraph.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinStatistics.h"
#include "Basics/AttributeNameParser.h"
#include "Indexes/IndexType.h"

#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {
class ExecutionPlan;

/// @brief the index properties this model consults, lifted out of Index so
/// the selection rules below can be exercised without a storage engine.
struct IndexFacts {
  IndexType type = IndexType::Unknown;
  std::vector<std::vector<basics::AttributeName>> fields;
  bool hidden = false;
  bool inProgress = false;
  bool sparse = false;
  bool hasSelectivityEstimate = false;
  // Only meaningful when hasSelectivityEstimate is true; the Index contract
  // forbids calling Index::selectivityEstimate() otherwise.
  double selectivityEstimate = 0.0;
};

/// @brief |C_S| under the subset rule: consider only candidates whose fields
/// are a subset of `attributes`, and take the maximum of
/// selectivityEstimate() * count over them.
auto distinctFromIndexFacts(std::span<IndexFacts const> candidates,
                            double count,
                            std::span<AttributePath const> attributes)
    -> DistinctEstimate;

auto coveringFromIndexFacts(std::span<IndexFacts const> candidates,
                            std::span<AttributePath const> attributes) -> bool;

/// @brief statistics read from whatever indexes happen to exist on the
/// collections. This runs before index selection, so it consults the
/// collection's indexes directly rather than any IndexNode.
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
  mutable std::unordered_map<EnumerateCollectionNode const*, double> _counts;
  mutable std::unordered_map<std::string, DistinctEstimate> _distinct;
  mutable std::unordered_map<std::string, bool> _covering;
};

}  // namespace arangodb::aql
