////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/ResourceUsage.h"
#include "Aql/ShortestPathExecutor.h"
#include "Aql/Stats.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Graph/TraverserCache.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>
#include "tests/Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace aql {

class TokenTranslator : public TraverserCache {
 public:
  TokenTranslator(Query* query) : TraverserCache(query) {}
  ~TokenTranslator(){};

  arangodb::velocypack::StringRef makeVertex(std::string const& id) {
    VPackBuilder vertex;
    vertex.openObject();
    vertex.add(StaticStrings::IdString, VPackValue(id));
    vertex.add(StaticStrings::KeyString, VPackValue(id));  // This is not corect but nevermind we fake it anyways.
    vertex.add(StaticStrings::RevString, VPackValue("123"));  // just to have it there
    vertex.close();
    auto vslice = vertex.slice();
    arangodb::velocypack::StringRef ref(vslice.get(StaticStrings::IdString));
    _dataLake.emplace_back(vertex.steal());
    _vertices.emplace(ref, vslice);
    return ref;
  }

  EdgeDocumentToken makeEdge(std::string const& s, std::string const& t) {
    VPackBuilder edge;
    std::string fromVal = s;
    std::string toVal = t;
    edge.openObject();
    edge.add(StaticStrings::RevString, VPackValue("123"));  // just to have it there
    edge.add(StaticStrings::FromString, VPackValue(fromVal));
    edge.add(StaticStrings::ToString, VPackValue(toVal));
    edge.close();
    auto eslice = edge.slice();
    _dataLake.emplace_back(edge.steal());
    _edges.emplace(eslice);
    return EdgeDocumentToken{eslice};
  }

  VPackSlice translateVertex(arangodb::velocypack::StringRef idString) {
    auto it = _vertices.find(idString);
    TRI_ASSERT(it != _vertices.end());
    return it->second;
  }

  AqlValue fetchVertexAqlResult(arangodb::velocypack::StringRef idString) override {
    return AqlValue{translateVertex(idString)};
  }

  AqlValue fetchEdgeAqlResult(EdgeDocumentToken const& edgeTkn) override {
    auto it = _edges.find(VPackSlice(edgeTkn.vpack()));
    TRI_ASSERT(it != _edges.end());
    return AqlValue{*it};
  }

 private:
  std::vector<std::shared_ptr<VPackBuffer<uint8_t>>> _dataLake;
  std::unordered_map<arangodb::velocypack::StringRef, VPackSlice> _vertices;
  std::unordered_set<VPackSlice> _edges;
};

class FakePathFinder : public ShortestPathFinder {
 public:
  FakePathFinder(ShortestPathOptions& opts, TokenTranslator& translator)
      : ShortestPathFinder(opts), _paths(), _translator(translator) {}

  ~FakePathFinder() = default;

  void addPath(std::vector<std::string>&& path) {
    _paths.emplace_back(std::move(path));
  }

  bool shortestPath(VPackSlice const& source, VPackSlice const& target,
                    arangodb::graph::ShortestPathResult& result) override {
    TRI_ASSERT(source.isString());
    TRI_ASSERT(target.isString());
    _calledWith.emplace_back(std::make_pair(source.copyString(), target.copyString()));
    std::string const s = source.copyString();
    std::string const t = target.copyString();
    for (auto const& p : _paths) {
      if (p.front() == s && p.back() == t) {
        // Found a path
        for (size_t i = 0; i < p.size() - 1; ++i) {
          result.addVertex(_translator.makeVertex(p[i]));
          result.addEdge(_translator.makeEdge(p[i], p[i + 1]));
        }
        result.addVertex(_translator.makeVertex(p.back()));
        return true;
      }
    }
    return false;
  }

  std::vector<std::string> const& findPath(std::pair<std::string, std::string> const& src) {
    for (auto const& p : _paths) {
      if (p.front() == src.first && p.back() == src.second) {
        return p;
      }
    }
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  std::pair<std::string, std::string> const& calledAt(size_t index) {
    TRI_ASSERT(index < _calledWith.size());
    return _calledWith[index];
  }

  // Needs to provide lookupFunctionality for Cache
 private:
  std::vector<std::vector<std::string>> _paths;
  std::vector<std::pair<std::string, std::string>> _calledWith;
  TokenTranslator& _translator;
};

struct TestShortestPathOptions : public ShortestPathOptions {
  TestShortestPathOptions(Query* query) : ShortestPathOptions(query) {
    std::unique_ptr<TraverserCache> cache = std::make_unique<TokenTranslator>(query);
    injectTestCache(std::move(cache));
  }
};

class ShortestPathExecutorTest : public ::testing::Test {
 protected:
  RegisterId sourceIn;
  RegisterId targetIn;
  ShortestPathExecutorInfos::InputVertex constSource;
  ShortestPathExecutorInfos::InputVertex constTarget;
  ShortestPathExecutorInfos::InputVertex regSource;
  ShortestPathExecutorInfos::InputVertex regTarget;
  ShortestPathExecutorInfos::InputVertex brokenSource;
  ShortestPathExecutorInfos::InputVertex brokenTarget;

  ShortestPathExecutorTest()
      : sourceIn(0),
        targetIn(1),
        constSource("vertex/source"),
        constTarget("vertex/target"),
        regSource(sourceIn),
        regTarget(targetIn),
        brokenSource{"IwillBreakYourSearch"},
        brokenTarget{"I will also break your search"} {}

  void ValidateResult(ShortestPathExecutorInfos& infos, OutputAqlItemRow& result,
                      std::vector<std::pair<std::string, std::string>> const& resultPaths) {
    if (!resultPaths.empty()) {
      FakePathFinder& finder = static_cast<FakePathFinder&>(infos.finder());
      TokenTranslator& translator = *(static_cast<TokenTranslator*>(infos.cache()));
      auto block = result.stealBlock();
      ASSERT_TRUE(block != nullptr);
      size_t index = 0;
      for (size_t i = 0; i < resultPaths.size(); ++i) {
        auto path = finder.findPath(resultPaths[i]);
        for (size_t j = 0; j < path.size(); ++j) {
          if (infos.usesOutputRegister(ShortestPathExecutorInfos::VERTEX)) {
            AqlValue value =
                block->getValue(index, infos.getOutputRegister(ShortestPathExecutorInfos::VERTEX));
            EXPECT_TRUE(value.isObject());
            EXPECT_TRUE(arangodb::basics::VelocyPackHelper::compare(
                            value.slice(),
                            translator.translateVertex(arangodb::velocypack::StringRef(path[j])),
                            false) == 0);
          }
          if (infos.usesOutputRegister(ShortestPathExecutorInfos::EDGE)) {
            AqlValue value =
                block->getValue(index, infos.getOutputRegister(ShortestPathExecutorInfos::EDGE));
            if (j == 0) {
              EXPECT_TRUE(value.isNull(false));
            } else {
              EXPECT_TRUE(value.isObject());
              VPackSlice edge = value.slice();
              // FROM and TO checks are enough here.
              EXPECT_TRUE(arangodb::velocypack::StringRef(edge.get(StaticStrings::FromString))
                              .compare(path[j - 1]) == 0);
              EXPECT_TRUE(arangodb::velocypack::StringRef(edge.get(StaticStrings::ToString))
                              .compare(path[j]) == 0);
            }
          }
          ++index;
        }
      }
    }
  }

  void TestExecutor(bool waiting, ShortestPathExecutorInfos& infos,
                    std::shared_ptr<VPackBuilder> const& input,
                    std::vector<std::pair<std::string, std::string>> const& resultPaths) {
    if (waiting) {
      TestExecutorWaiting(infos, input, resultPaths);
    } else {
      TestExecutorNotWaiting(infos, input, resultPaths);
    }
  }

  void TestExecutorWaiting(ShortestPathExecutorInfos& infos,
                           std::shared_ptr<VPackBuilder> const& input,
                           std::vector<std::pair<std::string, std::string>> const& resultPaths) {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager{&monitor};
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 4)};

    NoStats stats{};
    ExecutionState state = ExecutionState::HASMORE;
    auto& finder = dynamic_cast<FakePathFinder&>(infos.finder());

    SingleRowFetcherHelper<false> fetcher(itemBlockManager, input->steal(), true);
    OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear());
    ShortestPathExecutor testee(fetcher, infos);
    // Fetch fullPath
    for (size_t i = 0; i < resultPaths.size(); ++i) {
      EXPECT_TRUE(state == ExecutionState::HASMORE);
      // if we pull, we always wait
      std::tie(state, stats) = testee.produceRows(result);
      EXPECT_TRUE(state == ExecutionState::WAITING);
      EXPECT_TRUE(!result.produced());
      state = ExecutionState::HASMORE;  // For simplicity on path fetching.
      auto path = finder.findPath(resultPaths[i]);
      for (ADB_IGNORE_UNUSED auto const& v : path) {
        ASSERT_TRUE(state == ExecutionState::HASMORE);
        std::tie(state, stats) = testee.produceRows(result);
        EXPECT_TRUE(result.produced());
        result.advanceRow();
      }
      auto gotCalledWith = finder.calledAt(i);
      EXPECT_TRUE(gotCalledWith.first == resultPaths[i].first);
      EXPECT_TRUE(gotCalledWith.second == resultPaths[i].second);
    }
    if (resultPaths.empty()) {
      // Fetch at least twice, one waiting
      std::tie(state, stats) = testee.produceRows(result);
      EXPECT_TRUE(state == ExecutionState::WAITING);
      EXPECT_TRUE(!result.produced());
      // One no findings
      std::tie(state, stats) = testee.produceRows(result);
    }

    EXPECT_TRUE(state == ExecutionState::DONE);
    EXPECT_TRUE(!result.produced());
    ValidateResult(infos, result, resultPaths);
  }

  void TestExecutorNotWaiting(ShortestPathExecutorInfos& infos,
                              std::shared_ptr<VPackBuilder> const& input,
                              std::vector<std::pair<std::string, std::string>> const& resultPaths) {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager{&monitor};
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 4)};

    NoStats stats{};
    ExecutionState state = ExecutionState::HASMORE;
    auto& finder = dynamic_cast<FakePathFinder&>(infos.finder());

    SingleRowFetcherHelper<false> fetcher(itemBlockManager, input->steal(), false);
    OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear());
    ShortestPathExecutor testee(fetcher, infos);
    // Fetch fullPath
    for (size_t i = 0; i < resultPaths.size(); ++i) {
      EXPECT_TRUE(state == ExecutionState::HASMORE);
      auto path = finder.findPath(resultPaths[i]);
      for (ADB_IGNORE_UNUSED auto const& v : path) {
        ASSERT_TRUE(state == ExecutionState::HASMORE);
        std::tie(state, stats) = testee.produceRows(result);
        EXPECT_TRUE(result.produced());
        result.advanceRow();
      }
      auto gotCalledWith = finder.calledAt(i);
      EXPECT_TRUE(gotCalledWith.first == resultPaths[i].first);
      EXPECT_TRUE(gotCalledWith.second == resultPaths[i].second);
    }
    if (resultPaths.empty()) {
      // We need to fetch once
      std::tie(state, stats) = testee.produceRows(result);
    }
    EXPECT_TRUE(!result.produced());
    EXPECT_TRUE(state == ExecutionState::DONE);
    ValidateResult(infos, result, resultPaths);
  }

  void RunSimpleTest(bool waiting, ShortestPathExecutorInfos::InputVertex&& source,
                            ShortestPathExecutorInfos::InputVertex&& target) {
    RegisterId vOutReg = 2;
    mocks::MockAqlServer server{};
    std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
    auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{vOutReg});
    std::unordered_map<ShortestPathExecutorInfos::OutputName, RegisterId, ShortestPathExecutorInfos::OutputNameHash> registerMapping{
        {ShortestPathExecutorInfos::OutputName::VERTEX, vOutReg}};
    TestShortestPathOptions options(fakedQuery.get());
    TokenTranslator& translator = *(static_cast<TokenTranslator*>(options.cache()));
    std::unique_ptr<ShortestPathFinder> finderPtr =
        std::make_unique<FakePathFinder>(options, translator);
    std::shared_ptr<VPackBuilder> input;
    ShortestPathExecutorInfos infos{inputRegisters,
                                    outputRegisters,
                                    2,
                                    4,
                                    {},
                                    {0, 1},
                                    std::move(finderPtr),
                                    std::move(registerMapping),
                                    std::move(source),
                                    std::move(target)};

    std::vector<std::pair<std::string, std::string>> resultPaths;
    resultPaths.clear();
    input = VPackParser::fromJson(R"([["vertex/source","vertex/target"]])");
    TestExecutor(waiting, infos, input, resultPaths);
  }

  void RunTestWithNoRowsUpstream(bool waiting, ShortestPathExecutorInfos::InputVertex&& source,
                                 ShortestPathExecutorInfos::InputVertex&& target,
                                 bool useEdgeOutput) {
    RegisterId vOutReg = 2;
    RegisterId eOutReg = 3;
    mocks::MockAqlServer server{};
    std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
    auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{vOutReg});
    std::unordered_map<ShortestPathExecutorInfos::OutputName, RegisterId, ShortestPathExecutorInfos::OutputNameHash> registerMapping{
        {ShortestPathExecutorInfos::OutputName::VERTEX, vOutReg}};
    if (useEdgeOutput) {
      registerMapping.emplace(ShortestPathExecutorInfos::OutputName::EDGE, eOutReg);
      outputRegisters->emplace(eOutReg);
    }
    TestShortestPathOptions options(fakedQuery.get());
    TokenTranslator& translator = *(static_cast<TokenTranslator*>(options.cache()));
    std::unique_ptr<ShortestPathFinder> finderPtr =
        std::make_unique<FakePathFinder>(options, translator);
    std::shared_ptr<VPackBuilder> input;
    ShortestPathExecutorInfos infos{inputRegisters,
                                    outputRegisters,
                                    2,
                                    4,
                                    {},
                                    {0, 1},
                                    std::move(finderPtr),
                                    std::move(registerMapping),
                                    std::move(source),
                                    std::move(target)};

    std::vector<std::pair<std::string, std::string>> resultPaths;
    resultPaths.clear();
    input = VPackParser::fromJson("[]");
    TestExecutor(waiting, infos, input, resultPaths);
  }

  void RunTestWithRowsUpstreamNoPaths(bool waiting,
                                      ShortestPathExecutorInfos::InputVertex&& source,
                                      ShortestPathExecutorInfos::InputVertex&& target,
                                      bool useEdgeOutput) {
    RegisterId vOutReg = 2;
    RegisterId eOutReg = 3;
    mocks::MockAqlServer server{};
    std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
    auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{vOutReg});
    std::unordered_map<ShortestPathExecutorInfos::OutputName, RegisterId, ShortestPathExecutorInfos::OutputNameHash> registerMapping{
        {ShortestPathExecutorInfos::OutputName::VERTEX, vOutReg}};
    if (useEdgeOutput) {
      registerMapping.emplace(ShortestPathExecutorInfos::OutputName::EDGE, eOutReg);
      outputRegisters->emplace(eOutReg);
    }
    TestShortestPathOptions options(fakedQuery.get());
    TokenTranslator& translator = *(static_cast<TokenTranslator*>(options.cache()));
    std::unique_ptr<ShortestPathFinder> finderPtr =
        std::make_unique<FakePathFinder>(options, translator);
    std::shared_ptr<VPackBuilder> input;
    ShortestPathExecutorInfos infos{inputRegisters,
                                    outputRegisters,
                                    2,
                                    4,
                                    {},
                                    {0, 1},
                                    std::move(finderPtr),
                                    std::move(registerMapping),
                                    std::move(source),
                                    std::move(target)};

    std::vector<std::pair<std::string, std::string>> resultPaths;
    input = VPackParser::fromJson(R"([["vertex/source","vertex/target"]])");
    TestExecutor(waiting, infos, input, resultPaths);
  }

  void RunTestWithRowsUpstreamOnePath(bool waiting,
                                      ShortestPathExecutorInfos::InputVertex&& source,
                                      ShortestPathExecutorInfos::InputVertex&& target,
                                      bool useEdgeOutput) {
    RegisterId vOutReg = 2;
    RegisterId eOutReg = 3;
    mocks::MockAqlServer server{};
    std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
    auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{vOutReg});
    std::unordered_map<ShortestPathExecutorInfos::OutputName, RegisterId, ShortestPathExecutorInfos::OutputNameHash> registerMapping{
        {ShortestPathExecutorInfos::OutputName::VERTEX, vOutReg}};
    if (useEdgeOutput) {
      registerMapping.emplace(ShortestPathExecutorInfos::OutputName::EDGE, eOutReg);
      outputRegisters->emplace(eOutReg);
    }
    TestShortestPathOptions options(fakedQuery.get());
    TokenTranslator& translator = *(static_cast<TokenTranslator*>(options.cache()));
    std::unique_ptr<ShortestPathFinder> finderPtr =
        std::make_unique<FakePathFinder>(options, translator);
    std::shared_ptr<VPackBuilder> input;
    ShortestPathExecutorInfos infos{inputRegisters,
                                    outputRegisters,
                                    2,
                                    4,
                                    {},
                                    {0, 1},
                                    std::move(finderPtr),
                                    std::move(registerMapping),
                                    std::move(source),
                                    std::move(target)};

    std::vector<std::pair<std::string, std::string>> resultPaths;
    FakePathFinder& finder = static_cast<FakePathFinder&>(infos.finder());
    input = VPackParser::fromJson(R"([["vertex/source","vertex/target"]])");
    finder.addPath(std::vector<std::string>{"vertex/source", "vertex/intermed",
                                            "vertex/target"});
    resultPaths.emplace_back(std::make_pair("vertex/source", "vertex/target"));
    TestExecutor(waiting, infos, input, resultPaths);
  }

  void RunTestWithMultipleRowsUpstream(bool waiting,
                                       ShortestPathExecutorInfos::InputVertex&& source,
                                       ShortestPathExecutorInfos::InputVertex&& target,
                                       bool useEdgeOutput) {
    RegisterId vOutReg = 2;
    RegisterId eOutReg = 3;
    mocks::MockAqlServer server{};
    std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
    auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{vOutReg});
    std::unordered_map<ShortestPathExecutorInfos::OutputName, RegisterId, ShortestPathExecutorInfos::OutputNameHash> registerMapping{
        {ShortestPathExecutorInfos::OutputName::VERTEX, vOutReg}};
    if (useEdgeOutput) {
      registerMapping.emplace(ShortestPathExecutorInfos::OutputName::EDGE, eOutReg);
      outputRegisters->emplace(eOutReg);
    }
    TestShortestPathOptions options(fakedQuery.get());
    TokenTranslator& translator = *(static_cast<TokenTranslator*>(options.cache()));
    std::unique_ptr<ShortestPathFinder> finderPtr =
        std::make_unique<FakePathFinder>(options, translator);
    std::shared_ptr<VPackBuilder> input;
    ShortestPathExecutorInfos infos{inputRegisters,
                                    outputRegisters,
                                    2,
                                    4,
                                    {},
                                    {0, 1},
                                    std::move(finderPtr),
                                    std::move(registerMapping),
                                    std::move(source),
                                    std::move(target)};

    std::vector<std::pair<std::string, std::string>> resultPaths;
    FakePathFinder& finder = static_cast<FakePathFinder&>(infos.finder());
    input = VPackParser::fromJson(R"([["vertex/source","vertex/target"], ["vertex/a", "vertex/d"]])");
    // We add enough paths for all combinations
    // Otherwise waiting / more / done is getting complicated
    finder.addPath(std::vector<std::string>{"vertex/source", "vertex/intermed",
                                            "vertex/target"});
    finder.addPath(std::vector<std::string>{"vertex/a", "vertex/b", "vertex/c",
                                            "vertex/d"});
    finder.addPath(std::vector<std::string>{"vertex/source", "vertex/b",
                                            "vertex/c", "vertex/d"});
    finder.addPath(
        std::vector<std::string>{"vertex/a", "vertex/b", "vertex/target"});
    resultPaths.emplace_back(std::make_pair("vertex/source", "vertex/target"));
    // Add the expected second path
    if (infos.useRegisterForInput(false)) {
      // Source is register
      if (infos.useRegisterForInput(true)) {
        // Target is register
        resultPaths.emplace_back(std::make_pair("vertex/a", "vertex/d"));
      } else {
        // Target constant
        resultPaths.emplace_back(std::make_pair("vertex/a", "vertex/target"));
      }
    } else {
      // Source is constant
      if (infos.useRegisterForInput(true)) {
        // Target is register
        resultPaths.emplace_back(std::make_pair("vertex/source", "vertex/d"));
      } else {
        // Target constant
        resultPaths.emplace_back(
            std::make_pair("vertex/source", "vertex/target"));
      }
    }
    TestExecutor(waiting, infos, input, resultPaths);
  }
};

// simple tests

TEST_F(ShortestPathExecutorTest, Waiting_TestingInvalidInputs_UsingBrokenStartVertex) {
  RunSimpleTest(true, std::move(brokenSource), std::move(constTarget));
}

TEST_F(ShortestPathExecutorTest, Waiting_TestingInvalidInputs_UsingBrokenEndVertex) {
  RunSimpleTest(true, std::move(constSource), std::move(brokenTarget));
}

TEST_F(ShortestPathExecutorTest, Waiting_TestingInvalidInputs_UsingBrokenStartAndEndVertex) {
  RunSimpleTest(true, std::move(brokenSource), std::move(brokenTarget));
}

TEST_F(ShortestPathExecutorTest, NotWaiting_TestingInvalidInputs_UsingBrokenStartVertex) {
  RunSimpleTest(false, std::move(brokenSource), std::move(constTarget));
}

TEST_F(ShortestPathExecutorTest, NotWaiting_TestingInvalidInputs_UsingBrokenEndVertex) {
  RunSimpleTest(false, std::move(constSource), std::move(brokenTarget));
}

TEST_F(ShortestPathExecutorTest, NotWaiting_TestingInvalidInputs_UsingBrokenStartAndEndVertex) {
  RunSimpleTest(false, std::move(brokenSource), std::move(brokenTarget));
}

// no rows

TEST_F(ShortestPathExecutorTest,
       Waiting_NoRows_UsingVertexOutputOnly_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithNoRowsUpstream(true, std::move(constSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_NoRows_UsingVertexOutputOnly_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithNoRowsUpstream(true, std::move(constSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_NoRows_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithNoRowsUpstream(true, std::move(regSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_NoRows_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithNoRowsUpstream(true, std::move(regSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_NoRows_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithNoRowsUpstream(true, std::move(constSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_NoRows_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithNoRowsUpstream(true, std::move(constSource), std::move(regTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_NoRows_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithNoRowsUpstream(true, std::move(regSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_NoRows_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithNoRowsUpstream(true, std::move(regSource), std::move(regTarget), true);
}

// with rows, no path

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsNoPath_UsingVertexOutputOnly_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(constSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsNoPath_UsingVertexOutputOnly_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(constSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsNoPath_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(regSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsNoPath_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(regSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsNoPath_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(constSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsNoPath_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(constSource), std::move(regTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsNoPath_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(regSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsNoPath_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(regSource), std::move(regTarget), true);
}

// with rows, one path

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsOnePath_UsingVertexOutputOnly_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(constSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsOnePath_UsingVertexOutputOnly_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(constSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsOnePath_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(regSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsOnePath_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(regSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsOnePath_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(constSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsOnePath_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(constSource), std::move(regTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsOnePath_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(regSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithRowsOnePath_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(true, std::move(regSource), std::move(regTarget), true);
}

// with multiple rows

TEST_F(ShortestPathExecutorTest,
       Waiting_WithMultipleRows_UsingVertexOutputOnly_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithMultipleRowsUpstream(true, std::move(constSource),
                                  std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithMultipleRows_UsingVertexOutputOnly_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithMultipleRowsUpstream(true, std::move(constSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithMultipleRows_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithMultipleRowsUpstream(true, std::move(regSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithMultipleRows_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithMultipleRowsUpstream(true, std::move(regSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithMultipleRows_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithMultipleRowsUpstream(true, std::move(constSource),
                                  std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithMultipleRows_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithMultipleRowsUpstream(true, std::move(constSource), std::move(regTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithMultipleRows_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithMultipleRowsUpstream(true, std::move(regSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       Waiting_WithMultipleRows_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithMultipleRowsUpstream(true, std::move(regSource), std::move(regTarget), true);
}

// no rows

TEST_F(ShortestPathExecutorTest,
       NotWaiting_NoRows_UsingVertexOutputOnly_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithNoRowsUpstream(false, std::move(constSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_NoRows_UsingVertexOutputOnly_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithNoRowsUpstream(false, std::move(constSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_NoRows_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithNoRowsUpstream(false, std::move(regSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_NoRows_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithNoRowsUpstream(false, std::move(regSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_NoRows_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithNoRowsUpstream(false, std::move(constSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_NoRows_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithNoRowsUpstream(false, std::move(constSource), std::move(regTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_NoRows_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithNoRowsUpstream(false, std::move(regSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_NoRows_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithNoRowsUpstream(false, std::move(regSource), std::move(regTarget), true);
}

// with rows, no path

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsNoPath_UsingVertexOutputOnly_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(constSource),
                                 std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsNoPath_UsingVertexOutputOnly_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(constSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsNoPath_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(regSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsNoPath_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(regSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsNoPath_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(constSource),
                                 std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsNoPath_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(constSource), std::move(regTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsNoPath_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(regSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsNoPath_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(regSource), std::move(regTarget), true);
}

// with rows, one path

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsOnePath_UsingVertexOutputOnly_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(constSource),
                                 std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsOnePath_UsingVertexOutputOnly_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(constSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsOnePath_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(regSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsOnePath_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(regSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsOnePath_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(constSource),
                                 std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsOnePath_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(constSource), std::move(regTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsOnePath_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(regSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithRowsOnePath_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithRowsUpstreamOnePath(false, std::move(regSource), std::move(regTarget), true);
}

// with multiple rows

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithMultipleRows_UsingVertexOutputOnly_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithMultipleRowsUpstream(false, std::move(constSource),
                                  std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithMultipleRows_UsingVertexOutputOnly_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithMultipleRowsUpstream(false, std::move(constSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithMultipleRows_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithMultipleRowsUpstream(false, std::move(regSource), std::move(constTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithMultipleRows_UsingVertexOutputOnly_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithMultipleRowsUpstream(false, std::move(regSource), std::move(regTarget), false);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithMultipleRows_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingConstantTargetInput) {
  RunTestWithMultipleRowsUpstream(false, std::move(constSource),
                                  std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithMultipleRows_UsingVertexAndEdgeOutput_UsingConstantSourceInput_UsingRegisterTargetInput) {
  RunTestWithMultipleRowsUpstream(false, std::move(constSource), std::move(regTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithMultipleRows_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingConstantTargetInput) {
  RunTestWithMultipleRowsUpstream(false, std::move(regSource), std::move(constTarget), true);
}

TEST_F(ShortestPathExecutorTest,
       NotWaiting_WithMultipleRows_UsingVertexAndEdgeOutput_UsingRegisterSourceInput_UsingRegisterTargetInput) {
  RunTestWithMultipleRowsUpstream(false, std::move(regSource), std::move(regTarget), true);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
