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
#include "Aql/AqlItemBlockHelper.h"
#include "Aql/AqlItemBlockManager.h"
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

#include "../Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::tests::mocks;

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

  std::vector<std::string> const& findPath(std::pair<std::string, std::string> const& src) const {
    for (auto const& p : _paths) {
      if (p.front() == src.first && p.back() == src.second) {
        return p;
      }
    }
    return _theEmptyPath;
  }

  std::pair<std::string, std::string> const& calledAt(size_t index) {
    TRI_ASSERT(index < _calledWith.size());
    return _calledWith[index];
  }

  [[nodiscard]] auto getCalledWith() -> std::vector<std::pair<std::string, std::string>> {
    return _calledWith;
  };

  // Needs to provide lookupFunctionality for Cache
 private:
  std::vector<std::vector<std::string>> _paths;
  std::vector<std::pair<std::string, std::string>> _calledWith;
  std::vector<std::string> const _theEmptyPath{};
  TokenTranslator& _translator;
};

struct TestShortestPathOptions : public ShortestPathOptions {
  TestShortestPathOptions(Query* query) : ShortestPathOptions(query) {
    std::unique_ptr<TraverserCache> cache = std::make_unique<TokenTranslator>(query, this);
    injectTestCache(std::move(cache));
  }
};

using Vertex = ShortestPathExecutorInfos::InputVertex;
using RegisterSet = std::unordered_set<RegisterId>;
using RegisterMapping =
    std::unordered_map<ShortestPathExecutorInfos::OutputName, RegisterId, ShortestPathExecutorInfos::OutputNameHash>;
using PathSequence = std::vector<std::vector<std::string>>;
using EdgeSequence = std::vector<std::pair<std::string, std::string>>;

enum class ShortestPathOutput { VERTEX_ONLY, VERTEX_AND_EDGE };

// TODO: this needs a << operator
struct ShortestPathTestParameters {
  static RegisterSet _makeOutputRegisters(ShortestPathOutput in) {
    switch (in) {
      case ShortestPathOutput::VERTEX_ONLY:
        return RegisterSet{std::initializer_list<RegisterId>{2}};
      case ShortestPathOutput::VERTEX_AND_EDGE:
        return RegisterSet{std::initializer_list<RegisterId>{2, 3}};
    }
    return RegisterSet{};
  }
  static RegisterMapping _makeRegisterMapping(ShortestPathOutput in) {
    switch (in) {
      case ShortestPathOutput::VERTEX_ONLY:
        return RegisterMapping{{ShortestPathExecutorInfos::OutputName::VERTEX, 2}};
        break;
      case ShortestPathOutput::VERTEX_AND_EDGE:
        return RegisterMapping{{ShortestPathExecutorInfos::OutputName::VERTEX, 2},
                               {ShortestPathExecutorInfos::OutputName::EDGE, 3}};
    }
    return RegisterMapping{};
  }

  ShortestPathTestParameters(std::tuple<Vertex, Vertex, MatrixBuilder<2>, PathSequence, AqlCall, ShortestPathOutput> params)
      : _source(std::get<0>(params)),
        _target(std::get<1>(params)),
        _outputRegisters(_makeOutputRegisters(std::get<5>(params))),
        _registerMapping(_makeRegisterMapping(std::get<5>(params))),
        _inputMatrix{std::get<2>(params)},
        _paths(std::get<3>(params)),
        _call(std::get<4>(params)) {}

  Vertex _source;
  Vertex _target;
  RegisterSet _inputRegisters;
  RegisterSet _outputRegisters;
  RegisterMapping _registerMapping;
  MatrixBuilder<2> _inputMatrix;
  PathSequence _paths;
  AqlCall _call;
};

class ShortestPathExecutorTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<Vertex, Vertex, MatrixBuilder<2>, PathSequence, AqlCall, ShortestPathOutput>> {
 protected:
  MockAqlServer server;
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;

  std::unique_ptr<arangodb::aql::Query> fakedQuery;
  TestShortestPathOptions options;
  TokenTranslator& translator;

  // parameters are copied because they are const otherwise
  // and that doesn't mix with std::move
  ShortestPathTestParameters parameters;
  ShortestPathExecutorInfos infos;

  FakePathFinder& finder;

  SharedAqlItemBlockPtr inputBlock;
  AqlItemBlockInputRange input;

  std::shared_ptr<Builder> fakeUnusedBlock;
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher;

  ShortestPathExecutor testee;
  OutputAqlItemRow output;

  ShortestPathExecutorTest()
      : server{},
        itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        // 1000 rows, 4 registers
        block(new AqlItemBlock(itemBlockManager, 1000, 4)),
        fakedQuery(server.createFakeQuery()),
        options(fakedQuery.get()),
        translator(*(static_cast<TokenTranslator*>(options.cache()))),
        parameters(GetParam()),
        infos(std::make_shared<RegisterSet>(parameters._inputRegisters),
              std::make_shared<RegisterSet>(parameters._outputRegisters), 2, 4,
              {}, {0, 1}, std::make_unique<FakePathFinder>(options, translator),
              std::move(parameters._registerMapping),
              std::move(parameters._source), std::move(parameters._target)),
        finder(static_cast<FakePathFinder&>(infos.finder())),
        inputBlock(buildBlock<2>(itemBlockManager, std::move(parameters._inputMatrix))),
        input(AqlItemBlockInputRange(ExecutorState::DONE, inputBlock, 0,
                                     inputBlock->size())),
        fakeUnusedBlock(VPackParser::fromJson("[]")),
        fetcher(itemBlockManager, fakeUnusedBlock->steal(), false),
        testee(fetcher, infos),
        output(std::move(block), infos.getOutputRegisters(),
               infos.registersToKeep(), infos.registersToClear()) {
    for (auto&& p : parameters._paths) {
      finder.addPath(std::move(p));
    }

    // We need the limits to verify the outputs later
    AqlCall passCall{parameters._call};
    output.setCall(std::move(passCall));
  }

  size_t ExpectedNumberOfRowsProduced(size_t expectedFound) {
    if (parameters._call.getOffset() >= expectedFound) {
      return 0;
    } else {
      expectedFound -= parameters._call.getOffset();
    }
    if (parameters._call.getLimit() >= expectedFound) {
      return expectedFound;
    } else {
      return parameters._call.getLimit();
    }
  }

  void ValidateResult(OutputAqlItemRow& result, size_t skipped) {
    auto pathsQueriedBetween = finder.getCalledWith();

    FakePathFinder& finder = static_cast<FakePathFinder&>(infos.finder());
    TokenTranslator& translator = *(static_cast<TokenTranslator*>(infos.cache()));
    auto block = result.stealBlock();

    auto expectedRowsFound = std::vector<std::string>{};
    auto expectedPathStarts = std::set<size_t>{};
    for (auto&& p : pathsQueriedBetween) {
      auto& f = finder.findPath(p);
      expectedPathStarts.insert(expectedRowsFound.size());
      expectedRowsFound.insert(expectedRowsFound.end(), f.begin(), f.end());
    }

    auto expectedNrRowsSkipped =
        std::min(parameters._call.getOffset(), expectedRowsFound.size());

    auto expectedNrRowsProduced = ExpectedNumberOfRowsProduced(expectedRowsFound.size());

    EXPECT_EQ(skipped, expectedNrRowsSkipped);

    // No outputs, this means either we were limited to 0, or we got only
    // inputs that did not yield any paths, otherwise we need to fail.
    if (block == nullptr) {
      EXPECT_EQ(expectedNrRowsProduced, 0);
      return;
    }

    ASSERT_NE(block, nullptr);

    for (size_t blockIndex = 0; blockIndex < block->size(); ++blockIndex) {
      if (infos.usesOutputRegister(ShortestPathExecutorInfos::VERTEX)) {
        AqlValue value =
            block->getValue(blockIndex,
                            infos.getOutputRegister(ShortestPathExecutorInfos::VERTEX));
        EXPECT_TRUE(value.isObject());
        EXPECT_TRUE(arangodb::basics::VelocyPackHelper::compare(
                        value.slice(),
                        translator.translateVertex(arangodb::velocypack::StringRef(
                            expectedRowsFound[blockIndex + skipped])),
                        false) == 0);
      }
      if (infos.usesOutputRegister(ShortestPathExecutorInfos::EDGE)) {
        AqlValue value =
            block->getValue(blockIndex,
                            infos.getOutputRegister(ShortestPathExecutorInfos::EDGE));

        if (expectedPathStarts.find(blockIndex + skipped) != expectedPathStarts.end()) {
          EXPECT_TRUE(value.isNull(false));
        } else {
          EXPECT_TRUE(value.isObject());
          VPackSlice edge = value.slice();
          // FROM and TO checks are enough here.
          EXPECT_TRUE(arangodb::velocypack::StringRef(edge.get(StaticStrings::FromString))
                          .compare(expectedRowsFound[blockIndex + skipped - 1]) == 0);
          EXPECT_TRUE(arangodb::velocypack::StringRef(edge.get(StaticStrings::ToString))
                          .compare(expectedRowsFound[blockIndex + skipped]) == 0);
        }
      }
    }
  }

  void TestExecutor(ShortestPathExecutorInfos& infos, AqlItemBlockInputRange& input) {
    // This fetcher will not be called!
    // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
    // This will fetch everything now, unless we give a small enough atMost

    // We use a copy here because skipRowsRange modifies the the call
    auto skipCall = AqlCall{parameters._call};

    auto const [skipState, skipped, resultSkipCall] = testee.skipRowsRange(input, skipCall);

    // TODO: Do we want to assert more here?
    EXPECT_TRUE(skipState == ExecutorState::HASMORE || skipState == ExecutorState::DONE);

    // TODO: What (is supposed to) happen(s) if skip already returned DONE?
    auto const [produceState, stats, resultProduceCall] =
        testee.produceRows(input, output);

    // TODO: should we make a way to assert the state?
    EXPECT_TRUE(produceState == ExecutorState::HASMORE || produceState == ExecutorState::DONE);

    ValidateResult(output, skipped);
  }
};

TEST_P(ShortestPathExecutorTest, the_test) { TestExecutor(infos, input); }

Vertex const constSource("vertex/source"), constTarget("vertex/target"),
    regSource(0), regTarget(1), brokenSource{"IwillBreakYourSearch"},
    brokenTarget{"I will also break your search"};
MatrixBuilder<2> const noneRow{{{{}}}};
MatrixBuilder<2> const oneRow{{{{R"("vertex/source")"}, {R"("vertex/target")"}}}};
MatrixBuilder<2> const twoRows{{{{R"("vertex/source")"}, {R"("vertex/target")"}}},
                               {{{R"("vertex/a")"}, {R"("vertex/b")"}}}};
MatrixBuilder<2> const threeRows{{{{R"("vertex/source")"}, {R"("vertex/target")"}}},
                                 {{{R"("vertex/a")"}, {R"("vertex/b")"}}},
                                 {{{R"("vertex/a")"}, {R"("vertex/target")"}}}};

std::vector<std::vector<std::string>> const noPath = {};
std::vector<std::vector<std::string>> const onePath = {
    {"vertex/source", "vertex/intermed", "vertex/target"}};
std::vector<std::vector<std::string>> const threePaths = {
    {"vertex/source", "vertex/intermed", "vertex/target"},
    {"vertex/a", "vertex/b", "vertex/c", "vertex/d"},
    {"vertex/source", "vertex/b", "vertex/c", "vertex/d"},
    {"vertex/a", "vertex/b", "vertex/target"}};

auto generateALongerPath(size_t n) -> std::vector<std::vector<std::string>> {
  auto path = std::vector<std::string>{};
  path.push_back("vertex/source");
  for (size_t i = 0; i < n; ++i) {
    path.push_back(std::to_string(i));
  }
  path.push_back("vertex/target");
  return {path};
}

auto sources = testing::Values(constSource, regSource, brokenSource);
auto targets = testing::Values(constTarget, regTarget, brokenTarget);
auto inputs = testing::Values(noneRow, oneRow, twoRows, threeRows);
auto paths = testing::Values(noPath, onePath, threePaths,
                             generateALongerPath(999), generateALongerPath(1000),
                             generateALongerPath(1001), generateALongerPath(2000));
auto calls = testing::Values(AqlCall{}, AqlCall{0, 0, 0, false}, AqlCall{0, 1, 0, false},
                             AqlCall{0, 0, 1, false}, AqlCall{0, 1, 1, false},
                             AqlCall{1, 1, 1}, AqlCall{100, 1, 1}, AqlCall{1000});
auto variants = testing::Values(ShortestPathOutput::VERTEX_ONLY,
                                ShortestPathOutput::VERTEX_AND_EDGE);

INSTANTIATE_TEST_CASE_P(ShortestPathExecutorTestInstance, ShortestPathExecutorTest,
                        testing::Combine(sources, targets, inputs, paths, calls, variants));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
