#include <gtest/gtest.h>
#include <velocypack/HashedStringRef.h>
#include "Graph/SimplifiedTraversal/IGraphView.h"
#include "Graph/SimplifiedTraversal/InMemoryGraph.h"
#include "Graph/SimplifiedTraversal/SingleServerTraversalEnumerator.h"
#include "Graph/SimplifiedTraversal/SingleServerPathResult.h"
#include "Graph/Types/VertexRef.h"

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::graph::experimental;

// auto graph = Graph({{"v/0", "v/0"}, {"v/0", "v/1"}, {"v/1", "v/2"}});
// inteface: graph.neighbours("v/1", filterExpression);
// auto enumerator = SingleServerTraversalEnumerator(graph, start, order,
// vertexUniqueness, edgeUniqueness, minDepth, maxDepth, globalVertexFilter,
// perDepthVertexFilter, edgeFilter, prune);

auto assertEqual(IPathResult& a, IPathResult& b) {
  velocypack::Builder builder_a;
  a.toVelocyPack(builder_a);
  velocypack::Builder builder_b;
  b.toVelocyPack(builder_b);
  ASSERT_TRUE(arangodb::basics::VelocyPackHelper::equal(
      builder_a.slice(), builder_b.slice(), true));
}

TEST(SingleServerTraversalEnumeratorTest, is_done_before_setting_start_vertex) {
  auto graph = experimental::InMemoryGraphView();
  auto enumerator = SingleServerTraversalEnumerator(graph);
  EXPECT_TRUE(enumerator.isDone());
}

TEST(SingleServerTraversalEnumeratorTest, is_not_done_before_querying) {
  auto graph = experimental::InMemoryGraphView();
  auto enumerator = SingleServerTraversalEnumerator(graph);
  auto start = std::string{"v/0"};
  enumerator.reset(graph::VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});
  EXPECT_FALSE(enumerator.isDone());
}

TEST(SingleServerTraversalEnumeratorTest,
     fails_when_not_reset_after_construction) {
  auto graph = experimental::InMemoryGraphView();
  auto enumerator = SingleServerTraversalEnumerator(graph);

  // at the moment it cannot be communicated (other than by an exception) that
  // the traversal enumerator was not reset
  // so it returns that it has no path and that it is done.
  EXPECT_EQ(enumerator.getNextPath(), nullptr);
  EXPECT_TRUE(enumerator.isDone());
}

TEST(SingleServerTraversalEnumeratorTest, is_done_after_querying_sole_vertex) {
  auto graph = experimental::InMemoryGraphView();
  auto enumerator = SingleServerTraversalEnumerator(graph);
  auto start = std::string{"v/0"};
  enumerator.reset(VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});

  auto nextPath = enumerator.getNextPath();

  EXPECT_TRUE(enumerator.isDone());
}

TEST(SingleServerTraversalEnumeratorTest,
     querying_single_vertex_not_contained_in_graph) {
  auto graph = experimental::InMemoryGraphView();
  auto enumerator = SingleServerTraversalEnumerator(graph);
  auto start = std::string{"v/0"};
  enumerator.reset(VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});
  auto vertexId = graph.queryVertex(VertexIdLabel{start});
  EXPECT_TRUE(vertexId.has_value());

  auto nextPath = enumerator.getNextPath();

  EXPECT_NE(nextPath, nullptr);
  // if a vertex document for a vertex id does not exist in the database this is
  // fine in arangodb, the query will register a warning about the non-existing
  // document and return null for the vertex document
  auto expected = SingleServerPathResult{{vertexId.value()}, {}};
  assertEqual(*nextPath, expected);
}

// is currently irrelevant because only toVelocypack on path result should read
// from real graph TEST(SingleServerTraversalEnumeratorTest,
//      querying_single_vertex_contained_in_graph) {
//   // auto v0 = std::string{"v/0"};
//   // auto v0Ref = VertexRef{velocypack::HashedStringRef{
//   //     v0.c_str(), static_cast<uint32_t>(v0.length())}};
//   auto graph = experimental::InMemoryGraphView({"v/0"}, {});
//   auto enumerator = SingleServerTraversalEnumerator(graph);
//   auto start = std::string{"v/0"};
//   enumerator.reset(start);
//   auto vertexId = graph.queryVertex(VertexIdLabel{start});
//   EXPECT_TRUE(vertexId.has_value());

//   auto nextPath = enumerator.getNextPath();

//   EXPECT_NE(nextPath, nullptr);
//   auto expected = SingleServerPathResult{{vertexId.value()}, {}};
//   assertEqual(*nextPath, expected);
// }

TEST(SingleServerTraversalEnumeratorTest, querying_path_of_length_one) {
  auto v0 = std::string{"v/0"};
  auto v1 = std::string{"v/1"};
  auto graph = experimental::InMemoryGraph{
      {VertexIdLabel{v0}, VertexIdLabel{v1}},
      {{EdgeIdLabel{"e/0"}, {VertexIdLabel{v0}, VertexIdLabel{v1}}}}};
  auto graphView = experimental::InMemoryGraphView(graph);
  auto enumerator = SingleServerTraversalEnumerator(graphView);
  enumerator.reset(VertexRef{velocypack::HashedStringRef{
      v0.c_str(), static_cast<uint32_t>(v0.length())}});
  auto vertexIdv0 = graphView.ensureVertex(VertexIdLabel{v0});
  auto vertexIdv1 = graphView.ensureVertex(VertexIdLabel{v1});
  auto edgeId = graphView.queryEdge(vertexIdv0, vertexIdv1);
  EXPECT_TRUE(edgeId.has_value());

  {  // path of lenght 0
    auto nextPath = enumerator.getNextPath();
    EXPECT_NE(nextPath, nullptr);
    auto expected = SingleServerPathResult{{vertexIdv0}, {}};
    assertEqual(*nextPath, expected);
    EXPECT_FALSE(enumerator.isDone());
  }
  {  // path of length 1
    auto nextPath = enumerator.getNextPath();
    EXPECT_NE(nextPath, nullptr);
    auto expected =
        SingleServerPathResult{{vertexIdv0, vertexIdv1}, {edgeId.value()}};
    assertEqual(*nextPath, expected);
    EXPECT_TRUE(enumerator.isDone());
  }
}

TEST(SingleServerTraversalEnumeratorTest, querying_two_paths_of_length_one) {
  auto v0 = std::string{"v/0"};
  auto v1 = std::string{"v/1"};
  auto v2 = std::string{"v/2"};
  auto graph = experimental::InMemoryGraph{
      {VertexIdLabel{v0}, VertexIdLabel{v1}, VertexIdLabel{v2}},
      {{EdgeIdLabel{"e/0"}, {VertexIdLabel{v0}, VertexIdLabel{v1}}},
       {EdgeIdLabel{"e/1"}, {VertexIdLabel{v0}, VertexIdLabel{v2}}}}};
  auto graphView = experimental::InMemoryGraphView(graph);
  auto enumerator = SingleServerTraversalEnumerator(graphView);
  enumerator.reset(VertexRef{velocypack::HashedStringRef{
      v0.c_str(), static_cast<uint32_t>(v0.length())}});
  auto vertexIdv0 = graphView.ensureVertex(VertexIdLabel{v0});
  auto vertexIdv1 = graphView.ensureVertex(VertexIdLabel{v1});
  auto vertexIdv2 = graphView.ensureVertex(VertexIdLabel{v1});
  auto edgeId = graphView.queryEdge(vertexIdv0, vertexIdv1);
  EXPECT_TRUE(edgeId.has_value());

  {  // path of lenght 0
    auto nextPath = enumerator.getNextPath();
    EXPECT_NE(nextPath, nullptr);
    auto expected = SingleServerPathResult{{vertexIdv0}, {}};
    assertEqual(*nextPath, expected);
    EXPECT_FALSE(enumerator.isDone());
  }
  {  // path of length 1
    auto nextPath = enumerator.getNextPath();
    EXPECT_NE(nextPath, nullptr);
    auto expected =
        SingleServerPathResult{{vertexIdv0, vertexIdv1}, {edgeId.value()}};
    assertEqual(*nextPath, expected);
    EXPECT_FALSE(enumerator.isDone());
  }
  {  // path of length 1
    auto nextPath = enumerator.getNextPath();
    EXPECT_NE(nextPath, nullptr);
    auto expected =
        SingleServerPathResult{{vertexIdv0, vertexIdv2}, {edgeId.value()}};
    assertEqual(*nextPath, expected);
    EXPECT_TRUE(enumerator.isDone());
  }
}
