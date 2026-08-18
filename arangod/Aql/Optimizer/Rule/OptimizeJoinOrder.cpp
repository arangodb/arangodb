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
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <optional>
#include <tuple>
#include <utility>

namespace arangodb::aql {

// -----------------------------------------------------------------------------
// attribute-access extraction
// -----------------------------------------------------------------------------

namespace {

auto extractAttributeAccess(AstNode const* n, AttributePath& path)
    -> Variable const* {
  if (n->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    auto var = extractAttributeAccess(n->getMemberUnchecked(0), path);
    if (var != nullptr) {
      auto attr = n->getStringView();
      if (path.empty() && attr == "_id") {
        attr = "_key";
      }
      path.emplace_back(attr);
    }
    return var;
  } else if (n->type == NODE_TYPE_REFERENCE) {
    return static_cast<Variable const*>(n->getData());
  }
  return nullptr;
}

auto extractAttributeAccess(AstNode const* n)
    -> std::optional<std::pair<Variable const*, AttributePath>> {
  AttributePath path;
  auto var = extractAttributeAccess(n, path);
  if (var != nullptr) {
    return std::make_pair(var, std::move(path));
  }
  return std::nullopt;
}

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
        // calculations feeding filters are inspected via the FILTER case; the
        // calculation node itself is part of the run but carries no join info.
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
// the optimizer rule
// -----------------------------------------------------------------------------

void optimizeJoinOrder(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                       OptimizerRule const& rule) {
  // Scaffolding: construct the join graph(s) for the plan. This does
  // NOT reorder joins and does NOT modify the plan yet; the reordering
  // algorithm is a follow-up task. We report the rule as "applied" only when we
  // actually discovered a join so that it is observable via `explain`.
  bool foundJoin = false;

  ExecutionNode* n = plan->root()->getSingleton();
  while (n != nullptr) {
    if (n->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
      ExecutionNode* next = nullptr;

      // This graph is discarded now, but will be retained and used for join
      // reordering later.
      JoinGraph graph = buildJoinGraph(plan.get(), n, next);
      if (graph.hasJoin()) {
        foundJoin = true;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        traceJoinGraph(graph);
#endif
      }
      // continue scanning after the run that we just consumed
      n = next;
    } else {
      n = n->getFirstParent();
    }
  }

  opt->addPlan(std::move(plan), rule, foundJoin);
}

}  // namespace arangodb::aql
