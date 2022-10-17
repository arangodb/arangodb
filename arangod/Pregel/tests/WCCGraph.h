#pragma once

#include <string>
#include <unordered_set>
#include <map>

#include <Basics/VelocyPackStringLiteral.h>
#include <Inspection/VPackPure.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include "DisjointSet.h"

using namespace arangodb::velocypack;

struct WCCVertexProperties {
  uint64_t value;
};

struct EmptyEdgeProperties {};

using VertexKey = std::string;
using EdgeKey = std::string;

template<typename VertexProperties>
struct Vertex {
  VertexKey _key;
  VertexProperties properties;

  auto operator==(Vertex const& other) { return _key == other._key; }
};

using WCCVertex = Vertex<WCCVertexProperties>;

template<typename EdgeProperties = EmptyEdgeProperties>
struct Edge {
  EdgeKey _key;
  VertexKey _from;
  VertexKey _to;
  EdgeProperties properties;
};

template<typename EdgeProperties>
class WCCGraph {
 public:
  WCCGraph() = default;
  WCCGraph(SharedSlice graphJson, bool checkDuplicateVertices);
  /**
   * Compute the weakly connected components. Each component has an id of type
   * size_t and is written into the value property of each of its vertices.
   * @return the number of weakly connected components.
   */
  auto computeWCC() -> size_t;

  std::vector<WCCVertex> vertices;

  auto printWccs() -> void;

 private:
  std::map<VertexKey, size_t> _vertexPosition;
  DisjointSet _wccs;

  auto _checkEdgeEnds(Edge<EdgeProperties>,
                      std::map<VertexKey, size_t> const& vertexPosition)
      -> void;
};

template<typename Inspector>
auto inspect(Inspector& f, WCCVertexProperties& x) {
  return f.object(x).fields(f.field("value", x.value));
}

template<typename Inspector, typename VertexProperties>
auto inspect(Inspector& f, Vertex<VertexProperties>& p) {
  return f.object(p).fields(f.field("_key", p._key),
                            f.embedFields(p.properties));
}

template<typename Inspector>
auto inspect(Inspector& f, EmptyEdgeProperties& x) {
  return f.object(x).fields();
}

template<typename Inspector, typename EdgeProperties = EmptyEdgeProperties>
auto inspect(Inspector& f, Edge<EdgeProperties>& p) {
  return f.object(p).fields(f.field("_key", p._key), f.field("_from", p._from),
                            f.field("_to", p._to), f.embedFields(p.properties));
}
