#include "Graph/SimplifiedTraversal/GraphViewBFS.h"
#include <unordered_set>
#include <deque>

namespace arangodb::graph::experimental {

namespace {
struct Interior {
  // only suppport paths of length 65535 for now
  std::vector<std::uint16_t> _screamer;
  std::vector<EdgeId> _edges;
};
}  // namespace

auto graphViewBFS(std::shared_ptr<IGraphView> view, VertexIdLabel start)
    -> std::vector<EdgeId> {
  std::vector<EdgeId> result;

  std::deque<EdgeId> queue;
  std::unordered_set<EdgeId> seen;

  // TODO: possibly this should be a single command for the GraphView
  auto startId = view->ensureVertex(start);
  auto initialEdges = view->outEdges(startId);

  std::cerr << "adding initial edges" << std::endl;
  for (auto&& ie : initialEdges) {
    std::cerr << "edge";
    result.emplace_back(ie);
    queue.emplace_back(std::move(ie));
  }

  // TODO: definitely one command (i.e. this needs to be a hand rolled queue
  while (!queue.empty()) {
    std::cerr << "queue not empty, next run!" << std::endl;
    auto next = queue.front();
    queue.pop_front();

    std::cerr << " 2" << std::endl;
    auto const outEdges = view->outEdges(next);

    std::cerr << " 4" << std::endl;
    seen.emplace(next);
    std::cerr << "outetsches";
    for (auto&& ie : outEdges) {
      std::cerr << "and in it goes!";
      if (!seen.contains(ie)) {
        queue.emplace_back(ie);
      }
    }
  }
  return result;
}
}  // namespace arangodb::graph::experimental
