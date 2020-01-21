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

// The FakeShortestPathsFinder does not do any real k shortest paths search; it
// is merely initialized with a set of "paths" and then outputs them, keeping a
// record of which paths it produced. This record is used in the validation
// whether the executor output the correct sequence of rows.
class FakeKShortestPathsFinder : public KShortestPathsFinder {
 public:
  FakeKShortestPathsFinder(ShortestPathOptions& options, PathSequence const& kpaths)
      : KShortestPathsFinder(options), _kpaths(kpaths), _pathAvailable(false) {}
  ~FakeKShortestPathsFinder() = default;

  auto gotoNextPath() -> bool {
    EXPECT_NE(_source, "");
    EXPECT_NE(_target, "");
    EXPECT_NE(_source, _target);

    while (_finder != std::end(_kpaths)) {
      if (_finder->front() == _source && _finder->back() == _target) {
        return true;
      }
      _finder++;
    }
    return false;
  }

  bool startKShortestPathsTraversal(Slice const& start, Slice const& end) override {
    _source = std::string{start.copyString()};
    _target = std::string{end.copyString()};

    EXPECT_NE(_source, "");
    EXPECT_NE(_target, "");
    EXPECT_NE(_source, _target);

    _finder = _kpaths.begin();
    _pathAvailable = gotoNextPath();
    return true;
  }

  bool getNextPathAql(Builder& builder) override {
    _pathsProduced.emplace_back(*_finder);
    // fill builder with something sensible?
    builder.openArray();
    for (auto&& v : *_finder) {
      builder.add(VPackValue(v));
    }
    builder.close();
    _pathAvailable = gotoNextPath();
    return _pathAvailable;
  }

  bool skipPath() override {
    Builder builder{};
    return getNextPathAql(builder);
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
  KShortestPathsTestParameters(std::tuple<Vertex, Vertex, MatrixBuilder<2>, PathSequence, AqlCall> params)
      : _source(std::get<0>(params)),
        _target(std::get<1>(params)),
        // TODO: Make output registers configurable?
        _outputRegisters(std::initializer_list<RegisterId>{2}),
        _inputMatrix(std::get<2>(params)),
        _paths(std::get<3>(params)),
        _call(std::get<4>(params)){};

  Vertex _source;
  Vertex _target;
  RegisterSet _inputRegisters;
  RegisterSet _outputRegisters;
  MatrixBuilder<2> _inputMatrix;
  PathSequence _paths;
  AqlCall _call;
};

class KShortestPathsExecutorTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<Vertex, Vertex, MatrixBuilder<2>, PathSequence, AqlCall>> {
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
  OutputAqlItemRow output;

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
        testee(fetcher, infos),
        output(std::move(block), infos.getOutputRegisters(),
               infos.registersToKeep(), infos.registersToClear()) {}

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
    auto resultBlock = result.stealBlock();
    auto pathsFound = finder.getPathsProduced();

    // We expect to be getting exactly the rows returned
    // that we produced with the shortest path finder.
    // in exactly the order they were produced in.

    auto expectedNrRowsSkipped =
        std::min(parameters._call.getOffset(), pathsFound.size());

    auto expectedNrRowsProduced = ExpectedNumberOfRowsProduced(pathsFound.size());
    EXPECT_EQ(skipped, expectedNrRowsSkipped);

    if (resultBlock == nullptr) {
      EXPECT_EQ(expectedNrRowsProduced, 0);
      return;
    }

    ASSERT_NE(resultBlock, nullptr);

    for (auto blockIndex = size_t{skipped}; blockIndex < resultBlock->size(); ++blockIndex) {
      AqlValue value = resultBlock->getValue(blockIndex, infos.getOutputRegister());

      EXPECT_TRUE(value.isArray());

      auto verticesResult = VPackArrayIterator(value.slice());

      auto pathExpected = pathsFound.at(blockIndex);
      auto verticesExpected = std::begin(pathExpected);

      while (verticesExpected != std::end(pathExpected) &&
             verticesResult != verticesResult.end()) {
        ASSERT_EQ((*verticesResult).copyString(), *verticesExpected);
        verticesResult++;
        verticesExpected++;
      }
      ASSERT_TRUE((verticesExpected == std::end(pathExpected)) &&
                  // Yes, really, they didn't implement == for iterators
                  !(verticesResult != verticesResult.end()));
    }
  }
  void TestExecutor(KShortestPathsExecutorInfos& infos, AqlItemBlockInputRange& input) {
    // This will fetch everything now, unless we give a small enough atMost

    auto skipCall = AqlCall{parameters._call};

    auto const [skipState, skipped, resultSkipCall] = testee.skipRowsRange(input, skipCall);

    auto const [produceState, stats, resultProduceCall] =
        testee.produceRows(input, output);

    ValidateResult(output, skipped);
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
PathSequence generateSomeBiggerCase(size_t n) {
  auto paths = PathSequence{};

  for (size_t i = 0; i < n; i++) {
    paths.push_back({"vertex/source", "vertex/intermed0", "vertex/target"});
  }

  return paths;
}

auto sources = testing::Values(constSource, regSource, brokenSource);
auto targets = testing::Values(constTarget, regTarget, brokenTarget);
auto inputs = testing::Values(noneRow, oneRow, twoRows, threeRows);
auto paths =
    testing::Values(noPath, onePath, threePaths, somePaths,
                    generateSomeBiggerCase(100), generateSomeBiggerCase(999),
                    generateSomeBiggerCase(1000), generateSomeBiggerCase(2000));
auto calls = testing::Values(AqlCall{}, AqlCall{0, 0, 0, false}, AqlCall{0, 1, 0, false},
                             AqlCall{0, 0, 1, false}, AqlCall{0, 1, 1, false},
                             AqlCall{1, 1, 1}, AqlCall{100, 1, 1}, AqlCall{1000});

INSTANTIATE_TEST_CASE_P(KShortestPathExecutorTestInstance, KShortestPathsExecutorTest,
                        testing::Combine(sources, targets, inputs, paths, calls));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
