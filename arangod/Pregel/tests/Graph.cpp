#include <unordered_set>
#include <istream>

#include "Graph.h"
#include "WCCGraph.h"
#include "velocypack/Parser.h"

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
auto Graph<EdgeProperties, VertexProperties>::checkSliceAddEdge(
    Slice slice, bool checkEdgeEnds, std::function<void(Edge edge)>& onReadEdge)
    -> void {
  Edge edge;
  auto result = deserializeWithStatus<Edge>(slice, edge);
  if (not result.ok()) {
    throw std::runtime_error("Could not read edge: " + result.error());
  }

  if (checkEdgeEnds) {
    _checkEdgeEnds(edge, _vertexPosition);
  }
  onReadEdge(edge);
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
    auto const slice = es[i].slice();
    checkSliceAddEdge(slice, checkEdgeEnds, onReadEdge);
  }
  return es.length();
}

template<typename EdgeProperties, typename VertexProperties>
size_t Graph<EdgeProperties, VertexProperties>::readEdges(
    std::istream& file, bool checkEdgeEnds,
    std::function<void(Edge edge)> onReadEdge) {
  std::string line;
  size_t count = 0;
  while (std::getline(file, line)) {
    if (!line.empty() && line[0] != '#') {
      ++count;
      std::shared_ptr<Builder> v;
      try {
        v = arangodb::velocypack::Parser::fromJson(line);
      } catch (...) {
        throw std::runtime_error("Could not parse " + line);
      }
      auto slice = v->slice();
      checkSliceAddEdge(slice, checkEdgeEnds, onReadEdge);
    }
  }
  return count;
}

template<typename EdgeProperties, typename VertexProperties>
auto Graph<EdgeProperties, VertexProperties>::checkSliceAddVertex(
    Slice vertexSlice, size_t count, bool checkDuplicateVertices) -> void {
  Vertex vertex;
  auto result = deserializeWithStatus<Vertex>(vertexSlice, vertex);
  if (not result.ok()) {
    throw std::runtime_error("Could not read vertex: " + result.error());
  }
  if (checkDuplicateVertices) {
    if (_vertexPosition.contains(vertex._key)) {
      throw std::runtime_error("Vertex with _key " + vertex._key +
                               " appears "
                               "more than once.");
    }
  }

  _vertexPosition[vertex._key] = count;
  vertices.push_back(vertex);
}

template<typename EdgeProperties, typename VertexProperties>
auto Graph<EdgeProperties, VertexProperties>::readVertices(
    const SharedSlice& graphJson, bool checkDuplicateVertices) -> void {
  auto vs = graphJson["vertices"];
  for (VertexIndex i = 0; i < vs.length(); ++i) {
    Vertex vertex;
    auto const slice = vs[i].slice();
    checkSliceAddVertex(slice, i, checkDuplicateVertices);
  }
}

template<typename EdgeProperties, typename VertexProperties>
auto Graph<EdgeProperties, VertexProperties>::readVertices(
    std::istream& file, bool checkDuplicateVertices) -> void {
  std::string line;
  size_t count = 0;
  while (std::getline(file, line)) {
    if (!line.empty() && line[0] != '#') {
      ++count;
      Vertex vertex;
      std::shared_ptr<Builder> v;
      try {
        v = arangodb::velocypack::Parser::fromJson(line);
      } catch (...) {
        throw std::runtime_error("Could not parse " + line);
      }
      auto slice = v->slice();
      checkSliceAddVertex(slice, count, checkDuplicateVertices);
    }
  }
}

template class Graph<EmptyEdgeProperties, EmptyVertexProperties>;
template class Graph<EmptyEdgeProperties, WCCVertexProperties>;
template class Graph<EmptyEdgeProperties, SCCVertexProperties>;