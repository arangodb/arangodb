//
// Created by roman on 25.02.22.
//

#include "Graph02.h"
#include "velocypack/Value.h"

using namespace arangodb;
using namespace arangodb::pregel3;
using namespace arangodb::velocypack;

// get a free edge index
// amortized O(1)
namespace {
size_t _getEdgeIdx(std::vector<size_t>& holes, size_t edgesSize) {
  while (!holes.empty() && holes.back() > edgesSize - 1) {
    holes.pop_back();
  }
  size_t idx;
  if (!holes.empty()) {
    idx = holes.back();
    holes.pop_back();
  } else {
    idx = edgesSize;
  }
  return idx;
}
}  // namespace

// template<class V, class E>
// std::pair<size_t, bool> Graph<V, E>::addEdge(size_t from, size_t to) {
//   TRI_ASSERT(from < numVertices());
//   TRI_ASSERT(to < numVertices());
//   size_t idx = _getEdgeIdx(holes, edges.size());
//
//   auto [it, success] =
//       edges.emplace(std::make_pair(idx, Edge(from, to, idx)));
//   if (success) {
//     auto& e = it->second;
//     vertices[from].outEdges.insert({to, e});
//     vertices[to].outEdges.insert({from, e});
//     return {e.idx, true};
//   }
//   return {0, false};
// }

template<class V, class E>
void Graph<V, E>::removeEdge(E& e) {
  size_t idx = e.idx;
  size_t const to = e.to;
  size_t const from = e.from;
  vertices[from].outEdges.erase(to);
  vertices[to].inEdges.erase(from);
  if (idx < edges.size() - 1) {
    holes.push_back(idx);
  }
  edges.erase(idx);
}

std::pair<size_t, bool> MinCutGraph::addEdge(size_t fromIdx, size_t toIdx,
                                             double capacity) {
  TRI_ASSERT(fromIdx < numVertices());
  TRI_ASSERT(toIdx < numVertices());
  size_t eIdx = _getEdgeIdx(holes, edges.size());

  auto const& fromOutEdges = vertex(fromIdx).outEdges;
  auto it = fromOutEdges.find(toIdx);
  bool notContained = it == fromOutEdges.end();
  if (notContained) {
    auto const& [e, _] = edges.emplace(
        std::make_pair(eIdx, Edge(fromIdx, toIdx, eIdx, capacity)));

    vertices[fromIdx].outEdges.insert({toIdx, e->second});
    vertices[toIdx].inEdges.insert({fromIdx, e->second});
    return {eIdx, true};
  }
  return {0, false};
}

std::pair<size_t, bool> EmptyPropertiesGraph::addEdge(size_t fromIdx,
                                                      size_t toIdx) {
  TRI_ASSERT(fromIdx < numVertices());
  TRI_ASSERT(toIdx < numVertices());
  size_t eIdx = _getEdgeIdx(holes, edges.size());

  auto const& fromOutEdges = vertex(fromIdx).outEdges;
  auto it = fromOutEdges.find(toIdx);
  bool success = it not_eq fromOutEdges.end();
  if (success) {
    auto const& [e, _] =
        edges.emplace(std::make_pair(eIdx, Edge(fromIdx, toIdx, eIdx)));

    vertices[fromIdx].outEdges.insert({toIdx, e->second});
    vertices[toIdx].outEdges.insert({fromIdx, e->second});
    return {eIdx, true};
  }
  return {0, false};
}

template struct arangodb::pregel3::Graph<MinCutVertex, MinCutEdge>;