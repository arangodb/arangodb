////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "./MockGraph.h"
#include "./MockGraphProvider.h"

// Used for StringUtils size_t variant
#include "Basics/operating-system.h"

#include "../Mocks/Servers.h"

#include "Aql/Query.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Graph/Enumerators/TwoSidedEnumerator.h"
#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathStore.h"
#include "Graph/Queues/FifoQueue.h"

// Needed in case of enabled tracing
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/Queues/QueueTracer.h"
#include "Graph/algorithm-aliases.h"

#include <velocypack/HashedStringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

class KPathFinderTest
    : public ::testing::TestWithParam<MockGraphProvider::LooseEndBehaviour> {
  using KPathFinder = KPathEnumerator<MockGraphProvider>;
  // using KPathFinder = TracedKPathEnumerator<MockGraphProvider>;

 protected:
  bool activateLogging{false};
  MockGraph mockGraph;
  mocks::MockAqlServer _server{true};
  std::unique_ptr<arangodb::aql::Query> _query{_server.createFakeQuery()};
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor resourceMonitor{global};

  KPathFinderTest() {
    if (activateLogging) {
      Logger::GRAPHS.setLogLevel(LogLevel::TRACE);
    }

    /* a chain 1->2->3->4 */
    mockGraph.addEdge(1, 2);
    mockGraph.addEdge(2, 3);
    mockGraph.addEdge(3, 4);

    /* a diamond 5->6|7|8->9 */
    mockGraph.addEdge(5, 6);
    mockGraph.addEdge(5, 7);
    mockGraph.addEdge(5, 8);
    mockGraph.addEdge(6, 9);
    mockGraph.addEdge(7, 9);
    mockGraph.addEdge(8, 9);

    /* many path lengths */
    mockGraph.addEdge(10, 11);
    mockGraph.addEdge(10, 12);
    mockGraph.addEdge(12, 11);
    mockGraph.addEdge(12, 13);
    mockGraph.addEdge(13, 11);
    mockGraph.addEdge(13, 14);
    mockGraph.addEdge(14, 11);

    /* loop path */
    mockGraph.addEdge(20, 21);
    mockGraph.addEdge(21, 20);
    mockGraph.addEdge(21, 21);
    mockGraph.addEdge(21, 22);

    /* triangle loop */
    mockGraph.addEdge(30, 31);
    mockGraph.addEdge(31, 32);
    mockGraph.addEdge(32, 33);
    mockGraph.addEdge(33, 31);
    mockGraph.addEdge(32, 34);

    /* many neighbors at source (35 -> 40) */
    /* neighbors at start loop back to start */
    mockGraph.addEdge(35, 36);
    mockGraph.addEdge(36, 37);
    mockGraph.addEdge(37, 38);
    mockGraph.addEdge(38, 39);
    mockGraph.addEdge(39, 40);
    mockGraph.addEdge(35, 41);
    mockGraph.addEdge(35, 42);
    mockGraph.addEdge(35, 43);
    mockGraph.addEdge(35, 44);
    mockGraph.addEdge(35, 45);
    mockGraph.addEdge(35, 46);
    mockGraph.addEdge(35, 47);
    mockGraph.addEdge(41, 35);
    mockGraph.addEdge(42, 35);
    mockGraph.addEdge(43, 35);
    mockGraph.addEdge(44, 35);
    mockGraph.addEdge(45, 35);
    mockGraph.addEdge(46, 35);
    mockGraph.addEdge(47, 35);

    /* many neighbors at target (48 -> 53) */
    /* neighbors at target loop back to target */
    mockGraph.addEdge(48, 49);
    mockGraph.addEdge(49, 50);
    mockGraph.addEdge(50, 51);
    mockGraph.addEdge(51, 52);
    mockGraph.addEdge(52, 53);
    mockGraph.addEdge(54, 53);
    mockGraph.addEdge(55, 53);
    mockGraph.addEdge(56, 53);
    mockGraph.addEdge(57, 53);
    mockGraph.addEdge(58, 53);
    mockGraph.addEdge(59, 53);
    mockGraph.addEdge(53, 52);
    mockGraph.addEdge(53, 54);
    mockGraph.addEdge(53, 55);
    mockGraph.addEdge(53, 56);
    mockGraph.addEdge(53, 57);
    mockGraph.addEdge(53, 58);
    mockGraph.addEdge(53, 59);
  }

  auto looseEndBehaviour() const -> MockGraphProvider::LooseEndBehaviour {
    return GetParam();
  }

  auto pathFinder(size_t minDepth, size_t maxDepth) -> KPathFinder {
    arangodb::graph::TwoSidedEnumeratorOptions options{minDepth, maxDepth};
    return KPathFinder{MockGraphProvider(mockGraph, *_query.get(), looseEndBehaviour(), false),
                       MockGraphProvider(mockGraph, *_query.get(), looseEndBehaviour(), true),
                       std::move(options), resourceMonitor};
  }

  auto vId(size_t nr) -> std::string {
    return "v/" + basics::StringUtils::itoa(nr);
  }

  auto pathStructureValid(VPackSlice path, size_t depth) -> void {
    ASSERT_TRUE(path.isObject());
    {
      // Check Vertices
      ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryVertices));
      auto vertices = path.get(StaticStrings::GraphQueryVertices);
      ASSERT_TRUE(vertices.isArray());
      ASSERT_EQ(vertices.length(), depth + 1);
      for (auto const& v : VPackArrayIterator(vertices)) {
        EXPECT_TRUE(v.isObject());
      }
    }
    {
      // Check Edges
      ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryEdges));
      auto edges = path.get(StaticStrings::GraphQueryEdges);
      ASSERT_TRUE(edges.isArray());
      ASSERT_EQ(edges.length(), depth);
      for (auto const& e : VPackArrayIterator(edges)) {
        EXPECT_TRUE(e.isObject());
      }
    }
  }

  auto verticesToString(VPackSlice path) -> std::string {
    TRI_ASSERT(path.isObject());
    TRI_ASSERT(path.hasKey(StaticStrings::GraphQueryVertices));
    auto vertices = path.get(StaticStrings::GraphQueryVertices);

    std::string res;
    for (auto const& v : VPackArrayIterator(vertices)) {
      res += v.get(StaticStrings::KeyString).copyString();
    }
    return res;
  }

  auto edgesToString(VPackSlice path) -> std::string {
    TRI_ASSERT(path.isObject());
    TRI_ASSERT(path.hasKey(StaticStrings::GraphQueryEdges));
    auto edges = path.get(StaticStrings::GraphQueryEdges);

    std::string res;
    for (auto const& e : VPackArrayIterator(edges)) {
      res += e.get(StaticStrings::KeyString).copyString();
    }
    return res;
  }

  auto pathEquals(VPackSlice path, std::vector<size_t> const& vertexIds) -> void {
    ASSERT_TRUE(path.isObject());
    ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryVertices));
    auto vertices = path.get(StaticStrings::GraphQueryVertices);
    size_t i = 0;
    ASSERT_EQ(vertices.length(), vertexIds.size());

    for (auto const& v : VPackArrayIterator(vertices)) {
      auto key = v.get(StaticStrings::KeyString);
      EXPECT_TRUE(key.isEqualString(basics::StringUtils::itoa(vertexIds[i])))
          << key.toJson() << " does not match " << vertexIds[i]
          << " at position: " << i;
      ++i;
    }
  }

  auto toHashedStringRef(std::string const& id) -> HashedStringRef {
    return HashedStringRef(id.data(), static_cast<uint32_t>(id.length()));
  }
};

INSTANTIATE_TEST_CASE_P(KPathFinderTestRunner, KPathFinderTest,
                        ::testing::Values(MockGraphProvider::LooseEndBehaviour::NEVER,
                                          MockGraphProvider::LooseEndBehaviour::ALWAYS));

TEST_P(KPathFinderTest, no_path_exists) {
  VPackBuilder result;
  // No path between those
  auto source = vId(91);
  auto target = vId(99);
  auto finder = pathFinder(1, 1);
  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
  {
    aql::TraversalStats stats = finder.stealStats();
    EXPECT_EQ(stats.getScannedIndex(), 0);
  }
}

TEST_P(KPathFinderTest, path_depth_0) {
  VPackBuilder result;
  // Search 0 depth
  auto finder = pathFinder(0, 0);

  // Source and target identical
  auto source = vId(91);
  auto target = vId(91);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 0);
    pathEquals(result.slice(), {91});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup the vertex
    EXPECT_EQ(stats.getScannedIndex(), 1);
  }

  {
    // Make sure stats are stolen and resettet
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup the vertex
    EXPECT_EQ(stats.getScannedIndex(), 0);
  }
}

TEST_P(KPathFinderTest, path_depth_1) {
  VPackBuilder result;
  auto finder = pathFinder(1, 1);

  // Source and target are direct neighbors, there is only one path between them
  auto source = vId(1);
  auto target = vId(2);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 1);
    pathEquals(result.slice(), {1, 2});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup both vertices, and the edge
    EXPECT_EQ(stats.getScannedIndex(), 3);
  }
}

TEST_P(KPathFinderTest, path_depth_2) {
  VPackBuilder result;
  auto finder = pathFinder(2, 2);

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(1);
  auto target = vId(3);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);
    pathEquals(result.slice(), {1, 2, 3});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup 3 vertices + 2 edges
    EXPECT_EQ(stats.getScannedIndex(), 5);
  }
}

TEST_P(KPathFinderTest, path_depth_3) {
  VPackBuilder result;
  // Search 0 depth
  auto finder = pathFinder(3, 3);

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(1);
  auto target = vId(4);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 3);
    pathEquals(result.slice(), {1, 2, 3, 4});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup 4 vertices + 3 edges
    EXPECT_EQ(stats.getScannedIndex(), 7);
  }
}

TEST_P(KPathFinderTest, path_diamond) {
  VPackBuilder result;
  // Search 0 depth
  auto finder = pathFinder(2, 2);

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(5);
  auto target = vId(9);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);

    EXPECT_FALSE(finder.isDone());
  }
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);

    EXPECT_FALSE(finder.isDone());
  }
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
  {
    aql::TraversalStats stats = finder.stealStats();
    // We have 3 paths.
    // Each path has 3 vertices + 2 edges to lookup
    EXPECT_EQ(stats.getScannedIndex(), 15);
  }
}

TEST_P(KPathFinderTest, path_depth_1_to_2) {
  VPackBuilder result;
  auto finder = pathFinder(1, 2);

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(10);
  auto target = vId(11);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 1);
    pathEquals(result.slice(), {10, 11});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);
    pathEquals(result.slice(), {10, 12, 11});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_P(KPathFinderTest, path_depth_2_to_3) {
  VPackBuilder result;
  auto finder = pathFinder(2, 3);

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(10);
  auto target = vId(11);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);
    pathEquals(result.slice(), {10, 12, 11});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 3);
    pathEquals(result.slice(), {10, 12, 13, 11});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_P(KPathFinderTest, path_depth_2_to_3_skip) {
  VPackBuilder result;
  auto finder = pathFinder(2, 3);

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(10);
  auto target = vId(11);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    // Skip one path.
    // We still have another one
    result.clear();
    auto skipped = finder.skipPath();
    EXPECT_TRUE(skipped);
    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 3);
    pathEquals(result.slice(), {10, 12, 13, 11});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
}
TEST_P(KPathFinderTest, path_loop) {
  VPackBuilder result;
  auto finder = pathFinder(1, 10);

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(20);
  auto target = vId(22);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);
    pathEquals(result.slice(), {20, 21, 22});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_P(KPathFinderTest, triangle_loop) {
  VPackBuilder result;
  auto finder = pathFinder(1, 10);

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(30);
  auto target = vId(34);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 3);
    pathEquals(result.slice(), {30, 31, 32, 34});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_P(KPathFinderTest, triangle_loop_skip) {
  auto finder = pathFinder(1, 10);

  // Source and target are direkt neighbors, there is only one path between them
  auto source = vId(30);
  auto target = vId(34);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    auto skippedPath = finder.skipPath();
    EXPECT_TRUE(skippedPath);

    EXPECT_FALSE(finder.isDone());
  }

  {
    // Try to skip again to make sure we are not looping here
    auto skippedPath = finder.skipPath();
    EXPECT_FALSE(skippedPath);
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_P(KPathFinderTest, many_neighbours_source) {
  VPackBuilder result;
  auto finder = pathFinder(1, 10);

  // source has a lot of neighbors, it is better to start at target
  auto source = vId(35);
  auto target = vId(40);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 5);
    pathEquals(result.slice(), {35, 36, 37, 38, 39, 40});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_P(KPathFinderTest, many_neighbours_target) {
  VPackBuilder result;
  auto finder = pathFinder(1, 10);

  // target has a lot of neighbors, it is better to start at source
  auto source = vId(48);
  auto target = vId(53);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 5);
    pathEquals(result.slice(), {48, 49, 50, 51, 52, 53});

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
}

}  // namespace graph
}  // namespace tests
}  // namespace arangodb
