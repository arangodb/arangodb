#include "WCCGraph.h"
#include <Inspection/VPackPure.h>


template<typename EdgeProperties>
void WCCGraph<EdgeProperties>::_checkEdgeEnds(
    Edge<EdgeProperties> edge,
    std::map<VertexKey, size_t> const& vertexPosition) {
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

template<typename EdgeProperties>
WCCGraph<EdgeProperties>::WCCGraph(SharedSlice graphJson,
                                   bool checkDuplicateVertices) {
  auto vs = graphJson["vertices"];
  for (size_t i = 0; i < vs.length(); ++i) {
    WCCVertex vertex;
    auto result = deserializeWithStatus<WCCVertex>(vs[i].slice(), vertex);
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
    size_t const vertexPosition = vertices.size();
    _vertexPosition[vertex._key] = vertexPosition;
    vertices.push_back(vertex);
    _wccs.addSingleton(vertexPosition);
  }

  auto es = graphJson["edges"];
  for (size_t i = 0; i < es.length(); ++i) {
    Edge<EmptyEdgeProperties> edge;
    auto result =
        deserializeWithStatus<Edge<EmptyEdgeProperties>>(es[i].slice(), edge);
    if (not result.ok()) {
      throw std::runtime_error("Could not read edge: " + result.error());
    }

    _checkEdgeEnds(edge, _vertexPosition);
    _wccs.merge(_vertexPosition[edge._from], _vertexPosition[edge._to]);
  }
}

template<typename EdgeProperties>
auto WCCGraph<EdgeProperties>::computeWCC() -> size_t {
  std::unordered_map<size_t, size_t> markedRepresentatives;
  size_t counter = 0;
  for (size_t i = 0; i < vertices.size(); ++i) {
    auto id = markedRepresentatives.find(_wccs.representative(i));
    if (id == markedRepresentatives.end()) {
      size_t const newId = counter++;
      vertices[i].properties.value = newId;
      vertices[_wccs.representative(i)].properties.value = newId;
    } else {
      vertices[i].properties.value = id->second;
    }
  }
  return counter;
}

template class WCCGraph<EmptyEdgeProperties>;