#include "Graph/Enumerators/ITraversalEnumerator.h"
#include "Graph/PathManagement/IPathResult.h"
#include "Graph/PathManagement/SingleServerPathResult.h"

#include <optional>
#include <string>
#include <utility>
#include <memory>

namespace arangodb::graph::experimental {
struct SingleServerTraversalEnumerator : ITraversalEnumerator {
  SingleServerTraversalEnumerator() {}
  ~SingleServerTraversalEnumerator() {}
  void clear(bool keepPathStore) override { TRI_ASSERT(false); }
  [[nodiscard]] bool isDone() const override { return _isDone; }

  // does not validate that vertex exists on graph
  void reset(VertexRef source, size_t depth = 0, double weight = 0.0,
             bool keepPathStore = false) override {
    _isDone = false;
    _startVertex = source;
  }
  void resetManyStartVertices(
      std::vector<VertexDescription> const& vertices) override {
    TRI_ASSERT(false);
  };

  auto prepareIndexExpressions(aql::Ast* ast) -> void override {
    TRI_ASSERT(false);
  };
  auto getNextPath() -> std::unique_ptr<IPathResult> override {
    if (not _startVertex.has_value()) {
      return nullptr;
    }
    _isDone = true;
    return std::make_unique<SingleServerPathResult>(
        std::vector<std::optional<VertexId>>{std::nullopt},
        std::vector<Edge>{});
  };

  auto neighbourhood() {
    auto [previous, next] = _queue.pop_front();
    auto neighbours = graph.createNeighbourCursor(next);
    for (auto const neighbour : neighbours) {
      _queue.push_back({next, neighbour});
    }
    _paths.add(next);  // Need a data structure (similar to _interior)
    // _results: vec<Step> end steps of paths; and _interior
    _interior.append(previous, next);  // need actually to and edge
    // auto append(Step, Step)
    // auto reverseReconstruction
  }

#ifdef USE_ENTERPRISE
  auto smartSearch(size_t amountOfExpansions, velocypack::Builder& result)
      -> void override {
    TRI_ASSERT(false);
  }
#endif
  bool skipPath() override {
    TRI_ASSERT(false);
    return false;
  }
  auto destroyEngines() -> void override { TRI_ASSERT(false); };

  auto stealStats() -> aql::TraversalStats override {
    TRI_ASSERT(false); /*TODO*/
  };

  auto validatorUsesPrune() const -> bool override {
    TRI_ASSERT(false);
    return false;
  }
  auto validatorUsesPostFilter() const -> bool override {
    TRI_ASSERT(false);
    return false;
  }
  auto setValidatorContext(aql::InputAqlItemRow& inputRow) -> void override {
    TRI_ASSERT(false);
  }
  auto unprepareValidatorContext() -> void override { TRI_ASSERT(false); }

 private:
  bool _isDone = true;
  std::optional<VertexRef> _startVertex;
  std::deque<VertexRef> _queue;
};
}  // namespace arangodb::graph::experimental
