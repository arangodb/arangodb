#pragma once

#include <string>
#include <unordered_set>
#include <map>

#include <Basics/VelocyPackStringLiteral.h>
#include <Inspection/VPackPure.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include "Graph.h"
#include "DisjointSet.h"

using namespace arangodb::velocypack;


using VertexIndex = size_t;
using EdgeIndex = size_t;


struct WCCVertexProperties {
  uint64_t value;
};

template<typename Inspector>
auto inspect(Inspector& f, WCCVertexProperties& x) {
  return f.object(x).fields(f.field("value", x.value));
}

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
class WCCGraph : public Graph<EdgeProperties, VertexProperties> {
 public:
  using GraphVertex = Vertex<VertexProperties>;
  using GraphEdge = Edge<EdgeProperties>;

  WCCGraph(SharedSlice const& graphJson, bool checkDuplicateVertices);
  /**
   * Compute the weakly connected components. Each component has an id of type
   * size_t and is written into the value property of each of its vertices.
   * @return the number of weakly connected components.
   */
  auto computeWCC() -> size_t;

  auto printWccs() -> void;

 private:
  DisjointSet _wccs;
};

