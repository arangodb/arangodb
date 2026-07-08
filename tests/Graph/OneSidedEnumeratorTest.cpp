#include <gtest/gtest.h>

TEST(OneSidedEnumeratorTest, bla) {
  auto graph = Graph({{"v/0", "v/0"}, {"v/0", "v/1"}, {"v/1", "v/2"}});
  // inteface: graph.neighbours("v/1", filterExpression);
  auto enumerator = OneSidedEnumerator(
      graph, start, order, vertexUniqueness, edgeUniqueness, minDepth, maxDepth,
      globalVertexFilter, perDepthVertexFilter, edgeFilter, prune);
  auto nextPath = enumerator.getNextPath();
}
