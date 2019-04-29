////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019-2019 ArangoDB GmbH, Cologne, Germany
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

#include "catch.hpp"
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
#include "RestServer/TraverserEngineRegistryFeature.h"
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
#include "IResearch/common.h"
#include "GraphTestTools.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

TEST_CASE("KShortestPathsFinder", "[graph]") {
  Setup s;
  UNUSED(s);
  MockGraphDatabase gdb("testVocbase");

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

  auto query = gdb.getQuery("RETURN 1");
  auto spo = gdb.getShortestPathOptions(query);

  auto finder = new KShortestPathsFinder(*spo);

  SECTION("path from vertex to itself") {
    auto start = velocypack::Parser::fromJson("\"v/0\"");
    auto end = velocypack::Parser::fromJson("\"v/0\"");
    ShortestPathResult result;

    finder->startKShortestPathsTraversal(start->slice(), end->slice());

    REQUIRE(true == finder->getNextPathShortestPathResult(result));
    REQUIRE(false == finder->getNextPathShortestPathResult(result));
  }


  SECTION("no path exists") {
    auto start = velocypack::Parser::fromJson("\"v/0\"");
    auto end = velocypack::Parser::fromJson("\"v/1\"");
    ShortestPathResult result;

    finder->startKShortestPathsTraversal(start->slice(), end->slice());
    REQUIRE(false == finder->getNextPathShortestPathResult(result));
    // Repeat to see that we keep returning false and don't crash
    REQUIRE(false == finder->getNextPathShortestPathResult(result));
  }

  SECTION("path of length 1") {
    auto start = velocypack::Parser::fromJson("\"v/1\"");
    auto end = velocypack::Parser::fromJson("\"v/2\"");
    ShortestPathResult result;
    std::string msgs;

    finder->startKShortestPathsTraversal(start->slice(), end->slice());

    REQUIRE(true == finder->getNextPathShortestPathResult(result));
    auto cpr = checkPath(spo, result, {"1", "2"}, {{}, {"v/1", "v/2"}}, msgs);
    REQUIRE_MESSAGE(cpr, msgs);
  }

  SECTION("path of length 4") {
    auto start = velocypack::Parser::fromJson("\"v/1\"");
    auto end = velocypack::Parser::fromJson("\"v/4\"");
    ShortestPathResult result;
    std::string msgs;

    finder->startKShortestPathsTraversal(start->slice(), end->slice());

    REQUIRE(true == finder->getNextPathShortestPathResult(result));
    auto cpr = checkPath(spo, result, {"1", "2", "3", "4"},
                    {{}, {"v/1", "v/2"}, {"v/2", "v/3"}, {"v/3", "v/4"}}, msgs);
    REQUIRE_MESSAGE(cpr, msgs);
  }

  SECTION("path of length 5 with loops to start/end") {
    auto start = velocypack::Parser::fromJson("\"v/30\"");
    auto end = velocypack::Parser::fromJson("\"v/35\"");
    ShortestPathResult result;

    finder->startKShortestPathsTraversal(start->slice(), end->slice());

    REQUIRE(true == finder->getNextPathShortestPathResult(result));
    REQUIRE(result.length() == 5);
  }

  SECTION("two paths of length 5") {
    auto start = velocypack::Parser::fromJson("\"v/21\"");
    auto end = velocypack::Parser::fromJson("\"v/25\"");
    ShortestPathResult result;
    std::string msgs;

    finder->startKShortestPathsTraversal(start->slice(), end->slice());

    REQUIRE(true == finder->getNextPathShortestPathResult(result));
    auto cpr = checkPath(spo, result, {"21", "22", "23", "24", "25"},
                         {{},
                          {"v/21", "v/22"},
                          {"v/22", "v/23"},
                          {"v/23", "v/24"},
                          {"v/24", "v/25"}},
                         msgs) ||
               checkPath(spo, result, {"21", "26", "27", "28", "25"},
                         {{},
                          {"v/21", "v/26"},
                          {"v/26", "v/27"},
                          {"v/27", "v/28"},
                          {"v/28", "v/25"}},
                         msgs);
    REQUIRE_MESSAGE(cpr, msgs);
    REQUIRE(true == finder->getNextPathShortestPathResult(result));
    cpr = checkPath(spo, result, {"21", "22", "23", "24", "25"},
                    {{},
                     {"v/21", "v/22"},
                     {"v/22", "v/23"},
                     {"v/23", "v/24"},
                     {"v/24", "v/25"}},
                    msgs) ||
          checkPath(spo, result, {"21", "26", "27", "28", "25"},
                    {{},
                     {"v/21", "v/26"},
                     {"v/26", "v/27"},
                     {"v/27", "v/28"},
                     {"v/28", "v/25"}},
                    msgs);
    REQUIRE_MESSAGE(cpr, msgs);
  }

  SECTION("many edges between two nodes") {
    auto start = velocypack::Parser::fromJson("\"v/70\"");
    auto end = velocypack::Parser::fromJson("\"v/71\"");
    ShortestPathResult result;

    finder->startKShortestPathsTraversal(start->slice(), end->slice());

    REQUIRE(true == finder->getNextPathShortestPathResult(result));
    REQUIRE(true == finder->getNextPathShortestPathResult(result));
    REQUIRE(true == finder->getNextPathShortestPathResult(result));
    REQUIRE(false == finder->getNextPathShortestPathResult(result));
  }

  delete finder;
}
}  // namespace graph
}  // namespace tests
}  // namespace arangodb
