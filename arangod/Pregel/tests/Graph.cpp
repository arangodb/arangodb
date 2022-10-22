#include <unordered_set>
#include "Graph.h"
#include "SCCGraph.h"
#include "WCCGraph.h"

template<typename EdgeProperties, typename VertexProperties>
void Graph<EdgeProperties, VertexProperties>::_checkEdgeEnds(
    Edge edge, std::map<VertexKey, size_t> const& vertexPosition) {
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
    std::function<void(Edge edge)> onReadEdge) {
  auto es = graphJson["edges"];
  if (not es.isArray()) {
    throw std::runtime_error("The input graph slice " + es.toString() +
                             " has not field \"edges\"");
  }
  for (EdgeIndex i = 0; i < es.length(); ++i) {
    Edge edge;
    auto result = deserializeWithStatus<Edge>(es[i].slice(), edge);
    if (not result.ok()) {
      throw std::runtime_error("Could not read edge: " + result.error());
    }

    if (checkEdgeEnds) {
      _checkEdgeEnds(edge, _vertexPosition);
    }
    onReadEdge(edge);
  }
  return es.length();
}

template<typename EdgeProperties, typename VertexProperties>
auto Graph<EdgeProperties, VertexProperties>::readVertices(
    const SharedSlice& graphJson, bool checkDuplicateVertices) -> void {
  auto vs = graphJson["vertices"];
  for (VertexIndex i = 0; i < vs.length(); ++i) {
    Vertex vertex;
    auto result = deserializeWithStatus<Vertex>(vs[i].slice(), vertex);
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
template<typename EdgeProperties, typename VertexProperties>
auto Graph<EdgeProperties, VertexProperties>::
    writeEquivalenceRelationIntoVertices(DisjointSet& eqRel) -> size_t {
  // todo: if a lot of (small) connected components, it may be better to use a
  //  bitset
  std::unordered_set<size_t> markedRepresentatives;
  size_t counter = 0;
  for (size_t i = 0; i < this->vertices.size(); ++i) {
    size_t const representative = eqRel.representative(i);
    if (markedRepresentatives.contains(representative)) {
      this->vertices[i].properties.value =
          this->vertices[representative].properties.value;
    } else {
      size_t const newId = counter++;
      this->vertices[i].properties.value = newId;
      this->vertices[representative].properties.value = newId;
      markedRepresentatives.insert(representative);
    }
  }
  return counter;
}

template class Graph<EmptyEdgeProperties, WCCVertexProperties>;
template class Graph<EmptyEdgeProperties, SCCVertexProperties>;