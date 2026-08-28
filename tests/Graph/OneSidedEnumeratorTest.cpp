#include <gtest/gtest.h>
#include <velocypack/HashedStringRef.h>
#include "Graph/Enumerators/SingleServerTraversalEnumerator.h"
#include "Graph/PathManagement/SingleServerPathResult.h"
#include "Graph/Types/VertexRef.h"

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::graph::experimental;

// auto graph = Graph({{"v/0", "v/0"}, {"v/0", "v/1"}, {"v/1", "v/2"}});
// inteface: graph.neighbours("v/1", filterExpression);
// auto enumerator = SingleServerTraversalEnumerator(graph, start, order,
// vertexUniqueness, edgeUniqueness, minDepth, maxDepth, globalVertexFilter,
// perDepthVertexFilter, edgeFilter, prune);

TEST(OneSidedEnumeratorTest, is_done_before_setting_start_vertex) {
  auto enumerator = SingleServerTraversalEnumerator();
  EXPECT_TRUE(enumerator.isDone());
}

TEST(OneSidedEnumeratorTest, is_not_done_before_querying) {
  auto enumerator = SingleServerTraversalEnumerator();
  auto start = std::string{"v/0"};
  enumerator.reset(graph::VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});
  EXPECT_FALSE(enumerator.isDone());
}

TEST(OneSidedEnumeratorTest, fails_when_not_reset_after_construction) {
  auto enumerator = SingleServerTraversalEnumerator();

  // at the moment it cannot be communicated (other than by an exception) that
  // the traversal enumerator was not reset
  // so it returns that it has no path and that it is done.
  EXPECT_EQ(enumerator.getNextPath(), nullptr);
  EXPECT_TRUE(enumerator.isDone());
}

TEST(OneSidedEnumeratorTest, is_done_after_querying_sole_vertex) {
  auto enumerator = SingleServerTraversalEnumerator();
  auto start = std::string{"v/0"};
  enumerator.reset(VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});

  auto nextPath = enumerator.getNextPath();

  EXPECT_TRUE(enumerator.isDone());
}

TEST(OneSidedEnumeratorTest, querying_single_vertex_not_contained_in_graph) {
  auto enumerator = SingleServerTraversalEnumerator();
  auto start = std::string{"v/0"};
  enumerator.reset(VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});

  auto nextPath = enumerator.getNextPath();

  EXPECT_NE(nextPath, nullptr);
  // if a vertex document for a vertex id does not exist in the database this is
  // fine in arangodb, the query will register a warning about the non-existing
  // document and return null for the vertex document
  auto expected = SingleServerPathResult{{std::nullopt}, {}};
  {
    velocypack::Builder b;
    nextPath->toVelocyPack(b);
    velocypack::Builder b_expected;
    expected.toVelocyPack(b_expected);
    ASSERT_TRUE(arangodb::basics::VelocyPackHelper::equal(
        b.slice(), b_expected.slice(), true));
  }
}

TEST(OneSidedEnumeratorTest, querying_path_of_length_one) {
  // TODO create graph
  // auto graph = Graph({"v/0", "v/1"}, {{"v/0", "v/1"}});
  // TODO give graph to enumerator
  auto enumerator = SingleServerTraversalEnumerator();
  auto start = std::string{"v/0"};
  enumerator.reset(graph::VertexRef{velocypack::HashedStringRef{
      start.c_str(), static_cast<uint32_t>(start.length())}});

  {
    auto nextPath = enumerator.getNextPath();
    EXPECT_NE(nextPath, nullptr);
    // TODO make this work
    // ASSERT_EQ((static_cast<const SingleServerPathResult&>(*nextPath)),
    //           (SingleServerPathResult{{"v/0", "v/1"},
    //                                   {Edge{._from = "v/0", ._to =
    //                                   "v/1"}}}));

    // TODO make this work
    // TODO really false? or directly true here?
    // EXPECT_FALSE(enumerator.isDone());
  }

  // TODO make this work
  // {
  //   auto nextPath = enumerator.getNextPath();
  //   EXPECT_EQ(nextPath, nullptr);
  //   EXPECT_TRUE(enumerator.isDone());
  // }
}
