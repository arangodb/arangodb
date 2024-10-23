////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/Variable.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Graph/Enumerators/WeightedTwoSidedEnumerator.h"
#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathStore.h"
#include "Graph/Queues/WeightedQueue.h"

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

class WeightedShortestPathTest
    : public ::testing::TestWithParam<MockGraphProvider::LooseEndBehaviour> {
  using WeightedShortestPathFinder =
      WeightedShortestPathEnumeratorAlias<MockGraphProvider>;

  static constexpr size_t minDepth = 0;
  static constexpr size_t maxDepth = std::numeric_limits<size_t>::max();

  /* "complex graph" */
 public:
  enum Vertices { A = 101, B = 102, C = 103, D = 104, E = 105, F = 106 };

 protected:
  bool activateLogging{false};
  MockGraph mockGraph;
  mocks::MockAqlServer _server{true};
  std::shared_ptr<arangodb::aql::Query> _query{_server.createFakeQuery()};
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor resourceMonitor{global};

  // PathValidatorOptions parts (used for API not under test here)
  aql::Variable _tmpVar{"tmp", 0, false, resourceMonitor};
  arangodb::aql::AqlFunctionsInternalCache _functionsCache{};

  arangodb::transaction::Methods _trx{_query->newTrxContext()};
  arangodb::aql::FixedVarExpressionContext _expressionContext{
      _trx, *_query.get(), _functionsCache};
  WeightedShortestPathTest() {
    if (activateLogging) {
      Logger::GRAPHS.setLogLevel(LogLevel::TRACE);
    }

    /* a chain 1->2->3->4 */
    mockGraph.addEdge(1, 2, 1);
    mockGraph.addEdge(2, 3, 1);
    mockGraph.addEdge(3, 4, 1);

    /* chain to diamond connection */
    mockGraph.addEdge(4, 5, 1);

    /* a diamond 5->6|7|8->9 */
    mockGraph.addEdge(5, 6, 3);
    mockGraph.addEdge(5, 7, 2);
    mockGraph.addEdge(5, 8, 1);
    mockGraph.addEdge(6, 9, 3);
    mockGraph.addEdge(7, 9, 2);
    mockGraph.addEdge(8, 9, 1);

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

    /* complex graph */
    mockGraph.addEdge(Vertices::A, Vertices::B, 4);
    mockGraph.addEdge(Vertices::A, Vertices::C, 2);
    mockGraph.addEdge(Vertices::B, Vertices::A, 2);
    mockGraph.addEdge(Vertices::B, Vertices::C, 5);
    mockGraph.addEdge(Vertices::B, Vertices::D, 10);
    mockGraph.addEdge(Vertices::C, Vertices::A, 2);
    mockGraph.addEdge(Vertices::C, Vertices::E, 3);
    mockGraph.addEdge(Vertices::D, Vertices::F, 11);
    mockGraph.addEdge(Vertices::E, Vertices::B, 6);
    mockGraph.addEdge(Vertices::E, Vertices::D, 4);
    mockGraph.addEdge(Vertices::F, Vertices::C, 14);
    mockGraph.addEdge(Vertices::F, Vertices::E, 6);
  }

  auto looseEndBehaviour() const -> MockGraphProvider::LooseEndBehaviour {
    return GetParam();
  }

  auto pathFinder(bool reverse = false) -> WeightedShortestPathFinder {
    arangodb::graph::PathType::Type pathType =
        arangodb::graph::PathType::Type::ShortestPath;
    arangodb::graph::TwoSidedEnumeratorOptions options{minDepth, maxDepth,
                                                       pathType};
    options.setStopAtFirstDepth(false);
    PathValidatorOptions validatorOpts{&_tmpVar, _expressionContext};
    auto forwardProviderOptions =
        MockGraphProviderOptions{mockGraph, looseEndBehaviour(), false};
    auto backwardProviderOptions =
        MockGraphProviderOptions{mockGraph, looseEndBehaviour(), true};

    double defaultWeight = 1.0;
    std::string weightAttribute = "weight";
    forwardProviderOptions.setWeightEdgeCallback(
        [weightAttribute = weightAttribute, defaultWeight](
            double previousWeight, VPackSlice edge) -> double {
          auto const weight =
              arangodb::basics::VelocyPackHelper::getNumericValue<double>(
                  edge, weightAttribute, defaultWeight);
          if (weight < 0.) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT);
          }

          return previousWeight + weight;
        });
    backwardProviderOptions.setWeightEdgeCallback(
        [weightAttribute = weightAttribute, defaultWeight](
            double previousWeight, VPackSlice edge) -> double {
          auto const weight =
              arangodb::basics::VelocyPackHelper::getNumericValue<double>(
                  edge, weightAttribute, defaultWeight);
          if (weight < 0.) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT);
          }

          return previousWeight + weight;
        });

    return WeightedShortestPathFinder{
        MockGraphProvider(*_query.get(), std::move(forwardProviderOptions),
                          resourceMonitor),
        MockGraphProvider(*_query.get(), std::move(backwardProviderOptions),
                          resourceMonitor),
        std::move(options), std::move(validatorOpts), resourceMonitor};
  }

  auto vId(size_t nr) -> std::string {
    return "v/" + basics::StringUtils::itoa(nr);
  }

  auto pathStructureValid(VPackSlice path, size_t pathLength) -> void {
    ASSERT_TRUE(path.isObject());
    {
      // Check Vertices
      ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryVertices));
      auto vertices = path.get(StaticStrings::GraphQueryVertices);
      ASSERT_TRUE(vertices.isArray());
      ASSERT_EQ(vertices.length(), pathLength + 1);
      for (auto const& v : VPackArrayIterator(vertices)) {
        EXPECT_TRUE(v.isObject());
      }
    }
    {
      // Check Edges
      ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryEdges));
      auto edges = path.get(StaticStrings::GraphQueryEdges);
      ASSERT_TRUE(edges.isArray());
      ASSERT_EQ(edges.length(), pathLength);
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

  auto pathEquals(VPackSlice path, std::vector<size_t> const& vertexIds)
      -> void {
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

INSTANTIATE_TEST_CASE_P(
    WeightedShortestPathTestRunner, WeightedShortestPathTest,
    ::testing::Values(MockGraphProvider::LooseEndBehaviour::NEVER,
                      MockGraphProvider::LooseEndBehaviour::ALWAYS));

TEST_P(WeightedShortestPathTest, no_path_exists) {
  VPackBuilder result;
  // No path between those
  auto source = vId(91);
  auto target = vId(99);
  auto finder = pathFinder();
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
    EXPECT_EQ(stats.getScannedIndex(), 0U);
  }
}

TEST_P(WeightedShortestPathTest, shortest_path_V1_V3) {
  VPackBuilder result;
  auto finder = pathFinder();
  auto source = vId(1);
  auto target = vId(3);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    bool hasPath = false;
    while (!finder.isDone()) {
      finder.getNextPath(result);
    }

    EXPECT_FALSE(hasPath);
    pathStructureValid(result.slice(), 2);
    pathEquals(result.slice(), {1, 2, 3});
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
    // We have to lookup the vertex
    // 3x vertices, 3x edges
    EXPECT_EQ(stats.getScannedIndex(), 6U);
  }

  {
    // Make sure stats are stolen and reset
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup the vertex
    EXPECT_EQ(stats.getScannedIndex(), 0U);
  }
}

TEST_P(WeightedShortestPathTest, shortest_path_V4_V9) {
  VPackBuilder result;
  auto finder = pathFinder();

  // Source and target identical
  auto source = vId(4);
  auto target = vId(9);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    bool hasPath = false;
    while (!finder.isDone()) {
      finder.getNextPath(result);
    }

    EXPECT_FALSE(hasPath);
    pathStructureValid(result.slice(), 3);
    pathEquals(result.slice(), {4, 5, 8, 9});
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
    // We have to lookup the vertex
    // 4x vertices, 3x edges
    EXPECT_EQ(stats.getScannedIndex(), 13U);
  }

  {
    // Make sure stats are stolen and reset
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup the vertex
    EXPECT_EQ(stats.getScannedIndex(), 0U);
  }
}

TEST_P(WeightedShortestPathTest, shortest_path_A_F_outbound) {
  VPackBuilder result;
  auto finder = pathFinder();

  // Source and target identical
  auto source = vId(Vertices::A);
  auto target = vId(Vertices::F);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    bool hasPath = false;
    while (!finder.isDone()) {
      finder.getNextPath(result);
    }

    EXPECT_FALSE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathEquals(result.slice(), {Vertices::A, Vertices::C, Vertices::E,
                                Vertices::D, Vertices::F});
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
    // We have to lookup the vertex
    // 4x vertices, 3x edges
    EXPECT_EQ(stats.getScannedIndex(), 17U);
  }

  {
    // Make sure stats are stolen and reset
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup the vertex
    EXPECT_EQ(stats.getScannedIndex(), 0U);
  }
}

TEST_P(WeightedShortestPathTest, shortest_path_A_F_inbound) {
  VPackBuilder result;
  auto finder = pathFinder(true);

  // Source and target identical
  auto source = vId(Vertices::A);
  auto target = vId(Vertices::F);

  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    bool hasPath = false;
    while (!finder.isDone()) {
      finder.getNextPath(result);
    }

    EXPECT_FALSE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathEquals(result.slice(), {Vertices::A, Vertices::C, Vertices::E,
                                Vertices::D, Vertices::F});
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
    // We have to lookup the vertex
    // 4x vertices, 3x edges
    EXPECT_EQ(stats.getScannedIndex(), 17U);
  }

  {
    // Make sure stats are stolen and reset
    aql::TraversalStats stats = finder.stealStats();
    // We have to lookup the vertex
    EXPECT_EQ(stats.getScannedIndex(), 0U);
  }
}

}  // namespace graph
}  // namespace tests
}  // namespace arangodb
