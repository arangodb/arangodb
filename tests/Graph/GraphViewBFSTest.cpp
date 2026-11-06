#include <gtest/gtest.h>

#include "Graph/SimplifiedTraversal/IGraphView.h"
#include "Graph/SimplifiedTraversal/InMemoryGraph.h"
#include "Graph/SimplifiedTraversal/GraphViewBFS.h"

#include "Inspection/Format.h"

namespace arangodb::graph::experimental {

TEST(GraphViewBFSTest, empty) {
  auto graph = InMemoryGraph{};
  auto graphView = std::make_shared<InMemoryGraphView>(graph);

  auto paths = graphViewBFS(graphView, VertexIdLabel("Bollocks"));

  ASSERT_EQ(
      paths.size(),
      0);  // TODO: probably 1, because the start vertex is assumed to exist
}

TEST(GraphViewBFSTest, two_vertices_no_edge) {
  auto graph = InMemoryGraph{{VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}, {}};
  auto graphView = std::make_shared<InMemoryGraphView>(graph);

  auto paths = graphViewBFS(graphView, VertexIdLabel("v/0"));

  ASSERT_EQ(paths.size(), 1);
}

TEST(GraphViewBFSTest, single_edge) {
  auto v0 = VertexIdLabel{"v/0"};
  auto v1 = VertexIdLabel{"v/1"};
  auto graph = InMemoryGraph{{v0, v1}, {{EdgeIdLabel{"e/0"}, {v0, v1}}}};
  auto graphView = std::make_shared<InMemoryGraphView>(graph);

  auto paths = graphViewBFS(graphView, VertexIdLabel("v/0"));

  ASSERT_EQ(paths.size(),
            2);  // the path consisting of v/0 and the path v/0 e/0 v/1
}

TEST(GraphViewBFSTest, simple) {
  auto v0 = std::string{"v/0"};
  auto v1 = std::string{"v/1"};
  auto v2 = std::string{"v/2"};

  auto graph = InMemoryGraph{
      {VertexIdLabel{v0}, VertexIdLabel{v1}, VertexIdLabel{v2}},
      {{EdgeIdLabel{"e/0"}, {VertexIdLabel{v0}, VertexIdLabel{v1}}},
       {EdgeIdLabel{"e/1"}, {VertexIdLabel{v0}, VertexIdLabel{v2}}}}};

  auto graphView = std::make_shared<experimental::InMemoryGraphView>(graph);

  auto paths = graphViewBFS(graphView, VertexIdLabel{v0});

  for (auto ee : paths) {
    std::cerr << inspection::json(ee) << ",";
  }
}

}  // namespace arangodb::graph::experimental
