#include "WCCGraph.h"
#include <Inspection/VPackPure.h>

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

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
WCCGraph<EdgeProperties, VertexProperties>::WCCGraph(
    const SharedSlice& graphJson, bool checkDuplicateVertices) : _wccs() {
  this->readVertices(graphJson, checkDuplicateVertices);
  for (EdgeIndex idx = 0; idx < this->vertices.size(); ++idx) {
    _wccs.addSingleton(idx);
  }

  this->readEdges(graphJson, true,   [&](GraphEdge edge) {
    _wccs.merge(this->getVertexPosition(edge._from),
                this->getVertexPosition(edge._to));
  });
  this->clearVertexPositions();
}

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
auto WCCGraph<EdgeProperties, VertexProperties>::computeWCC() -> size_t {
  std::unordered_set<size_t> markedRepresentatives;
  size_t counter = 0;
  for (size_t i = 0; i < this->vertices.size(); ++i) {
    size_t const representative = _wccs.representative(i);
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

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
auto WCCGraph<EdgeProperties, VertexProperties>::printWccs() -> void {
  _wccs.print();
}

template class WCCGraph<EmptyEdgeProperties, WCCVertexProperties>;