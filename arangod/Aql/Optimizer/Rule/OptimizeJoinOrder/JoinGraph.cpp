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

#include "JoinGraph.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Variable.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Assertions/ProdAssert.h"

#include <algorithm>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace arangodb::aql {

auto JoinGraph::nodeForVariable(Variable const* variable) -> Node* {
  auto iter = nodes.find(variable);
  if (iter == nodes.end()) {
    return nullptr;
  }
  return &iter->second;
}

void JoinGraph::addNode(EnumerateCollectionNode* en) {
  nodes.emplace(en->outVariable(), en);
}

auto JoinGraph::ensureEdge(Variable const* v, Variable const* w) -> Edge& {
  // find vertices for variables
  auto* from = nodeForVariable(v);
  auto* to = nodeForVariable(w);
  ADB_PROD_ASSERT(from != nullptr && to != nullptr);

  // now find the edge (undirected: match either orientation)
  auto iter = std::find_if(edges.begin(), edges.end(), [&](auto const& e) {
    return (e.from == from && e.to == to) || (e.from == to && e.to == from);
  });
  if (iter == edges.end()) {
    edges.emplace_back(from, to);
    return edges.back();
  }
  return *iter;
}

auto JoinGraph::addJoinCondition(Variable const* v, AttributePath vAttributes,
                                 Variable const* w, AttributePath wAttributes)
    -> Edge& {
  auto& edge = ensureEdge(v, w);
  if (edge.from->executionNode->outVariable() == v) {
    edge.fromAttributes.emplace_back(std::move(vAttributes));
    edge.toAttributes.emplace_back(std::move(wAttributes));
  } else {
    edge.toAttributes.emplace_back(std::move(vAttributes));
    edge.fromAttributes.emplace_back(std::move(wAttributes));
  }
  return edge;
}

void JoinGraph::addResidual(AstNode const* node) {
  // A residual that constrains exactly one graph variable is a genuine
  // restriction on that node's row count, so attach it there. Anything that
  // touches none or several is kept graph-level and left unmodelled.
  VarSet referenced;
  Ast::getReferencedVariables(node, referenced);

  Node* single = nullptr;
  for (auto const* var : referenced) {
    if (auto* candidate = nodeForVariable(var); candidate != nullptr) {
      if (single != nullptr && single != candidate) {
        single = nullptr;  // more than one graph variable
        break;
      }
      single = candidate;
    }
  }

  if (single != nullptr) {
    single->residuals.emplace_back(node);
  } else {
    residuals.emplace_back(node);
  }
}

auto JoinGraph::getEdgesForNode(Node* node) -> std::vector<Edge*> {
  std::vector<Edge*> result;
  for (auto& e : edges) {
    if (e.from == node || e.to == node) {
      result.emplace_back(&e);
    }
  }
  return result;
}

auto JoinGraph::connectedComponents() const
    -> std::vector<std::vector<Variable const*>> {
  // build undirected adjacency keyed by the (stable) Node addresses in `nodes`.
  std::unordered_map<Node const*, std::vector<Node const*>> adjacency;
  for (auto const& [var, node] : nodes) {
    adjacency.try_emplace(&node);
  }
  for (auto const& e : edges) {
    adjacency[e.from].emplace_back(e.to);
    adjacency[e.to].emplace_back(e.from);
  }

  std::vector<std::vector<Variable const*>> components;
  std::unordered_set<Node const*> visited;
  for (auto const& [var, node] : nodes) {
    Node const* start = &node;
    if (visited.contains(start)) {
      continue;
    }
    // BFS/DFS over this component
    std::vector<Variable const*> component;
    std::vector<Node const*> stack{start};
    visited.insert(start);
    while (!stack.empty()) {
      Node const* current = stack.back();
      stack.pop_back();
      component.emplace_back(current->executionNode->outVariable());
      for (Node const* neighbour : adjacency[current]) {
        if (visited.insert(neighbour).second) {
          stack.emplace_back(neighbour);
        }
      }
    }
    components.emplace_back(std::move(component));
  }
  return components;
}

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

}  // namespace

auto extractAttributeAccess(AstNode const* n)
    -> std::optional<std::pair<Variable const*, AttributePath>> {
  AttributePath path;
  auto var = extractAttributeAccess(n, path);
  if (var != nullptr) {
    return std::make_pair(var, std::move(path));
  }
  return std::nullopt;
}

namespace {

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

}  // namespace

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

}  // namespace arangodb::aql
