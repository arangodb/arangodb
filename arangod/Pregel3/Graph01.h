#pragma once

#include <vector>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include "Basics/debugging.h"
#include "velocypack/Builder.h"

namespace arangodb::pregel3 {

template<class VertexProperties>
struct Vertex {
  std::vector<size_t> outEdges;
  std::vector<size_t> inEdges;
  VertexProperties props;

  void toVelocyPack(VPackBuilder& builder, std::string_view id, size_t idx);

  size_t degree() { return outEdges.size() + inEdges.size(); }

  size_t inDegree() { return inEdges.size(); }

  size_t outDegree() { return outEdges.size(); }
};

struct EmptyVertexProperties {
  virtual ~EmptyVertexProperties() = default;
  static void toVelocyPack(VPackBuilder& builder, std::string_view id,
                           size_t idx);
};
using VertexWithEmptyProps = Vertex<EmptyVertexProperties>;

struct MinCutVertex : public Vertex<EmptyVertexProperties> {
  size_t label;
  double excess;
  bool isLeaf = false;

  void toVelocyPack(VPackBuilder& builder, std::string_view id, size_t idx);
};

template<class EdgeProperties>
struct Edge {
  size_t from;
  size_t to;
  EdgeProperties props;

  Edge(size_t from, size_t to) : from(from), to(to) {}

  void toVelocyPack(VPackBuilder& builder);
};

template<class EdgeProperties>
struct MultiEdge {
  size_t from;
  size_t to;
  std::vector<size_t> edgeIdxs;

  void toVelocyPack(VPackBuilder& builder);
};

struct EmptyEdgeProperties {
  virtual ~EmptyEdgeProperties() = default;
  static void toVelocyPack(VPackBuilder& builder);
};
using EdgeWithEmptyProps = Edge<EmptyEdgeProperties>;

struct MinCutEdge : public Edge<EmptyEdgeProperties> {
  double const capacity;
  double flow = 0.0;
  std::optional<size_t> edgeRev;

  MinCutEdge(size_t from, size_t to, double capacity)
      : Edge(from, to), capacity(capacity){};
  MinCutEdge(size_t from, size_t to, double capacity, size_t edgeRev)
      : Edge(from, to), capacity(capacity), edgeRev(edgeRev){};

  void toVelocyPack(VPackBuilder& builder);

  double residual() const {
    TRI_ASSERT(capacity >= flow);
    return capacity - flow;
  }

  void decreaseFlow(double val) {
    TRI_ASSERT(val <= flow);
    flow -= val;
  }

  void increaseFlow(double val) {
    TRI_ASSERT(val <= residual());
    flow += val;
  }
};

struct BaseGraph {
  virtual ~BaseGraph() = default;
  virtual void toVelocyPack(VPackBuilder&) = 0;
};

template<class V, class E>
struct Graph : BaseGraph {
  using Vertex = V;
  using Edge = E;

  std::vector<Vertex> vertices;
  std::vector<Edge> edges;
  std::vector<std::string>
      vertexIds;  // not in Vertex: needed only at the end of a computation;
                  // vertices know each other by their indexes in the vector
                  // vertices

  ~Graph() override = default;
  auto numVertices() -> size_t { return vertices.size(); }
  auto numEdges() -> size_t { return edges.size(); }
  void toVelocyPack(VPackBuilder& builder) override;
};

template<class V, class E>
void Graph<V, E>::toVelocyPack(VPackBuilder& builder) {
  VPackObjectBuilder ob(&builder);
  builder.add(VPackValue("vertices"));
  {
    VPackArrayBuilder ab(&builder);
    for (size_t i = 0; i < vertices.size(); ++i) {
      auto& v = vertices.at(i);
      v.toVelocyPack(builder, vertexIds.at(i), i);
    }
  }
  {
    builder.add(VPackValue("edges"));
    VPackArrayBuilder ab(&builder);
    for (auto& e : edges) {
      e.toVelocyPack(builder);
    }
  }
  // todo add graph properties: number of vertices/edges
}

using EmptyPropertiesGraph = Graph<VertexWithEmptyProps, EdgeWithEmptyProps>;

class MinCutGraph : public Graph<MinCutVertex, MinCutEdge> {
  size_t source;
  size_t target;

  void markLeaves() {
    for (auto v : vertices) {
      if (v.outDegree() == 0) {
        v.isLeaf = true;
      }
    }
    vertices.at(target).isLeaf = false;
  }

  void pushCapacitiesOnLastEdge() {
    for (Edge edge : edges) {
    }
  }

  std::unordered_set<size_t> applicableEdges;
  std::unordered_set<size_t> relabableVertices;
};

}  // namespace arangodb::pregel3