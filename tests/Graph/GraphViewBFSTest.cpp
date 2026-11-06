#include <gtest/gtest.h>
#include <ranges>

#include "Graph/SimplifiedTraversal/IGraphView.h"
#include "Graph/SimplifiedTraversal/InMemoryGraph.h"
#include "Graph/SimplifiedTraversal/GraphViewBFS.h"

#include "Inspection/Format.h"

namespace arangodb::graph::experimental {
namespace {
struct Path {
  std::vector<VertexIdLabel> vertices;
  std::vector<EdgeIdLabel> edges;

  bool operator==(Path const& other) const = default;
};

template<typename Inspector>
auto inspect(Inspector& f, Path& p) {
  return f.object(p).fields(f.field("vertices", p.vertices),
                            f.field("edges", p.edges));
}

auto materializePaths(std::shared_ptr<IGraphView> view, Interior interior)
    -> std::vector<Path> {
  std::vector<Path> result;

  for (auto [prev, edgeId] : interior._screamer | std::views::reverse) {
    auto path = Path{};

    while (prev != -1) {
      path.edges.emplace_back(view->queryEdgeLabel(edgeId));

      auto toV = view->ensureEdgeToVertex(edgeId);

      path.vertices.emplace_back(*view->retrieveDocument(toV));

      std::tie(prev, edgeId) = interior._screamer[prev];
    }
    result.emplace_back(path);
  }

  return result;
}
}  // namespace

TEST(GraphViewBFSTest, empty) {
  auto graph = InMemoryGraph{};
  auto graphView = std::make_shared<InMemoryGraphView>(graph);

  /* Expect:
     "Bollocks"
  */
  auto paths = graphViewBFS(graphView, VertexIdLabel("Bollocks"));
  auto materialisedPaths = materializePaths(graphView, paths);

  ASSERT_EQ(materialisedPaths,
            (std::vector<Path>{Path{{VertexIdLabel("Bollocks")}, {}}}));
}

TEST(GraphViewBFSTest, two_vertices_no_edge) {
  auto graph = InMemoryGraph{{VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}, {}};
  auto graphView = std::make_shared<InMemoryGraphView>(graph);

  /* Expect:
     v/0
  */
  auto paths = graphViewBFS(graphView, VertexIdLabel("v/0"));
  auto materialisedPaths = materializePaths(graphView, paths);

  ASSERT_EQ(materialisedPaths,
            (std::vector<Path>{Path{{VertexIdLabel("v/0")}, {}}}))
      << inspection::json(materialisedPaths);
}

TEST(GraphViewBFSTest, single_edge) {
  auto v0 = VertexIdLabel{"v/0"};
  auto v1 = VertexIdLabel{"v/1"};
  auto graph = InMemoryGraph{{v0, v1}, {{EdgeIdLabel{"e/0"}, {v0, v1}}}};
  auto graphView = std::make_shared<InMemoryGraphView>(graph);

  /* Expect:
     v/0
     v/0 -[e/0]-> v/1
  */

  auto paths = graphViewBFS(graphView, VertexIdLabel("v/0"));
  auto materialisedPaths = materializePaths(graphView, paths);

  ASSERT_EQ(materialisedPaths,
            (std::vector<Path>{Path{{v0}, {}},
                               Path{{v0, v1}, {EdgeIdLabel{"e/0"}}}}));
}

TEST(GraphViewBFSTest, simple) {
  auto v0 = VertexIdLabel{"v/0"};
  auto v1 = VertexIdLabel{"v/1"};
  auto v2 = VertexIdLabel{"v/2"};
  auto e0 = EdgeIdLabel{"e/0"};
  auto e1 = EdgeIdLabel{"e/1"};

  auto graph = InMemoryGraph{{v0, v1, v2}, {{e0, {v0, v1}}, {e1, {v0, v2}}}};

  auto graphView = std::make_shared<experimental::InMemoryGraphView>(graph);

  /* Expect:
     v/0
     v/0 -[e/0]-> v/1
     v/0 -[e/1]-> v/2
  */
  auto paths = graphViewBFS(graphView, VertexIdLabel{v0});
  auto materialisedPaths = materializePaths(graphView, paths);
  ASSERT_EQ(materialisedPaths, (std::vector<Path>{Path{{v0}, {}},        //
                                                  Path{{v0, v1}, {e0}},  //
                                                  Path{{v0, v2}, {e1}}}));
}

TEST(GraphViewBFSTest, two_edges_between_two_vertices) {
  auto v0 = VertexIdLabel{"v/0"};
  auto v1 = VertexIdLabel{"v/1"};
  auto v2 = VertexIdLabel{"v/2"};
  auto e0 = EdgeIdLabel{"e/0"};
  auto e1 = EdgeIdLabel{"e/1"};
  auto e2 = EdgeIdLabel{"e/2"};

  auto graph = InMemoryGraph{{v0, v1, v2},
                             {{e0, {v0, v1}}, {e1, {v0, v1}}, {e2, {v1, v2}}}};

  auto graphView = std::make_shared<experimental::InMemoryGraphView>(graph);

  /* Expect:
     v/0
     v/0 -[e/0]-> v/1 -[e/2] ->v/2
     v/0 -[e/1]-> v/1 -[e/2] ->v/2
  */
  auto paths = graphViewBFS(graphView, VertexIdLabel{v0});
  auto materialisedPaths = materializePaths(graphView, paths);

  ASSERT_EQ(materialisedPaths,
            (std::vector<Path>{Path{{v0}, {}},                //
                               Path{{v0, v1, v2}, {e0, e2}},  //
                               Path{{v0, v1, v2}, {e1, e2}}}))
      << inspection::json(materialisedPaths);
}

}  // namespace arangodb::graph::experimental
