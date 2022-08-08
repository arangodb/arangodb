////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Anthony Mahanna
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

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

class AllShortestPathsFinderTest
    : public ::testing::TestWithParam<MockGraphProvider::LooseEndBehaviour> {
  using AllShortestPathsFinder = AllShortestPathsEnumerator<MockGraphProvider>;

 protected:
  bool activateLogging{false};
  MockGraph mockGraph;
  mocks::MockAqlServer _server{true};
  std::shared_ptr<arangodb::aql::Query> _query{_server.createFakeQuery()};
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor resourceMonitor{global};

  // PathValidatorOptions parts (used for API not under test here)
  aql::Variable _tmpVar{"tmp", 0, false};
  arangodb::aql::AqlFunctionsInternalCache _functionsCache{};

  arangodb::transaction::Methods _trx{_query->newTrxContext()};
  arangodb::aql::FixedVarExpressionContext _expressionContext{
      _trx, *_query.get(), _functionsCache};
  AllShortestPathsFinderTest() {
    if (activateLogging) {
      Logger::GRAPHS.setLogLevel(LogLevel::TRACE);
    }

    /* a chain 1->2->3->4->5 with shortcuts */
    mockGraph.addEdge(1, 2);
    mockGraph.addEdge(2, 3);
    mockGraph.addEdge(3, 4);
    mockGraph.addEdge(4, 5);
    mockGraph.addEdge(1, 3);
    mockGraph.addEdge(1, 4);
    mockGraph.addEdge(3, 5);

    /* a hexagon 6->7->8->9->10->11->6 */
    mockGraph.addEdge(6, 7);
    mockGraph.addEdge(7, 8);
    mockGraph.addEdge(8, 9);
    mockGraph.addEdge(9, 10);
    mockGraph.addEdge(10, 11);
    mockGraph.addEdge(11, 6);
    mockGraph.addEdge(6, 11);

    /* a balanced binary tree 12 -> [13, 14] -> [15, 16, 17, 18] */
    mockGraph.addEdge(12, 13);
    mockGraph.addEdge(12, 14);
    mockGraph.addEdge(13, 15);
    mockGraph.addEdge(13, 16);
    mockGraph.addEdge(14, 17);
    mockGraph.addEdge(14, 18);
    mockGraph.addEdge(15, 13);
    mockGraph.addEdge(13, 12);

    /* another balanced binary tree */
    mockGraph.addEdge(20, 21);
    mockGraph.addEdge(20, 22);
    mockGraph.addEdge(21, 23);
    mockGraph.addEdge(21, 24);
    mockGraph.addEdge(22, 25);
    mockGraph.addEdge(22, 26);

    /* connect the two binary trees together */
    mockGraph.addEdge(12, 19);
    mockGraph.addEdge(19, 20);

    /* a 3x3 grid */
    mockGraph.addEdge(27, 28);
    mockGraph.addEdge(28, 29);
    mockGraph.addEdge(30, 31);
    mockGraph.addEdge(31, 32);
    mockGraph.addEdge(33, 34);
    mockGraph.addEdge(34, 35);

    mockGraph.addEdge(27, 30);
    mockGraph.addEdge(30, 33);
    mockGraph.addEdge(28, 31);
    mockGraph.addEdge(31, 34);
    mockGraph.addEdge(29, 32);
    mockGraph.addEdge(32, 35);

    /* multiple edges in between two vertices */
    mockGraph.addEdge(36, 37);
    mockGraph.addEdge(36, 37);
  }

  auto looseEndBehaviour() const -> MockGraphProvider::LooseEndBehaviour {
    return GetParam();
  }

  auto pathFinder(size_t minDepth, size_t maxDepth) -> AllShortestPathsFinder {
    arangodb::graph::TwoSidedEnumeratorOptions options{minDepth, maxDepth};
    options.setStopAtFirstDepth(true);
    PathValidatorOptions validatorOpts{&_tmpVar, _expressionContext};
    return AllShortestPathsFinder{
        MockGraphProvider(
            *_query.get(),
            MockGraphProviderOptions{mockGraph, looseEndBehaviour(), false},
            resourceMonitor),
        MockGraphProvider(
            *_query.get(),
            MockGraphProviderOptions{mockGraph, looseEndBehaviour(), true},
            resourceMonitor),
        std::move(options), std::move(validatorOpts), resourceMonitor};
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

  auto pathIsIn(VPackSlice path,
                std::vector<std::vector<size_t>> const& vertexIdsList) -> void {
    ASSERT_TRUE(path.isObject());
    ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryVertices));

    bool pathIsInList = false;
    for (auto const& vertexIds : vertexIdsList) {
      if (pathIsInList) {
        break;
      }
      auto vertices = path.get(StaticStrings::GraphQueryVertices);
      ASSERT_EQ(vertices.length(), vertexIds.size());
      pathIsInList = pathEquals(vertices, vertexIds);
    }
    EXPECT_TRUE(pathIsInList)
        << "Path not found in 'vertexIdsList': " << path.toJson();
  }

  auto pathEquals(Slice vertices, std::vector<size_t> const& vertexIds)
      -> bool {
    size_t i = 0;
    bool isEqual = false;
    for (auto const& v : VPackArrayIterator(vertices)) {
      auto key = v.get(StaticStrings::KeyString);
      isEqual = key.isEqualString(basics::StringUtils::itoa(vertexIds[i]));
      if (!isEqual) {
        break;
      }

      ++i;
    }

    return isEqual;
  }

  auto toHashedStringRef(std::string const& id) -> HashedStringRef {
    return HashedStringRef(id.data(), static_cast<uint32_t>(id.length()));
  }
};

INSTANTIATE_TEST_CASE_P(
    AllShortestPathsFinderTestRunner, AllShortestPathsFinderTest,
    ::testing::Values(MockGraphProvider::LooseEndBehaviour::NEVER,
                      MockGraphProvider::LooseEndBehaviour::ALWAYS));

TEST_P(AllShortestPathsFinderTest, no_path_exists) {
  VPackBuilder result;
  // No path between those
  auto source = vId(99);
  auto target = vId(100);
  auto finder = pathFinder(0, 1000);
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

TEST_P(AllShortestPathsFinderTest, path_depth_0) {
  VPackBuilder result;
  // Search 0 depth
  auto finder = pathFinder(0, 0);

  // Source and target identical
  auto source = vId(1);
  auto target = vId(1);

  const std::vector<std::vector<size_t>> vertexIdsList{{1}};

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 0);
    pathIsIn(result.slice(), vertexIdsList);

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

TEST_P(AllShortestPathsFinderTest, shortcut_paths) {
  VPackBuilder result;
  auto finder = pathFinder(0, 1000);

  // Source and target are direct neighbors, there is only one path between them
  auto source = vId(1);
  auto target = vId(5);

  const std::vector<std::vector<size_t>> vertexIdsList{{1, 3, 5}, {1, 4, 5}};

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath) << result.slice().toJson();

    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup both vertices, and the edge
    EXPECT_EQ(stats.getScannedIndex(), 11);
  }
}

TEST_P(AllShortestPathsFinderTest, hexagon_path) {
  VPackBuilder result;
  auto finder = pathFinder(0, 1000);

  // Source and target are direct neighbors via a hexagon-shaped loop
  auto source = vId(6);
  auto target = vId(11);

  const std::vector<std::vector<size_t>> vertexIdsList{{6, 11}};

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 1);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath) << result.slice().toJson();

    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup both vertices, and the edge
    EXPECT_EQ(stats.getScannedIndex(), 4);
  }
}

TEST_P(AllShortestPathsFinderTest, binary_tree) {
  VPackBuilder result;
  auto finder = pathFinder(0, 1000);

  // Source and target are leaves on each side of the bt root
  auto source = vId(15);
  auto target = vId(18);

  const std::vector<std::vector<size_t>> vertexIdsList{{15, 13, 12, 14, 18}};

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath) << result.slice().toJson();

    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup both vertices, and the edge
    EXPECT_EQ(stats.getScannedIndex(), 11);
  }
}

TEST_P(AllShortestPathsFinderTest, binary_trees_connected) {
  VPackBuilder result;
  auto finder = pathFinder(0, 1000);

  // Source and target are the roots of each binary tree
  auto source = vId(12);
  auto target = vId(20);

  const std::vector<std::vector<size_t>> vertexIdsList{{12, 19, 20}};

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 2);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath) << result.slice().toJson();

    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup both vertices, and the edge
    EXPECT_EQ(stats.getScannedIndex(), 7);
  }
}

TEST_P(AllShortestPathsFinderTest, grid_paths) {
  VPackBuilder result;
  auto finder = pathFinder(0, 1000);

  // Source and target are in a 3x3 grid with multiple shortest paths
  auto source = vId(27);
  auto target = vId(35);

  const std::vector<std::vector<size_t>> vertexIdsList{
      {27, 28, 29, 32, 35}, {27, 28, 31, 32, 35}, {27, 28, 31, 34, 35},
      {27, 30, 31, 32, 35}, {27, 30, 31, 34, 35}, {27, 30, 33, 34, 35}};

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath) << result.slice().toJson();

    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup both vertices, and the edge
    EXPECT_EQ(stats.getScannedIndex(), 42);
  }
}

TEST_P(AllShortestPathsFinderTest, multiple_edges_between_pair) {
  VPackBuilder result;
  auto finder = pathFinder(0, 1000);

  // Source and target have two edges in between each other
  auto source = vId(36);
  auto target = vId(37);

  const std::vector<std::vector<size_t>> vertexIdsList{{36, 37}};

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 1);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 1);
    pathIsIn(result.slice(), vertexIdsList);

    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    // Try again to make sure we stay at non-existing
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath) << result.slice().toJson();

    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }

  {
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup both vertices, and the edge
    EXPECT_EQ(stats.getScannedIndex(), 6);
  }
}

}  // namespace graph
}  // namespace tests
}  // namespace arangodb
