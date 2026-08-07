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

TEST(OneSidedEnumeratorTest, is_done_after_querying_sole_vertex) {
  // auto graph = Graph({{"v/0", "v/0"}, {"v/0", "v/1"}, {"v/1", "v/2"}});
  // inteface: graph.neighbours("v/1", filterExpression);
  auto enumerator = SingleServerTraversalEnumerator();
  // graph, start, order, vertexUniqueness, edgeUniqueness, minDepth, maxDepth,
  // globalVertexFilter, perDepthVertexFilter, edgeFilter, prune);
  // enumerator.reset(vertex);
  auto nextPath = enumerator.getNextPath();
  EXPECT_EQ(nextPath, nullptr);  // TODO
  EXPECT_TRUE(enumerator.isDone());
}
