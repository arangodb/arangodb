#pragma once

#include <velocypack/HashedStringRef.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include "Graph/Types/VertexRef.h"

// Q: use VertexRef or using VertexId = std::string (as defined in
// SingleServerPathResult.h)

namespace arangodb::graph::experimental {

// Q: what to use here?
struct EdgeId {
  size_t id;
  bool operator==(EdgeId const&) const = default;
};

struct Edge {
  VertexRef from;
  VertexRef to;
};

}  // namespace arangodb::graph::experimental

template<>
struct std::hash<arangodb::graph::experimental::EdgeId> {
  std::size_t operator()(
      arangodb::graph::experimental::EdgeId const& value) const noexcept {
    return std::hash<size_t>()(value.id);
  }
};

namespace arangodb::graph::experimental {

struct InMemoryGraph {
  InMemoryGraph(std::unordered_set<VertexRef> vertices,
                std::vector<std::tuple<VertexRef, VertexRef>> edges)
      : _vertices{std::move(vertices)} {
    for (auto const& [from, to] : edges) {
      _edges.insert({EdgeId{_next_edge_id++}, Edge{from, to}});
    }
  }

  std::unordered_set<VertexRef> _vertices;
  std::unordered_map<EdgeId, Edge> _edges;

 private:
  size_t _next_edge_id = 0;
  // indexes
  // std::unordered_map<VertexId, EdgeId> _from_edges;
  // std::unordered_map<VertexId, EdgeId> _to_edges;
};

}  // namespace arangodb::graph::experimental
// namespace arangodb::graph
