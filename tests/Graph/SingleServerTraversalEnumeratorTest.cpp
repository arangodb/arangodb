#include <gtest/gtest.h>
#include <velocypack/HashedStringRef.h>
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
  auto graph = experimental::InMemoryGraph();
  auto enumerator = SingleServerTraversalEnumerator(graph);
  EXPECT_TRUE(enumerator.isDone());
}

TEST(SingleServerTraversalEnumeratorTest, is_not_done_before_querying) {
  auto graph = experimental::InMemoryGraph();
  auto enumerator = SingleServerTraversalEnumerator(graph);
  auto start = std::string{"v/0"};
  enumerator.reset(graph::VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});
  EXPECT_FALSE(enumerator.isDone());
}

TEST(SingleServerTraversalEnumeratorTest,
     fails_when_not_reset_after_construction) {
  auto graph = experimental::InMemoryGraph();
  auto enumerator = SingleServerTraversalEnumerator(graph);

  // at the moment it cannot be communicated (other than by an exception) that
  // the traversal enumerator was not reset
  // so it returns that it has no path and that it is done.
  EXPECT_EQ(enumerator.getNextPath(), nullptr);
  EXPECT_TRUE(enumerator.isDone());
}

TEST(SingleServerTraversalEnumeratorTest, is_done_after_querying_sole_vertex) {
  auto graph = experimental::InMemoryGraph();
  auto enumerator = SingleServerTraversalEnumerator(graph);
  auto start = std::string{"v/0"};
  enumerator.reset(VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});

  auto nextPath = enumerator.getNextPath();

  EXPECT_TRUE(enumerator.isDone());
}

TEST(SingleServerTraversalEnumeratorTest,
     querying_single_vertex_not_contained_in_graph) {
  auto graph = experimental::InMemoryGraph();
  auto enumerator = SingleServerTraversalEnumerator(graph);
  auto start = std::string{"v/0"};
  enumerator.reset(VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});

  auto nextPath = enumerator.getNextPath();

  EXPECT_NE(nextPath, nullptr);
  // if a vertex document for a vertex id does not exist in the database this is
  // fine in arangodb, the query will register a warning about the non-existing
  // document and return null for the vertex document
  auto expected = SingleServerPathResult{{std::nullopt}, {}};
  assertEqual(*nextPath, expected);
}

TEST(SingleServerTraversalEnumeratorTest,
     querying_single_vertex_contained_in_graph) {
  auto v0 = std::string{"v/0"};
  auto v0Ref = VertexRef{velocypack::HashedStringRef{
      v0.c_str(), static_cast<uint32_t>(v0.length())}};
  auto graph = experimental::InMemoryGraph({v0Ref}, {});
  auto enumerator = SingleServerTraversalEnumerator(graph);
  enumerator.reset(v0Ref);

  auto nextPath = enumerator.getNextPath();

  EXPECT_NE(nextPath, nullptr);
  auto expected = SingleServerPathResult{{v0Ref}, {}};
  assertEqual(*nextPath, expected);
}

TEST(SingleServerTraversalEnumeratorTest, querying_path_of_length_one) {
  auto v0 = std::string{"v/0"};
  auto v0Ref = VertexRef{velocypack::HashedStringRef{
      v0.c_str(), static_cast<uint32_t>(v0.length())}};
  auto v1 = std::string{"v/1"};
  auto v1Ref = VertexRef{velocypack::HashedStringRef{
      v1.c_str(), static_cast<uint32_t>(v1.length())}};
  auto graph = experimental::InMemoryGraph({v0Ref, v1Ref}, {{v0Ref, v1Ref}});
  auto enumerator = SingleServerTraversalEnumerator(graph);
  enumerator.reset(v0Ref);

  {
    auto nextPath = enumerator.getNextPath();
    EXPECT_NE(nextPath, nullptr);
    auto expected = SingleServerPathResult{{v0Ref, v1Ref},
                                           {Edge{.from = v0Ref, .to = v1Ref}}};
    // TODO make this work
    assertEqual(*nextPath, expected);

    // TODO make this work
    // TODO really false? or directly true here?
    EXPECT_FALSE(enumerator.isDone());
  }

  {
    auto nextPath = enumerator.getNextPath();
    // TODO make this work
    EXPECT_EQ(nextPath, nullptr);
    EXPECT_TRUE(enumerator.isDone());
  }
}
