#include "Graph.h"

template<typename EdgeProperties, typename VertexProperties>
void Graph<EdgeProperties, VertexProperties>::_checkEdgeEnds(
    GraphEdge edge, std::map<VertexKey, size_t> const& vertexPosition) {
  if (not vertexPosition.contains(edge._from)) {
    throw std::runtime_error("Edge " + edge._key + " has a _from vertex " +
                             edge._from +
                             " that is not declared in the list of vertices.");
  }

  if (not vertexPosition.contains(edge._to)) {
    throw std::runtime_error("Edge " + edge._key + " has a _to vertex " +
                             edge._to +
                             " that is not declared in the list of vertices.");
  }
}

template<typename EdgeProperties, typename VertexProperties>
size_t Graph<EdgeProperties, VertexProperties>::readEdges(
    const SharedSlice& graphJson, bool checkEdgeEnds,
    std::function<void(GraphEdge edge)> onReadEdge) {
  auto es = graphJson["edges"];
  if (not es.isArray()) {
    throw std::runtime_error("The input graph slice " + es.toString() +
                             " has not field \"edges\"");
  }
  for (EdgeIndex i = 0; i < es.length(); ++i) {
    GraphEdge edge;
    auto result = deserializeWithStatus<GraphEdge>(es[i].slice(), edge);
    if (not result.ok()) {
      throw std::runtime_error("Could not read edge: " + result.error());
    }

    if (checkEdgeEnds) {
      _checkEdgeEnds(edge, _vertexPosition);
    }
    onReadEdge(edge);
  }
}

template<typename EdgeProperties, typename VertexProperties>
auto Graph<EdgeProperties, VertexProperties>::readVertices(
    const SharedSlice& graphJson, bool checkDuplicateVertices) -> void {
  auto vs = graphJson["vertices"];
  for (VertexIndex i = 0; i < vs.length(); ++i) {
    GraphVertex vertex;
    auto result = deserializeWithStatus<GraphVertex>(vs[i].slice(), vertex);
    if (not result.ok()) {
      throw std::runtime_error("Could not red vertex: " + result.error());
    }
    if (checkDuplicateVertices) {
      if (_vertexPosition.contains(vertex._key)) {
        throw std::runtime_error("Vertex with _key " + vertex._key +
                                 " appears "
                                 "more than once.");
      }
    }

    _vertexPosition[vertex._key] = i;
    vertices.push_back(vertex);
  }
}

#include "WCCGraph.h"

template class Graph<EmptyEdgeProperties, WCCVertexProperties>;