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
#include <optional>
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
auto getEstimateForOrder(JoinGraph& graph, JoinCostEstimator const& estimator,
                         std::vector<EnumerateCollectionNode*> const& order)
    -> JoinEstimate;

/// @brief order one connected component by multi-start greedy: try every
/// vertex as the start, grow by repeatedly absorbing the adjacent vertex that
/// costs least, and keep the cheapest completed order. Ties break on
/// ExecutionNode::id() so the result is reproducible.
auto orderComponent(JoinGraph& graph,
                    std::vector<Variable const*> const& component,
                    JoinCostEstimator const& estimator) -> JoinOrder;

/// @brief above this many enumerations in one run the per-component search is
/// not worth the optimizer time, so the order is left untouched.
constexpr size_t kMaxEnumerationsToReorder = 16;

/// @brief the relative cost improvement required before rewriting. Note the
/// arithmetic: the check is `chosen < current / (1 + kImprovementMargin)`, so
/// 0.25 demands that the chosen order cost at most 0.8 of the current one --
/// a 20% reduction, not 25%. Differences below the estimator's own error
/// (measured 1.7-3.1x on non-unique index estimates) are not signal, so the
/// exact figure matters far less than its direction: bias toward not rewriting.
constexpr double kImprovementMargin = 0.25;

/// @brief the enumerations of one run, in spine order, from
/// `firstEnumeration` up to but excluding `next` (pass nullptr for a run that
/// reached the top of the plan). Calculations and filters are skipped.
auto collectEnumerationOrder(ExecutionNode* firstEnumeration,
                             ExecutionNode* next)
    -> std::vector<EnumerateCollectionNode*>;

/// @brief pick an order for the whole graph, or std::nullopt to leave the plan
/// alone. Returns nullopt when the run is too large; otherwise two
/// independent accept/decline decisions are made, each guarded the same way:
///
/// - Per component: a component's greedy order replaces its written order
///   only when neither side of that component's own comparison rests on a
///   fallback statistic and the greedy order clears kImprovementMargin
///   against the component's own written cost.
/// - Sequencing: components are then sequenced by the cheapest concatenation
///   (ties broken by node id), using whichever order each component ended up
///   with -- but that resequencing itself is only adopted when neither it nor
///   the written component sequence (components in the order they first
///   appear in currentOrder) rests on a fallback statistic, and the
///   resequenced concatenation clears kImprovementMargin against that
///   written sequence. Otherwise components stay in their written relative
///   order. With a single component this is a structural no-op.
///
/// Returns nullopt when neither decision actually changed anything, so a
/// graph left untouched is still reported unapplied.
auto chooseJoinOrder(JoinGraph& graph, JoinCostEstimator const& estimator,
                     std::vector<EnumerateCollectionNode*> const& currentOrder)
    -> std::optional<std::vector<EnumerateCollectionNode*>>;

/// @brief splice `order` into the plan in place of the run's current
/// enumeration order. Unlinking the enumerations leaves the run's calculations
/// and filters chained onto the run's first dependency; reinserting the
/// enumerations above them yields a valid, if un-optimised, plan, which
/// move-calculations-up-2 and move-filters-up-2 then repair.
void rewriteJoinGraph(ExecutionPlan& plan, ExecutionNode* firstEnumeration,
                      ExecutionNode* next,
                      std::vector<EnumerateCollectionNode*> const& order);

/// @brief the `optimize-join-order` optimizer rule.
void optimizeJoinOrder(Optimizer*, std::unique_ptr<ExecutionPlan>,
                       OptimizerRule const&);

}  // namespace arangodb::aql
