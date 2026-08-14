#include <gtest/gtest.h>
#include <velocypack/HashedStringRef.h>
#include "Graph/Enumerators/SingleServerTraversalEnumerator.h"
#include "Graph/Types/VertexRef.h"

using namespace arangodb;
using namespace arangodb::graph::experimental;

//
TEST(OneSidedEnumeratorTest, is_not_done_before_querying) {
  // auto graph = Graph({{"v/0", "v/0"}, {"v/0", "v/1"}, {"v/1", "v/2"}});
  // inteface: graph.neighbours("v/1", filterExpression);
  auto enumerator = SingleServerTraversalEnumerator();
  // graph, start, order, vertexUniqueness, edgeUniqueness, minDepth, maxDepth,
  // globalVertexFilter, perDepthVertexFilter, edgeFilter, prune);
  // enumerator.reset(vertex);
  EXPECT_FALSE(enumerator.isDone());
  auto start = std::string{"v/0"};
  enumerator.reset(graph::VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});
  EXPECT_FALSE(enumerator.isDone());
}

TEST(OneSidedEnumeratorTest, fails_when_not_reset_after_construction) {
  // auto graph = Graph({{"v/0", "v/0"}, {"v/0", "v/1"}, {"v/1", "v/2"}});
  // inteface: graph.neighbours("v/1", filterExpression);
  auto enumerator = SingleServerTraversalEnumerator();
  // graph, start, order, vertexUniqueness, edgeUniqueness, minDepth, maxDepth,
  // globalVertexFilter, perDepthVertexFilter, edgeFilter, prune);

  // do not call reset intentionally
  // enumerator.reset(vertex);
  auto nextPath = enumerator.getNextPath();

  // at the moment it cannot be communicated (other than by an exception) that
  // the traversal enumerator was not reset
  // so it returns that it has no path and that it is done.
  EXPECT_EQ(nextPath, nullptr);
  EXPECT_TRUE(enumerator.isDone());
}

// TODO: does enumerator check whether vertex is contained in graph?
TEST(OneSidedEnumeratorTest, is_done_after_querying_sole_vertex) {
  // auto graph = Graph({"v/0"});
  auto enumerator = SingleServerTraversalEnumerator();
  // graph, start, order, vertexUniqueness, edgeUniqueness, minDepth, maxDepth,
  // globalVertexFilter, perDepthVertexFilter, edgeFilter, prune);

  auto start = std::string{"v/0"};
  enumerator.reset(graph::VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});

  auto nextPath = enumerator.getNextPath();
  EXPECT_NE(nextPath, nullptr);

  // Check that path contains exactly "v/0"
  // can't look at the path?

  velocypack::Builder b;
  nextPath->lastVertexToVelocyPack(b);
  ASSERT_EQ(b.toString(), "v/0");

  EXPECT_TRUE(enumerator.isDone());
}

TEST(OneSidedEnumeratorTest, querying_single_vertex_not_contained_in_graph) {
  // auto graph = Graph({"v/0"});
  auto enumerator = SingleServerTraversalEnumerator();
  // graph, start, order, vertexUniqueness, edgeUniqueness, minDepth, maxDepth,
  // globalVertexFilter, perDepthVertexFilter, edgeFilter, prune);

  // Intentionally not contained in graph
  auto start = std::string{"v/1"};
  enumerator.reset(graph::VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});

  auto nextPath = enumerator.getNextPath();
  EXPECT_NE(nextPath, nullptr);

  // TODO: what is supposed to happen: if a vertex document for a vertex id does
  // not exist in the database this is fine in arangodb, the query will register
  // a warning about the non-existing document and return null for the vertex
  // document

  // TODO: this path object has to only contain the single vertex entry `null`
  velocypack::Builder b;
  nextPath->lastVertexToVelocyPack(b);
  ASSERT_EQ(b.slice().isNull(), true);

  EXPECT_TRUE(enumerator.isDone());
}

/// FOR start IN coll
///     FOR v,e,p IN 1..5 OUTBOUND start ...

TEST(OneSidedEnumeratorTest, querying_path_of_length_one) {
  // auto graph = Graph({"v/0", "v/1"}, {{"v/0", "v/1"}});
  auto enumerator = SingleServerTraversalEnumerator();
  // graph, start, order, vertexUniqueness, edgeUniqueness, minDepth, maxDepth,
  // globalVertexFilter, perDepthVertexFilter, edgeFilter, prune);

  // Intentionally not contained in graph
  auto start = std::string{"v/0"};
  enumerator.reset(graph::VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});

  {
    auto nextPath = enumerator.getNextPath();
    EXPECT_NE(nextPath, nullptr);
    EXPECT_FALSE(enumerator.isDone());

    // TODO: what is supposed to happen: if a vertex document for a vertex id
    // does not exist in the database this is fine in arangodb, the query will
    // register a warning about the non-existing document and return null for
    // the vertex document

    // TODO: this path object has to only contain the single vertex entry `null`
    velocypack::Builder b;
    nextPath->lastVertexToVelocyPack(b);
    ASSERT_EQ(b.slice().isNull(), true);
  }

  {
    auto nextPath = enumerator.getNextPath();
    EXPECT_TRUE(enumerator.isDone());

    // TODO: test path structure
  }
}
