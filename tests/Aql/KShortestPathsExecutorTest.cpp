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
#include "Aql/KShortestPathsExecutor.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/ResourceUsage.h"
#include "Aql/Stats.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/GraphTestTools.h"
#include "Graph/KShortestPathsFinder.h"
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

using Vertex = KShortestPathsExecutorInfos::InputVertex;
using RegisterSet = std::unordered_set<RegisterId>;
using Path = std::vector<std::string>;
using PathSequence = std::vector<Path>;

class FakeKShortestPathsFinder : public KShortestPathsFinder {
 public:
  FakeKShortestPathsFinder(ShortestPathOptions& options, PathSequence const& kpaths)
      : KShortestPathsFinder(options), _kpaths(kpaths), _pathAvailable(false) {}
  ~FakeKShortestPathsFinder() = default;

  bool startKShortestPathsTraversal(Slice const& start, Slice const& end) override {
    _source = std::string{start.copyString()};
    _target = std::string{end.copyString()};

    EXPECT_NE(_source, "");
    EXPECT_NE(_target, "");
    EXPECT_NE(_source, _target);

    _finder = std::begin(_kpaths);
    _pathAvailable = true;
    return true;
  }

  bool getNextPathAql(Builder& builder) override {
    while (_finder != std::end(_kpaths)) {
      if (_finder->front() == _source && _finder->back() == _target) {
        _pathsProduced.emplace_back(*_finder);

        // fill builder with something sensible?
        builder.openArray();
        for (auto&& v : *_finder) {
          builder.add(VPackValue(v));
        }
        builder.close();
        _finder++;
        return true;
      } else {
        _finder++;
      }
    }
    _pathAvailable = false;
    return false;
  }

  bool isPathAvailable() const override { return _pathAvailable; }

  PathSequence& getPathsProduced() noexcept { return _pathsProduced; }

 private:
  // We emulate a number of paths between PathPair
  PathSequence const& _kpaths;
  std::string _source;
  std::string _target;
  bool _pathAvailable;
  PathSequence::const_iterator _finder;
  PathSequence _pathsProduced;
};

// TODO: this needs a << operator
struct KShortestPathsTestParameters {
  KShortestPathsTestParameters(Vertex source, Vertex target, RegisterId vertexOut,
                               MatrixBuilder<2> matrix, PathSequence paths)
      : _source(source),
        _target(target),
        _outputRegisters(std::initializer_list<RegisterId>{vertexOut}),
        _inputMatrix{matrix},
        _paths(paths){};

  KShortestPathsTestParameters(Vertex source, Vertex target, RegisterId vertexOut,
                               RegisterId edgeOut, MatrixBuilder<2> matrix, PathSequence paths)
      : _source(source),
        _target(target),
        _outputRegisters(std::initializer_list<RegisterId>{vertexOut, edgeOut}),
        _inputMatrix{matrix},
        _paths(paths){};

  Vertex _source;
  Vertex _target;
  RegisterSet _inputRegisters;
  RegisterSet _outputRegisters;
  MatrixBuilder<2> _inputMatrix;
  PathSequence _paths;
};

class KShortestPathsExecutorTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<KShortestPathsTestParameters> {
 protected:
  MockAqlServer server;
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;

  std::unique_ptr<arangodb::aql::Query> fakedQuery;
  ShortestPathOptions options;

  // parameters are copied because they are const otherwise
  // and that doesn't mix with std::move
  KShortestPathsTestParameters parameters;
  KShortestPathsExecutorInfos infos;

  FakeKShortestPathsFinder& finder;

  SharedAqlItemBlockPtr inputBlock;
  AqlItemBlockInputRange input;

  std::shared_ptr<Builder> fakeUnusedBlock;
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher;

  KShortestPathsExecutor testee;
  //  OutputAqlItemRow output;

  PathSequence expectedPaths;
  size_t resultsToCheck;

  KShortestPathsExecutorTest()
      : server{},
        itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        // 1000 rows, 3 registers
        block(new AqlItemBlock(itemBlockManager, 1000, 3)),
        fakedQuery(server.createFakeQuery()),
        options(fakedQuery.get()),
        parameters(GetParam()),
        infos(std::make_shared<RegisterSet>(parameters._inputRegisters),
              std::make_shared<RegisterSet>(parameters._outputRegisters), 2, 3, {}, {0},
              std::make_unique<FakeKShortestPathsFinder>(options, parameters._paths),
              std::move(parameters._source), std::move(parameters._target)),
        finder(static_cast<FakeKShortestPathsFinder&>(infos.finder())),
        inputBlock(buildBlock<2>(itemBlockManager, std::move(parameters._inputMatrix))),
        input(AqlItemBlockInputRange(ExecutorState::HASMORE, inputBlock, 0,
                                     inputBlock->size())),
        fakeUnusedBlock(VPackParser::fromJson("[]")),
        fetcher(itemBlockManager, fakeUnusedBlock->steal(), false),
        testee(fetcher, infos)
  /*  output(std::move(block), infos.getOutputRegisters(),
      infos.registersToKeep(), infos.registersToClear()) */
  {}

  void ValidateResult(KShortestPathsExecutorInfos& infos,
                      OutputAqlItemRow& result, size_t atMost) {
    auto expectedPaths = finder.getPathsProduced();

    // We expect to be getting exactly the rows returned
    // that we produced with the shortest path finder.
    // in exactly the order they were produced in.

    auto resultsToCheckHere = size_t{std::min(resultsToCheck, atMost)};

    if (resultsToCheckHere > 0) {
      auto resultBlock = result.stealBlock();
      ASSERT_NE(resultBlock, nullptr);
      for (size_t i = 0; i < resultsToCheckHere; ++i) {
        AqlValue value = resultBlock->getValue(i, infos.getOutputRegister());
        EXPECT_TRUE(value.isArray());

        auto verticesResult = VPackArrayIterator(value.slice());
        auto verticesExpected = std::begin(expectedPaths.at(i));

        while (verticesExpected != std::end(expectedPaths.at(i)) &&
               verticesResult != verticesResult.end()) {
          ASSERT_EQ((*verticesResult).copyString(), *verticesExpected);
          verticesResult++;
          verticesExpected++;
        }
        ASSERT_TRUE((verticesExpected == std::end(expectedPaths.at(i))) &&
                    // Yes, really, they didn't implement == for iterators
                    !(verticesResult != verticesResult.end()));
        resultsToCheck--;
      }
    }
  }
  void TestExecutor(KShortestPathsExecutorInfos& infos, AqlItemBlockInputRange& input) {
    // This will fetch everything now, unless we give a small enough atMost

    ExecutorState _state;

    resultsToCheck = expectedPaths.size();
    do {
      block.reset(new AqlItemBlock(itemBlockManager, 1000, 3));
      auto output = OutputAqlItemRow(std::move(block), infos.getOutputRegisters(),
                                     infos.registersToKeep(), infos.registersToClear());
      auto const [state, stats, call] = testee.produceRows(1000, input, output);
      ValidateResult(infos, output, 1000);
      _state = state;
    } while (_state == ExecutorState::HASMORE);
    EXPECT_EQ(_state, ExecutorState::DONE);
    // We checked all results we expected
    ASSERT_EQ(resultsToCheck, 0);
  }
};  // namespace aql

TEST_P(KShortestPathsExecutorTest, the_test) { TestExecutor(infos, input); }

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

PathSequence const noPath = {};
PathSequence const onePath = {
    {"vertex/source", "vertex/intermed", "vertex/target"}};

PathSequence const threePaths = {
    {"vertex/source", "vertex/intermed", "vertex/target"},
    {"vertex/a", "vertex/b", "vertex/c", "vertex/d"},
    {"vertex/source", "vertex/b", "vertex/c", "vertex/d"},
    {"vertex/a", "vertex/b", "vertex/target"}};

PathSequence const somePaths = {
    {"vertex/source", "vertex/intermed0", "vertex/target"},
    {"vertex/a", "vertex/b", "vertex/c", "vertex/d"},
    {"vertex/source", "vertex/intermed1", "vertex/target"},
    {"vertex/source", "vertex/intermed2", "vertex/target"},
    {"vertex/a", "vertex/b", "vertex/c", "vertex/d"},
    {"vertex/source", "vertex/intermed3", "vertex/target"},
    {"vertex/source", "vertex/intermed4", "vertex/target"},
    {"vertex/a", "vertex/b", "vertex/c", "vertex/d"},
    {"vertex/source", "vertex/intermed5", "vertex/target"},
};

// Some of the bigger test cases we should generate and not write out like a caveperson
KShortestPathsTestParameters generateSomeBiggerCase(size_t n) {
  auto paths = PathSequence{};

  for (size_t i = 0; i < n; i++) {
    paths.push_back({"vertex/source", "vertex/intermed0", "vertex/target"});
  }

  return KShortestPathsTestParameters(constSource, constTarget, 2, noneRow, paths);
}

INSTANTIATE_TEST_CASE_P(
    KShortestPathExecutorTestInstance, KShortestPathsExecutorTest,
    testing::Values(
        KShortestPathsTestParameters(constSource, constTarget, 2, noneRow, noPath),
        KShortestPathsTestParameters(constSource, brokenTarget, 2, noneRow, noPath),
        KShortestPathsTestParameters(brokenSource, constTarget, 2, noneRow, noPath),
        KShortestPathsTestParameters(brokenSource, brokenTarget, 2, noneRow, noPath),
        KShortestPathsTestParameters(regSource, constTarget, 2, noneRow, noPath),
        KShortestPathsTestParameters(regSource, brokenTarget, 2, noneRow, noPath),
        KShortestPathsTestParameters(constSource, regTarget, 2, noneRow, noPath),
        KShortestPathsTestParameters(brokenSource, regTarget, 2, noneRow, noPath),

        KShortestPathsTestParameters(constSource, constTarget, 2, noneRow, onePath),

        KShortestPathsTestParameters(constSource, constTarget, 2, noneRow, somePaths),

        KShortestPathsTestParameters(Vertex{"vertex/a"}, Vertex{"vertex/target"},
                                     2, noneRow, threePaths),

        KShortestPathsTestParameters(regSource, regTarget, 2, oneRow, onePath),

        KShortestPathsTestParameters(regSource, regTarget, 2, twoRows, threePaths),

        KShortestPathsTestParameters(regSource, regTarget, 2, threeRows, threePaths),
        generateSomeBiggerCase(999), generateSomeBiggerCase(1500),
        generateSomeBiggerCase(2001)));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
