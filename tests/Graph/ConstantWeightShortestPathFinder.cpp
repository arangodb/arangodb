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
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Graph/algorithm-aliases.h"
#include "Graph/Enumerators/PathEnumeratorInterface.h"
#include "Graph/Enumerators/TwoSidedEnumerator.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/PathManagement/PathValidatorOptions.h"
#include "Graph/ConstantWeightShortestPathFinder.h"
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

class ConstantWeightShortestPathFinderTest : public ::testing::Test {
 protected:
  GraphTestSetup s;
  MockGraphDatabase gdb;

  std::shared_ptr<arangodb::aql::Query> query;
  std::unique_ptr<arangodb::graph::ShortestPathOptions> spo;

  ConstantWeightShortestPathFinder* finder;
  std::unique_ptr<PathEnumeratorInterface> nextFinder;

  ConstantWeightShortestPathFinderTest() : gdb(s.server, "testVocbase") {
    gdb.addVertexCollection("v", 100);
    gdb.addEdgeCollection(
        "e", "v",
        {{1, 2},   {2, 3},   {3, 4},   {5, 4},   {6, 5},   {7, 6},   {8, 7},
         {1, 10},  {10, 11}, {11, 12}, {12, 4},  {12, 5},  {21, 22}, {22, 23},
         {23, 24}, {24, 25}, {21, 26}, {26, 27}, {27, 28}, {28, 25}});

    query = gdb.getQuery("RETURN 1", std::vector<std::string>{"v", "e"});

    spo = gdb.getShortestPathOptions(query.get());

    finder = new ConstantWeightShortestPathFinder(*spo);

    // Transform old style options to Provider Based ones:
    TwoSidedEnumeratorOptions enumeratorOptions{
        0, std::numeric_limits<uint64_t>::max()};
    PathValidatorOptions validatorOptions(spo->tmpVar(),
                                          spo->getExpressionCtx());
    std::pair<std::vector<IndexAccessor>,
              std::unordered_map<uint64_t, std::vector<IndexAccessor>>>
        outIndexes{};
    outIndexes.first = gdb.buildIndexAccessors(query.get(), spo->tmpVar(), false);

    std::pair<std::vector<IndexAccessor>,
              std::unordered_map<uint64_t, std::vector<IndexAccessor>>>
        inIndexes{};
    inIndexes.first = gdb.buildIndexAccessors(query.get(), spo->tmpVar(), true);

    SingleServerBaseProviderOptions forwardProviderOptions(
        spo->tmpVar(), std::move(outIndexes), spo->getExpressionCtx(), {},
        spo->collectionToShard(), spo->getVertexProjections(),
        spo->getEdgeProjections());


    SingleServerBaseProviderOptions backwardProviderOptions(
        spo->tmpVar(), std::move(inIndexes), spo->getExpressionCtx(), {},
        spo->collectionToShard(), spo->getVertexProjections(),
        spo->getEdgeProjections());

    nextFinder = std::make_unique<ShortestPathEnumerator<SingleServerProvider<SingleServerProviderStep>>>(
        SingleServerProvider<SingleServerProviderStep>{spo->query(), std::move(forwardProviderOptions), spo->query().resourceMonitor()},
        SingleServerProvider<SingleServerProviderStep>{spo->query(), std::move(backwardProviderOptions), spo->query().resourceMonitor()},
        std::move(enumeratorOptions), std::move(validatorOptions),
        spo->query().resourceMonitor()
        );
  }

  ~ConstantWeightShortestPathFinderTest() { delete finder; }
};

TEST_F(ConstantWeightShortestPathFinderTest, path_from_vertex_to_itself) {
  auto start = velocypack::Parser::fromJson("\"v/0\"");
  auto end = velocypack::Parser::fromJson("\"v/0\"");
  ShortestPathResult result;

  nextFinder->reset(HashedStringRef{start->slice()}, HashedStringRef{end->slice()}, 0);
  ASSERT_FALSE(nextFinder->isDone());
  std::string msgs;
  auto path = nextFinder->getNextPath_New();
  ASSERT_TRUE(path != nullptr);
  auto cpr =
      checkPath(spo.get(), *path, {"0"}, {}, msgs);
  ASSERT_TRUE(cpr) << msgs;

  ASSERT_TRUE(finder->shortestPath(start->slice(), end->slice(), result));
}

TEST_F(ConstantWeightShortestPathFinderTest, no_path_exists) {
  auto start = velocypack::Parser::fromJson("\"v/0\"");
  auto end = velocypack::Parser::fromJson("\"v/1\"");
  ShortestPathResult result;

  nextFinder->reset(HashedStringRef{start->slice()}, HashedStringRef{end->slice()}, 0);
  ASSERT_FALSE(nextFinder->isDone());
  std::string msgs;
  auto path = nextFinder->getNextPath_New();
  ASSERT_TRUE(path == nullptr);

  ASSERT_FALSE(finder->shortestPath(start->slice(), end->slice(), result));
  EXPECT_EQ(result.length(), 0);
}

TEST_F(ConstantWeightShortestPathFinderTest, path_of_length_1) {
  auto start = velocypack::Parser::fromJson("\"v/1\"");
  auto end = velocypack::Parser::fromJson("\"v/2\"");
  ShortestPathResult result;
  std::string msgs;

  nextFinder->reset(HashedStringRef{start->slice()}, HashedStringRef{end->slice()}, 0);
  ASSERT_FALSE(nextFinder->isDone());
  auto path = nextFinder->getNextPath_New();
  ASSERT_TRUE(path != nullptr);
  auto cpr =
      checkPath(spo.get(), *path, {"1", "2"}, {{"v/1", "v/2"}}, msgs);
  ASSERT_TRUE(cpr) << msgs;

  msgs = "";
  auto rr = finder->shortestPath(start->slice(), end->slice(), result);
  ASSERT_TRUE(rr);

  cpr = checkPath(spo.get(), result, {"1", "2"}, {{}, {"v/1", "v/2"}}, msgs);
  ASSERT_TRUE(cpr) << msgs;
}

TEST_F(ConstantWeightShortestPathFinderTest, path_of_length_4) {
  auto start = velocypack::Parser::fromJson("\"v/1\"");
  auto end = velocypack::Parser::fromJson("\"v/4\"");
  ShortestPathResult result;
  nextFinder->reset(HashedStringRef{start->slice()}, HashedStringRef{end->slice()}, 0);
  ASSERT_FALSE(nextFinder->isDone());
  std::string msgs;
  auto path = nextFinder->getNextPath_New();
  ASSERT_TRUE(path != nullptr);
  auto cpr =
      checkPath(spo.get(), *path, {"1", "2", "3", "4"},
                {{"v/1", "v/2"}, {"v/2", "v/3"}, {"v/3", "v/4"}}, msgs);
  ASSERT_TRUE(cpr) << msgs;

  msgs = "";

  auto rr = finder->shortestPath(start->slice(), end->slice(), result);
  ASSERT_TRUE(rr);
  cpr =
      checkPath(spo.get(), result, {"1", "2", "3", "4"},
                {{}, {"v/1", "v/2"}, {"v/2", "v/3"}, {"v/3", "v/4"}}, msgs);
  ASSERT_TRUE(cpr) << msgs;
}

TEST_F(ConstantWeightShortestPathFinderTest, two_paths_of_length_5) {
  auto start = velocypack::Parser::fromJson("\"v/21\"");
  auto end = velocypack::Parser::fromJson("\"v/25\"");
  nextFinder->reset(HashedStringRef{start->slice()}, HashedStringRef{end->slice()}, 0);
  ASSERT_FALSE(nextFinder->isDone());
  std::string msgs;
  auto path = nextFinder->getNextPath_New();
  ASSERT_TRUE(path != nullptr);
  // One of the two has to be returned
  // This of course leads to output from the LOG_DEVEL in checkPath
  auto cpr =
      checkPath(spo.get(), *path, {"21", "22", "23", "24", "25"},
                 {
                  {"v/21", "v/22"},
                  {"v/22", "v/23"},
                  {"v/23", "v/24"},
                  {"v/24", "v/25"}}, msgs) ||
    checkPath(spo.get(), *path,{"21", "26", "27", "28", "25"},
{
 {"v/21", "v/26"},
 {"v/26", "v/27"},
 {"v/27", "v/28"},
 {"v/28", "v/25"}}, msgs);
  ASSERT_TRUE(cpr) << msgs;

  msgs = "";

  ShortestPathResult result;

  {
    auto rr = finder->shortestPath(start->slice(), end->slice(), result);

    ASSERT_TRUE(rr);
    // One of the two has to be returned
    // This of course leads to output from the LOG_DEVEL in checkPath
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
  }

  {
    auto rr = finder->shortestPath(end->slice(), start->slice(), result);
    ASSERT_FALSE(rr);
  }
}

}  // namespace graph
}  // namespace tests
}  // namespace arangodb
