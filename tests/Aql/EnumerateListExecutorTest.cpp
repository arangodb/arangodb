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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"
#include "RowFetcherHelper.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/EnumerateListExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/Stats.h"
#include "AqlItemBlockHelper.h"
#include "Mocks/Servers.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// test inner executor behaviour
class EnumerateListExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  NoStats stats;
  AqlCall call;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};
  EnumerateListExecutorTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS) {}
};

TEST_F(EnumerateListExecutorTest, test_check_state_first_row_border) {
  // old styled test, to test the inner step-states of our executor

  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 5)};
  EnumerateListExecutorInfos infos(3, 4, 4, 5, {}, {0, 1, 2, 3});
  EnumerateListExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<4>(itemBlockManager, {{{{1}, {2}, {3}, {R"([true, 1, 2])"}}},
                                       {{{1}, {2}, {3}, {R"([true, 1, 2])"}}}});

  AqlItemBlockInputRange input{ExecutorState::DONE, 0, inBlock, 0};
  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  // receive first 3 of 6 results in total
  AqlCall myCall{0, AqlCall::Infinity{}, 3, false};

  output.setCall(std::move(myCall));
  EXPECT_EQ(output.numRowsWritten(), 0);
  {
    // reach the end (edge) of our first row, check that we do not return DONE here!
    auto const [state, stats, call] = testee.produceRows(input, output);
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_EQ(output.numRowsWritten(), 3);
  }
}

TEST_F(EnumerateListExecutorTest, test_check_state_second_row_border) {
  // old styled test, to test the inner step-states of our executor

  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 5)};
  EnumerateListExecutorInfos infos(3, 4, 4, 5, {}, {0, 1, 2, 3});
  EnumerateListExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<4>(itemBlockManager, {{{{1}, {2}, {3}, {R"([true, 1, 2])"}}},
                                       {{{1}, {2}, {3}, {R"([true, 1, 2])"}}}});

  AqlItemBlockInputRange input{ExecutorState::DONE, 0, inBlock, 0};
  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  // receive 6 of 6 results in total
  AqlCall myCall{0, AqlCall::Infinity{}, 6, false};

  output.setCall(std::move(myCall));
  EXPECT_EQ(output.numRowsWritten(), 0);
  {
    // reach the end (edge) of our second row, check that we do not return DONE here!
    auto const [state, stats, call] = testee.produceRows(input, output);
    EXPECT_EQ(state, ExecutorState::DONE);
    EXPECT_EQ(output.numRowsWritten(), 6);
  }
}

// new framework tests
using EnumerateListTestHelper = ExecutorTestHelper<1, 1>;
using EnumerateListSplitType = EnumerateListTestHelper::SplitType;
using EnumerateListParamType = std::tuple<EnumerateListSplitType>;

class EnumerateListExecutorTestProduce
    : public AqlExecutorTestCaseWithParam<EnumerateListParamType, false> {
 protected:
  EnumerateListExecutorInfos infos;

  SharedAqlItemBlockPtr block;
  NoStats stats;

  EnumerateListExecutorTestProduce() : infos(0, 1, 1, 2, {}, {0}) {}

  auto makeInfos(RegisterId inputRegister = 0, RegisterId outputRegister = 1,
                 RegisterId nrInputRegister = 1, RegisterId nrOutputRegister = 2,
                 std::unordered_set<RegisterId> regToClear = {},
                 std::unordered_set<RegisterId> regToKeep = {0}) -> EnumerateListExecutorInfos {
    EnumerateListExecutorInfos infos{inputRegister,   outputRegister,
                                     nrInputRegister, nrOutputRegister,
                                     regToClear,      regToKeep};
    block = SharedAqlItemBlockPtr{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    return infos;
  }
};

TEST_P(EnumerateListExecutorTestProduce, empty_array_1) {
  auto [split] = GetParam();

  makeExecutorTestHelper<1, 1>()
      .addConsumer<EnumerateListExecutor>(makeInfos())
      .setInputValue({{{R"([])"}}})
      .setInputSplitType(split)
      .expectOutput({}, {})
      .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, invalid_value_1) {
  auto [split] = GetParam();

  try {
    makeExecutorTestHelper<1, 1>()
        .addConsumer<EnumerateListExecutor>(makeInfos())
        .setInputValue({{1}})
        .setInputSplitType(split)
        .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
        .expectOutput({}, {})
        .expectSkipped(0)
        .expectedState(ExecutionState::DONE)
        .run();
    FAIL();
  } catch (const arangodb::basics::Exception& e) {
    ASSERT_EQ(e.code(), 1563);
  }
}

TEST_P(EnumerateListExecutorTestProduce, default_1) {
  auto [split] = GetParam();

  makeExecutorTestHelper<1, 1>()
      .addConsumer<EnumerateListExecutor>(makeInfos())
      .setInputValue({{{R"([1, 1, 2])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectOutput({1}, {{1}, {1}, {2}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, offset_1) {
  auto [split] = GetParam();

  makeExecutorTestHelper<1, 1>()
      .addConsumer<EnumerateListExecutor>(makeInfos())
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{5, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectOutput({1}, {{6}, {7}, {8}, {9}, {10}})
      .expectSkipped(5)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, offset_2) {
  auto [split] = GetParam();

  makeExecutorTestHelper<1, 1>()
      .addConsumer<EnumerateListExecutor>(makeInfos())
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{3, AqlCall::Infinity{}, 2, false})
      .expectOutput({1}, {{4}, {5}})
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, offset_3) {
  auto [split] = GetParam();

  makeExecutorTestHelper<1, 1>()
      .addConsumer<EnumerateListExecutor>(makeInfos())
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{7, AqlCall::Infinity{}, 3, false})
      .expectOutput({1}, {{8}, {9}, {10}})
      .expectSkipped(7)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, offset_4) {
  auto [split] = GetParam();

  makeExecutorTestHelper<1, 1>()
      .addConsumer<EnumerateListExecutor>(makeInfos())
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{5, AqlCall::Infinity{}, 2, true})
      .expectOutput({1}, {{6}, {7}})
      .expectSkipped(8)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, offset_5) {
  auto [split] = GetParam();

  makeExecutorTestHelper<1, 1>()
      .addConsumer<EnumerateListExecutor>(makeInfos())
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{7, AqlCall::Infinity{}, 3, true})
      .expectOutput({1}, {{8}, {9}, {10}})
      .expectSkipped(7)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, default_multiple_1) {
  auto [split] = GetParam();

  makeExecutorTestHelper<4, 5>()
      .addConsumer<EnumerateListExecutor>(makeInfos(3, 4, 4, 5, {}, {0, 1, 2, 3}))
      .setInputValue({{1, 2, 3, R"([1, 2, 3])"}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectOutput({0, 1, 2, 3, 4}, {{RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 1},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 2},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 3}}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, default_multiple_2) {
  auto [split] = GetParam();

  makeExecutorTestHelper<4, 5>()
      .addConsumer<EnumerateListExecutor>(makeInfos(3, 4, 4, 5, {}, {0, 1, 2, 3}))
      .setInputValue({RowBuilder<4>{1, 2, 3, R"([1, 2, 3])"},
                      RowBuilder<4>{1, 2, 3, R"([4, 5, 6])"}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectOutput({0, 1, 2, 3, 4}, {{RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 1},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 2},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 3},
                                       RowBuilder<5>{1, 2, 3, R"([4, 5, 6])", 4},
                                       RowBuilder<5>{1, 2, 3, R"([4, 5, 6])", 5},
                                       RowBuilder<5>{1, 2, 3, R"([4, 5, 6])", 6}}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, default_border_first_array_soft) {
  auto [split] = GetParam();

  makeExecutorTestHelper<4, 5>()
      .addConsumer<EnumerateListExecutor>(makeInfos(3, 4, 4, 5, {}, {0, 1, 2, 3}))
      .setInputValue({RowBuilder<4>{1, 2, 3, R"([1, 2, 3])"},
                      RowBuilder<4>{1, 2, 3, R"([4, 5, 6])"}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, 3, AqlCall::Infinity{}, false})
      .expectOutput({0, 1, 2, 3, 4}, {{RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 1},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 2},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 3}}})
      .expectSkipped(0)
      .expectedState(ExecutionState::HASMORE)  // hasmore because of softLimit
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, default_border_first_array_hard) {
  auto [split] = GetParam();

  makeExecutorTestHelper<4, 5>()
      .addConsumer<EnumerateListExecutor>(makeInfos(3, 4, 4, 5, {}, {0, 1, 2, 3}))
      .setInputValue({RowBuilder<4>{1, 2, 3, R"([1, 2, 3])"},
                      RowBuilder<4>{1, 2, 3, R"([4, 5, 6])"}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, 3, false})
      .expectOutput({0, 1, 2, 3, 4}, {{RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 1},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 2},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 3}}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)  // done because of hardLimit
      .run();
}

TEST_P(EnumerateListExecutorTestProduce, default_border_first_array_hard_fullcount) {
  auto [split] = GetParam();

  makeExecutorTestHelper<4, 5>()
      .addConsumer<EnumerateListExecutor>(makeInfos(3, 4, 4, 5, {}, {0, 1, 2, 3}))
      .setInputValue({RowBuilder<4>{1, 2, 3, R"([1, 2, 3])"},
                      RowBuilder<4>{1, 2, 3, R"([4, 5, 6])"}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, 3, true})
      .expectOutput({0, 1, 2, 3, 4}, {{RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 1},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 2},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 3}}})
      .expectSkipped(3)  // skipped amount of 3 in the fullCount phase
      .expectedState(ExecutionState::DONE)
      .run();
}

template <size_t... vs>
const EnumerateListSplitType splitIntoBlocks =
    EnumerateListSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const EnumerateListSplitType splitStep = EnumerateListSplitType{step};

INSTANTIATE_TEST_CASE_P(EnumerateListExecutor, EnumerateListExecutorTestProduce,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<2>));

// namespace aql

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
