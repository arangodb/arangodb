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
#include "Aql/InputAqlItemRow.h"
#include "Aql/KShortestPathsExecutor.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Basics/ResourceUsage.h"
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
using RegisterSet = RegIdSet;
using Path = std::vector<std::string>;
using PathSequence = std::vector<Path>;

// The FakeShortestPathsFinder does not do any real k shortest paths search; it
// is merely initialized with a set of "paths" and then outputs them, keeping a
// record of which paths it produced. This record is used in the validation
// whether the executor output the correct sequence of rows.
class FakeKShortestPathsFinder : public KShortestPathsFinder {
 public:
  FakeKShortestPathsFinder(ShortestPathOptions& options, PathSequence const& kpaths)
      : KShortestPathsFinder(options), _kpaths(kpaths), _traversalDone(true) {}
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

    _calledWith.emplace_back(std::make_pair(_source, _target));

    EXPECT_NE(_source, "");
    EXPECT_NE(_target, "");
    EXPECT_NE(_source, _target);

    _finder = _kpaths.begin();
    return true;
  }

  bool getNextPathAql(Builder& builder) override {
    _traversalDone = !gotoNextPath();

    if (_traversalDone) {
      return false;
    } else {
      _pathsProduced.emplace_back(*_finder);
      // fill builder with something sensible?
      builder.openArray();
      for (auto&& v : *_finder) {
        builder.add(VPackValue(v));
      }
      builder.close();

      // HACK
      _finder++;
      return true;
    }
  }

  bool skipPath() override {
    Builder builder{};
    return getNextPathAql(builder);
  }

  bool isDone() const override { return _traversalDone; }

  PathSequence& getPathsProduced() noexcept { return _pathsProduced; }
  std::vector<std::pair<std::string, std::string>> getCalledWith() noexcept {
    return _calledWith;
  }

 private:
  // We emulate a number of paths between PathPair
  PathSequence const& _kpaths;
  std::string _source;
  std::string _target;
  bool _traversalDone;
  PathSequence::const_iterator _finder;
  PathSequence _pathsProduced;
  std::vector<std::pair<std::string, std::string>> _calledWith;
};

// TODO: this needs a << operator
struct KShortestPathsTestParameters {
  KShortestPathsTestParameters(std::tuple<Vertex, Vertex, MatrixBuilder<2>, PathSequence, AqlCall, size_t> params)
      : _source(std::get<0>(params)),
        _target(std::get<1>(params)),
        // TODO: Make output registers configurable?
        _outputRegisters(std::initializer_list<RegisterId>{2}),
        _inputMatrix(std::get<2>(params)),
        _paths(std::get<3>(params)),
        _call(std::get<4>(params)),
        _blockSize(std::get<5>(params)){};

  Vertex _source;
  Vertex _target;
  RegisterSet _inputRegisters;
  RegisterSet _outputRegisters;
  MatrixBuilder<2> _inputMatrix;
  PathSequence _paths;
  AqlCall _call;
  size_t _blockSize{1000};
};

class KShortestPathsExecutorTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<Vertex, Vertex, MatrixBuilder<2>, PathSequence, AqlCall, size_t>> {
 protected:
  // parameters are copied because they are const otherwise
  // and that doesn't mix with std::move
  KShortestPathsTestParameters parameters;

  MockAqlServer server;
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;

  std::unique_ptr<arangodb::aql::Query> fakedQuery;
  ShortestPathOptions options;

  RegisterInfos registerInfos;
  KShortestPathsExecutorInfos executorInfos;

  FakeKShortestPathsFinder& finder;

  SharedAqlItemBlockPtr inputBlock;
  AqlItemBlockInputRange input;

  std::shared_ptr<Builder> fakeUnusedBlock;
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher;

  KShortestPathsExecutor testee;
  OutputAqlItemRow output;

  KShortestPathsExecutorTest()
      : parameters(GetParam()),
        server{},
        itemBlockManager(monitor, SerializationFormat::SHADOWROWS),
        fakedQuery(server.createFakeQuery()),
        options(*fakedQuery.get()),
        registerInfos(parameters._inputRegisters, parameters._outputRegisters,
                      2, 3, RegIdFlatSet{}, RegIdFlatSetStack{{}}),
        executorInfos(0,
                      std::make_unique<FakeKShortestPathsFinder>(options,
                                                                 parameters._paths),
                      std::move(parameters._source), std::move(parameters._target)),
        finder(static_cast<FakeKShortestPathsFinder&>(executorInfos.finder())),
        inputBlock(buildBlock<2>(itemBlockManager, std::move(parameters._inputMatrix))),
        input(AqlItemBlockInputRange(ExecutorState::DONE, 0, inputBlock, 0)),
        fakeUnusedBlock(VPackParser::fromJson("[]")),
        fetcher(itemBlockManager, fakeUnusedBlock->steal(), false),
        testee(fetcher, executorInfos),
        output(std::move(block), registerInfos.getOutputRegisters(),
               registerInfos.registersToKeep(), registerInfos.registersToClear()) {}

  size_t ExpectedNumberOfRowsProduced(size_t expectedFound) {
    if (parameters._call.getOffset() >= expectedFound) {
      return 0;
    } else {
      expectedFound -= parameters._call.getOffset();
    }
    return parameters._call.clampToLimit(expectedFound);
  }

  void ValidateCalledWith() {
    auto calledWith = finder.getCalledWith();
    auto block = buildBlock<2>(itemBlockManager, std::move(parameters._inputMatrix));

    // We should always only call the finder at most for all input rows
    ASSERT_LE(calledWith.size(), block->size());

    auto blockIndex = size_t{0};
    for (auto const& input : calledWith) {
      auto source = std::string{};
      auto target = std::string{};

      if (executorInfos.useRegisterForSourceInput()) {
        AqlValue value =
            block->getValue(blockIndex, executorInfos.getSourceInputRegister());
        ASSERT_TRUE(value.isString());
        source = value.slice().copyString();
      } else {
        source = executorInfos.getSourceInputValue();
      }

      if (executorInfos.useRegisterForTargetInput()) {
        AqlValue value =
            block->getValue(blockIndex, executorInfos.getTargetInputRegister());
        ASSERT_TRUE(value.isString());
        target = value.slice().copyString();
      } else {
        target = executorInfos.getTargetInputValue();
      }
      ASSERT_EQ(source, input.first);
      ASSERT_EQ(target, input.second);
      blockIndex++;
    }
  }

  void ValidateResult(std::vector<SharedAqlItemBlockPtr>& results,
                      size_t skippedInitial, size_t skippedFullCount) {
    auto pathsFound = finder.getPathsProduced();

    // We expect to be getting exactly the rows returned
    // that we produced with the shortest path finder.
    // in exactly the order they were produced in.

    auto expectedNrRowsSkippedInitial =
        std::min(parameters._call.getOffset(), pathsFound.size());
    EXPECT_EQ(skippedInitial, expectedNrRowsSkippedInitial);

    auto expectedNrRowsProduced = ExpectedNumberOfRowsProduced(pathsFound.size());

    auto expectedRowsIndex = size_t{skippedInitial};
    for (auto const& block : results) {
      if (block != nullptr) {
        for (size_t blockIndex = 0; blockIndex < block->size(); ++blockIndex, ++expectedRowsIndex) {
          AqlValue value =
              block->getValue(blockIndex, executorInfos.getOutputRegister());
          EXPECT_TRUE(value.isArray());

          // Note that the correct layout of the result path is currently the
          // responsibility of the path finder (tested separately), so we get
          // away with fake outputs.
          auto verticesResult = VPackArrayIterator(value.slice());
          auto pathExpected = pathsFound.at(expectedRowsIndex);
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
    }

    ASSERT_EQ(expectedRowsIndex - skippedInitial, expectedNrRowsProduced);

    // If a fullCount was requested, the sum (skippedInitial + produced +
    // skippedFullCount) should be exactly the number of rows we produced.
    if (parameters._call.fullCount) {
      ASSERT_EQ(skippedInitial + (expectedRowsIndex - skippedInitial) + skippedFullCount,
                pathsFound.size());
    }
  }

  void TestExecutor(RegisterInfos& registerInfos, KShortestPathsExecutorInfos& executorInfos,
                    AqlItemBlockInputRange& input) {
    // This will fetch everything now, unless we give a small enough atMost

    auto stats = NoStats{};
    auto ourCall = AqlCall{parameters._call};
    auto skippedInitial = size_t{0};
    auto skippedFullCount = size_t{0};
    auto state = ExecutorState{ExecutorState::HASMORE};
    auto outputs = std::vector<SharedAqlItemBlockPtr>{};

    if (ourCall.getOffset() > 0) {
      std::tie(state, stats, skippedInitial, std::ignore) =
          testee.skipRowsRange(input, ourCall);
    }
    ourCall.resetSkipCount();

    while (state == ExecutorState::HASMORE && ourCall.getLimit() > 0) {
      SharedAqlItemBlockPtr block =
          itemBlockManager.requestBlock(parameters._blockSize, 4);

      OutputAqlItemRow output(std::move(block), registerInfos.getOutputRegisters(),
                              registerInfos.registersToKeep(),
                              registerInfos.registersToClear());
      output.setCall(std::move(ourCall));

      std::tie(state, std::ignore, std::ignore) = testee.produceRows(input, output);

      outputs.emplace_back(output.stealBlock());
      ourCall = output.stealClientCall();
    }

    if (ourCall.needsFullCount()) {
      std::tie(state, stats, skippedFullCount, std::ignore) =
          testee.skipRowsRange(input, ourCall);
    }
    ourCall.resetSkipCount();

    ValidateCalledWith();
    ValidateResult(outputs, skippedInitial, skippedFullCount);
  }
};  // namespace aql

TEST_P(KShortestPathsExecutorTest, the_test) {
  TestExecutor(registerInfos, executorInfos, input);
}

// Conflict with the other shortest path finder
namespace {

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
MatrixBuilder<2> const someRows{{{{R"("vertex/c")"}, {R"("vertex/target")"}}},
                                {{{R"("vertex/b")"}, {R"("vertex/target")"}}},
                                {{{R"("vertex/e")"}, {R"("vertex/target")"}}},
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
auto calls =
    testing::Values(AqlCall{}, AqlCall{0, 0u, 0u, false}, AqlCall{0, 1u, 0u, false},
                    AqlCall{0, 0u, 1u, false}, AqlCall{0, 1u, 1u, false},
                    AqlCall{1, 1u, 1u}, AqlCall{100, 1u, 1u}, AqlCall{1000},
                    AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, true});
auto blockSizes = testing::Values(5, 1000);

INSTANTIATE_TEST_CASE_P(KShortestPathExecutorTestInstance, KShortestPathsExecutorTest,
                        testing::Combine(sources, targets, inputs, paths, calls, blockSizes));
}  // namespace

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
