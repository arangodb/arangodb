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

#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinCostEstimator.h"
#include "Aql/Optimizer/Rule/OptimizeJoinOrder/JoinGraph.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace arangodb::aql {
class EnumerateCollectionNode;
class ExecutionNode;
class ExecutionPlan;
struct Variable;

/// @brief a chosen enumeration order together with the estimate that chose it.
struct JoinOrder {
  std::vector<EnumerateCollectionNode*> order;
  JoinEstimate estimate;
};

/// @brief cost a complete enumeration order by replaying it through the
/// estimator.
auto getEstimateForOrder(JoinGraph& graph, JoinCostEstimator const& estimator,
                         std::vector<EnumerateCollectionNode*> const& order)
    -> JoinEstimate;

/// @brief the cheapest order for one connected component, by greedy search
/// from every possible start vertex. Ties break on ExecutionNode::id() so the
/// result is reproducible across processes.
auto getBestOrderForComponent(JoinGraph& graph,
                              std::vector<Variable const*> const& component,
                              JoinCostEstimator const& estimator) -> JoinOrder;

/// @brief above this many enumerations in one run the per-component search is
/// not worth the optimizer time, so the order is left untouched.
constexpr size_t kMaxEnumerationsToReorder = 16;

/// @brief the run's enumerations in spine order, from `firstEnumeration` up
/// to but excluding `next` (nullptr for a run reaching the top of the plan).
auto collectEnumerationOrder(ExecutionNode* firstEnumeration,
                             ExecutionNode* next)
    -> std::vector<EnumerateCollectionNode*>;

/// @brief an order for the whole graph, or nullopt to leave the plan alone.
/// Two decisions are taken independently -- each component's internal order,
/// then the sequence of components -- and both are guarded the same way. See
/// decideComponentOrders and acceptsResequencing.
auto chooseJoinOrder(JoinGraph& graph, JoinCostEstimator const& estimator,
                     std::vector<EnumerateCollectionNode*> const& writtenOrder)
    -> std::optional<std::vector<EnumerateCollectionNode*>>;

/// @brief splice `order` into the plan in place of the run's current
/// enumeration order. Unlinking the enumerations leaves the run's calculations
/// and filters chained onto the run's first dependency; reinserting the
/// enumerations above them yields a valid, if un-optimised, plan, which
/// move-calculations-up-2 and move-filters-up-2 then repair.
void rewriteJoinGraph(ExecutionPlan& plan, ExecutionNode* firstEnumeration,
                      ExecutionNode* next,
                      std::vector<EnumerateCollectionNode*> const& order);

}  // namespace arangodb::aql
