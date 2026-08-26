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

#include "JoinGraph.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Assertions/ProdAssert.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

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

// -----------------------------------------------------------------------------
// JoinGraph
// -----------------------------------------------------------------------------

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
    // Appending may reallocate `edges`, invalidating every Edge* the
    // adjacency index holds. Drop it; the next reader rebuilds it.
    _adjacency.clear();
    _adjacencyBuilt = false;
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

void JoinGraph::buildAdjacency() {
  _adjacency.clear();
  _adjacency.reserve(nodes.size());
  for (auto& [variable, node] : nodes) {
    _adjacency.try_emplace(&node);
  }
  for (auto& e : edges) {
    _adjacency[e.from].emplace_back(&e);
    if (e.to != e.from) {
      _adjacency[e.to].emplace_back(&e);
    }
  }
  _adjacencyBuilt = true;
}

auto JoinGraph::getEdgesForNode(Node* node) -> std::vector<Edge*> const& {
  if (!_adjacencyBuilt) {
    buildAdjacency();
  }
  auto const it = _adjacency.find(node);
  ADB_PROD_ASSERT(it != _adjacency.end());
  return it->second;
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

}  // namespace arangodb::aql
