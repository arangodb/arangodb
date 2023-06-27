////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Query.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

// test setup
#include "./MockGraph.h"
#include "./MockGraphProvider.h"
#include "Graph/algorithm-aliases.h"

#include "GraphTestTools.h"
#include "Basics/GlobalResourceMonitor.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

auto vId(size_t nr) -> std::string {
  return "v/" + basics::StringUtils::itoa(nr);
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

auto pathWeight(VPackSlice path, int64_t expectedWeight) -> void {
  ASSERT_TRUE(path.isObject());
  ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryWeight));
  int64_t calculatedWeight = path.get(StaticStrings::GraphQueryWeight).getInt();
  ASSERT_EQ(expectedWeight, calculatedWeight);
}

auto pathWeightDouble(VPackSlice path, double expectedWeight) -> void {
  ASSERT_TRUE(path.isObject());
  ASSERT_TRUE(path.hasKey(StaticStrings::GraphQueryWeight));
  double calculatedWeight =
      path.get(StaticStrings::GraphQueryWeight).getDouble();
  ASSERT_EQ(expectedWeight, calculatedWeight);
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

auto toHashedStringRef(std::string const& id) -> HashedStringRef {
  return HashedStringRef(id.data(), static_cast<uint32_t>(id.length()));
}

class KShortestPathsFinderTest : public ::testing::Test {
  using KShortestPathsFinder = KPathEnumerator<MockGraphProvider>;
  static constexpr size_t minDepth = 0;
  static constexpr size_t maxDepth = std::numeric_limits<size_t>::max();

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

  arangodb::transaction::Methods _trx{_query->newTrxContext(),
                                      transaction::Hints::TrxType::INTERNAL};
  arangodb::aql::FixedVarExpressionContext _expressionContext{
      _trx, *_query.get(), _functionsCache};

  KShortestPathsFinderTest() {
    /* Non Weighted */
    if (activateLogging) {
      Logger::GRAPHS.setLogLevel(LogLevel::TRACE);
    }
    // gdb.addVertexCollection("v", 100);
    mockGraph.addEdge(0, 0);
    mockGraph.addEdge(1, 2);
    mockGraph.addEdge(2, 3);
    mockGraph.addEdge(3, 4);
    mockGraph.addEdge(5, 4);
    mockGraph.addEdge(6, 5);
    mockGraph.addEdge(7, 6);
    mockGraph.addEdge(8, 7);
    mockGraph.addEdge(1, 10);
    mockGraph.addEdge(10, 11);
    mockGraph.addEdge(11, 12);
    mockGraph.addEdge(12, 4);
    mockGraph.addEdge(12, 5);
    mockGraph.addEdge(21, 22);
    mockGraph.addEdge(22, 23);
    mockGraph.addEdge(23, 24);
    mockGraph.addEdge(24, 25);
    mockGraph.addEdge(21, 26);
    mockGraph.addEdge(26, 27);
    mockGraph.addEdge(27, 28);
    mockGraph.addEdge(28, 25);
    mockGraph.addEdge(30, 31);
    mockGraph.addEdge(31, 32);
    mockGraph.addEdge(32, 33);
    mockGraph.addEdge(33, 34);
    mockGraph.addEdge(34, 35);
    mockGraph.addEdge(32, 30);
    mockGraph.addEdge(33, 35);
    mockGraph.addEdge(40, 41);
    mockGraph.addEdge(41, 42);
    mockGraph.addEdge(41, 43);
    mockGraph.addEdge(42, 44);
    mockGraph.addEdge(43, 44);
    mockGraph.addEdge(44, 45);
    mockGraph.addEdge(45, 46);
    mockGraph.addEdge(46, 47);
    mockGraph.addEdge(48, 47);
    mockGraph.addEdge(49, 47);
    mockGraph.addEdge(50, 47);
    mockGraph.addEdge(48, 46);
    mockGraph.addEdge(50, 46);
    mockGraph.addEdge(50, 47);
    mockGraph.addEdge(48, 46);
    mockGraph.addEdge(50, 46);
    mockGraph.addEdge(40, 60);
    mockGraph.addEdge(60, 61);
    mockGraph.addEdge(61, 62);
    mockGraph.addEdge(62, 63);
    mockGraph.addEdge(63, 64);
    mockGraph.addEdge(64, 47);
    mockGraph.addEdge(70, 71);
    mockGraph.addEdge(70, 71);
    mockGraph.addEdge(70, 71);
  }

  auto looseEndBehaviour() const -> MockGraphProvider::LooseEndBehaviour {
    return MockGraphProvider::LooseEndBehaviour::NEVER;
  }

  auto pathFinder(bool reverse = false) -> KShortestPathsFinder {
    arangodb::graph::PathType::Type pathType =
        arangodb::graph::PathType::Type::KShortestPaths;
    arangodb::graph::TwoSidedEnumeratorOptions options{minDepth, maxDepth,
                                                       pathType};
    options.setStopAtFirstDepth(false);
    PathValidatorOptions validatorOpts{&_tmpVar, _expressionContext};
    auto forwardProviderOptions =
        MockGraphProviderOptions{mockGraph, looseEndBehaviour(), false};
    auto backwardProviderOptions =
        MockGraphProviderOptions{mockGraph, looseEndBehaviour(), true};

    return KShortestPathsFinder{
        MockGraphProvider(*_query.get(), std::move(forwardProviderOptions),
                          resourceMonitor),
        MockGraphProvider(*_query.get(), std::move(backwardProviderOptions),
                          resourceMonitor),
        std::move(options), std::move(validatorOpts), resourceMonitor};
  }
};

TEST_F(KShortestPathsFinderTest, path_from_vertex_to_itself) {
  VPackBuilder result;

  // Source and target identical
  auto source = vId(0);
  auto target = vId(0);
  auto finder = pathFinder();
  finder.reset(toHashedStringRef(source), toHashedStringRef(target));

  EXPECT_FALSE(finder.isDone());
  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_TRUE(hasPath);
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(finder.isDone());
  }

  {
    result.clear();
    auto hasPath = finder.getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_F(KShortestPathsFinderTest, no_path_exists) {
  VPackBuilder result;
  // No path between those
  auto source = vId(0);
  auto target = vId(1);
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
    EXPECT_EQ(stats.getScannedIndex(), 1U);
  }
}

TEST_F(KShortestPathsFinderTest, path_of_length_1) {
  VPackBuilder result;
  auto source = vId(1);
  auto target = vId(2);
  auto finder = pathFinder();
  finder.reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(finder.isDone());

  {
    result.clear();
    bool hasPath = false;
    while (!finder.isDone()) {
      hasPath = finder.getNextPath(result);
    }

    EXPECT_FALSE(hasPath);
    pathStructureValid(result.slice(), 1);
    pathEquals(result.slice(), {1, 2});
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_F(KShortestPathsFinderTest, path_of_length_4) {
  VPackBuilder result;
  auto source = vId(1);
  auto target = vId(4);
  auto finder = pathFinder();
  finder.reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(finder.isDone());

  {
    result.clear();
    bool hasPath = false;
    while (!finder.isDone()) {
      hasPath = finder.getNextPath(result);
    }

    EXPECT_FALSE(hasPath);
    pathStructureValid(result.slice(), 3);
    pathEquals(result.slice(), {1, 2, 3, 4});
    pathWeight(result.slice(), 3);
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_F(KShortestPathsFinderTest, path_of_length_5_with_loops_to_start_end) {
  VPackBuilder result;
  auto source = vId(30);
  auto target = vId(35);
  auto finder = pathFinder();
  finder.reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(finder.isDone());

  {
    result.clear();
    bool hasPath = false;
    while (!finder.isDone()) {
      hasPath = finder.getNextPath(result);
    }

    EXPECT_FALSE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathEquals(result.slice(), {30, 31, 32, 33, 35});
    pathWeight(result.slice(), 4);
    EXPECT_TRUE(finder.isDone());
  }
}

TEST_F(KShortestPathsFinderTest, two_paths_of_length_5) {
  VPackBuilder result;
  auto source = vId(21);
  auto target = vId(25);
  auto finder = pathFinder();
  finder.reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(finder.isDone());

  {
    // First expected path
    finder.getNextPath(result);
    pathEquals(result.slice(), {21, 22, 23, 24, 25});
  }

  {
    // Second expected path
    result.clear();
    finder.getNextPath(result);
    pathEquals(result.slice(), {21, 26, 27, 28, 25});
  }

  {
    // Finish
    result.clear();
    bool foundPath = finder.getNextPath(result);
    ASSERT_FALSE(foundPath);
  }
}

TEST_F(KShortestPathsFinderTest, many_edges_between_two_nodes) {
  VPackBuilder result;
  auto source = vId(70);
  auto target = vId(71);
  auto finder = pathFinder();
  finder.reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(finder.isDone());

  bool foundPath = false;
  foundPath = finder.getNextPath(result);
  ASSERT_TRUE(foundPath);
  foundPath = finder.getNextPath(result);
  ASSERT_TRUE(foundPath);
  foundPath = finder.getNextPath(result);
  ASSERT_TRUE(foundPath);
  foundPath = finder.getNextPath(result);
  ASSERT_FALSE(foundPath);
}

/*
 * Weighted KPathsTests
 */

class WeightedKShortestPathsFinderTest : public ::testing::Test {
  using WeightedKShortestPathsFinder =
      WeightedKShortestPathsEnumerator<MockGraphProvider>;
  static constexpr size_t minDepth = 0;
  static constexpr size_t maxDepth = std::numeric_limits<size_t>::max();

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

  arangodb::transaction::Methods _trx{_query->newTrxContext(),
                                      transaction::Hints::TrxType::INTERNAL};
  arangodb::aql::FixedVarExpressionContext _expressionContext{
      _trx, *_query.get(), _functionsCache};

  WeightedKShortestPathsFinderTest() {
    if (activateLogging) {
      Logger::GRAPHS.setLogLevel(LogLevel::TRACE);
    }

    // Diamond Graph with weights
    mockGraph.addEdge(0, 0);
    mockGraph.addEdge(1, 2, 10);
    mockGraph.addEdge(1, 3, 10);
    mockGraph.addEdge(1, 10, 100);
    mockGraph.addEdge(2, 4, 10);
    mockGraph.addEdge(3, 4, 20);
    mockGraph.addEdge(7, 3, 10);
    mockGraph.addEdge(8, 3, 10);
    mockGraph.addEdge(9, 3, 10);

    // Circular graph
    mockGraph.addEdge(100, 101, 1);
    mockGraph.addEdge(101, 102, 2);
    mockGraph.addEdge(102, 103, 3);
    mockGraph.addEdge(103, 100, 4);
    mockGraph.addEdge(100, 104, 1);
    mockGraph.addEdge(104, 105, 1);
    mockGraph.addEdge(105, 106, 1);
    mockGraph.addEdge(106, 100, 1);
  }

  auto looseEndBehaviour() const -> MockGraphProvider::LooseEndBehaviour {
    return MockGraphProvider::LooseEndBehaviour::NEVER;
  }

  auto pathFinder(bool reverse = false) -> WeightedKShortestPathsFinder {
    arangodb::graph::PathType::Type pathType =
        arangodb::graph::PathType::Type::KShortestPaths;
    arangodb::graph::TwoSidedEnumeratorOptions options{minDepth, maxDepth,
                                                       pathType};
    options.setStopAtFirstDepth(false);
    PathValidatorOptions validatorOpts{&_tmpVar, _expressionContext};
    auto forwardProviderOptions =
        MockGraphProviderOptions{mockGraph, looseEndBehaviour(), false};
    auto backwardProviderOptions =
        MockGraphProviderOptions{mockGraph, looseEndBehaviour(), true};

    // Adds Weight callback to initialize WeightedKShortestPathsFinder
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

    return WeightedKShortestPathsFinder{
        MockGraphProvider(*_query.get(), std::move(forwardProviderOptions),
                          resourceMonitor),
        MockGraphProvider(*_query.get(), std::move(backwardProviderOptions),
                          resourceMonitor),
        std::move(options), std::move(validatorOpts), resourceMonitor};
  }
};

TEST_F(WeightedKShortestPathsFinderTest, diamond_path) {
  VPackBuilder result;
  auto source = vId(1);
  auto target = vId(4);
  auto finder = pathFinder();
  finder.reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(finder.isDone());

  {
    result.clear();
    ASSERT_TRUE(finder.getNextPath(result));
    pathStructureValid(result.slice(), 2);
    pathEquals(result.slice(), {1, 2, 4});
    pathWeightDouble(result.slice(), 20);
    EXPECT_FALSE(finder.isDone());
  }
}

TEST_F(WeightedKShortestPathsFinderTest,
       path_where_source_and_target_are_equal) {
  VPackBuilder result;
  auto source = vId(100);
  auto target = vId(100);
  auto finder = pathFinder();
  finder.reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(finder.isDone());

  {
    result.clear();
    ASSERT_TRUE(finder.getNextPath(result));
    pathStructureValid(result.slice(), 0);
    pathEquals(result.slice(), {100});
    pathWeightDouble(result.slice(), 0);
    EXPECT_TRUE(finder.isDone());
  }
}

}  // namespace graph
}  // namespace tests
}  // namespace arangodb
