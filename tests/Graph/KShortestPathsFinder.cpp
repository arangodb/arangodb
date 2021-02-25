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

#include "fakeit.hpp"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Graph/KShortestPathsFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Random/RandomGenerator.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

// test setup
#include "../Mocks/Servers.h"
#include "../Mocks/StorageEngineMock.h"
#include "GraphTestTools.h"
#include "IResearch/common.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

class KShortestPathsFinderTest : public ::testing::Test {
 protected:
  GraphTestSetup s;
  MockGraphDatabase gdb;

  std::unique_ptr<arangodb::aql::Query> query;
  std::unique_ptr<arangodb::graph::ShortestPathOptions> spo;

  KShortestPathsFinder* finder;

  KShortestPathsFinderTest() : gdb(s.server, "testVocbase") {
    gdb.addVertexCollection("v", 100);
    gdb.addEdgeCollection(
        "e", "v",
        {{1, 2},   {2, 3},   {3, 4},   {5, 4},   {6, 5},   {7, 6},   {8, 7},
         {1, 10},  {10, 11}, {11, 12}, {12, 4},  {12, 5},  {21, 22}, {22, 23},
         {23, 24}, {24, 25}, {21, 26}, {26, 27}, {27, 28}, {28, 25}, {30, 31},
         {31, 32}, {32, 33}, {33, 34}, {34, 35}, {32, 30}, {33, 35}, {40, 41},
         {41, 42}, {41, 43}, {42, 44}, {43, 44}, {44, 45}, {45, 46}, {46, 47},
         {48, 47}, {49, 47}, {50, 47}, {48, 46}, {50, 46}, {50, 47}, {48, 46},
         {50, 46}, {40, 60}, {60, 61}, {61, 62}, {62, 63}, {63, 64}, {64, 47},
         {70, 71}, {70, 71}, {70, 71}});

    query = gdb.getQuery("RETURN 1", std::vector<std::string>{"v", "e"});
    spo = gdb.getShortestPathOptions(query.get());

    finder = new KShortestPathsFinder(*spo);
  }

  ~KShortestPathsFinderTest() { delete finder; }
};

TEST_F(KShortestPathsFinderTest, path_from_vertex_to_itself) {
  auto start = velocypack::Parser::fromJson("\"v/0\"");
  auto end = velocypack::Parser::fromJson("\"v/0\"");
  ShortestPathResult result;

  finder->startKShortestPathsTraversal(start->slice(), end->slice());

  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  ASSERT_FALSE(finder->getNextPathShortestPathResult(result));
}

TEST_F(KShortestPathsFinderTest, no_path_exists) {
  auto start = velocypack::Parser::fromJson("\"v/0\"");
  auto end = velocypack::Parser::fromJson("\"v/1\"");
  ShortestPathResult result;

  finder->startKShortestPathsTraversal(start->slice(), end->slice());
  ASSERT_FALSE(finder->getNextPathShortestPathResult(result));
  // Repeat to see that we keep returning false and don't crash
  ASSERT_FALSE(finder->getNextPathShortestPathResult(result));
}

TEST_F(KShortestPathsFinderTest, path_of_length_1) {
  auto start = velocypack::Parser::fromJson("\"v/1\"");
  auto end = velocypack::Parser::fromJson("\"v/2\"");
  ShortestPathResult result;
  std::string msgs;

  finder->startKShortestPathsTraversal(start->slice(), end->slice());

  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  auto cpr = checkPath(spo.get(), result, {"1", "2"}, {{}, {"v/1", "v/2"}}, msgs);
  ASSERT_TRUE(cpr) << msgs;
}

TEST_F(KShortestPathsFinderTest, path_of_length_4) {
  auto start = velocypack::Parser::fromJson("\"v/1\"");
  auto end = velocypack::Parser::fromJson("\"v/4\"");
  ShortestPathResult result;
  std::string msgs;

  finder->startKShortestPathsTraversal(start->slice(), end->slice());

  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  auto cpr = checkPath(spo.get(), result, {"1", "2", "3", "4"},
                       {{}, {"v/1", "v/2"}, {"v/2", "v/3"}, {"v/3", "v/4"}}, msgs);
  ASSERT_TRUE(cpr) << msgs;
}

TEST_F(KShortestPathsFinderTest, path_of_length_5_with_loops_to_start_end) {
  auto start = velocypack::Parser::fromJson("\"v/30\"");
  auto end = velocypack::Parser::fromJson("\"v/35\"");
  ShortestPathResult result;

  finder->startKShortestPathsTraversal(start->slice(), end->slice());

  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  ASSERT_EQ(result.length(), 5);
}

TEST_F(KShortestPathsFinderTest, two_paths_of_length_5) {
  auto start = velocypack::Parser::fromJson("\"v/21\"");
  auto end = velocypack::Parser::fromJson("\"v/25\"");
  ShortestPathResult result;
  std::string msgs;

  finder->startKShortestPathsTraversal(start->slice(), end->slice());

  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  auto cpr = checkPath(spo.get(), result, {"21", "22", "23", "24", "25"},
                       {{},
                        {"v/21", "v/22"},
                        {"v/22", "v/23"},
                        {"v/23", "v/24"},
                        {"v/24", "v/25"}},
                       msgs) ||
             checkPath(spo.get(), result, {"21", "26", "27", "28", "25"},
                       {{},
                        {"v/21", "v/26"},
                        {"v/26", "v/27"},
                        {"v/27", "v/28"},
                        {"v/28", "v/25"}},
                       msgs);
  ASSERT_TRUE(cpr) << msgs;
  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  cpr = checkPath(spo.get(), result, {"21", "22", "23", "24", "25"},
                  {{},
                   {"v/21", "v/22"},
                   {"v/22", "v/23"},
                   {"v/23", "v/24"},
                   {"v/24", "v/25"}},
                  msgs) ||
        checkPath(spo.get(), result, {"21", "26", "27", "28", "25"},
                  {{},
                   {"v/21", "v/26"},
                   {"v/26", "v/27"},
                   {"v/27", "v/28"},
                   {"v/28", "v/25"}},
                  msgs);
  ASSERT_TRUE(cpr) << msgs;
}

TEST_F(KShortestPathsFinderTest, many_edges_between_two_nodes) {
  auto start = velocypack::Parser::fromJson("\"v/70\"");
  auto end = velocypack::Parser::fromJson("\"v/71\"");
  ShortestPathResult result;

  finder->startKShortestPathsTraversal(start->slice(), end->slice());

  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  ASSERT_TRUE(finder->getNextPathShortestPathResult(result));
  ASSERT_FALSE(finder->getNextPathShortestPathResult(result));
}

class KShortestPathsFinderTestWeights : public ::testing::Test {
 protected:
  GraphTestSetup s;
  MockGraphDatabase gdb;

  std::unique_ptr<arangodb::aql::Query> query;
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
    spo->weightAttribute = "cost";

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
