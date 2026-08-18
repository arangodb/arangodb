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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Optimizer/Utils/JoinCostEstimator.h"
#include "Aql/Optimizer/Utils/JoinGraph.h"

#include <memory>
#include <vector>

namespace arangodb::aql {
class EnumerateCollectionNode;
class ExecutionNode;
class ExecutionPlan;
class Optimizer;
struct OptimizerRule;

/// @brief build the join graph for the maximal run of adjacent collection
/// enumerations that starts at `firstEnumeration`. Walks upwards (towards the
/// root) collecting enumerations and classifying the filters between them.
/// Returns the graph and, via `next`, the first node that terminated the run
/// (or nullptr if the run reached the top of the plan). This is a read-only
/// analysis: the plan is not modified.
auto buildJoinGraph(ExecutionPlan const* plan, ExecutionNode* firstEnumeration,
                    ExecutionNode*& next) -> JoinGraph;

/// @brief a chosen enumeration order together with the estimate that chose it.
struct JoinOrder {
  std::vector<EnumerateCollectionNode*> order;
  JoinEstimate estimate;
};

/// @brief cost a complete enumeration order by replaying it through the
/// estimator: seed on the first vertex, then extend by each subsequent vertex
/// using every edge that connects it to the prefix already placed.
auto estimateOrder(JoinGraph& graph, JoinCostEstimator const& estimator,
                   std::vector<EnumerateCollectionNode*> const& order)
    -> JoinEstimate;

/// @brief order one connected component by multi-start greedy: try every
/// vertex as the start, grow by repeatedly absorbing the adjacent vertex that
/// costs least, and keep the cheapest completed order. Ties break on
/// ExecutionNode::id() so the result is reproducible.
auto orderComponent(JoinGraph& graph,
                    std::vector<Variable const*> const& component,
                    JoinCostEstimator const& estimator) -> JoinOrder;

/// @brief the `optimize-join-order` optimizer rule.
void optimizeJoinOrder(Optimizer*, std::unique_ptr<ExecutionPlan>,
                       OptimizerRule const&);

}  // namespace arangodb::aql
