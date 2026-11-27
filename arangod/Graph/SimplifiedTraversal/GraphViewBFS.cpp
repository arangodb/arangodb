#include "Graph/SimplifiedTraversal/GraphViewBFS.h"
#include <cstdint>
#include <iterator>
#include <limits>
#include <unordered_set>
#include <deque>
#include <generator>

#include "Graph/SimplifiedTraversal/Interior.h"
#include "Inspection/Format.h"

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

//

auto graphViewBFS(std::shared_ptr<IGraphView> view, VertexIdLabel start)
    -> std::generator<std::vector<EdgeId>> {
  // queue contains Interior::Entry, because the queue contains candidates for
  // extensions of an already found path; the entry on the queue points to the
  // end of the path that is being extended.
  //
  // This also means that Interior and queue are in a relationship;
  std::deque<Interior::Entry> queue;

  // TODO: possibly this should be a single command for the GraphView
  auto startId = view->ensureVertex(start);

  Interior interior{startId};

  co_yield {};  // path that only contains the start vertex!

  auto initialEdges = view->outEdges(startId);
  std::cerr << inspection::json(initialEdges);

  for (auto&& ie : initialEdges) {
    queue.emplace_back(Interior::Entry{Interior::Root{}, std::move(ie)});
  }

  // TODO: definitely one command (i.e. this needs to be a hand rolled queue
  while (!queue.empty()) {
    auto nextItem = queue.front();
    queue.pop_front();

    auto loc = interior.add(nextItem);
    auto path = interior.trace(loc).value();
    co_yield path;

    auto const outEdges = view->outEdges(nextItem.second);
    for (auto&& ie : outEdges) {
      // shouldEmplace(view, interior, ie)
      //  should this support
      //    - filtering,
      //    - pruning,
      //    - min/max depth,
      //    - (global) uniqueness of vertices
      //    - uniqueness of edges
      if (!interior.shouldEmplace(ie)) {
        queue.emplace_back(Interior::Entry{nextItem.first, ie});
      }
    }
  }
  std::cerr << "done";
}
}  // namespace arangodb::graph::experimental
