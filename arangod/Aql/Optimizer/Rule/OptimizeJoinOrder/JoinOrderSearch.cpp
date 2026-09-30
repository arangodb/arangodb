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
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace arangodb::aql {

namespace {

/// @brief the relative cost improvement required before rewriting. Note the
/// arithmetic: the check is `chosen < current / (1 + kImprovementMargin)`, so
/// 0.25 demands that the chosen order cost at most 0.8 of the current one --
/// a 20% reduction, not 25%. Differences below the estimator's own error
/// on non-unique index estimates are not signal, so the
/// exact figure matters far less than its direction: bias toward not rewriting.
constexpr double kImprovementMargin = 0.25;

/// @brief every edge joining `candidate` to a vertex already in `placed`,
/// written into `out`, which is cleared first.
///
/// The caller owns the buffer so it can be reused across the search's inner
/// loop rather than reallocated on each of its O(n^3) iterations.
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

/// @brief the estimate after appending `next` to a prefix, joined through
/// every edge to a vertex in `placed`. `connecting` is the caller's scratch
/// buffer and holds those edges on return, so the caller can tell a join
/// from a cross product. Inserting into `placed` is left to the caller,
/// which may be trying `next` rather than committing to it.
auto extendPrefix(JoinGraph& graph, JoinCostEstimator const& estimator,
                  JoinEstimate const& prefix,
                  std::unordered_set<JoinGraph::Node const*> const& placed,
                  JoinGraph::Node* next,
                  std::vector<JoinGraph::Edge const*>& connecting)
    -> JoinEstimate {
  edgesToPrefix(graph, next, placed, connecting);
  return estimator.extend(prefix, *next, connecting);
}

/// @brief the component's nodes in a reproducible order. JoinGraph::nodes is
/// keyed by Variable const*, so iterating it is address-ordered and would make
/// plan choice vary between runs.
auto nodesInIdOrder(JoinGraph& graph,
                    std::vector<Variable const*> const& component)
    -> std::vector<JoinGraph::Node*> {
  std::vector<JoinGraph::Node*> nodes;
  nodes.reserve(component.size());
  for (auto const* variable : component) {
    if (auto* node = graph.nodeForVariable(variable); node != nullptr) {
      nodes.emplace_back(node);
    }
  }
  std::sort(nodes.begin(), nodes.end(), [](auto const* l, auto const* r) {
    return l->executionNode->id() < r->executionNode->id();
  });
  return nodes;
}

/// @brief one component's vertices, in the order they already appear in
/// `writtenOrder` (i.e. the plan as written). This is the baseline a
/// component's greedy order is judged against: each component's accept/
/// decline decision compares against its own written order, never the whole
/// graph's.
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

  // buildJoinGraph and collectEnumerationOrder walk the same node range with
  // the same predicate, so every component member appears here exactly once.
  // Assert at the cause: a drift between those two walks would otherwise only
  // surface as the permutation assertion much later.
  ADB_PROD_ASSERT(order.size() == component.size())
      << "component of " << component.size() << " vertices matched only "
      << order.size() << " of the " << writtenOrder.size()
      << " written enumerations";
  return order;
}

/// @brief a connected component and the outcome of its own accept/decline
/// decision. `order` carries the winner either way, so callers concatenate
/// these without checking which way each decision went.
struct DecidedComponent {
  /// @brief the greedy order if it was accepted, the written order if not.
  JoinOrder order;
  /// @brief where this component's earliest member sits in the plan's written
  /// enumeration order. Sequences the components against one another.
  size_t firstAppearance;
  /// @brief whether the greedy order replaced the written one.
  bool reordered;
};

/// @brief order each connected component internally, then, independently
/// for each, decide whether its greedy order is confident and cheap enough
/// to replace the order it was written in. This decision must be made per
/// component, not once for the whole graph: a run with two components, one
/// fully indexed and one not, must not lose the confident reordering of the
/// first just because the second's statistics are guesses. Each component
/// stands or falls on a comparison against its own written order.
auto decideInternalOrdersForEachComponent(
    JoinGraph& graph, JoinCostEstimator const& estimator,
    std::vector<EnumerateCollectionNode*> const& writtenOrder)
    -> std::vector<DecidedComponent> {
  std::unordered_map<EnumerateCollectionNode*, size_t> positionInWritten;
  positionInWritten.reserve(writtenOrder.size());
  for (size_t i = 0; i < writtenOrder.size(); ++i) {
    positionInWritten.emplace(writtenOrder[i], i);
  }

  std::vector<DecidedComponent> decided;
  for (auto const& component : graph.connectedComponents()) {
    auto greedy = getBestOrderForComponent(graph, component, estimator);
    auto written = writtenComponentOrder(component, writtenOrder);
    auto writtenEstimate = getEstimateForOrder(graph, estimator, written);

    ADB_PROD_ASSERT(!written.empty());
    auto const positionIt = positionInWritten.find(written.front());
    ADB_PROD_ASSERT(positionIt != positionInWritten.end());
    size_t const firstAppearance = positionIt->second;

    // Either side resting on a fallback statistic makes this a comparison
    // between two guesses -- decline.
    if (greedy.estimate.defaulted || writtenEstimate.defaulted) {
      LOG_TOPIC("a7f04", TRACE, Logger::AQL)
          << "optimize-join-order: keeping a component's written order, "
             "estimate rests on defaulted statistics";
      decided.emplace_back(
          DecidedComponent{JoinOrder{std::move(written), writtenEstimate},
                           firstAppearance, false});
      continue;
    }

    // Require a real margin against this component's own written cost.
    if (greedy.estimate.cost >=
        writtenEstimate.cost / (1.0 + kImprovementMargin)) {
      LOG_TOPIC("a7f05", TRACE, Logger::AQL)
          << "optimize-join-order: keeping a component's written order, "
          << writtenEstimate.cost << " -> " << greedy.estimate.cost
          << " does not clear the margin";
      decided.emplace_back(
          DecidedComponent{JoinOrder{std::move(written), writtenEstimate},
                           firstAppearance, false});
      continue;
    }

    decided.emplace_back(
        DecidedComponent{std::move(greedy), firstAppearance, true});
  }

  return decided;
}

/// @brief the components' own final orders concatenated in the order those
/// components first appear in the written plan. This is the baseline the
/// resequencing decision is judged against. With a single component there is
/// only one possible sequence, so that decision is a structural no-op.
auto concatenateInWrittenSequence(std::vector<DecidedComponent> const& decided,
                                  size_t total)
    -> std::vector<EnumerateCollectionNode*> {
  std::vector<size_t> byAppearance(decided.size());
  for (size_t i = 0; i < byAppearance.size(); ++i) {
    byAppearance[i] = i;
  }
  std::sort(byAppearance.begin(), byAppearance.end(), [&](size_t l, size_t r) {
    return decided[l].firstAppearance < decided[r].firstAppearance;
  });

  std::vector<EnumerateCollectionNode*> baseline;
  baseline.reserve(total);
  for (size_t index : byAppearance) {
    auto const& order = decided[index].order.order;
    baseline.insert(baseline.end(), order.begin(), order.end());
  }
  return baseline;
}

/// @brief the cheapest concatenation of the components: they join by cross
/// product, so they are sequenced greedily too, one winner at a time.
///
/// Every concatenation ends at the same cardinality -- a cross product
/// multiplies, and multiplication commutes. Cost still differs, because a
/// cross-product step charges `prefix.cardinality * count(next)`: each
/// component's work is multiplied by the accumulated cardinality of everything
/// placed before it. Hence the candidates below are costed as whole prefixes
/// rather than in isolation.
///
/// The estimates already stored in `decided` cannot stand in for that replay.
/// Each was built with its component's first node going through seed(); in any
/// position but the first, that node goes through extend() with no connecting
/// edges instead, and nothing in the JoinCostEstimator contract makes the two
/// agree. For the System-R estimator they do not: seed() may charge an index
/// lookup over the constant restrictions, a cross-product extend() always
/// charges a full scan per outer row. Scaling the stored estimate by the
/// prefix cardinality is no substitute either, since it assumes cost is linear
/// in the prefix -- a property of one estimator, broken even there by the
/// cardinality floor, and not part of the contract.
auto getCheapestConcatenation(JoinGraph& graph,
                              JoinCostEstimator const& estimator,
                              std::vector<DecidedComponent> decided,
                              size_t total)
    -> std::vector<EnumerateCollectionNode*> {
  // connectedComponents() iterates a std::map<Variable const*, Node>, so the
  // component list it returns comes out in heap-address order, which varies
  // between processes. Without this sort, the selection loop below -- which
  // only replaces `bestIndex` on a strict cost improvement -- tie-breaks
  // equal-cost components by that address order, making the final
  // concatenation (and therefore whether it clears the improvement margin)
  // non-deterministic.
  std::sort(decided.begin(), decided.end(),
            [](DecidedComponent const& lhs, DecidedComponent const& rhs) {
              return lhs.order.order.front()->id() <
                     rhs.order.order.front()->id();
            });

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

/// @brief whether the cheapest concatenation may replace the written
/// sequence of components.
auto acceptsResequencing(JoinGraph& graph, JoinCostEstimator const& estimator,
                         JoinEstimate const& baselineEstimate,
                         std::vector<EnumerateCollectionNode*> const& candidate)
    -> bool {
  TRI_ASSERT(!baselineEstimate.defaulted);
  auto const candidateEstimate =
      getEstimateForOrder(graph, estimator, candidate);

  if (candidateEstimate.defaulted) {
    LOG_TOPIC("a7f06", TRACE, Logger::AQL)
        << "optimize-join-order: keeping the written component sequence, "
           "estimate rests on defaulted statistics";
    return false;
  }

  if (candidateEstimate.cost >=
      baselineEstimate.cost / (1.0 + kImprovementMargin)) {
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
  auto const nodes = nodesInIdOrder(graph, component);
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

      // Defensive: a disconnected "component" would leave nothing adjacent.
      // Fall back to the lowest-id remaining vertex as a cross product so the
      // order still covers every vertex.
      if (chosen == nullptr) {
        for (auto* next : nodes) {
          if (!placed.contains(next)) {
            chosen = next;
            chosenEstimate = extendPrefix(graph, estimator, candidate.estimate,
                                          placed, next, connecting);
            break;
          }
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

  // Estimated before the search, not after: getCheapestConcatenation() costs
  // O(k^2) replays for k components, and all of them are wasted if the
  // sequence cannot be replaced anyway.
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

  ADB_PROD_ASSERT(chosen.size() == graph.nodes.size());
  ADB_PROD_ASSERT(std::unordered_set(chosen.begin(), chosen.end()).size() ==
                  chosen.size());
  TRI_ASSERT(chosen != writtenOrder);

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

  // A permutation check, not merely a size check: the loop below unlinks every
  // enumeration and reinserts only what `order` holds, so a duplicate paired
  // with an omission would silently delete a FOR loop from the query -- which
  // no assertion on the resulting *order* would catch.
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
