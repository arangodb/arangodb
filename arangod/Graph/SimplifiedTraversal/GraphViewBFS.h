#pragma once

#include <generator>
#include <vector>
#include "Graph/SimplifiedTraversal/IGraphView.h"

namespace arangodb::graph::experimental {
struct Interior {
  using Entry = std::pair<ssize_t, EdgeId>;

  Interior() : _screamer() {}

  [[nodiscard]] auto shouldEmplace(EdgeId edge) const -> bool { return true; }
  [[nodiscard]] auto size() const -> size_t { return _screamer.size(); }
  [[nodiscard]] auto trace(size_t index) -> std::vector<EdgeId> {
    auto result = std::vector<EdgeId>{};
    while (index > 0) {
      auto [prev, edge] = _screamer.at(index);
      result.emplace_back(edge);
      index = prev;
    }
    return result;
  }

  std::vector<Entry> _screamer;
  VertexId _startVertex;
};

auto graphViewBFS(std::shared_ptr<IGraphView> view, VertexIdLabel startLabel)
  -> std::generator<std::vector<EdgeId>>;

}  // namespace arangodb::graph::experimental
