#pragma once

#include <vector>
#include "Graph/SimplifiedTraversal/IGraphView.h"

namespace arangodb::graph::experimental {
struct Interior {
  using Entry = std::pair<ssize_t, EdgeId>;

  Interior() : _screamer() {}

  [[nodiscard]] auto shouldEmplace(EdgeId edge) const -> bool { return true; }
  [[nodiscard]] auto size() const -> size_t { return _screamer.size(); }

  std::vector<Entry> _screamer;
};

auto graphViewBFS(std::shared_ptr<IGraphView> view, VertexIdLabel startLabel)
    -> Interior;

}  // namespace arangodb::graph::experimental
