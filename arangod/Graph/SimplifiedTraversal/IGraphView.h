#pragma once

#include <vector>
#include "Graph/Types/VertexRef.h"

namespace arangodb::graph::experimental {

// Q: what to use here?
struct EdgeId {
  size_t id;
  bool operator==(EdgeId const&) const = default;
};

struct Edge {
  VertexRef from;
  VertexRef to;
  bool operator==(Edge const& other) const {
    return (from == other.from && to == other.to);
  }
};
template<typename Inspector>
auto inspect(Inspector& f, Edge& x) {
  return f.object(x).fields(f.field("from", x.from), f.field("to", x.to));
}

}  // namespace arangodb::graph::experimental

template<>
struct std::hash<arangodb::graph::experimental::EdgeId> {
  std::size_t operator()(
      arangodb::graph::experimental::EdgeId const& value) const noexcept {
    return std::hash<size_t>()(value.id);
  }
};

// Collect the functions called on providers by enumerators (pathresult,
// validator?)

namespace arangodb::graph::experimental {

// An IGraphView provides access to the topology of a graph stored in the
// database.
//
// It supports filtering on vertices and edges
//
// Primary access to the topology is by requesting NeighbourCursors for given
// vertices.
struct IGraphView {
  // should include which collections, which indexes with which
  // index-expressions

  virtual ~IGraphView() = default;

  // Q: use VertexRef or using VertexId = std::string (as defined in
  //    SingleServerPathResult.h)
  // Q: return Edge or Step?
  virtual auto outEdges(VertexRef vertex) -> std::vector<Edge> = 0;

  // TODO needs to return at some point Vertex data
  virtual auto vertex(VertexRef vertex) -> std::optional<VertexRef> = 0;

  // -----------------------------
  // // Hausmeisterschrott
  // auto stealStats() -> aql::TraversalStats;

  // // Creates a cursor that returns out edges
  // auto createNeighbourCursor(VertexRef v, uint64_t depth) -> NeighbourCursor;

  // auto prepareIndexExpressions(aql::Ast* ast) -> void;

  // auto prepareContext(aql::InputAqlItemRow input) -> void;
  // auto unPrepareContext() -> void;
  // -------------------------------

  // Instead of using addVertexToBuilder, addEdgeToBuilder we intent the
  // *Enumerator* to return a struct like this:
  //
  // struct PathTopo {
  // std::vector<VertexRef> vertices;
  // std::vector<EdgeRef> edges;
  // };
  //
  // which can then be used to retrieve a velocypack representation of the path
};
}  // namespace arangodb::graph::experimental
