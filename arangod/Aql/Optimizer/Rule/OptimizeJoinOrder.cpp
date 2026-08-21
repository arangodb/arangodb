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

#include "OptimizeJoinOrder.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Variable.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Assertions/ProdAssert.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <algorithm>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace arangodb::aql {

namespace {

// @brief classify the predicates of a filter expression into the join graph:
// equijoins between two graph variables become edges, constant restrictions on
// a single graph variable become node conditions, everything else is recorded
// verbatim as a residual.
void handleExpression(JoinGraph& graph, ExecutionPlan const* plan,
                      AstNode const* original) {
  Condition cond{plan->getAst()};
  cond.andCombine(original);
  cond.normalize();
  AstNode const* root = cond.root();

  // normalize() produces disjunctive normal form: OR( AND(...), AND(...), ...
  // ). We can only reason about a single conjunction of predicates; anything
  // with a real disjunction is kept as a residual for later stages.
  if (root->type != NODE_TYPE_OPERATOR_NARY_OR || root->numMembers() != 1) {
    graph.addResidual(original);
    return;
  }

  auto ands = root->getMemberUnchecked(0);
  if (ands->type != NODE_TYPE_OPERATOR_NARY_AND) {
    graph.addResidual(original);
    return;
  }

  auto checkIsGraphVariableAccess =
      [&](std::optional<std::pair<Variable const*, AttributePath>> maybeAccess)
      -> std::optional<
          std::tuple<Variable const*, JoinGraph::Node*, AttributePath>> {
    if (maybeAccess.has_value()) {
      auto& [var, path] = *maybeAccess;
      if (auto node = graph.nodeForVariable(var); node != nullptr) {
        return std::make_tuple(var, node, std::move(path));
      }
    }
    return std::nullopt;
  };

  for (size_t i = 0; i < ands->numMembers(); i++) {
    auto predicate = ands->getMemberUnchecked(i);

    // only equijoins (`==`) are understood; anything else is a residual
    if (predicate->type != NODE_TYPE_OPERATOR_BINARY_EQ) {
      graph.addResidual(predicate);
      continue;
    }

    auto lhs = predicate->getMemberUnchecked(0);
    auto maybeLhsAccess =
        checkIsGraphVariableAccess(extractAttributeAccess(lhs));

    auto rhs = predicate->getMemberUnchecked(1);
    auto maybeRhsAccess =
        checkIsGraphVariableAccess(extractAttributeAccess(rhs));

    // canonicalize: if only one side is a graph variable, keep it on the left
    if (maybeRhsAccess.has_value() && !maybeLhsAccess.has_value()) {
      std::swap(maybeLhsAccess, maybeRhsAccess);
    }

    if (maybeLhsAccess.has_value() && maybeRhsAccess.has_value()) {
      // `a.x == b.y` with both a and b in the graph -> join edge
      [[maybe_unused]] auto& [lhsVar, lhsNode, lhsPath] =
          maybeLhsAccess.value();
      [[maybe_unused]] auto& [rhsVar, rhsNode, rhsPath] =
          maybeRhsAccess.value();
      graph.addJoinCondition(lhsVar, lhsPath, rhsVar, rhsPath);
    } else if (maybeLhsAccess.has_value()) {
      // `a.x == <constant>` -> constant restriction on node a
      [[maybe_unused]] auto& [lhsVar, lhsNode, lhsPath] =
          maybeLhsAccess.value();
      lhsNode->conditions.emplace_back(std::move(lhsPath));
    } else {
      // neither side references a graph variable -> residual
      graph.addResidual(predicate);
    }
  }
}

/// @brief every edge joining `candidate` to a vertex already in `placed`.
/// Self-loops are skipped: they are single-node filters, not join predicates.
auto edgesToPrefix(JoinGraph& graph, JoinGraph::Node* candidate,
                   std::unordered_set<JoinGraph::Node const*> const& placed)
    -> std::vector<JoinGraph::Edge const*> {
  std::vector<JoinGraph::Edge const*> result;
  for (auto* edge : graph.getEdgesForNode(candidate)) {
    if (edge->from == edge->to) {
      continue;
    }
    auto const* other = (edge->from == candidate) ? edge->to : edge->from;
    if (placed.contains(other)) {
      result.emplace_back(edge);
    }
  }
  return result;
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
/// `currentOrder` (i.e. the plan as written). This is the baseline a
/// component's greedy order is judged against: each component's accept/
/// decline decision compares against its own written order, never the whole
/// graph's.
auto writtenComponentOrder(
    std::vector<Variable const*> const& component,
    std::vector<EnumerateCollectionNode*> const& currentOrder)
    -> std::vector<EnumerateCollectionNode*> {
  std::unordered_set<Variable const*> members(component.begin(),
                                              component.end());
  std::vector<EnumerateCollectionNode*> order;
  order.reserve(component.size());
  for (auto* node : currentOrder) {
    if (members.contains(node->outVariable())) {
      order.emplace_back(node);
    }
  }

  // `currentOrder` and the graph's vertices are produced by two separate
  // walks over the same node range -- buildJoinGraph and
  // collectEnumerationOrder -- with the same "is an ENUMERATE_COLLECTION"
  // predicate, and connectedComponents() partitions exactly those vertices.
  // Every component member therefore appears here exactly once, and this
  // subsequence has the component's full size. Should the two walks ever
  // drift apart, a component would silently lose a vertex here; that only
  // surfaces much later, as the permutation assertion at the end of
  // chooseJoinOrder. Fail at the cause instead.
  ADB_PROD_ASSERT(order.size() == component.size())
      << "component of " << component.size() << " vertices matched only "
      << order.size() << " of the " << currentOrder.size()
      << " written enumerations";
  return order;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void traceJoinGraph(JoinGraph const& graph) {
  auto components = graph.connectedComponents();
  LOG_TOPIC("a7f01", TRACE, Logger::AQL)
      << "optimize-join-order: join graph with " << graph.nodes.size()
      << " node(s), " << graph.edges.size() << " edge(s), "
      << graph.residuals.size() << " residual(s), " << components.size()
      << " component(s)";
  for (auto const& e : graph.edges) {
    LOG_TOPIC("a7f02", TRACE, Logger::AQL)
        << "optimize-join-order:   edge "
        << e.from->executionNode->outVariable()->name << " <-> "
        << e.to->executionNode->outVariable()->name;
  }
}
#endif

}  // namespace

// -----------------------------------------------------------------------------
// graph construction
// -----------------------------------------------------------------------------

auto buildJoinGraph(ExecutionPlan const* plan, ExecutionNode* firstEnumeration,
                    ExecutionNode*& next) -> JoinGraph {
  JoinGraph graph;
  next = nullptr;

  ExecutionNode* nextNode = nullptr;
  for (ExecutionNode* n = firstEnumeration; n != nullptr; n = nextNode) {
    nextNode = n->getFirstParent();

    switch (n->getType()) {
      case ExecutionNode::ENUMERATE_COLLECTION: {
        graph.addNode(ExecutionNode::castTo<EnumerateCollectionNode*>(n));
        break;
      }

      case ExecutionNode::CALCULATION: {
        auto* calc = ExecutionNode::castTo<CalculationNode*>(n);
        if (!calc->expression()->isDeterministic()) {
          graph.hasNonDeterministicCalculation = true;
        }
        break;
      }

      case ExecutionNode::FILTER: {
        auto* filter = ExecutionNode::castTo<FilterNode*>(n);
        auto* setter = plan->getVarSetBy(filter->inVariable()->id);
        // the filter's condition is only analyzable when it is produced by a
        // calculation; otherwise we simply leave it in place (it keeps
        // executing, so nothing is lost) and do not model it.
        if (setter == nullptr ||
            setter->getType() != ExecutionNode::CALCULATION) {
          break;
        }
        auto* calc = ExecutionNode::castTo<CalculationNode*>(setter);
        handleExpression(graph, plan, calc->expression()->node());
        break;
      }

      default:
        // any other node type terminates the run of adjacent enumerations
        next = n;
        return graph;
    }
  }

  return graph;
}

// -----------------------------------------------------------------------------
// ordering
// -----------------------------------------------------------------------------

auto getEstimateForOrder(JoinGraph& graph, JoinCostEstimator const& estimator,
                         std::vector<EnumerateCollectionNode*> const& order)
    -> JoinEstimate {
  JoinEstimate estimate;
  std::unordered_set<JoinGraph::Node const*> placed;

  for (size_t i = 0; i < order.size(); ++i) {
    auto* node = graph.nodeForVariable(order[i]->outVariable());
    ADB_PROD_ASSERT(node != nullptr);
    if (i == 0) {
      estimate = estimator.seed(*node);
    } else {
      auto const connecting = edgesToPrefix(graph, node, placed);
      estimate = estimator.extend(estimate, *node, connecting);
    }
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
        auto connecting = edgesToPrefix(graph, next, placed);
        if (connecting.empty()) {
          // not adjacent to the prefix yet; within a connected component some
          // other vertex is, so defer this one rather than cross-producting.
          continue;
        }
        auto estimate = estimator.extend(candidate.estimate, *next, connecting);
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
            chosenEstimate = estimator.extend(candidate.estimate, *next, {});
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
                     std::vector<EnumerateCollectionNode*> const& currentOrder)
    -> std::optional<std::vector<EnumerateCollectionNode*>> {
  if (graph.nodes.size() > kMaxEnumerationsToReorder) {
    LOG_TOPIC("a7f03", TRACE, Logger::AQL)
        << "optimize-join-order: skipping a run of " << graph.nodes.size()
        << " enumerations, above the reordering cap";
    return std::nullopt;
  }

  // Position of each enumeration in the written order, so each component's
  // place in the written *sequence of components* (used by the resequencing
  // guard below) can be recovered from where its first vertex sits here.
  std::unordered_map<EnumerateCollectionNode*, size_t> positionInCurrent;
  positionInCurrent.reserve(currentOrder.size());
  for (size_t i = 0; i < currentOrder.size(); ++i) {
    positionInCurrent.emplace(currentOrder[i], i);
  }

  // Order each component internally, then -- independently for each
  // component -- decide whether its greedy order is confident and cheap
  // enough to replace the order it was written in. This decision must be
  // made per component, not once for the whole graph: a run with two
  // components, one fully indexed and one not, must not lose the confident
  // reordering of the first just because the second's statistics are
  // guesses. Each component stands or falls on a comparison against its own
  // written order.
  bool anyComponentReordered = false;
  std::vector<JoinOrder> componentOrders;
  std::vector<size_t> firstAppearance;  // parallel to componentOrders
  for (auto const& component : graph.connectedComponents()) {
    auto greedy = getBestOrderForComponent(graph, component, estimator);
    auto written = writtenComponentOrder(component, currentOrder);
    auto writtenEstimate = getEstimateForOrder(graph, estimator, written);

    ADB_PROD_ASSERT(!written.empty());
    auto const positionIt = positionInCurrent.find(written.front());
    ADB_PROD_ASSERT(positionIt != positionInCurrent.end());
    firstAppearance.emplace_back(positionIt->second);

    // If either side of this component's comparison rests on a fallback
    // statistic, the comparison is between two guesses -- decline, same
    // reasoning as the old whole-graph guard, just scoped to this component.
    if (greedy.estimate.defaulted || writtenEstimate.defaulted) {
      LOG_TOPIC("a7f04", TRACE, Logger::AQL)
          << "optimize-join-order: keeping a component's written order, "
             "estimate rests on defaulted statistics";
      componentOrders.emplace_back(
          JoinOrder{std::move(written), std::move(writtenEstimate)});
      continue;
    }

    // Require a real margin against this component's own written cost:
    // differences below the estimator's own error are not signal.
    if (greedy.estimate.cost >=
        writtenEstimate.cost / (1.0 + kImprovementMargin)) {
      LOG_TOPIC("a7f05", TRACE, Logger::AQL)
          << "optimize-join-order: keeping a component's written order, "
          << writtenEstimate.cost << " -> " << greedy.estimate.cost
          << " does not clear the margin";
      componentOrders.emplace_back(
          JoinOrder{std::move(written), std::move(writtenEstimate)});
      continue;
    }

    componentOrders.emplace_back(std::move(greedy));
    anyComponentReordered = true;
  }

  // The written *sequence* of components: each component's own final order
  // (as just decided above), concatenated in the order those components
  // first appear in currentOrder. This is the baseline the resequencing
  // decision below is judged against. With a single component there is only
  // one possible sequence, so that decision is a structural no-op there.
  std::vector<size_t> byAppearance(componentOrders.size());
  for (size_t i = 0; i < byAppearance.size(); ++i) {
    byAppearance[i] = i;
  }
  std::sort(byAppearance.begin(), byAppearance.end(), [&](size_t l, size_t r) {
    return firstAppearance[l] < firstAppearance[r];
  });

  std::vector<EnumerateCollectionNode*> baseline;
  baseline.reserve(graph.nodes.size());
  for (size_t index : byAppearance) {
    auto const& order = componentOrders[index].order;
    baseline.insert(baseline.end(), order.begin(), order.end());
  }

  // connectedComponents() iterates a std::map<Variable const*, Node>, so the
  // component list it returns comes out in heap-address order, which varies
  // between processes. Without this sort, the selection loop below -- which
  // only replaces `bestIndex` on a strict cost improvement -- tie-breaks
  // equal-cost components by that address order, making the final
  // concatenation (and therefore whether it clears the improvement margin)
  // non-deterministic. Sorting by each component's first vertex id here
  // fixes the tie-break for every round below: erasing the winner each round
  // never disturbs the relative id-order of what remains, so this single
  // sort is enough for the whole sequencing loop. Do not remove this as
  // "redundant" -- ties are the common case, not an edge case, here.
  std::sort(componentOrders.begin(), componentOrders.end(),
            [](JoinOrder const& lhs, JoinOrder const& rhs) {
              return lhs.order.front()->id() < rhs.order.front()->id();
            });

  // The cheapest concatenation of components, same greedy selection as
  // before this change: components join by cross product, so they are
  // sequenced greedily too, one winner at a time.
  std::vector<EnumerateCollectionNode*> candidate;
  while (!componentOrders.empty()) {
    size_t bestIndex = 0;
    std::optional<double> bestCost;
    for (size_t i = 0; i < componentOrders.size(); ++i) {
      auto attempt = candidate;
      attempt.insert(attempt.end(), componentOrders[i].order.begin(),
                     componentOrders[i].order.end());
      double const cost = getEstimateForOrder(graph, estimator, attempt).cost;
      if (!bestCost.has_value() || cost < *bestCost) {
        bestCost = cost;
        bestIndex = i;
      }
    }
    auto& winner = componentOrders[bestIndex];
    candidate.insert(candidate.end(), winner.order.begin(), winner.order.end());
    componentOrders.erase(componentOrders.begin() +
                          static_cast<std::ptrdiff_t>(bestIndex));
  }

  // The selection loop above has no guard of its own -- it always picks the
  // cheapest concatenation, unconditionally, and that cost is computed by
  // replaying every component's vertices, including one that was just
  // declined above for resting on defaulted statistics. Left unguarded,
  // resequencing would apply exactly the statistics one branch earlier
  // declared untrustworthy to decide which component runs first. So
  // resequencing gets the same defaulted/margin treatment as a component's
  // own internal order, just judged against the written *sequence* of
  // components (`baseline`) rather than any one component's written order.
  // `defaulted` propagates through every `extend` call, so "neither estimate
  // is defaulted" already reduces to "no component's chosen order rests on
  // a fallback statistic" -- no separate per-component scan is needed here.
  auto const baselineEstimate = getEstimateForOrder(graph, estimator, baseline);
  auto const candidateEstimate =
      getEstimateForOrder(graph, estimator, candidate);

  bool const sequencingDefaulted =
      baselineEstimate.defaulted || candidateEstimate.defaulted;
  bool const sequencingClearsMargin =
      candidateEstimate.cost <
      baselineEstimate.cost / (1.0 + kImprovementMargin);
  bool const acceptSequencing = !sequencingDefaulted && sequencingClearsMargin;

  if (!acceptSequencing) {
    if (sequencingDefaulted) {
      LOG_TOPIC("a7f06", TRACE, Logger::AQL)
          << "optimize-join-order: keeping the written component sequence, "
             "estimate rests on defaulted statistics";
    } else {
      LOG_TOPIC("a7f07", TRACE, Logger::AQL)
          << "optimize-join-order: keeping the written component sequence, "
          << baselineEstimate.cost << " -> " << candidateEstimate.cost
          << " does not clear the margin";
    }
  }

  bool const sequenceChanged = acceptSequencing && candidate != baseline;
  std::vector<EnumerateCollectionNode*> chosen =
      acceptSequencing ? std::move(candidate) : std::move(baseline);

  // Nothing to do: every component kept the order it was written in, and
  // resequencing did not change which component runs first either.
  if (!anyComponentReordered && !sequenceChanged) {
    return std::nullopt;
  }

  // connectedComponents() partitions the vertex set, so the concatenation must
  // cover every vertex exactly once.
  ADB_PROD_ASSERT(chosen.size() == graph.nodes.size());
  ADB_PROD_ASSERT(
      std::unordered_set<EnumerateCollectionNode*>(chosen.begin(), chosen.end())
          .size() == chosen.size());

  // Defensive: anyComponentReordered or sequenceChanged being true means
  // `chosen` differs from its respective baseline, so it should differ from
  // currentOrder too -- but guard the invariant explicitly rather than
  // relying on that argument holding for every future change above.
  if (chosen == currentOrder) {
    return std::nullopt;
  }

  return chosen;
}

// -----------------------------------------------------------------------------
// plan rewriting
// -----------------------------------------------------------------------------

void rewriteJoinGraph(ExecutionPlan& plan, ExecutionNode* firstEnumeration,
                      ExecutionNode* next,
                      std::vector<EnumerateCollectionNode*> const& order) {
  // Capture the anchor before touching anything: after the unlink loop the
  // spine no longer contains the enumerations.
  ExecutionNode* firstDependency = firstEnumeration->getFirstDependency();
  ADB_PROD_ASSERT(firstDependency != nullptr);

  // A permutation check, not merely a size check: the loop below unlinks every
  // enumeration and reinserts only what `order` holds, so a duplicate paired
  // with an omission would silently delete a FOR loop from the query -- which
  // no assertion on the resulting *order* would catch.
  auto const current = collectEnumerationOrder(firstEnumeration, next);
  {
    auto sortedCurrent = current;
    auto sortedOrder = order;
    auto byId = [](auto const* l, auto const* r) { return l->id() < r->id(); };
    std::sort(sortedCurrent.begin(), sortedCurrent.end(), byId);
    std::sort(sortedOrder.begin(), sortedOrder.end(), byId);
    ADB_PROD_ASSERT(sortedCurrent == sortedOrder);
  }

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

// -----------------------------------------------------------------------------
// the optimizer rule
// -----------------------------------------------------------------------------

void optimizeJoinOrder(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                       OptimizerRule const& rule) {
  auto estimator = makeDefaultJoinCostEstimator(*plan);
  bool modified = false;

  ExecutionNode* n = plan->root()->getSingleton();
  while (n != nullptr) {
    if (n->getType() != ExecutionNode::ENUMERATE_COLLECTION) {
      n = n->getFirstParent();
      continue;
    }

    ExecutionNode* firstEnumeration = n;
    ExecutionNode* next = nullptr;
    JoinGraph graph = buildJoinGraph(plan.get(), firstEnumeration, next);

    if (graph.hasJoin() && !graph.hasNonDeterministicCalculation) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      traceJoinGraph(graph);
#endif
      auto const current = collectEnumerationOrder(firstEnumeration, next);
      if (auto chosen = chooseJoinOrder(graph, *estimator, current);
          chosen.has_value()) {
        rewriteJoinGraph(*plan, firstEnumeration, next, *chosen);
        modified = true;
      }
    }

    n = next;
  }

  if (modified) {
    // Cost-based reordering ran and actually rewrote a join here, so running
    // interchange-adjacent-enumerations afterwards would fan out n! plans
    // that just re-permute what this rule already chose, leaving the generic
    // cost estimate to pick among near-duplicates -- the exact mechanism
    // cost-based reordering exists to replace. Suppression is conditional on
    // `modified`, not on this rule merely being enabled: when it declines
    // (e.g. no usable statistics), interchange must still run, so opting
    // into cost-based join ordering can never leave a query worse optimized
    // than the default.
    opt->disableRules(plan.get(), [](OptimizerRule const& r) {
      return r.level == OptimizerRule::interchangeAdjacentEnumerationsRule;
    });
  }

  // Report the rule applied only when the order actually changed, so an
  // unchanged plan does not needlessly re-trigger the downstream rules.
  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
