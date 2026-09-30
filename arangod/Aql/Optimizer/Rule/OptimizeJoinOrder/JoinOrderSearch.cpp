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

#include "JoinOrderSearch.h"

#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Assertions/Assert.h"
#include "Assertions/ProdAssert.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>

namespace arangodb::aql {

namespace {

/// @brief a rewrite must cost less than this share of the written order.
/// Smaller differences are within the estimator's own error; bias toward not
/// rewriting.
constexpr double kMaxCostRatio = 0.8;

/// @brief every edge joining `candidate` to a vertex already in `placed`,
/// written into `out`, which is cleared first.
void edgesToPrefix(JoinGraph& graph, JoinGraph::Node* candidate,
                   std::unordered_set<JoinGraph::Node const*> const& placed,
                   std::vector<JoinGraph::Edge const*>& out) {
  out.clear();
  for (auto* edge : graph.getEdgesForNode(candidate)) {
    TRI_ASSERT(edge->from != edge->to);
    auto const* other = (edge->from == candidate) ? edge->to : edge->from;
    if (placed.contains(other)) {
      out.emplace_back(edge);
    }
  }
}

/// @brief the estimate after appending `next` to a prefix. `connecting` holds
/// the edges used on return, so the caller can tell a join from a cross
/// product; inserting into `placed` is the caller's call, since it may only
/// be trying `next`.
auto extendPrefix(JoinGraph& graph, JoinCostEstimator const& estimator,
                  JoinEstimate const& prefix,
                  std::unordered_set<JoinGraph::Node const*> const& placed,
                  JoinGraph::Node* next,
                  std::vector<JoinGraph::Edge const*>& connecting)
    -> JoinEstimate {
  edgesToPrefix(graph, next, placed, connecting);
  return estimator.extend(prefix, *next, connecting);
}

auto nodesOf(JoinGraph& graph, std::vector<Variable const*> const& component)
    -> std::vector<JoinGraph::Node*> {
  std::vector<JoinGraph::Node*> nodes;
  nodes.reserve(component.size());
  for (auto const* variable : component) {
    nodes.emplace_back(graph.nodeForVariable(variable));
  }
  return nodes;
}

/// @brief one component's vertices in the order the plan was written.
auto writtenComponentOrder(
    std::vector<Variable const*> const& component,
    std::vector<EnumerateCollectionNode*> const& writtenOrder)
    -> std::vector<EnumerateCollectionNode*> {
  std::unordered_set members(component.begin(), component.end());
  std::vector<EnumerateCollectionNode*> order;
  order.reserve(component.size());
  for (auto* node : writtenOrder) {
    if (members.contains(node->outVariable())) {
      order.emplace_back(node);
    }
  }

  return order;
}

/// @brief a component and the order that won its accept/decline decision.
struct DecidedComponent {
  /// @brief the greedy order if it was accepted, the written order if not.
  JoinOrder order;
  /// @brief whether the greedy order replaced the written one.
  bool reordered;
};

/// @brief decided per component, not per graph, so that one component's
/// guessed statistics do not cost another its confident reordering.
auto decideInternalOrdersForEachComponent(
    JoinGraph& graph, JoinCostEstimator const& estimator,
    std::vector<EnumerateCollectionNode*> const& writtenOrder)
    -> std::vector<DecidedComponent> {
  std::vector<DecidedComponent> decided;
  for (auto const& component : graph.connectedComponents()) {
    auto greedy = getBestOrderForComponent(graph, component, estimator);
    auto written = writtenComponentOrder(component, writtenOrder);
    auto writtenEstimate = getEstimateForOrder(graph, estimator, written);

    // Both orders read the same statistics, so one flag covers both: a
    // fallback anywhere makes this a comparison between guesses -- decline.
    if (writtenEstimate.defaulted) {
      LOG_TOPIC("a7f04", TRACE, Logger::AQL)
          << "optimize-join-order: keeping a component's written order, "
             "estimate rests on defaulted statistics";
      decided.emplace_back(DecidedComponent{
          JoinOrder{std::move(written), writtenEstimate}, false});
      continue;
    }

    // Require a real margin against this component's own written cost.
    if (greedy.estimate.cost >= writtenEstimate.cost * kMaxCostRatio) {
      LOG_TOPIC("a7f05", TRACE, Logger::AQL)
          << "optimize-join-order: keeping a component's written order, "
          << writtenEstimate.cost << " -> " << greedy.estimate.cost
          << " does not clear the margin";
      decided.emplace_back(DecidedComponent{
          JoinOrder{std::move(written), writtenEstimate}, false});
      continue;
    }

    decided.emplace_back(DecidedComponent{std::move(greedy), true});
  }

  return decided;
}

/// @brief the decided orders concatenated in written sequence, which is the
/// order connectedComponents() yields: the baseline for resequencing.
auto concatenateInWrittenSequence(std::vector<DecidedComponent> const& decided,
                                  size_t total)
    -> std::vector<EnumerateCollectionNode*> {
  std::vector<EnumerateCollectionNode*> baseline;
  baseline.reserve(total);
  for (auto const& component : decided) {
    auto const& order = component.order.order;
    baseline.insert(baseline.end(), order.begin(), order.end());
  }
  return baseline;
}

/// @brief the cheapest concatenation of the components, sequenced greedily.
/// Candidates are costed as whole prefixes: a component's cost scales with
/// the cardinality placed before it, and its stored estimate began with
/// seed() where a later position goes through a cross-product extend().
auto getCheapestConcatenation(JoinGraph& graph,
                              JoinCostEstimator const& estimator,
                              std::vector<DecidedComponent> decided,
                              size_t total)
    -> std::vector<EnumerateCollectionNode*> {
  std::vector<EnumerateCollectionNode*> candidate;
  candidate.reserve(total);
  while (!decided.empty()) {
    size_t bestIndex = 0;
    std::optional<double> bestCost;
    for (size_t i = 0; i < decided.size(); ++i) {
      auto attempt = candidate;
      attempt.insert(attempt.end(), decided[i].order.order.begin(),
                     decided[i].order.order.end());
      double const cost = getEstimateForOrder(graph, estimator, attempt).cost;
      if (!bestCost.has_value() || cost < *bestCost) {
        bestCost = cost;
        bestIndex = i;
      }
    }
    auto const& winner = decided[bestIndex].order.order;
    candidate.insert(candidate.end(), winner.begin(), winner.end());
    decided.erase(decided.begin() + static_cast<std::ptrdiff_t>(bestIndex));
  }
  return candidate;
}

auto names(std::vector<EnumerateCollectionNode*> const& order) -> std::string {
  std::string result;
  for (auto const* node : order) {
    result += (result.empty() ? "" : ", ") + node->outVariable()->name;
  }
  return result;
}

/// @brief whether the cheapest concatenation may replace the written
/// sequence of components.
auto acceptsResequencing(JoinGraph& graph, JoinCostEstimator const& estimator,
                         JoinEstimate const& baselineEstimate,
                         std::vector<EnumerateCollectionNode*> const& candidate)
    -> bool {
  auto const candidateEstimate =
      getEstimateForOrder(graph, estimator, candidate);

  if (candidateEstimate.cost >= baselineEstimate.cost * kMaxCostRatio) {
    LOG_TOPIC("a7f07", TRACE, Logger::AQL)
        << "optimize-join-order: keeping the written component sequence, "
        << baselineEstimate.cost << " -> " << candidateEstimate.cost
        << " does not clear the margin";
    return false;
  }

  return true;
}
}  // namespace

auto getEstimateForOrder(JoinGraph& graph, JoinCostEstimator const& estimator,
                         std::vector<EnumerateCollectionNode*> const& order)
    -> JoinEstimate {
  JoinEstimate estimate;
  std::unordered_set<JoinGraph::Node const*> placed;
  std::vector<JoinGraph::Edge const*> connecting;

  for (size_t i = 0; i < order.size(); ++i) {
    auto* node = graph.nodeForVariable(order[i]->outVariable());
    ADB_PROD_ASSERT(node != nullptr);
    estimate = (i == 0) ? estimator.seed(*node)
                        : extendPrefix(graph, estimator, estimate, placed, node,
                                       connecting);
    placed.insert(node);
  }
  return estimate;
}

auto getBestOrderForComponent(JoinGraph& graph,
                              std::vector<Variable const*> const& component,
                              JoinCostEstimator const& estimator) -> JoinOrder {
  auto const nodes = nodesOf(graph, component);
  ADB_PROD_ASSERT(!nodes.empty());

  std::optional<JoinOrder> best;
  std::vector<JoinGraph::Edge const*> connecting;

  for (auto* start : nodes) {
    JoinOrder candidate;
    candidate.order.reserve(nodes.size());
    candidate.order.emplace_back(start->executionNode);
    candidate.estimate = estimator.seed(*start);

    std::unordered_set<JoinGraph::Node const*> placed{start};

    while (candidate.order.size() < nodes.size()) {
      JoinGraph::Node* chosen = nullptr;
      JoinEstimate chosenEstimate;

      for (auto* next : nodes) {
        if (placed.contains(next)) {
          continue;
        }
        auto estimate = extendPrefix(graph, estimator, candidate.estimate,
                                     placed, next, connecting);
        if (connecting.empty()) {
          // not adjacent to the prefix yet; within a connected component some
          // other vertex is, so defer this one rather than cross-producting.
          continue;
        }
        if (chosen == nullptr || estimate.cost < chosenEstimate.cost) {
          chosen = next;
          chosenEstimate = estimate;
        }
      }

      ADB_PROD_ASSERT(chosen != nullptr);

      candidate.order.emplace_back(chosen->executionNode);
      candidate.estimate = chosenEstimate;
      placed.insert(chosen);
    }

    if (!best.has_value() || candidate.estimate.cost < best->estimate.cost) {
      best = std::move(candidate);
    }
  }

  return std::move(*best);
}

auto collectEnumerationOrder(ExecutionNode* firstEnumeration,
                             ExecutionNode* next)
    -> std::vector<EnumerateCollectionNode*> {
  std::vector<EnumerateCollectionNode*> order;
  for (ExecutionNode* n = firstEnumeration; n != nullptr && n != next;
       n = n->getFirstParent()) {
    if (n->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
      order.emplace_back(ExecutionNode::castTo<EnumerateCollectionNode*>(n));
    }
  }
  return order;
}

auto chooseJoinOrder(JoinGraph& graph, JoinCostEstimator const& estimator,
                     std::vector<EnumerateCollectionNode*> const& writtenOrder)
    -> std::optional<std::vector<EnumerateCollectionNode*>> {
  if (graph.nodes.size() > kMaxEnumerationsToReorder) {
    LOG_TOPIC("a7f03", TRACE, Logger::AQL)
        << "optimize-join-order: skipping a run of " << graph.nodes.size()
        << " enumerations, above the reordering cap ("
        << kMaxEnumerationsToReorder << ")";
    return std::nullopt;
  }

  auto const decided =
      decideInternalOrdersForEachComponent(graph, estimator, writtenOrder);
  bool const anyComponentReordered =
      std::any_of(decided.begin(), decided.end(),
                  [](DecidedComponent const& c) { return c.reordered; });

  auto baseline = concatenateInWrittenSequence(decided, graph.nodes.size());

  auto const baselineEstimate = getEstimateForOrder(graph, estimator, baseline);

  bool sequenceChanged = false;
  std::vector<EnumerateCollectionNode*> chosen;
  if (baselineEstimate.defaulted) {
    LOG_TOPIC("a7f08", TRACE, Logger::AQL)
        << "optimize-join-order: keeping the written component sequence, "
           "the sequence estimate rests on defaulted statistics";
    chosen = std::move(baseline);
  } else {
    auto candidate =
        getCheapestConcatenation(graph, estimator, decided, graph.nodes.size());
    sequenceChanged =
        acceptsResequencing(graph, estimator, baselineEstimate, candidate);
    chosen = sequenceChanged ? std::move(candidate) : std::move(baseline);
  }

  if (!anyComponentReordered && !sequenceChanged) {
    return std::nullopt;
  }

  TRI_ASSERT(chosen != writtenOrder);
  LOG_TOPIC("a7f06", TRACE, Logger::AQL)
      << "optimize-join-order: rewriting " << names(writtenOrder) << " -> "
      << names(chosen);

  return chosen;
}

void rewritePlan(ExecutionPlan& plan,
                 std::vector<EnumerateCollectionNode*> const& current,
                 std::vector<EnumerateCollectionNode*> const& order) {
  ADB_PROD_ASSERT(!current.empty());
  // Capture the anchor before touching anything: after the unlink loop the
  // spine no longer contains the enumerations.
  ExecutionNode* firstDependency = current.front()->getFirstDependency();
  ADB_PROD_ASSERT(firstDependency != nullptr);

  // a duplicate paired with an omission would silently delete a FOR loop
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    auto sortedCurrent = current;
    auto sortedOrder = order;
    auto byId = [](auto const* l, auto const* r) { return l->id() < r->id(); };
    std::sort(sortedCurrent.begin(), sortedCurrent.end(), byId);
    std::sort(sortedOrder.begin(), sortedOrder.end(), byId);
    TRI_ASSERT(sortedCurrent == sortedOrder);
  }
#endif

  for (auto* enumeration : current) {
    plan.unlinkNode(enumeration);
  }

  // insertAfter splices the new node in as the parent of `previous`, so
  // inserting in reverse against a fixed anchor yields the forward order.
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    plan.insertAfter(firstDependency, *it);
  }

  plan.clearVarUsageComputed();
}

}  // namespace arangodb::aql
