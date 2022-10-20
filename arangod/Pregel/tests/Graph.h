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

struct EmptyEdgeProperties {};

using VertexKey = std::string;
using EdgeKey = std::string;

template<typename VertexProperties>
struct Vertex {
  VertexKey _key;
  VertexProperties properties;

  auto operator==(Vertex const& other) { return _key == other._key; }
};

template<typename VertexProperties>
concept VertexPropertiesWithValue = requires(VertexProperties t) {
                                      {
                                        t.value
                                        } -> std::convertible_to<size_t>;
                                    };

template<typename EdgeProperties = EmptyEdgeProperties>
struct Edge {
  EdgeKey _key;
  VertexKey _from;
  VertexKey _to;
  EdgeProperties properties;
};

using VertexIndex = size_t;
using EdgeIndex = size_t;

template<typename EdgeProperties, typename VertexProperties>
class Graph {
 public:
  using GraphVertex = Vertex<VertexProperties>;
  using GraphEdge = Edge<EdgeProperties>;

  Graph() = default;

  auto readVertices(const SharedSlice& graphJson, bool checkDuplicateVertices)
      -> void;

  size_t readEdges(const SharedSlice& graphJson, bool checkEdgeEnds,
                   std::function<void(GraphEdge edge)> onReadEdge);

  auto getVertexPosition(VertexKey const& key) -> VertexIndex {
    return _vertexPosition[key];
  }
  auto clearVertexPositions() -> void { _vertexPosition.clear(); }

  std::vector<GraphVertex> vertices;

 private:
  std::map<VertexKey, VertexIndex> _vertexPosition;

 protected:
  auto _checkEdgeEnds(GraphEdge,
                      std::map<VertexKey, size_t> const& vertexPosition)
      -> void;
};

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
