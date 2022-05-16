////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"
#include "AqlItemBlockHelper.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/WindowExecutor.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Mocks/Servers.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "FixedOutputExecutionBlockMock.h"

#include <velocypack/Builder.h>

#include <chrono>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// This is only to get a split-type. The Type is independent of actual template
// parameters
using WindowTestHelper = ExecutorTestHelper<1, 1>;
using WindowSplitType = WindowTestHelper::SplitType;
using WindowInputParam = std::tuple<WindowSplitType, bool>;

template<size_t... vs>
const WindowSplitType splitIntoBlocks =
    WindowSplitType{std::vector<std::size_t>{vs...}};
template<size_t step>
const WindowSplitType splitStep = WindowSplitType{step};

struct WindowInput {
  WindowBounds bounds;
  RegisterId rangeReg;

  std::string name;  // aggregation function
  RegisterId inReg;  // aggregation in register
  MatrixBuilder<2> input;
  MatrixBuilder<3> expectedOutput;
};

std::ostream& operator<<(std::ostream& out, WindowInput const& agg) {
  VPackBuilder b;
  b.openObject();
  agg.bounds.toVelocyPack(b);
  b.close();
  out << b.toJson() << " ";
  out << agg.name;
  if (agg.inReg != RegisterPlan::MaxRegisterId) {
    out << " reg: " << agg.inReg.value();
  }
  return out;
}

using WindowAggregateInputParam = std::tuple<WindowSplitType, WindowInput>;

class WindowExecutorTest
    : public AqlExecutorTestCaseWithParam<WindowAggregateInputParam> {
 protected:
  auto getSplit() -> WindowSplitType {
    auto [split, unused] = GetParam();
    return split;
  }

  auto getWindowParams() -> WindowInput const& {
    auto const& [unused, info] = GetParam();
    return info;
  }

  auto buildRegisterInfos(RegisterCount nrInputRegisters,
                          RegisterCount nrOutputRegisters) -> RegisterInfos {
    RegIdSet registersToClear{};
    RegIdSetStack registersToKeep{{}};
    auto readableInputRegisters = RegIdSet{};
    auto writeableOutputRegisters = RegIdSet{};

    RegIdSet toKeep;
    for (RegisterId::value_t i = 0; i < nrInputRegisters; ++i) {
      // All registers need to be kept!
      toKeep.emplace(i);
    }
    registersToKeep.emplace_back(std::move(toKeep));

    WindowInput const& input = getWindowParams();
    if (input.inReg != RegisterPlan::MaxRegisterId) {
      readableInputRegisters.emplace(input.inReg);
    }
    if (input.rangeReg != RegisterPlan::MaxRegisterId) {
      readableInputRegisters.emplace(input.rangeReg);
    }

    writeableOutputRegisters.emplace(2);  // same as in buildExecutorInfos()

    return RegisterInfos{std::move(readableInputRegisters),
                         std::move(writeableOutputRegisters),
                         nrInputRegisters,
                         nrOutputRegisters,
                         registersToClear,
                         registersToKeep};
  };

  auto buildExecutorInfos() -> WindowExecutorInfos {
    WindowInput const& input = getWindowParams();
    std::vector<std::string> aggregateTypes{input.name};
    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters{
        {2, input.inReg}};

    return WindowExecutorInfos(
        input.bounds, input.rangeReg, std::move(aggregateTypes),
        std::move(aggregateRegisters), warnings, &VPackOptions::Defaults);
  };

  arangodb::aql::QueryWarnings warnings;
};

/**
 * Input used:
 *
 * [
 *   [1, 5]
 *   [1, 1]
 *   [2, 2]
 *   [1, 5]
 *   [6, 1]
 *   [2, 2]
 *   [3, 1]
 * ]
 * sorted:
 * [
 *   [1, 5]
 *   [1, 1]
 *   [1, 5]
 *   [2, 2]
 *   [2, 2]
 *   [3, 1]
 *   [6, 1]
 * ]
 */

/**
 * TODO:
 * [] Add tests for all aggregate functions
 */

auto inputRows = MatrixBuilder<2>{RowBuilder<2>{1, 5}, RowBuilder<2>{1, 1},
                                  RowBuilder<2>{2, 2}, RowBuilder<2>{1, 5},
                                  RowBuilder<2>{6, 1}, RowBuilder<2>{2, 2},
                                  RowBuilder<2>{3, 1}};
auto sortedRows = MatrixBuilder<2>{RowBuilder<2>{1, 5}, RowBuilder<2>{1, 1},
                                   RowBuilder<2>{1, 5}, RowBuilder<2>{2, 2},
                                   RowBuilder<2>{2, 2}, RowBuilder<2>{3, 1},
                                   RowBuilder<2>{6, 1}};

int t0 = 698976;                   // 01/09/1970
int t1 = t0 + 1 * 01 * 02 * 1000;  //+ 2s
int t2 = t1 + 1 * 05 * 60 * 1000;  //+ 5m
int t3 = t2 + 1 * 10 * 60 * 1000;  //+ 10m
int t4 = t3 + 1 * 60 * 60 * 1000;  //+ 1h
int t5 = t4 + 5 * 60 * 60 * 1000;  //+ 5h

auto sortedDateRows = MatrixBuilder<2>{
    RowBuilder<2>{t0, 5}, RowBuilder<2>{t1, 1}, RowBuilder<2>{t2, 5},
    RowBuilder<2>{t3, 2}, RowBuilder<2>{t4, 2}, RowBuilder<2>{t5, 1},
    RowBuilder<2>{t5, 1}};

auto vpackOptions = VPackOptions();
auto inf = VPackParser::fromJson("\"inf\"", &vpackOptions);
auto duration1h10m = VPackParser::fromJson("\"PT1H10M\"", &vpackOptions);
auto duration3s = VPackParser::fromJson("\"PT3S\"", &vpackOptions);

auto boundsRow1 =
    WindowBounds(WindowBounds::Type::Row, AqlValue(AqlValueHintInt(1)),
                 AqlValue(AqlValueHintInt(1)));
auto boundsRowAccum =
    WindowBounds(WindowBounds::Type::Row, AqlValue(inf->slice()),
                 AqlValue(AqlValueHintInt(0)));
auto boundsRange1 =
    WindowBounds(WindowBounds::Type::Range, AqlValue(AqlValueHintInt(1)),
                 AqlValue(AqlValueHintInt(1)));
auto boundsRangeP3 =
    WindowBounds(WindowBounds::Type::Range, AqlValue(AqlValueHintInt(3)),
                 AqlValue(AqlValueHintInt(0)));
// auto boundsDateRange = WindowBounds(WindowBounds::Type::Range,
// AqlValue(duration1h10m->slice()), AqlValue(duration3s->slice()));

auto WindowInputs = ::testing::Values(
    WindowInput{boundsRow1,
                RegisterPlan::MaxRegisterId,
                "SUM",
                0,
                inputRows,
                {{1, 5, 2},
                 {1, 1, 4},
                 {2, 2, 4},
                 {1, 5, 9},
                 {6, 1, 9},
                 {2, 2, 11},
                 {3, 1, 5}}},
    WindowInput{boundsRow1,
                RegisterPlan::MaxRegisterId,
                "SUM",
                1,
                inputRows,
                {{1, 5, 6},
                 {1, 1, 8},
                 {2, 2, 8},
                 {1, 5, 8},
                 {6, 1, 8},
                 {2, 2, 4},
                 {3, 1, 3}}},
    WindowInput{boundsRow1,
                RegisterPlan::MaxRegisterId,
                "MAX",
                1,
                inputRows,
                {{1, 5, 5},
                 {1, 1, 5},
                 {2, 2, 5},
                 {1, 5, 5},
                 {6, 1, 5},
                 {2, 2, 2},
                 {3, 1, 2}}},
    WindowInput{boundsRow1,
                RegisterPlan::MaxRegisterId,
                "MIN",
                0,
                inputRows,
                {{1, 5, 1},
                 {1, 1, 1},
                 {2, 2, 1},
                 {1, 5, 1},
                 {6, 1, 1},
                 {2, 2, 2},
                 {3, 1, 2}}},
    WindowInput{boundsRowAccum,
                RegisterPlan::MaxRegisterId,
                "SUM",
                0,
                inputRows,
                {{1, 5, 1},
                 {1, 1, 2},
                 {2, 2, 4},
                 {1, 5, 5},
                 {6, 1, 11},
                 {2, 2, 13},
                 {3, 1, 16}}},
    WindowInput{boundsRowAccum,
                RegisterPlan::MaxRegisterId,
                "MAX",
                0,
                inputRows,
                {{1, 5, 1},
                 {1, 1, 1},
                 {2, 2, 2},
                 {1, 5, 2},
                 {6, 1, 6},
                 {2, 2, 6},
                 {3, 1, 6}}},
    WindowInput{boundsRowAccum,
                RegisterPlan::MaxRegisterId,
                "MIN",
                0,
                inputRows,
                {{1, 5, 1},
                 {1, 1, 1},
                 {2, 2, 1},
                 {1, 5, 1},
                 {6, 1, 1},
                 {2, 2, 1},
                 {3, 1, 1}}},
    // range based input, offset of one each way
    WindowInput{boundsRange1,
                0,
                "SUM",
                1,
                sortedRows,
                {{1, 5, 15},
                 {1, 1, 15},
                 {1, 5, 15},
                 {2, 2, 16},
                 {2, 2, 16},
                 {3, 1, 5},
                 {6, 1, 1}}},
    WindowInput{boundsRange1,
                0,
                "MIN",
                1,
                sortedRows,
                {{1, 5, 1},
                 {1, 1, 1},
                 {1, 5, 1},
                 {2, 2, 1},
                 {2, 2, 1},
                 {3, 1, 1},
                 {6, 1, 1}}},
    // range based input, offset of offset 3 preceding
    WindowInput{boundsRangeP3,
                0,
                "SUM",
                1,
                sortedRows,
                {{1, 5, 11},
                 {1, 1, 11},
                 {1, 5, 11},
                 {2, 2, 15},
                 {2, 2, 15},
                 {3, 1, 16},
                 {6, 1, 2}}},
    WindowInput{boundsRangeP3,
                0,
                "SUM",
                1,
                sortedRows,
                {{1, 5, 11},
                 {1, 1, 11},
                 {1, 5, 11},
                 {2, 2, 15},
                 {2, 2, 15},
                 {3, 1, 16},
                 {6, 1, 2}}}
    // TODO: fix ISO duration regex to enable date range test
    // WindowInput{boundsDateRange, 0, "SUM", 1, sortedRows, {{t0, 5, 6}, {t1,
    // 1, 6}, { t2, 5, 11}, {t3, 2, 13}, {t4, 2, 15}, {t5, 1, 3}, {t5, 1, 1}}}
);

INSTANTIATE_TEST_CASE_P(
    Window, WindowExecutorTest,
    ::testing::Combine(::testing::Values(splitIntoBlocks<2, 3>,
                                         splitIntoBlocks<3, 4>, splitStep<1>,
                                         splitStep<2>),
                       WindowInputs));

TEST_P(WindowExecutorTest, runWindowExecutor) {
  auto const& params = getWindowParams();
  // range based variant needs sorted input

  auto registerInfos = buildRegisterInfos(2, 3);
  auto executorInfos = buildExecutorInfos();
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper<2, 3>()
      .addConsumer<WindowExecutor>(std::move(registerInfos),
                                   std::move(executorInfos))
      .setInputValue(params.input)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1, 2}, getWindowParams().expectedOutput)
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(/*loop*/ true);
}

// test AccuWindowExecutor
TEST_P(WindowExecutorTest, runAccuWindowExecutor) {
  auto const& params = getWindowParams();
  if (!params.bounds.unboundedPreceding()) {
    return;  // skip
  }
  // range based variant needs sorted input
  // AccuWindowExecutor is passthrough, needs 3 input registers
  MatrixBuilder<3> passthroughInput;
  for (auto const& r : params.input) {
    passthroughInput.emplace_back(RowBuilder<3>{r[0], r[1], NoneEntry{}});
  }
  auto registerInfos = buildRegisterInfos(3, 3);
  auto executorInfos = buildExecutorInfos();
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper<3, 3>()
      .addConsumer<AccuWindowExecutor>(std::move(registerInfos),
                                       std::move(executorInfos))
      .setInputValue(std::move(passthroughInput))
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1, 2}, params.expectedOutput)
      .allowAnyOutputOrder(false)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(/*loop*/ true);
}

class WindowExecutorInSubqueryTest : public AqlExecutorTestCase<false> {
 private:
  // Hardcode to preceeding: 1, lookahead: 1, and SUM method
  WindowBounds _preOnePostOne{WindowBounds::Type::Range,
                              AqlValue(AqlValueHintInt(1)),
                              AqlValue(AqlValueHintInt(1))};

 protected:
  auto buildRegisterInfos() -> RegisterInfos {
    RegisterCount nrInputRegisters = 1;
    RegisterCount nrOutputRegisters = 2;
    RegIdSet registersToClear{};
    RegIdSetStack registersToKeep{{}};
    auto readableInputRegisters = RegIdSet{};
    auto writeableOutputRegisters = RegIdSet{};

    RegIdSet toKeep;
    for (RegisterId::value_t i = 0; i < nrInputRegisters; ++i) {
      // All registers need to be kept!
      toKeep.emplace(i);
    }
    registersToKeep.emplace_back(toKeep);
    registersToKeep.emplace_back(std::move(toKeep));
    // Allow to read and write to register 0
    readableInputRegisters.emplace(0);
    writeableOutputRegisters.emplace(1);

    return RegisterInfos{std::move(readableInputRegisters),
                         std::move(writeableOutputRegisters),
                         nrInputRegisters,
                         nrOutputRegisters,
                         registersToClear,
                         registersToKeep};
  };

  auto buildExecutorInfos() -> WindowExecutorInfos {
    std::vector<std::string> aggregateTypes{"SUM"};
    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters{{1, 0}};

    return WindowExecutorInfos(_preOnePostOne, 0, std::move(aggregateTypes),
                               std::move(aggregateRegisters),
                               fakedQuery->warnings(), &VPackOptions::Defaults);
  };

  auto splitBlock(SharedAqlItemBlockPtr block, std::vector<size_t> splitAt)
      -> std::vector<SharedAqlItemBlockPtr> {
    std::vector<SharedAqlItemBlockPtr> response;
    // Start from first row
    size_t from = 0;
    for (size_t to : splitAt) {
      // Cannot test assert here, only possible in void methods.
      TRI_ASSERT(to < block->numRows());
      TRI_ASSERT(from < to);
      // Create a slice from->to.
      response.emplace_back(block->slice(from, to));
      // Start the next slice where we just stopped
      from = to;
    }
    // Include the last slice
    response.emplace_back(block->slice(from, block->numRows()));
    return response;
  }
};

/// @brief we test a couple of splitting combinations for input blocks.
/// especially we test here that splitting in "dangerous" places around
/// subqueries does not have an undesired effect.
TEST_F(WindowExecutorInSubqueryTest, test_input_fragmentation_does_not_matter) {
  auto buildTestInputBlock = [&]() -> SharedAqlItemBlockPtr {
    return buildBlock<1>(manager(),
                         {{1},
                          {2},
                          {3},
                          {4},
                          {NoneEntry{}},
                          {5},
                          {6},
                          {7},
                          {NoneEntry{}},
                          {1},
                          {2},
                          {3},
                          {4},
                          {NoneEntry{}},
                          {5},
                          {6},
                          {7},
                          {NoneEntry{}}},
                         {{4, 0}, {8, 0}, {13, 0}, {17, 0}});
  };

  auto buildTestExpectedOutputBlock = [&]() -> SharedAqlItemBlockPtr {
    return buildBlock<2>(manager(),
                         {{1, 3},
                          {2, 6},
                          {3, 9},
                          {4, 7},
                          {NoneEntry{}, NoneEntry{}},
                          {5, 11},
                          {6, 18},
                          {7, 13},
                          {NoneEntry{}, NoneEntry{}},
                          {1, 3},
                          {2, 6},
                          {3, 9},
                          {4, 7},
                          {NoneEntry{}, NoneEntry{}},
                          {5, 11},
                          {6, 18},
                          {7, 13},
                          {NoneEntry{}, NoneEntry{}}},
                         {{4, 0}, {8, 0}, {13, 0}, {17, 0}});
  };

  // Candidates to test here:
  // a) First block has no shadow row,
  // b) A sequence around a shadow row (2 before, 1 before, on the row, 1 after,
  // 2 after)
  for (size_t firstSplit : {2, 7, 8, 9, 10, 11}) {
    // For the second split we try to split again before, on and after a shadow
    // row
    for (size_t secondSplit : {12, 13, 14}) {
      std::deque<arangodb::aql::SharedAqlItemBlockPtr> inputData{};
      {
        auto input =
            splitBlock(buildTestInputBlock(), {firstSplit, secondSplit});
        for (auto const& it : input) {
          inputData.push_back(it);
        }
      }

      MockTypedNode inputNode{fakedQuery->plan(), ExecutionNodeId{1},
                              ExecutionNode::FILTER};
      FixedOutputExecutionBlockMock dependency{
          fakedQuery->rootEngine(), &inputNode, std::move(inputData)};

      MockTypedNode windowNode{fakedQuery->plan(), ExecutionNodeId{42},
                               ExecutionNode::WINDOW};
      ExecutionBlockImpl<WindowExecutor> testee{
          fakedQuery->rootEngine(), &windowNode, buildRegisterInfos(),
          buildExecutorInfos()};
      testee.addDependency(&dependency);

      // MainQuery fetch all
      AqlCallStack callStack{AqlCallList{AqlCall{}}};
      // Subquery fetch all
      // We are only testing correctness of ShadowRowHandling here.
      callStack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});

      // From the outside we will get away with a single call
      auto [state, skipped, block] = testee.execute(callStack);
      EXPECT_EQ(state, ExecutionState::DONE);
      asserthelper::ValidateBlocksAreEqual(block,
                                           buildTestExpectedOutputBlock());
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
