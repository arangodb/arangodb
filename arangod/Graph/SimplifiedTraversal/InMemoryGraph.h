#pragma once

#include <velocypack/HashedStringRef.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include "Graph/Types/VertexRef.h"
#include "Graph/SimplifiedTraversal/IGraphView.h"

namespace arangodb::graph::experimental {

struct InMemoryGraph : IGraphView {
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

  auto outEdges(VertexId vertex) -> std::vector<Edge> override {
    std::vector<Edge> result;
    for (auto const& [id, edge] : _edges) {
      if (edge.from == vertex) {
        result.emplace_back(edge);
      }
    }
    return result;
  }

  auto vertex(VertexIdLabel const& vertex) -> std::optional<VertexId> override {
    auto it = std::find(_vertexIdLabels.begin(), _vertexIdLabels.end(), vertex);

    if (it == _vertexIdLabels.end()) {
      return std::nullopt;
    }
    size_t distance = std::distance(_vertexIdLabels.begin(), it);
    return VertexId{distance};
  }

  std::vector<VertexIdLabel> _vertexIdLabels;
  std::unordered_map<EdgeId, Edge> _edges;

 private:
  size_t _next_edge_id = 0;
  // indexes
  // std::unordered_map<VertexId, std::vector<EdgeId>> _from_edges;
  // std::unordered_map<VertexId, EdgeId> _to_edges;
};

}  // namespace arangodb::graph::experimental
// namespace arangodb::graph
