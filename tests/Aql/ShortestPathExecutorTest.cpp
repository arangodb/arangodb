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

using Vertex = ShortestPathExecutorInfos::InputVertex;
using RegisterSet = std::unordered_set<RegisterId>;
using RegisterMapping =
    std::unordered_map<ShortestPathExecutorInfos::OutputName, RegisterId, ShortestPathExecutorInfos::OutputNameHash>;
using PathSequence = std::vector<std::vector<std::string>>;
using EdgeSequence = std::vector<std::pair<std::string, std::string>>;

// TODO: this needs a << operator
struct ShortestPathTestParameters {
  ShortestPathTestParameters(Vertex source, Vertex target, RegisterId vertexOut,
                             MatrixBuilder<2> matrix, PathSequence paths,
                             EdgeSequence resultPaths, AqlCall call = AqlCall{})
      : _source(source),
        _target(target),
        _outputRegisters(std::initializer_list<RegisterId>{vertexOut}),
        _registerMapping({{ShortestPathExecutorInfos::OutputName::VERTEX, vertexOut}}),
        _inputMatrix{matrix},
        _paths(paths),
        _resultPaths{resultPaths},
        _call(call){};

  ShortestPathTestParameters(Vertex source, Vertex target, RegisterId vertexOut,
                             RegisterId edgeOut, MatrixBuilder<2> matrix, PathSequence paths,
                             EdgeSequence resultPaths, AqlCall call = AqlCall{})
      : _source(source),
        _target(target),
        _outputRegisters(std::initializer_list<RegisterId>{vertexOut, edgeOut}),
        _registerMapping({{ShortestPathExecutorInfos::OutputName::VERTEX, vertexOut},
                          {ShortestPathExecutorInfos::OutputName::EDGE, edgeOut}}),
        _inputMatrix{matrix},
        _paths(paths),
        _resultPaths(resultPaths),
        _call(call){};

  Vertex _source;
  Vertex _target;
  RegisterSet _inputRegisters;
  RegisterSet _outputRegisters;
  RegisterMapping _registerMapping;
  MatrixBuilder<2> _inputMatrix;
  PathSequence _paths;
  EdgeSequence _resultPaths;
  AqlCall _call;
};

class ShortestPathExecutorTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ShortestPathTestParameters> {
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
        output(std::move(block), infos.getOutputRegisters(), infos.registersToKeep(),
               infos.registersToClear(), std::move(parameters._call)) {
    for (auto&& p : parameters._paths) {
      finder.addPath(std::move(p));
    }
  }

  void ValidateResult(ShortestPathExecutorInfos& infos, OutputAqlItemRow& result,
                      std::vector<std::pair<std::string, std::string>> const& resultPaths,
                      ExecutorState const state) {
    LOG_DEVEL << "num rows written: " << result.numRowsWritten();
    if (resultPaths.empty()) {
      //  TODO: this is really crude, but we can't currently, easily determine whether we got
      //        *exactly* the paths we were hoping  for.
      ASSERT_EQ(result.numRowsWritten(), 0);
    } else {
      FakePathFinder& finder = static_cast<FakePathFinder&>(infos.finder());
      TokenTranslator& translator = *(static_cast<TokenTranslator*>(infos.cache()));
      auto block = result.stealBlock();
      ASSERT_NE(block, nullptr);
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

  void TestExecutor(ShortestPathExecutorInfos& infos, AqlItemBlockInputRange& input,
                    std::vector<std::pair<std::string, std::string>> const& resultPaths) {
    // This fetcher will not be called!
    // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
    // This will fetch everything now, unless we give a small enough atMost

    auto const [state, stats, call] = testee.produceRows(input, output);
    // TODO: validate stats

    ValidateResult(infos, output, resultPaths, state);
  }
};  // namespace aql

TEST_P(ShortestPathExecutorTest, the_test) {
  TestExecutor(infos, input, parameters._resultPaths);
}

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

std::vector<std::vector<std::string>> const onePath = {
    {"vertex/source", "vertex/intermed", "vertex/target"}};

std::vector<std::vector<std::string>> const threePaths = {
    {"vertex/source", "vertex/intermed", "vertex/target"},
    {"vertex/a", "vertex/b", "vertex/c", "vertex/d"},
    {"vertex/source", "vertex/b", "vertex/c", "vertex/d"},
    {"vertex/a", "vertex/b", "vertex/target"}};

INSTANTIATE_TEST_CASE_P(
    ShortestPathExecutorTestInstance, ShortestPathExecutorTest,
    testing::Values(
        // No edge output
        ShortestPathTestParameters(constSource, constTarget, 2, noneRow, {}, {}),
        ShortestPathTestParameters(constSource, brokenTarget, 2, noneRow, {}, {}),
        ShortestPathTestParameters(brokenSource, constTarget, 2, noneRow, {}, {}),
        ShortestPathTestParameters(brokenSource, brokenTarget, 2, noneRow, {}, {}),
        ShortestPathTestParameters(regSource, constTarget, 2, noneRow, {}, {}),
        ShortestPathTestParameters(regSource, brokenTarget, 2, noneRow, {}, {}),
        ShortestPathTestParameters(constSource, regTarget, 2, noneRow, {}, {}),
        ShortestPathTestParameters(brokenSource, regTarget, 2, noneRow, {}, {}),

        ShortestPathTestParameters(constSource, constTarget, 2, noneRow, onePath,
                                   {{"vertex/source", "vertex/target"}}),

        ShortestPathTestParameters(Vertex{"vertex/a"}, Vertex{"vertex/target"}, 2, noneRow,
                                   threePaths, {{"vertex/a", "vertex/target"}}),

        ShortestPathTestParameters(regSource, regTarget, 2, oneRow, onePath,
                                   {{"vertex/source", "vertex/target"}}),

        ShortestPathTestParameters(regSource, regTarget, 2, twoRows, threePaths,
                                   {{"vertex/source", "vertex/target"}}),

        ShortestPathTestParameters(regSource, regTarget, 2, threeRows, threePaths,
                                   {{"vertex/source", "vertex/target"},
                                    {"vertex/a", "vertex/target"}}),

        ShortestPathTestParameters(constSource, constTarget, 2, noneRow,
                                   onePath, {{"vertex/source", "vertex/target"}},
                                   AqlCall{0, 1, 0, false}),

        // With edge output
        ShortestPathTestParameters(constSource, constTarget, 2, 3, noneRow, {}, {}),
        ShortestPathTestParameters(constSource, brokenTarget, 2, 3, noneRow, {}, {}),
        ShortestPathTestParameters(brokenSource, constTarget, 2, 3, noneRow, {}, {}),
        ShortestPathTestParameters(brokenSource, brokenTarget, 2, 3, noneRow, {}, {}),
        ShortestPathTestParameters(regSource, constTarget, 2, 3, noneRow, {}, {}),
        ShortestPathTestParameters(regSource, brokenTarget, 2, 3, noneRow, {}, {}),
        ShortestPathTestParameters(constSource, regTarget, 2, 3, noneRow, {}, {}),
        ShortestPathTestParameters(brokenSource, regTarget, 2, 3, noneRow, {}, {})));
}  // namespace aql
}  // namespace tests
}  // namespace arangodb
