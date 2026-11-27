#pragma once

#include <velocypack/HashedStringRef.h>
#include <boost/asio/detail/scheduler_thread_info.hpp>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include "Graph/Types/VertexRef.h"
#include "Graph/SimplifiedTraversal/IGraphView.h"
#include "Inspection/Format.h"

namespace arangodb::graph::experimental {

struct InMemoryGraph {
  InMemoryGraph() = default;
  InMemoryGraph(
      std::vector<VertexIdLabel> vertexIdLabels,
      std::unordered_map<EdgeIdLabel, std::tuple<VertexIdLabel, VertexIdLabel>>
          edges)
      : _vertexIdLabels{std::move(vertexIdLabels)},
        _edgeIdLabels{std::move(edges)} {}

  auto includesVertex(VertexIdLabel const& vertex) -> bool {
    auto it = std::find(_vertexIdLabels.begin(), _vertexIdLabels.end(), vertex);
    if (it == _vertexIdLabels.end()) {
      return false;
    }
    return true;
  }

  auto includesEdge(EdgeIdLabel const& edge) -> bool {
    return _edgeIdLabels.contains(edge);
  }

  auto retrieveEdgeIdLabel(VertexIdLabel const& from, VertexIdLabel const& to)
      -> std::optional<EdgeIdLabel> {
    for (auto const& [id, tuple] : _edgeIdLabels) {
      if (std::get<0>(tuple) == from && std::get<1>(tuple) == to) {
        return id;
      }
    }
    return std::nullopt;
  }

  auto getToVertex(EdgeIdLabel const& edge) -> std::optional<VertexIdLabel> {
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

   Converts VertexIdLabel and EdgeIdLabel into "lighter" VertexId and EdgeId.
 */
struct InMemoryGraphView : IGraphView {
  InMemoryGraphView() = default;
  InMemoryGraphView(InMemoryGraph const& graph) : _graph{graph} {}

  /**
     Memorizes this `vertex`.

     It adds it to the saved vertices and returns that, although it is not
     necessarily part of the graph.
   */
  auto ensureVertex(VertexIdLabel const& vertex) -> VertexId override {
    auto it = std::find(_vertexIdLabels.begin(), _vertexIdLabels.end(), vertex);

    if (it == _vertexIdLabels.end()) {
      _vertexIdLabels.emplace_back(vertex);
      return VertexId{_vertexIdLabels.size() - 1};
    }

    size_t distance = std::distance(_vertexIdLabels.begin(), it);
    return VertexId{distance};
  }

  /**
     Gives the corresponding edge if it exists in the graph.

     Edges can only exist if the graph has a corresponding edge because there
     are no public functions to add an edge to this view manually.
   */
  auto queryEdge(EdgeIdLabel const& edge) -> std::optional<EdgeId> {
    if (_graph.includesEdge(edge)) {
      return ensureEdge(edge);
    }
    return std::nullopt;
  }

  /**
     Gives the to-vertex of the given `edge`.
   */
  auto getToVertex(EdgeId const& edge) -> VertexId override {
    auto toLabel = _graph.getToVertex(_edgeIdLabels[edge.id]);
    // all edges in the view correspond to a real edge in the graph
    ADB_PROD_ASSERT(toLabel.has_value());
    return ensureVertex(toLabel.value());
  }

  /**
     Get out-edges vertex-centric

     Gives all edges that start with `vertex`.
   */
  auto outEdges(VertexId vertex) -> std::vector<EdgeId> override {
    std::vector<EdgeId> result;
    auto edgeLabels = _graph.outEdges(_vertexIdLabels[vertex.id]);
    for (auto const& edge : edgeLabels) {
      result.emplace_back(ensureEdge(edge));
    }
    return result;
  }

  /**
     Get out-edges edge-centric

     Gives all edges that start at end of `edge`.
   */
  auto outEdges(EdgeId edge) -> std::vector<EdgeId> override {
    return outEdges(getToVertex(edge));
  }

 private:
  /**
     Makes sure that the view knows the given ede.

     Crashes if the given `edge` is not part of the graph.
   */
  auto ensureEdge(EdgeIdLabel const& edge) -> EdgeId {
    auto it = std::find(_edgeIdLabels.begin(), _edgeIdLabels.end(), edge);

    if (it == _edgeIdLabels.end()) {
      // makes sure that the edge exists in the graph
      ADB_PROD_ASSERT(_graph.includesEdge(edge));
      _edgeIdLabels.emplace_back(edge);
      return EdgeId{_edgeIdLabels.size() - 1};
    }

    size_t distance = std::distance(_edgeIdLabels.begin(), it);
    return EdgeId{distance};
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
