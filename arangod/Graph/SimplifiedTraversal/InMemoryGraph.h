#pragma once

#include <velocypack/HashedStringRef.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include "Graph/Types/VertexRef.h"
#include "Graph/SimplifiedTraversal/IGraphView.h"

namespace arangodb::graph::experimental {

struct InMemoryGraph {
  InMemoryGraph() = default;
  InMemoryGraph(
      std::vector<VertexIdLabel> vertexIdLabels,
      std::unordered_map<EdgeIdLabel, std::tuple<VertexIdLabel, VertexIdLabel>>
          edges)
      : _vertexIdLabels{std::move(vertexIdLabels)},
        _edgeIdLabels{std::move(edges)} {}

  auto retrieveEdgeIdLabel(VertexIdLabel const& from, VertexIdLabel const& to)
      -> std::optional<EdgeIdLabel> {
    for (auto const& [id, tuple] : _edgeIdLabels) {
      if (std::get<0>(tuple) == from && std::get<1>(tuple) == to) {
        return id;
      }
    }
    return std::nullopt;
  }

  auto includesVertex(VertexIdLabel const& vertex) -> bool {
    auto it = std::find(_vertexIdLabels.begin(), _vertexIdLabels.end(), vertex);
    if (it == _vertexIdLabels.end()) {
      return false;
    }
    return true;
  }

  auto getVertexTo(EdgeIdLabel const& edge) -> std::optional<VertexIdLabel> {
    if (auto find = _edgeIdLabels.find(edge); find != _edgeIdLabels.end()) {
      return std::get<1>(find->second);
    }
    return std::nullopt;
  }

  auto outEdges(VertexIdLabel const& vertex) -> std::vector<EdgeIdLabel> {
    auto out = std::vector<EdgeIdLabel>{};
    for (auto const& [edge, tuple] : _edgeIdLabels) {
      if (std::get<0>(tuple) == vertex) {
        out.emplace_back(edge);
      }
    }
    return out;
  }

  std::vector<VertexIdLabel> _vertexIdLabels;
  std::unordered_map<EdgeIdLabel, std::tuple<VertexIdLabel, VertexIdLabel>>
      _edgeIdLabels;
};

/**
   Gives a view of the graph

   Converts VertexIdLabel into a "lighter" VertexId
 */
struct InMemoryGraphView : IGraphView {
  InMemoryGraphView() = default;
  InMemoryGraphView(InMemoryGraph const& graph) : _graph{graph} {}

  auto outEdges(VertexId vertex) -> std::vector<EdgeId> override {
    std::vector<EdgeId> result;
    auto edgeLabels = _graph.outEdges(_vertexIdLabels[vertex.id]);
    for (auto const& edge : edgeLabels) {
      if (auto e = ensureEdge(edge); e != std::nullopt) {
        result.emplace_back(e.value());
      }
    }
    return result;
  }

  auto outEdges(EdgeId edge) -> std::vector<EdgeId> override {
    std::vector<EdgeId> result;
    auto vertex = _graph.getVertexTo(_edgeIdLabels[edge.id]).value();
    auto id = ensureVertex(vertex);
    auto edgeLabels = _graph.outEdges(_vertexIdLabels[id.id]);
    for (auto const& edge : edgeLabels) {
      if (auto e = ensureEdge(edge); e != std::nullopt) {
        result.emplace_back(e.value());
      }
    }
    return result;
  }
  /**
     Assumes that a vertex exists in the graph

     It adds it to the saved vertices
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

  auto ensureEdgeToVertex(EdgeId const& edge) -> VertexId override {
    auto toLabel = _graph.getVertexTo(_edgeIdLabels[edge.id]);
    ADB_PROD_ASSERT(toLabel.has_value());
    return ensureVertex(toLabel.value());
  }

  auto ensureEdge(EdgeIdLabel const& edge) -> std::optional<EdgeId> {
    auto it = std::find(_edgeIdLabels.begin(), _edgeIdLabels.end(), edge);

    if (it == _edgeIdLabels.end()) {
      _edgeIdLabels.emplace_back(edge);
      it = std::find(_edgeIdLabels.begin(), _edgeIdLabels.end(), edge);
    }
    size_t distance = std::distance(_edgeIdLabels.begin(), it);
    return EdgeId{distance};
  }

  /**
     Gives a vertex handle

     Either we find it in the graph, then it is returned.
     Or the vertex was already requested from this view, then this handle is
     returned.
   */
  auto queryVertex(VertexIdLabel const& vertex)
      -> std::optional<VertexId> override {
    if (_graph.includesVertex(vertex)) {
      _vertexIdLabels.emplace_back(vertex);
      return VertexId{_vertexIdLabels.size() - 1};
    }
    auto it = std::find(_vertexIdLabels.begin(), _vertexIdLabels.end(), vertex);

    if (it == _vertexIdLabels.end()) {
      return std::nullopt;
    }
    size_t distance = std::distance(_vertexIdLabels.begin(), it);
    return VertexId{distance};
  }

  auto queryEdge(VertexId const& from, VertexId const& to)
      -> std::optional<EdgeId> {
    auto fromLabel = _vertexIdLabels[from.id];
    auto toLabel = _vertexIdLabels[to.id];
    if (auto edgeLabel = _graph.retrieveEdgeIdLabel(fromLabel, toLabel);
        edgeLabel != std::nullopt) {
      _edgeIdLabels.emplace_back(edgeLabel.value());
      return EdgeId{_edgeIdLabels.size() - 1};
    }
    return std::nullopt;
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
  // VertexId is the position in this vector
  std::vector<VertexIdLabel> _vertexIdLabels;
  std::vector<EdgeIdLabel> _edgeIdLabels;
  InMemoryGraph _graph;

  // indexes
  // std::unordered_map<VertexId, std::vector<EdgeId>> _from_edges;
  // std::unordered_map<VertexId, EdgeId> _to_edges;
};

}  // namespace arangodb::graph::experimental
// namespace arangodb::graph
