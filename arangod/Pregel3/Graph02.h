#pragma once

#include <vector>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include "Basics/debugging.h"
#include "velocypack/Builder.h"
#include "Containers/FlatHashSet.h"
#include "Vertex02.h"

namespace arangodb::pregel3 {

struct BaseGraph {
  virtual ~BaseGraph() = default;
  virtual void toVelocyPack(VPackBuilder&) const = 0;
};

template<class V, class E>
struct Graph : BaseGraph {
  using Vertex = V;
  using Edge = E;

  std::vector<Vertex> vertices;
  std::unordered_map<size_t, Edge> edges;  // allows to edit edges in-place
  std::vector<size_t> holes;               // unused edge indexes to be reused
  std::vector<std::string>
      vertexIds;  // not in Vertex: needed only at the end of a computation;
                  // vertices know each other by their indexes in the vector
                  // vertices

  ~Graph() override = default;
  auto numVertices() const -> size_t { return vertices.size(); }
  auto numEdges() const -> size_t { return edges.size(); }
  void toVelocyPack(VPackBuilder& builder) const override;

  void addVertex(V&& v) { vertices.template emplace_back(std::move(v)); }
  void addVertex() { vertices.emplace_back(); }
  std::pair<size_t, bool> addEdge(size_t from, size_t to);

  void removeEdge(E* e);

  V& vertex(size_t vIdx) {
    TRI_ASSERT(vIdx < numVertices());
    return vertices[vIdx];
  }

  V const& vertex(size_t vIdx) const {
    TRI_ASSERT(vIdx < numVertices());
    return vertices[vIdx];
  }

  E* edge(size_t eIdx) {
    auto it = edges.find(eIdx);
    if (it != edges.end()) {
      return &(it->second);
    }
    return nullptr;
  }

  E const* edge(size_t eIdx) const {
    auto it = edges.find(eIdx);
    if (it != edges.end()) {
      return &(it->second);
    }
    return nullptr;
  }

  E* edge(size_t from, size_t to) {
    TRI_ASSERT(from < numVertices());
    TRI_ASSERT(to < numVertices());
    auto it = vertices[from].outEdges.find(to);
    if (it != vertices[from].outEdges.end()) {
      return it->second;
    }
    return nullptr;
  }

  E const* edge(size_t from, size_t to) const {
    TRI_ASSERT(from < numVertices());
    TRI_ASSERT(to < numVertices());
    auto it = vertices[from].outEdges.find(to);
    if (it != vertices[from].outEdges.end()) {
      return it->second;
    }
    return nullptr;
  }

  E const* reverseEdge(E const& e) const { return edge(e.to, e.from); }

  E* reverseEdge(E const* e) { return edge(e->to, e->from); }
};

template<class V, class E>
void Graph<V, E>::toVelocyPack(VPackBuilder& builder) const {
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
      e.second.toVelocyPack(builder);
    }
  }
  // todo add graph properties: number of vertices/edges
}

using EmptyPropertiesGraph = Graph<VertexWithEmptyProps, EdgeWithEmptyProps>;

class MinCutGraph : public Graph<MinCutVertex, MinCutEdge> {
 public:
  size_t source;
  size_t target;

 private:
  std::unordered_set<size_t> applicableEdges;
  std::unordered_set<size_t> relabableVertices;
};

}  // namespace arangodb::pregel3