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

#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/Variable.h"
#include "Assertions/ProdAssert.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace arangodb::aql {

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
  residuals.emplace_back(node);
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

}  // namespace arangodb::aql
