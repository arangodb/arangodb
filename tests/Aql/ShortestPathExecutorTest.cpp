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

#include "gtest/gtest.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include "Aql/RowFetcherHelper.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"

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
#include "Graph/TraverserOptions.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace aql {

class TokenTranslator : public TraverserCache {
 public:
  TokenTranslator(Query* query, BaseOptions* opts)
      : TraverserCache(query, opts),
        _edges(11, arangodb::basics::VelocyPackHelper::VPackHash(),
               arangodb::basics::VelocyPackHelper::VPackEqual()) {}
  ~TokenTranslator() = default;

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
  std::unordered_set<VPackSlice, arangodb::basics::VelocyPackHelper::VPackHash, arangodb::basics::VelocyPackHelper::VPackEqual> _edges;
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
    std::unique_ptr<TraverserCache> cache = std::make_unique<TokenTranslator>(query, this);
    injectTestCache(std::move(cache));
  }
};

class ShortestPathExecutorTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::ERR> {
 protected:
  RegisterId vOutReg{2};
  RegisterId eOutReg{3};
  mocks::MockAqlServer server{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery{server.createFakeQuery()};
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

  void ValidateVertex(SharedAqlItemBlockPtr const& block, ShortestPathExecutorInfos& infos,
                      std::vector<std::string> const& path, size_t rowIndex,
                      size_t pathIndex) {
    if (infos.usesOutputRegister(ShortestPathExecutorInfos::VERTEX)) {
      TokenTranslator& translator = *(static_cast<TokenTranslator*>(infos.cache()));
      AqlValue value =
          block->getValue(rowIndex, infos.getOutputRegister(ShortestPathExecutorInfos::VERTEX));
      EXPECT_TRUE(value.isObject());
      EXPECT_TRUE(arangodb::basics::VelocyPackHelper::compare(
                      value.slice(),
                      translator.translateVertex(arangodb::velocypack::StringRef(path[pathIndex])),
                      false) == 0);
    }
  }

  void ValidateEdge(SharedAqlItemBlockPtr const& block, ShortestPathExecutorInfos& infos,
                    std::vector<std::string> const& path, size_t rowIndex, size_t pathIndex) {
    if (infos.usesOutputRegister(ShortestPathExecutorInfos::EDGE)) {
      AqlValue value =
          block->getValue(rowIndex, infos.getOutputRegister(ShortestPathExecutorInfos::EDGE));
      if (pathIndex == 0) {
        EXPECT_TRUE(value.isNull(false));
      } else {
        EXPECT_TRUE(value.isObject());
        VPackSlice edge = value.slice();
        // FROM and TO checks are enough here.
        EXPECT_TRUE(arangodb::velocypack::StringRef(edge.get(StaticStrings::FromString))
                        .compare(path[pathIndex - 1]) == 0);
        EXPECT_TRUE(arangodb::velocypack::StringRef(edge.get(StaticStrings::ToString))
                        .compare(path[pathIndex]) == 0);
      }
    }
  }

  void ValidateResult(ShortestPathExecutorInfos& infos, OutputAqlItemRow& result,
                      std::vector<std::pair<std::string, std::string>> const& resultPaths) {
    if (!resultPaths.empty()) {
      FakePathFinder& finder = static_cast<FakePathFinder&>(infos.finder());
      auto block = result.stealBlock();
      ASSERT_NE(block, nullptr);
      size_t index = 0;
      for (size_t i = 0; i < resultPaths.size(); ++i) {
        auto path = finder.findPath(resultPaths[i]);
        for (size_t j = 0; j < path.size(); ++j) {
          ValidateVertex(block, infos, path, index, j);
          ValidateEdge(block, infos, path, index, j);
          ++index;
        }
      }
    }
  }

  void TestExecutor(ShortestPathExecutorInfos& infos,
                    std::shared_ptr<VPackBuilder> const& input,
                    std::vector<std::pair<std::string, std::string>> const& resultPaths) {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

    SharedAqlItemBlockPtr inputBlock =
        vPackBufferToAqlItemBlock(itemBlockManager, input->buffer());
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 4)};

    auto& finder = dynamic_cast<FakePathFinder&>(infos.finder());

    SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
        itemBlockManager, input->steal(), false);
    OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear());
    ShortestPathExecutor testee(fetcher, infos);
    {
      // Test that a correct call is created on an empty input
      AqlItemBlockInputRange emptyRange{ExecutorState::HASMORE};
      auto const [state, stats, call] = testee.produceRows(emptyRange, result);
      EXPECT_EQ(state, ExecutorState::HASMORE);
      // Call, noOffSet, noLimits, noFullCount
      EXPECT_EQ(call.offset, 0);
      EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(call.softLimit));
      EXPECT_FALSE(call.hasHardLimit());
      EXPECT_FALSE(call.needsFullCount());
      // TODO Test stats ?
      ASSERT_FALSE(result.produced());
      EXPECT_EQ(result.numRowsWritten(), 0);
    }

    {
      // Fetch fullPath
      auto inputRange =
          inputBlock == nullptr
              ? AqlItemBlockInputRange{ExecutorState::DONE}
              : AqlItemBlockInputRange{ExecutorState::DONE, inputBlock, 0, 1000};
      auto const [state, stats, call] = testee.produceRows(inputRange, result);
      EXPECT_EQ(state, ExecutorState::DONE);

      // This Call is actually not required if this fails ok to remove.
      // noOffSet, noLimits, noFullCount
      EXPECT_EQ(call.offset, 0);
      EXPECT_TRUE(std::holds_alternative<AqlCall::Infinity>(call.softLimit));
      EXPECT_FALSE(call.hasHardLimit());
      EXPECT_FALSE(call.needsFullCount());

      // TODO Test stats ?

      // Now tests the paths, we expect to have found all.
      // Test order of Invocation
      for (size_t i = 0; i < resultPaths.size(); ++i) {
        auto path = finder.findPath(resultPaths[i]);
        auto gotCalledWith = finder.calledAt(i);
        EXPECT_EQ(gotCalledWith.first, resultPaths[i].first);
        EXPECT_EQ(gotCalledWith.second, resultPaths[i].second);
      }
      // Test Result Contents.
      ValidateResult(infos, result, resultPaths);
    }
  }

  void TestExecutorSingleLineOutput(ShortestPathExecutorInfos& infos,
                                    std::shared_ptr<VPackBuilder> const& input,
                                    std::vector<std::pair<std::string, std::string>> const& resultPaths) {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

    SharedAqlItemBlockPtr inputBlock =
        vPackBufferToAqlItemBlock(itemBlockManager, input->buffer());

    auto& finder = dynamic_cast<FakePathFinder&>(infos.finder());

    SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
        itemBlockManager, input->steal(), false);
    AqlItemBlockInputRange inputRange{ExecutorState::DONE, inputBlock, 0, 1000};
    ShortestPathExecutor testee(fetcher, infos);
    for (size_t i = 0; i < resultPaths.size(); ++i) {
      auto const& path = finder.findPath(resultPaths[i]);
      for (size_t j = 0; j < path.size(); ++j) {
        // We fetch every part of the path in a single step.
        // Input Range contains all we need.
        AqlCall call;
        call.softLimit = 1;
        SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 4)};
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(),
                                infos.registersToClear(), std::move(call));
        auto const [state, stats, upstreamCall] = testee.produceRows(inputRange, result);
        if (j + 1 >= path.size() && i + 1 >= resultPaths.size()) {
          // All produced
          EXPECT_EQ(state, ExecutorState::DONE);
        } else {
          // we still have paths available.
          EXPECT_EQ(state, ExecutorState::HASMORE);
        }
        // We write exactly 1 row at a time
        EXPECT_EQ(result.numRowsWritten(), 1);
        EXPECT_TRUE(result.isFull());
        auto resBlock = result.stealBlock();
        ValidateVertex(resBlock, infos, path, 0, j);
        ValidateEdge(resBlock, infos, path, 0, j);
      }
    }
  }

  void RunSimpleTest(ShortestPathExecutorInfos::InputVertex&& source,
                     ShortestPathExecutorInfos::InputVertex&& target) {
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
    TestExecutor(infos, input, resultPaths);
  }
};

// simple tests

TEST_F(ShortestPathExecutorTest, TestingInvalidInputs_UsingBrokenStartVertex) {
  RunSimpleTest(std::move(brokenSource), std::move(constTarget));
}

TEST_F(ShortestPathExecutorTest, TestingInvalidInputs_UsingBrokenEndVertex) {
  RunSimpleTest(std::move(constSource), std::move(brokenTarget));
}

TEST_F(ShortestPathExecutorTest, TestingInvalidInputs_UsingBrokenStartAndEndVertex) {
  RunSimpleTest(std::move(brokenSource), std::move(brokenTarget));
}

class ShortestPathExecutorTestInputs
    : public ShortestPathExecutorTest,
      public ::testing::WithParamInterface<std::tuple<ShortestPathExecutorInfos::InputVertex, ShortestPathExecutorInfos::InputVertex, bool>> {
 protected:
  std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters{
      std::make_shared<std::unordered_set<RegisterId>>(std::initializer_list<RegisterId>{})};
  std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters{
      std::make_shared<std::unordered_set<RegisterId>>(
          std::initializer_list<RegisterId>{vOutReg})};
  std::unordered_map<ShortestPathExecutorInfos::OutputName, RegisterId, ShortestPathExecutorInfos::OutputNameHash> registerMapping{
      {ShortestPathExecutorInfos::OutputName::VERTEX, vOutReg}};
  TestShortestPathOptions options{fakedQuery.get()};

 public:
  ShortestPathExecutorTestInputs() {
    auto const& [source, target, useEdgeOutput] = GetParam();
    if (useEdgeOutput) {
      registerMapping.emplace(ShortestPathExecutorInfos::OutputName::EDGE, eOutReg);
      outputRegisters->emplace(eOutReg);
    }
  }

  ShortestPathExecutorInfos makeInfos() {
    auto [source, target, useEdgeOutput] = GetParam();
    TokenTranslator& translator = *(static_cast<TokenTranslator*>(options.cache()));
    auto finderPtr = std::make_unique<FakePathFinder>(options, translator);
    return ShortestPathExecutorInfos(inputRegisters, outputRegisters, 2, 4,
                                     std::unordered_set<RegisterId>{},
                                     std::unordered_set<RegisterId>{0, 1},
                                     std::move(finderPtr), std::move(registerMapping),
                                     std::move(source), std::move(target));
  }
};

// no rows
TEST_P(ShortestPathExecutorTestInputs, no_rows) {
  std::shared_ptr<VPackBuilder> input = VPackParser::fromJson("[]");
  std::vector<std::pair<std::string, std::string>> resultPaths;
  auto infos = makeInfos();
  TestExecutor(infos, input, resultPaths);
}

// with rows, no path
TEST_P(ShortestPathExecutorTestInputs, with_rows_no_path) {
  std::shared_ptr<VPackBuilder> input =
      VPackParser::fromJson(R"([["vertex/source","vertex/target"]])");
  std::vector<std::pair<std::string, std::string>> resultPaths;
  auto infos = makeInfos();
  TestExecutor(infos, input, resultPaths);
}

// with rows, one path
TEST_P(ShortestPathExecutorTestInputs, with_rows_one_path) {
  std::shared_ptr<VPackBuilder> input =
      VPackParser::fromJson(R"([["vertex/source","vertex/target"]])");
  std::vector<std::pair<std::string, std::string>> resultPaths;
  auto infos = makeInfos();
  FakePathFinder& finder = static_cast<FakePathFinder&>(infos.finder());
  finder.addPath(std::vector<std::string>{"vertex/source", "vertex/intermed",
                                          "vertex/target"});
  resultPaths.emplace_back(std::make_pair("vertex/source", "vertex/target"));
  TestExecutor(infos, input, resultPaths);
}

// with multiple rows
TEST_P(ShortestPathExecutorTestInputs, with_multiple_rows_path) {
  std::shared_ptr<VPackBuilder> input = VPackParser::fromJson(
      R"([["vertex/source","vertex/target"], ["vertex/a", "vertex/d"]])");
  std::vector<std::pair<std::string, std::string>> resultPaths;
  auto infos = makeInfos();
  FakePathFinder& finder = static_cast<FakePathFinder&>(infos.finder());
  // We add enough paths for all combinations
  // Otherwise waiting / more / done is getting complicated
  finder.addPath(std::vector<std::string>{"vertex/source", "vertex/intermed",
                                          "vertex/target"});
  finder.addPath(
      std::vector<std::string>{"vertex/a", "vertex/b", "vertex/c", "vertex/d"});
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
  TestExecutor(infos, input, resultPaths);
}

TEST_P(ShortestPathExecutorTestInputs, with_1_line_output) {
  std::shared_ptr<VPackBuilder> input = VPackParser::fromJson(
      R"([["vertex/source","vertex/target"], ["vertex/a", "vertex/d"]])");
  std::vector<std::pair<std::string, std::string>> resultPaths;
  auto infos = makeInfos();
  FakePathFinder& finder = static_cast<FakePathFinder&>(infos.finder());
  // We add enough paths for all combinations
  // Otherwise waiting / more / done is getting complicated
  finder.addPath(std::vector<std::string>{"vertex/source", "vertex/intermed",
                                          "vertex/target"});
  finder.addPath(
      std::vector<std::string>{"vertex/a", "vertex/b", "vertex/c", "vertex/d"});
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
  TestExecutorSingleLineOutput(infos, input, resultPaths);
}

static const ShortestPathExecutorInfos::InputVertex constSource2{
    "vertex/source"};
static const ShortestPathExecutorInfos::InputVertex constTarget2{
    "vertex/target"};
static const ShortestPathExecutorInfos::InputVertex regSource2{0};
static const ShortestPathExecutorInfos::InputVertex regTarget2{1};

INSTANTIATE_TEST_CASE_P(
    ShortestPathExecutorPaths, ShortestPathExecutorTestInputs,
    testing::Values(std::tuple{constSource2, constTarget2, false},
                    std::tuple{constSource2, constTarget2, true},
                    std::tuple{constSource2, regTarget2, false},
                    std::tuple{constSource2, regTarget2, true},
                    std::tuple{regSource2, constTarget2, true},
                    std::tuple{regSource2, constTarget2, false},
                    std::tuple{regSource2, regTarget2, false},
                    std::tuple{regSource2, regTarget2, true}),
    [](testing::TestParamInfo<ShortestPathExecutorTestInputs::ParamType> const& info) {
      auto const& [source, target, useEdge] = info.param;
      std::string res = "";
      if (source.type == ::arangodb::aql::ShortestPathExecutorInfos::InputVertex::Type::CONSTANT) {
        res += "Constant";
      } else {
        res += "Register";
      }
      res += "";
      if (target.type == ::arangodb::aql::ShortestPathExecutorInfos::InputVertex::Type::CONSTANT) {
        res += "Constant";
      } else {
        res += "Register";
      }
      res += "";
      if (useEdge) {
        res += "True";
      } else {
        res += "False";
      }
      return res;
    });

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
