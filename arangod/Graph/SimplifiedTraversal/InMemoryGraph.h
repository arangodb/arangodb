#pragma once

#include <velocypack/HashedStringRef.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include "Graph/Types/VertexRef.h"
#include "Graph/SimplifiedTraversal/IGraphView.h"
#include "immer/flex_vector_transient.hpp"

namespace arangodb::graph::experimental {

struct InMemoryGraph {
  InMemoryGraph() = default;
  InMemoryGraph(std::vector<VertexIdLabel> vertexIdLabels,
                std::vector<std::tuple<std::string, std::string>> edges)
      : _vertexIdLabels{std::move(vertexIdLabels)} {
    for (auto const& edge : edges) {
      auto it = std::find_if(_vertexIdLabels.begin(), _vertexIdLabels.end(),
                             [&](VertexIdLabel const& label) {
                               return label.label == std::get<0>(edge);
                             });
      if (it == _vertexIdLabels.end()) {
        // error
      }
      size_t fromId = std::distance(_vertexIdLabels.begin(), it);
      it = std::find_if(_vertexIdLabels.begin(), _vertexIdLabels.end(),
                        [&](VertexIdLabel const& label) {
                          return label.label == std::get<1>(edge);
                        });
      if (it == _vertexIdLabels.end()) {
        // error
      }
      size_t toId = std::distance(_vertexIdLabels.begin(), it);

      _edges.insert(
          {EdgeId{_next_edge_id++}, Edge{VertexId{fromId}, VertexId{toId}}});
    }
  }
  auto includesEdge(VertexIdLabel const& from, VertexIdLabel const& to)
      -> bool {}

  auto includesVertex(VertexIdLabel const& vertex) -> bool {
    auto it = std::find(_vertexIdLabels.begin(), _vertexIdLabels.end(), vertex);
    if (it == _vertexIdLabels.end()) {
      return false;
    }
    return true;
  }
  std::vector<VertexIdLabel> _vertexIdLabels;
  // from        // to
  std::vector<std::vector<VertexIdLabel>> _adjacentVertices;

 private:
  size_t _next_edge_id = 0;
};

/**
   Gives a view of the graph

   Converts VertexIdLabel into a "lighter" VertexId
 */
struct InMemoryGraphView : IGraphView {
  InMemoryGraphView() = default;
  InMemoryGraphView(InMemoryGraph const& graph) : _graph{graph} {}

  auto outEdges(VertexId vertex) -> std::vector<Edge> override {
    std::vector<Edge> result;
    // for (auto const& [id, edge] : _edges) {
    //   if (edge.from == vertex) {
    //     result.emplace_back(edge);
    //   }
    // }
    return result;
  }

  /**
     Assumes that a vertex exists in the graph

     If the vertex does not exist in the graph, it adds it to the saved vertices
     and returns that, although it is not part of the graph.
   */
  auto ensureVertex(VertexIdLabel const& vertex) -> VertexId override {
    auto it = std::find(_vertexIdLabels.begin(), _vertexIdLabels.end(), vertex);

    if (it == _vertexIdLabels.end()) {
      _vertexIdLabels.emplace_back(vertex);
      it = std::find(_vertexIdLabels.begin(), _vertexIdLabels.end(), vertex);
    }
    size_t distance = std::distance(_vertexIdLabels.begin(), it);
    return VertexId{distance};
  }

  auto queryVertex(VertexIdLabel const& vertex)
      -> std::optional<VertexId> override {
    auto it = std::find(_vertexIdLabels.begin(), _vertexIdLabels.end(), vertex);

    if (it == _vertexIdLabels.end()) {
      return std::nullopt;
    }
    size_t distance = std::distance(_vertexIdLabels.begin(), it);
    return VertexId{distance};
  }

  auto queryEdge(VertexId const& from, VertexId const& to)
      -> std::optional<EdgeId> {
    // TODO
  }

  auto retrieveDocument(VertexId const& id)
      -> std::optional<VertexIdLabel> override {
    if (id.id >= _vertexIdLabels.size()) {
      return std::nullopt;
    }
    auto label = _vertexIdLabels[id.id];
    if (not doesVertexDocumentExist(label)) {
      return std::nullopt;
    }
    return label;
  }

  auto doesVertexDocumentExist(VertexIdLabel const& vertex) -> bool {
    return _graph.includesVertex(vertex);
  }

  // noted down labels (not necessarily part of the graph)
  std::vector<VertexIdLabel> _vertexIdLabels;
  // std::vector<EdgeIdLabel> _edgeIdLabels;
  InMemoryGraph _graph;

  // indexes
  // std::unordered_map<VertexId, std::vector<EdgeId>> _from_edges;
  // std::unordered_map<VertexId, EdgeId> _to_edges;
};

}  // namespace arangodb::graph::experimental
// namespace arangodb::graph
