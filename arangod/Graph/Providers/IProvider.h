
#pragma once

#include <vector>

// Collect the functions called on providers by enumerators (pathresult,
// validator?)

namespace arangodb::graph {

// An IGraphView provides access to the topology of a graph stored in the
// database.
//
// It supports filtering on vertices and edges
//
// Primary access to the topology is by requesting NeighbourCursors for given
// vertices.
struct IGraphView {
  // Hausmeisterschrott
  auto stealStats() -> aql::TraversalStats;

  // Creates a cursor that returns out edges
  auto createNeighbourCursor(VertexRef v, uint64_t depth) -> NeighbourCursor;

  auto prepareIndexExpressions(aql::Ast* ast) -> void;

  auto prepareContext(aql::InputAqlItemRow input) -> void;
  auto unPrepareContext() -> void;

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
}  // namespace arangodb::graph
