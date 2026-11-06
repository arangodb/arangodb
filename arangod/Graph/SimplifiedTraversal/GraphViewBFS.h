#pragma once

#include <vector>
#include "Graph/SimplifiedTraversal/IGraphView.h"

namespace arangodb::graph::experimental {

auto graphViewBFS(std::shared_ptr<IGraphView> view, VertexIdLabel startLabel)
    -> std::vector<EdgeId>;

}  // namespace arangodb::graph::experimental
