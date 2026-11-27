#include <gtest/gtest.h>
#include <optional>
#include "Graph/SimplifiedTraversal/InMemoryGraph.h"

namespace arangodb::graph::experimental {

TEST(InMemoryGraphTest, empty_graph_does_not_include_any_vertices_or_edges) {
  auto graph = InMemoryGraph{};
  EXPECT_EQ(
      graph.retrieveEdgeIdLabel(VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}),
      std::nullopt);
  EXPECT_FALSE(graph.includesVertex(VertexIdLabel{"v/0"}));
  EXPECT_EQ(graph.getToVertex(EdgeIdLabel{"e/0"}), std::nullopt);
  EXPECT_EQ(graph.outEdges(VertexIdLabel{"v/0"}), (std::vector<EdgeIdLabel>{}));
}

TEST(InMemoryGraphTest,
     graph_with_two_vertices_includes_vertices_but_not_any_edges) {
  auto graph = InMemoryGraph{{VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}, {}};
  EXPECT_TRUE(graph.includesVertex(VertexIdLabel{"v/0"}));
  EXPECT_TRUE(graph.includesVertex(VertexIdLabel{"v/1"}));
  EXPECT_FALSE(graph.includesVertex(VertexIdLabel{"v/2"}));
  EXPECT_EQ(
      graph.retrieveEdgeIdLabel(VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}),
      std::nullopt);
}

TEST(InMemoryGraphTest, graph_with_edge_includes_edge_but_not_any_vertices) {
  auto graph = InMemoryGraph{
      {}, {{EdgeIdLabel{"e/0"}, {VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}}}};
  EXPECT_FALSE(graph.includesVertex(VertexIdLabel{"v/0"}));
  EXPECT_FALSE(graph.includesVertex(VertexIdLabel{"v/1"}));
  EXPECT_EQ(
      graph.retrieveEdgeIdLabel(VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}),
      EdgeIdLabel{"e/0"});
}

TEST(InMemoryGraphTest, graph_with_edge_knows_details_about_edge) {
  auto graph = InMemoryGraph{
      {}, {{EdgeIdLabel{"e/0"}, {VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}}}};
  EXPECT_EQ(graph.getToVertex(EdgeIdLabel{"e/0"}), VertexIdLabel{"v/1"});
  EXPECT_EQ(graph.outEdges(VertexIdLabel{"v/0"}),
            (std::vector<EdgeIdLabel>{EdgeIdLabel{"e/0"}}));
}

// TODO fix
TEST(InMemoryGraphTest, graph_with_two_parallel_edges) {
  auto graph = InMemoryGraph{
      {},
      {
          {EdgeIdLabel{"e/0"}, {VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}},
          {EdgeIdLabel{"e/1"}, {VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}},
      }};
  // TODO should retrieve both edges
  EXPECT_EQ(
      graph.retrieveEdgeIdLabel(VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}),
      EdgeIdLabel{"e/1"});
  EXPECT_EQ(graph.getToVertex(EdgeIdLabel{"e/0"}), VertexIdLabel{"v/1"});
  EXPECT_EQ(graph.getToVertex(EdgeIdLabel{"e/1"}), VertexIdLabel{"v/1"});
  EXPECT_EQ(graph.outEdges(VertexIdLabel{"v/0"}),
            (std::vector<EdgeIdLabel>{EdgeIdLabel{"e/1"}, EdgeIdLabel{"e/0"}}));
}

TEST(InMemoryGraphTest, graph_with_same_edge_id_labels_ignores_later_label) {
  auto graph = InMemoryGraph{
      {},
      {{EdgeIdLabel{"e/0"}, {VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}},
       {EdgeIdLabel{"e/0"}, {VertexIdLabel{"v/3"}, VertexIdLabel{"v/4"}}}}};
  EXPECT_EQ(
      graph.retrieveEdgeIdLabel(VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}),
      EdgeIdLabel{"e/0"});
  EXPECT_NE(
      graph.retrieveEdgeIdLabel(VertexIdLabel{"v/3"}, VertexIdLabel{"v/4"}),
      EdgeIdLabel{"e/0"});
  EXPECT_EQ(graph.getToVertex(EdgeIdLabel{"e/0"}), VertexIdLabel{"v/1"});
}

TEST(InMemoryGraphTest, out_edges_gives_only_out_edges) {
  auto graph = InMemoryGraph{
      {},
      {
          {EdgeIdLabel{"e/0"}, {VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}},
          {EdgeIdLabel{"e/1"}, {VertexIdLabel{"v/1"}, VertexIdLabel{"v/2"}}},
          {EdgeIdLabel{"e/2"}, {VertexIdLabel{"v/1"}, VertexIdLabel{"v/3"}}},
      }};
  EXPECT_EQ(graph.outEdges(VertexIdLabel{"v/1"}),
            (std::vector<EdgeIdLabel>{{"e/2"}, {"e/1"}}));
}

// InMemoryGraphView

TEST(InMemoryGraphViewTest, empty_graph_does_not_have_any_edges) {
  auto view = InMemoryGraphView{};
  EXPECT_EQ(view.queryEdge(EdgeIdLabel{"e/0"}), std::nullopt);
}

TEST(InMemoryGraphViewTest, out_edges_of_empty_graph_are_empty) {
  auto view = InMemoryGraphView{};
  auto vertexId = view.ensureVertex(VertexIdLabel{"v/0"});
  EXPECT_EQ(view.outEdges(vertexId), (std::vector<EdgeId>{}));
}

TEST(InMemoryGraphViewTest, out_edges_gives_only_out_edges) {
  auto graph = InMemoryGraph{
      {},
      {
          {EdgeIdLabel{"e/0"}, {VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}},
          {EdgeIdLabel{"e/1"}, {VertexIdLabel{"v/1"}, VertexIdLabel{"v/2"}}},
          {EdgeIdLabel{"e/2"}, {VertexIdLabel{"v/1"}, VertexIdLabel{"v/3"}}},
      }};

  // vertex centric
  {
    auto view = InMemoryGraphView{graph};
    auto vertexId = view.ensureVertex(VertexIdLabel{"v/1"});
    auto outEdges = view.outEdges(vertexId);

    // check that edges exist in view
    auto edgeId1 = view.queryEdge(EdgeIdLabel{"e/1"});
    auto edgeId2 = view.queryEdge(EdgeIdLabel{"e/2"});
    EXPECT_TRUE(edgeId1.has_value());
    EXPECT_TRUE(edgeId2.has_value());

    EXPECT_EQ(outEdges,
              (std::vector<EdgeId>{edgeId2.value(), edgeId1.value()}));
  }

  // edge centric
  {
    auto view = InMemoryGraphView{graph};
    auto edgeId = view.queryEdge(EdgeIdLabel{"e/0"});
    EXPECT_TRUE(edgeId.has_value());
    auto outEdges = view.outEdges(edgeId.value());

    // check that edges exist in view
    auto edgeId1 = view.queryEdge(EdgeIdLabel{"e/1"});
    auto edgeId2 = view.queryEdge(EdgeIdLabel{"e/2"});
    EXPECT_TRUE(edgeId1.has_value());
    EXPECT_TRUE(edgeId2.has_value());

    EXPECT_EQ(outEdges,
              (std::vector<EdgeId>{edgeId2.value(), edgeId1.value()}));
  }
}

TEST(InMemoryGraphViewTest, requests_to_vertex_of_given_edge) {
  auto graph = InMemoryGraph{
      {}, {{EdgeIdLabel{"e/0"}, {VertexIdLabel{"v/0"}, VertexIdLabel{"v/1"}}}}};
  auto view = InMemoryGraphView{graph};
  auto edge = view.queryEdge(EdgeIdLabel{"e/0"});
  EXPECT_TRUE(edge.has_value());
  auto toVertex = view.getToVertex(edge.value());
  EXPECT_EQ(toVertex, view.ensureVertex(VertexIdLabel{"v/1"}));
}

}  // namespace arangodb::graph::experimental
