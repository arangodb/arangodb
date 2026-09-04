#pragma once

#include <velocypack/HashedStringRef.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include "Graph/Types/VertexRef.h"
#include "Graph/SimplifiedTraversal/IGraphView.h"

namespace arangodb::graph::experimental {

struct InMemoryGraph : IGraphView {
  InMemoryGraph() = default;
  InMemoryGraph(std::unordered_set<VertexRef> vertices,
                std::vector<std::tuple<VertexRef, VertexRef>> edges)
      : _vertices{std::move(vertices)} {
    for (auto const& [from, to] : edges) {
      _edges.insert({EdgeId{_next_edge_id++}, Edge{from, to}});
    }
  }

  auto outEdges(VertexRef vertex) -> std::vector<Edge> override {
    std::vector<Edge> result;
    for (auto const& [id, edge] : _edges) {
      if (edge.from == vertex) {
        result.emplace_back(edge);
      }
    }
    return result;
  }

  auto vertex(VertexRef vertex) -> std::optional<VertexRef> override {
    if (auto v = _vertices.find(vertex); v != _vertices.end()) {
      return *v;
    }
    return std::nullopt;
  }

  std::unordered_set<VertexRef> _vertices;
  std::unordered_map<EdgeId, Edge> _edges;

 private:
  size_t _next_edge_id = 0;
  // indexes
  // std::unordered_map<VertexId, std::vector<EdgeId>> _from_edges;
  // std::unordered_map<VertexId, EdgeId> _to_edges;
};

}  // namespace arangodb::graph::experimental
// namespace arangodb::graph
