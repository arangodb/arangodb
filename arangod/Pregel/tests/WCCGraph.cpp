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

template<typename EdgeProperties, VertexPropertiesWithValue VertexProperties>
WCCGraph<EdgeProperties, VertexProperties>::WCCGraph(
    const SharedSlice& graphJson, bool checkDuplicateVertices) : wccs() {
  this->readVertices(graphJson, checkDuplicateVertices);
  for (EdgeIndex idx = 0; idx < this->vertices.size(); ++idx) {
    wccs.addSingleton(idx);
  }

  this->readEdges(graphJson, true,   [&](WCCEdge edge) {
    wccs.merge(this->getVertexPosition(edge._from),
                this->getVertexPosition(edge._to));
  });
  this->clearVertexPositions();
}

template class WCCGraph<EmptyEdgeProperties, WCCVertexProperties>;