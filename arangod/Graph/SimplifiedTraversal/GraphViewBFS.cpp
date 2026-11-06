#include "Graph/SimplifiedTraversal/GraphViewBFS.h"
#include <cstdint>
#include <iterator>
#include <limits>
#include <unordered_set>
#include <deque>

namespace arangodb::graph::experimental {

/*

  v -1-> w -3-> x
    -2->

  queue: (-1, -1->), (-1,-2->)

  (-1, -1->) -> insert into seen, gets "handle" 0,
     because schreier vector [(-1,-1->)]

  find all neighbours
     -3->
  queue (-1,-2->), (0,-3->)

  (-1, -2->) -> insert into seen gets handle 1,
    schreier vector [(-1,-1->),(-1,-2->)]

  find all neighbour
     -3->

  queue: (0,-3->), (1,-3->)

  (0,-3->)
   schreier vector [(-1,-1->), (-1,-2->),(0,-3->)]
   no out edges -> nothing happens.

  queue: (1,-3->)
  (1,-3->)
   schreier vector [(-1,-1->), (-1,-2->),(0,-3->),(1,-3->)]
   no out edges -> nothing happens

  queue: empty
  done.

 */

auto graphViewBFS(std::shared_ptr<IGraphView> view, VertexIdLabel start)
    -> Interior {
  Interior interior;
  std::deque<Interior::Entry> queue;

  // TODO: possibly this should be a single command for the GraphView
  auto startId = view->ensureVertex(start);
  auto initialEdges = view->outEdges(startId);

  for (auto&& ie : initialEdges) {
    queue.emplace_back(Interior::Entry{-1, std::move(ie)});
  }

  // TODO: definitely one command (i.e. this needs to be a hand rolled queue
  while (!queue.empty()) {
    auto nextItem = queue.front();
    queue.pop_front();

    auto const outEdges = view->outEdges(nextItem.second);

    interior._screamer.emplace_back(nextItem);

    for (auto&& ie : outEdges) {
      if (!interior.shouldEmplace(ie)) {
        queue.emplace_back(Interior::Entry{nextItem.first, ie});
      }
    }
  }
  return interior;
}
}  // namespace arangodb::graph::experimental
