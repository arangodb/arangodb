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

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Graph/KShortestPathsFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Graph/Enumerators/PathEnumeratorInterface.h"
#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"

#include "Random/RandomGenerator.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

// test setup
#include "Mocks/MockGraphProvider.h"
#include "Mocks/Servers.h"
#include "GraphTestTools.h"
#include "Graph/PathManagement/PathValidatorOptions.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

class KShortestPathsFinderTest : public ::testing::Test {
 protected:
  graph::MockGraph emptyMockGraph;
  mocks::MockAqlServer server{true};
  std::shared_ptr<arangodb::aql::Query> fakedQuery;
  arangodb::transaction::Methods myTrx;
  arangodb::aql::AqlFunctionsInternalCache functionsCache;
  aql::Variable _tmpVar{"tmp", 0, false};
  arangodb::aql::FixedVarExpressionContext expressionContext;
  PathValidatorOptions validatorOpts;
  std::unique_ptr<arangodb::graph::PathEnumeratorInterface> newFinder;

  auto looseEndBehaviour() const
      -> graph::MockGraphProvider::LooseEndBehaviour {
    return graph::MockGraphProviderOptions::LooseEndBehaviour::ALWAYS;
  }

  auto toHashedStringRef(std::string const& id) -> HashedStringRef {
    return HashedStringRef(id.data(), static_cast<uint32_t>(id.length()));
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

  KShortestPathsFinderTest()
      : fakedQuery(server.createFakeQuery()),
        myTrx(fakedQuery.get()->newTrxContext()),
        functionsCache(),
        expressionContext(myTrx, *fakedQuery.get(), functionsCache),
        validatorOpts(&_tmpVar, expressionContext) {
    emptyMockGraph.addEdge(1, 2);
    emptyMockGraph.addEdge(2, 3);
    emptyMockGraph.addEdge(3, 4);
    emptyMockGraph.addEdge(5, 4);
    emptyMockGraph.addEdge(6, 5);
    emptyMockGraph.addEdge(7, 6);
    emptyMockGraph.addEdge(8, 7);
    emptyMockGraph.addEdge(1, 10);
    emptyMockGraph.addEdge(10, 11);
    emptyMockGraph.addEdge(11, 12);
    emptyMockGraph.addEdge(12, 4);
    emptyMockGraph.addEdge(12, 5);
    emptyMockGraph.addEdge(21, 22);
    emptyMockGraph.addEdge(22, 23);
    emptyMockGraph.addEdge(23, 24);
    emptyMockGraph.addEdge(24, 25);
    emptyMockGraph.addEdge(21, 26);
    emptyMockGraph.addEdge(26, 27);
    emptyMockGraph.addEdge(27, 28);
    emptyMockGraph.addEdge(28, 25);
    emptyMockGraph.addEdge(30, 31);
    emptyMockGraph.addEdge(31, 32);
    emptyMockGraph.addEdge(32, 33);
    emptyMockGraph.addEdge(33, 34);
    emptyMockGraph.addEdge(34, 35);
    emptyMockGraph.addEdge(32, 30);
    emptyMockGraph.addEdge(33, 35);
    emptyMockGraph.addEdge(40, 41);
    emptyMockGraph.addEdge(41, 42);
    emptyMockGraph.addEdge(41, 43);
    emptyMockGraph.addEdge(42, 44);
    emptyMockGraph.addEdge(43, 44);
    emptyMockGraph.addEdge(44, 45);
    emptyMockGraph.addEdge(45, 46);
    emptyMockGraph.addEdge(46, 47);
    emptyMockGraph.addEdge(48, 47);
    emptyMockGraph.addEdge(49, 47);
    emptyMockGraph.addEdge(50, 47);
    emptyMockGraph.addEdge(48, 46);
    emptyMockGraph.addEdge(50, 46);
    emptyMockGraph.addEdge(50, 47);
    emptyMockGraph.addEdge(48, 46);
    emptyMockGraph.addEdge(50, 46);
    emptyMockGraph.addEdge(40, 60);
    emptyMockGraph.addEdge(60, 61);
    emptyMockGraph.addEdge(61, 62);
    emptyMockGraph.addEdge(62, 63);
    emptyMockGraph.addEdge(63, 64);
    emptyMockGraph.addEdge(64, 47);
    emptyMockGraph.addEdge(70, 71);
    emptyMockGraph.addEdge(70, 71);
    emptyMockGraph.addEdge(70, 71);

    arangodb::graph::TwoSidedEnumeratorOptions enumeratorOptions{
        0, std::numeric_limits<uint64_t>::max()};

    tests::graph::MockGraphProviderOptions forwardMockProviderOptions(
        emptyMockGraph, looseEndBehaviour(), false);
    tests::graph::MockGraphProviderOptions backwardMockProviderOptions(
        emptyMockGraph, looseEndBehaviour(), true);

    newFinder = arangodb::graph::PathEnumeratorInterface::createEnumerator<
        graph::MockGraphProvider>(
        *fakedQuery.get(), std::move(forwardMockProviderOptions),
        std::move(backwardMockProviderOptions), std::move(enumeratorOptions),
        std::move(validatorOpts),
        PathEnumeratorInterface::PathEnumeratorType::K_SHORTEST_PATH, false);
  }

  ~KShortestPathsFinderTest() { newFinder.reset(); }
};

TEST_F(KShortestPathsFinderTest, path_from_vertex_to_itself) {
  VPackBuilder resultBuilder;

  auto source = vId(0);
  auto target = vId(0);
  newFinder->reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(newFinder->isDone());
  {
    resultBuilder.clear();
    auto hasPath = newFinder->getNextPath(resultBuilder);
    EXPECT_TRUE(hasPath);
    pathStructureValid(resultBuilder.slice(), 0);
    pathEquals(resultBuilder.slice(), {0});
    EXPECT_FALSE(resultBuilder.isEmpty());
    EXPECT_FALSE(newFinder->isDone());
  }

  {
    resultBuilder.clear();
    auto hasPath = newFinder->getNextPath(resultBuilder);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(resultBuilder.isEmpty());
    EXPECT_TRUE(newFinder->isDone());
  }
  {
    aql::TraversalStats stats = newFinder->stealStats();
    EXPECT_EQ(stats.getScannedIndex(), 1);
  }
}

TEST_F(KShortestPathsFinderTest, no_path_exists) {
  VPackBuilder resultBuilder;

  auto source = vId(0);
  auto target = vId(1);
  newFinder->reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(newFinder->isDone());
  {
    resultBuilder.clear();
    auto hasPath = newFinder->getNextPath(resultBuilder);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(resultBuilder.isEmpty());
    EXPECT_TRUE(newFinder->isDone());
  }

  // Repeat to see that we keep returning false and don't crash
  {
    resultBuilder.clear();
    auto hasPath = newFinder->getNextPath(resultBuilder);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(resultBuilder.isEmpty());
    EXPECT_TRUE(newFinder->isDone());
  }
  {
    aql::TraversalStats stats = newFinder->stealStats();
    EXPECT_EQ(stats.getScannedIndex(), 0);
  }
}

TEST_F(KShortestPathsFinderTest, path_of_length_1) {
  VPackBuilder result;

  auto source = vId(1);
  auto target = vId(2);
  newFinder->reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(newFinder->isDone());
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 1);
    pathEquals(result.slice(), {1, 2});
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(newFinder->isDone());
  }
  {
    aql::TraversalStats stats = newFinder->stealStats();
    // Stats: MockGraphProvider
    // 2x Vertex lookup (v1, v2)
    // 2x Expand lookup (same edge but both directions)
    EXPECT_EQ(stats.getScannedIndex(), 4);
  }

  // Repeat to see that we keep returning false and don't crash
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(newFinder->isDone());
  }
}

TEST_F(KShortestPathsFinderTest, path_of_length_4_5_6) {
  VPackBuilder result;

  auto source = vId(1);
  auto target = vId(4);
  newFinder->reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(newFinder->isDone());
  // PathLength 4
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 3);
    pathEquals(result.slice(), {1, 2, 3, 4});
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(newFinder->isDone());
  }
  {
    aql::TraversalStats stats = newFinder->stealStats();
    EXPECT_EQ(stats.getScannedIndex(), 11);

    /*
     * Explanation:
     *  Increase 4x vertices (v1, v2, v3, v4)
     *  Increase edges:
     *   - v1 (2x edges)
     *   - v4 (3x edges)
     *   - v10 (1x edge)
     *   - v2 (1x edge)
     *  => 4 + 2 + 3 + 1 + 1 = 11
     */
  }

  // PathLength 5
  // There is another Path (+1 length) from: 1 -> 10 -> 11 -> 12 -> 4
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathEquals(result.slice(), {1, 10, 11, 12, 4});
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(newFinder->isDone());
  }
  {
    aql::TraversalStats stats = newFinder->stealStats();
    EXPECT_EQ(stats.getScannedIndex(), 6);
    /*
     * Explanation:
     *  Increase 5x vertices (v1, v10, v11, v12, v4) (addVertexToBuilder in
     * Mock)
     * Increase edges:
     *   - v11 (1x edge)
     *  => 5 + 1 = 6
     */
  }

  // PathLength 6
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 5);
    pathEquals(result.slice(), {1, 10, 11, 12, 5, 4});
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(newFinder->isDone());
  }

  // Finally done, no more paths left.
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(newFinder->isDone());
  }
}

TEST_F(KShortestPathsFinderTest, path_of_length_5_with_loops_to_start_end) {
  VPackBuilder result;

  auto source = vId(30);
  auto target = vId(35);
  newFinder->reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(newFinder->isDone());
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathEquals(result.slice(), {30, 31, 32, 33, 35});
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(newFinder->isDone());
  }
}

TEST_F(KShortestPathsFinderTest, two_paths_of_length_5) {
  VPackBuilder result;

  auto source = vId(21);
  auto target = vId(25);
  newFinder->reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(newFinder->isDone());
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathEquals(result.slice(), {21, 22, 23, 24, 25});
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(newFinder->isDone());
  }
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    pathStructureValid(result.slice(), 4);
    pathEquals(result.slice(), {21, 26, 27, 28, 25});
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(newFinder->isDone());
  }
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(newFinder->isDone());
  }
}

TEST_F(KShortestPathsFinderTest, many_edges_between_two_nodes) {
  VPackBuilder result;

  auto source = vId(70);
  auto target = vId(71);
  newFinder->reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(newFinder->isDone());
  for (size_t i = 0; i < 3; i++) {
    {
      result.clear();
      auto hasPath = newFinder->getNextPath(result);
      EXPECT_TRUE(hasPath);
      pathStructureValid(result.slice(), 1);
      pathEquals(result.slice(), {70, 71});
      EXPECT_FALSE(result.isEmpty());
      EXPECT_FALSE(newFinder->isDone());
    }
  }
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_FALSE(hasPath);
    EXPECT_TRUE(result.isEmpty());
    EXPECT_TRUE(newFinder->isDone());
  }
}

class KShortestPathsFinderTestWeighted : public ::testing::Test {
 protected:
  std::string const weightAttribute = "weight";

  graph::MockGraph emptyMockGraph;
  mocks::MockAqlServer server{true};
  std::shared_ptr<arangodb::aql::Query> fakedQuery;
  arangodb::transaction::Methods myTrx;
  arangodb::aql::AqlFunctionsInternalCache functionsCache;
  aql::Variable _tmpVar{"tmp", 0, false};
  arangodb::aql::FixedVarExpressionContext expressionContext;
  PathValidatorOptions validatorOpts;
  std::unique_ptr<arangodb::graph::PathEnumeratorInterface> newFinder;

  auto looseEndBehaviour() const
      -> graph::MockGraphProvider::LooseEndBehaviour {
    return graph::MockGraphProviderOptions::LooseEndBehaviour::ALWAYS;
  }

  auto toHashedStringRef(std::string const& id) -> HashedStringRef {
    return HashedStringRef(id.data(), static_cast<uint32_t>(id.length()));
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

  KShortestPathsFinderTestWeighted()
      : fakedQuery(server.createFakeQuery()),
        myTrx(fakedQuery.get()->newTrxContext()),
        functionsCache(),
        expressionContext(myTrx, *fakedQuery.get(), functionsCache),
        validatorOpts(&_tmpVar, expressionContext) {
    emptyMockGraph.addEdge(1, 2, 10);
    emptyMockGraph.addEdge(1, 3, 10);
    emptyMockGraph.addEdge(1, 10, 100);
    emptyMockGraph.addEdge(2, 4, 10);
    emptyMockGraph.addEdge(3, 4, 20);
    emptyMockGraph.addEdge(7, 3, 10);
    emptyMockGraph.addEdge(8, 3, 10);
    emptyMockGraph.addEdge(9, 3, 10);

    tests::graph::MockGraphProviderOptions forwardMockProviderOptions(
        emptyMockGraph, looseEndBehaviour(), false);
    tests::graph::MockGraphProviderOptions backwardMockProviderOptions(
        emptyMockGraph, looseEndBehaviour(), true);

    double defaultWeight = 1.0;
    forwardMockProviderOptions.setWeightEdgeCallback(
        [weightAttribute = weightAttribute, defaultWeight](
            double previousWeight, VPackSlice edge) -> double {
          LOG_DEVEL << "- 1 -";
          LOG_DEVEL << edge.toJson();

          auto const weight =
              arangodb::basics::VelocyPackHelper::getNumericValue<double>(
                  edge, weightAttribute, defaultWeight);
          LOG_DEVEL << " -> Got weight: " << weight;
          if (weight < 0.) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT);
          }

          return previousWeight + weight;
        });
    backwardMockProviderOptions.setWeightEdgeCallback(
        [weightAttribute = weightAttribute, defaultWeight](
            double previousWeight, VPackSlice edge) -> double {
          LOG_DEVEL << "- 2 -";
          LOG_DEVEL << edge.toJson();

          auto const weight =
              arangodb::basics::VelocyPackHelper::getNumericValue<double>(
                  edge, weightAttribute, defaultWeight);
          LOG_DEVEL << " -> Got weight: " << weight;
          if (weight < 0.) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT);
          }

          return previousWeight + weight;
        });

    arangodb::graph::TwoSidedEnumeratorOptions enumeratorOptions{
        0, std::numeric_limits<uint64_t>::max()};

    newFinder = arangodb::graph::PathEnumeratorInterface::createEnumerator<
        graph::MockGraphProvider>(
        *fakedQuery.get(), std::move(forwardMockProviderOptions),
        std::move(backwardMockProviderOptions), std::move(enumeratorOptions),
        std::move(validatorOpts),
        PathEnumeratorInterface::PathEnumeratorType::K_SHORTEST_PATH, true);
  }

  ~KShortestPathsFinderTestWeighted() { newFinder.reset(); }
};

TEST_F(KShortestPathsFinderTestWeighted, diamond_path) {
  VPackBuilder result;

  auto source = vId(1);
  auto target = vId(4);
  newFinder->reset(toHashedStringRef(source), toHashedStringRef(target));
  EXPECT_FALSE(newFinder->isDone());
  {
    result.clear();
    auto hasPath = newFinder->getNextPath(result);
    EXPECT_TRUE(hasPath);
    LOG_DEVEL << "Weighted Result is: " << result.toJson();
    pathStructureValid(result.slice(), 2);
    pathEquals(result.slice(), {1, 2, 4});
    EXPECT_FALSE(result.isEmpty());
    EXPECT_FALSE(newFinder->isDone());
  }
  /*
  auto start = velocypack::Parser::fromJson("\"v/1\"");
  auto end = velocypack::Parser::fromJson("\"v/4\"");
  ShortestPathResult result;
  std::string msgs;

  finder->startKShortestPathsTraversal(start->slice(), end->slice());

  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  auto cpr = checkPath(spo.get(), result, {"1", "2", "4"},
                       {{}, {"v/1", "v/2"}, {"v/2", "v/4"}}, msgs);
  ASSERT_TRUE(cpr) << msgs;*/
}

class KShortestPathsFinderTestWeights : public ::testing::Test {
 protected:
  GraphTestSetup s;
  MockGraphDatabase gdb;

  std::shared_ptr<arangodb::aql::Query> query;
  std::unique_ptr<arangodb::graph::ShortestPathOptions> spo;

  KShortestPathsFinder* finder;

  KShortestPathsFinderTestWeights() : gdb(s.server, "testVocbase") {
    gdb.addVertexCollection("v", 10);
    gdb.addEdgeCollection("e", "v",
                          {{1, 2, 10},
                           {1, 3, 10},
                           {1, 10, 100},
                           {2, 4, 10},
                           {3, 4, 20},
                           {7, 3, 10},
                           {8, 3, 10},
                           {9, 3, 10}});

    query = gdb.getQuery("RETURN 1", std::vector<std::string>{"v", "e"});

    spo = gdb.getShortestPathOptions(query.get());
    spo->setWeightAttribute("cost");

    finder = new KShortestPathsFinder(*spo);
  }

  ~KShortestPathsFinderTestWeights() { delete finder; }
};

TEST_F(KShortestPathsFinderTestWeights, diamond_path) {
  auto start = velocypack::Parser::fromJson("\"v/1\"");
  auto end = velocypack::Parser::fromJson("\"v/4\"");
  ShortestPathResult result;
  std::string msgs;

  finder->startKShortestPathsTraversal(start->slice(), end->slice());

  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  auto cpr = checkPath(spo.get(), result, {"1", "2", "4"},
                       {{}, {"v/1", "v/2"}, {"v/2", "v/4"}}, msgs);
  ASSERT_TRUE(cpr) << msgs;
}

}  // namespace graph
}  // namespace tests
}  // namespace arangodb
