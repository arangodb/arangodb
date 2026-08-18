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

auto estimateOrder(JoinGraph& graph, JoinCostEstimator const& estimator,
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

auto orderComponent(JoinGraph& graph,
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

  // Order each component internally, then decide the sequence of components.
  // Components join by cross product, so they are concatenated, never
  // interleaved: interleaving would put a cross product in the middle of a
  // component's chain and cost more than the estimate that chose it.
  std::vector<JoinOrder> componentOrders;
  for (auto const& component : graph.connectedComponents()) {
    componentOrders.emplace_back(orderComponent(graph, component, estimator));
  }

  std::vector<EnumerateCollectionNode*> chosen;
  while (!componentOrders.empty()) {
    size_t bestIndex = 0;
    std::optional<double> bestCost;
    for (size_t i = 0; i < componentOrders.size(); ++i) {
      auto candidate = chosen;
      candidate.insert(candidate.end(), componentOrders[i].order.begin(),
                       componentOrders[i].order.end());
      double const cost = estimateOrder(graph, estimator, candidate).cost;
      if (!bestCost.has_value() || cost < *bestCost) {
        bestCost = cost;
        bestIndex = i;
      }
    }
    auto& winner = componentOrders[bestIndex];
    chosen.insert(chosen.end(), winner.order.begin(), winner.order.end());
    componentOrders.erase(componentOrders.begin() +
                          static_cast<std::ptrdiff_t>(bestIndex));
  }

  // connectedComponents() partitions the vertex set, so the concatenation must
  // cover every vertex exactly once.
  ADB_PROD_ASSERT(chosen.size() == graph.nodes.size());
  ADB_PROD_ASSERT(
      std::unordered_set<EnumerateCollectionNode*>(chosen.begin(), chosen.end())
          .size() == chosen.size());

  if (chosen == currentOrder) {
    return std::nullopt;
  }

  auto const chosenEstimate = estimateOrder(graph, estimator, chosen);
  auto const currentEstimate = estimateOrder(graph, estimator, currentOrder);

  // If any statistic was a fallback, the comparison is between two guesses.
  // Measured on real data, rewriting in that situation produced a plan slower
  // than the one it replaced, so decline.
  if (chosenEstimate.defaulted || currentEstimate.defaulted) {
    LOG_TOPIC("a7f04", TRACE, Logger::AQL)
        << "optimize-join-order: declining to reorder, estimate rests on "
           "defaulted statistics";
    return std::nullopt;
  }

  // Require a real margin: differences below the estimator's own error are not
  // signal.
  if (chosenEstimate.cost >=
      currentEstimate.cost / (1.0 + kImprovementMargin)) {
    LOG_TOPIC("a7f05", TRACE, Logger::AQL)
        << "optimize-join-order: keeping the written order, improvement "
        << currentEstimate.cost << " -> " << chosenEstimate.cost
        << " does not clear the margin";
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

  // Report the rule applied only when the order actually changed, so an
  // unchanged plan does not needlessly re-trigger the downstream rules.
  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
