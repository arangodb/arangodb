////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "AqlItemBlockHelper.h"
#include "Mocks/Servers.h"
#include "TestEmptyExecutorHelper.h"
#include "TestLambdaExecutor.h"
#include "WaitingExecutionBlockMock.h"
#include "fakeit.hpp"

#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/IdExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SubqueryEndExecutor.h"
#include "Aql/SubqueryStartExecutor.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include "Aql/AqlExecutorTestCase.h"
#include "Aql/TestLambdaExecutor.h"
#include "Aql/WaitingExecutionBlockMock.h"

// TODO: remove me
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::basics;

using SubqueryExecutorTestHelper = ExecutorTestHelper<1, 1>;
using SubqueryExecutorSplitType = SubqueryExecutorTestHelper::SplitType;
using SubqueryExecutorParamType = std::tuple<SubqueryExecutorSplitType>;

using RegisterSet = std::unordered_set<RegisterId>;
using LambdaExePassThrough = TestLambdaExecutor;
using LambdaExe = TestLambdaSkipExecutor;

class SplicedSubqueryIntegrationTest
    : public AqlExecutorTestCaseWithParam<SubqueryExecutorParamType, false> {
 protected:
  auto makeSubqueryStartRegisterInfos() -> RegisterInfos {
    auto inputRegisterSet = RegIdSet{0};
    auto outputRegisterSet = RegIdSet{};

    auto toKeepRegisterSet = RegIdSetStack{RegIdSet{0},RegIdSet{0},RegIdSet{0}};

    auto nrInputRegisters = static_cast<RegisterCount>(inputRegisterSet.size());
    auto nrOutputRegisters =
        static_cast<RegisterCount>(inputRegisterSet.size() + outputRegisterSet.size());
    return RegisterInfos(inputRegisterSet, outputRegisterSet, nrInputRegisters,
                         nrOutputRegisters, {}, toKeepRegisterSet);
  }
  auto makeSubqueryStartExecutorInfos() -> SubqueryStartExecutor::Infos {
    auto inputRegisterSet = RegIdSet{0};
    auto outputRegisterSet = RegIdSet{};

    auto toKeepRegisterSet = RegIdSetStack{RegIdSet{0}, RegIdSet{0}, RegIdSet{0}};

    auto nrInputRegisters = static_cast<RegisterCount>(inputRegisterSet.size());
    auto nrOutputRegisters =
        static_cast<RegisterCount>(inputRegisterSet.size() + outputRegisterSet.size());
    return SubqueryStartExecutor::Infos(inputRegisterSet, outputRegisterSet, nrInputRegisters,
                                        nrOutputRegisters, {}, toKeepRegisterSet);
  }

  auto makeSubqueryEndRegisterInfos(RegisterId inputRegister) -> RegisterInfos {
    auto inputRegisterSet = RegIdSet{inputRegister};
    auto const outputRegister = RegisterId{inputRegister + 1};
    for (RegisterId r = 0; r <= inputRegister; ++r) {
      inputRegisterSet.emplace(r);
    }
    auto outputRegisterSet = RegIdSet{outputRegister};
    auto toKeepRegisterSet = RegIdSetStack{RegIdSet{inputRegisterSet}, RegIdSet{inputRegisterSet}, RegIdSet{inputRegisterSet}};

    auto nrInputRegisters = static_cast<RegisterCount>(inputRegisterSet.size());
    auto nrOutputRegisters =
        static_cast<RegisterCount>(inputRegisterSet.size() + outputRegisterSet.size());

    return RegisterInfos(inputRegisterSet, outputRegisterSet, nrInputRegisters,
                         nrOutputRegisters, {}, toKeepRegisterSet);
  }

  auto makeSubqueryEndExecutorInfos(RegisterId inputRegister) -> SubqueryEndExecutor::Infos {
    auto const outputRegister = RegisterId{inputRegister + 1};

    return SubqueryEndExecutor::Infos(nullptr, monitor, inputRegister, outputRegister);
  }

  auto makeDoNothingRegisterInfos() -> RegisterInfos {
    auto numRegs = size_t{1};

    RegIdSet prototype{};
    for (RegisterId r = 0; r < numRegs; ++r) {
      prototype.emplace(r);
    }

    return RegisterInfos(RegIdSet{0}, RegIdSet{1}, 1, 2, {}, {prototype, prototype, prototype});
  }

  auto makeDoNothingExecutorInfos() -> LambdaExe::Infos {
    return LambdaExe::Infos(createProduceCall(), createSkipCall());
  }

  auto makeAssertRegisterInfos() -> RegisterInfos {
    auto numRegs = size_t{1};
    RegIdSet  prototype{};
    for (RegisterId r = 0; r < numRegs; ++r) {
      prototype.emplace(r);
    }

    return RegisterInfos(RegIdSet{0}, RegIdSet{1}, 1, 2, {}, {{prototype}, {prototype}});
  }

  auto makeAssertExecutorInfos() -> LambdaExe::Infos {
    return LambdaExe::Infos(createAssertCall(), createSkipCall());
  }

  auto makeAssertExecutorInfos(AqlCall call) -> LambdaExe::Infos {
    return LambdaExe::Infos(createAssertCallCall(call), createSkipCall());
  }

  auto createProduceCall() -> ProduceCall {
    return [](AqlItemBlockInputRange& input,
              OutputAqlItemRow& output) -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      while (input.hasDataRow() && !output.isFull()) {
        auto const [state, row] = input.nextDataRow();
        output.cloneValueInto(1, row, AqlValue("foo"));
        output.advanceRow();
      }
      NoStats stats{};
      AqlCall call{};

      return {input.upstreamState(), stats, call};
    };
  };

  auto createSkipCall() -> SkipCall {
    return [](AqlItemBlockInputRange& input,
              AqlCall& call) -> std::tuple<ExecutorState, LambdaExe::Stats, size_t, AqlCall> {
      while (call.shouldSkip() && input.skippedInFlight() > 0) {
        if (call.getOffset() > 0) {
          call.didSkip(input.skip(call.getOffset()));
        } else {
          EXPECT_TRUE(call.needsFullCount());
          EXPECT_EQ(call.getLimit(), 0);
          EXPECT_TRUE(call.hasHardLimit());
          call.didSkip(input.skipAll());
        }
      }
      // If we overfetched and have data, throw it away
      while (input.hasDataRow() && call.shouldSkip()) {
        auto const& [state, inputRow] = input.nextDataRow();
        EXPECT_TRUE(inputRow.isInitialized());
        call.didSkip(1);
      }
      auto upstreamCall = AqlCall{call};
      return {input.upstreamState(), NoStats{}, call.getSkipCount(), upstreamCall};
    };
  };

  // Asserts if called. This is to check that when we use skip to
  // skip over a subquery, the subquery's produce is not invoked
  // with data
  auto createAssertCall() -> ProduceCall {
    return [](AqlItemBlockInputRange& input,
              OutputAqlItemRow& output) -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      EXPECT_FALSE(input.hasDataRow());
      NoStats stats{};
      AqlCall call{};

      return {ExecutorState::DONE, stats, call};
    };
  }

  auto createAssertCallCall(AqlCall call) -> ProduceCall {
    return [call](AqlItemBlockInputRange& input,
                  OutputAqlItemRow& output) -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      auto clientCall = output.getClientCall();

      EXPECT_EQ(clientCall.offset, call.offset);
      EXPECT_EQ(clientCall.softLimit, call.softLimit);
      EXPECT_EQ(clientCall.hardLimit, call.hardLimit);
      EXPECT_EQ(clientCall.fullCount, call.fullCount);

      while (input.hasDataRow() && !output.isFull()) {
        auto const [state, row] = input.nextDataRow();
        output.cloneValueInto(1, row, AqlValue("foo"));
        output.advanceRow();
      }

      NoStats stats{};
      AqlCall call{};

      return {input.upstreamState(), stats, call};
    };
  }
  auto getSplit() -> SubqueryExecutorSplitType {
    auto [split] = GetParam();
    return split;
  }
};

template <size_t... vs>
const SubqueryExecutorSplitType splitIntoBlocks =
    SubqueryExecutorSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const SubqueryExecutorSplitType splitStep = SubqueryExecutorSplitType{step};

INSTANTIATE_TEST_CASE_P(SplicedSubqueryIntegrationTest, SplicedSubqueryIntegrationTest,
                        ::testing::Values(splitIntoBlocks<2, 3>, splitIntoBlocks<3, 4>,
                                          splitStep<2>, splitStep<1>));

TEST_P(SplicedSubqueryIntegrationTest, single_subquery_empty_input) {
  auto helper = makeExecutorTestHelper<1, 1>();
  auto call = AqlCall{};

  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList()
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, single_subquery) {
  auto helper = makeExecutorTestHelper<1, 2>();
  auto call = AqlCall{};

  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {{1, R"([1])"},
                             {2, R"([2])"},
                             {5, R"([5])"},
                             {2, R"([2])"},
                             {1, R"([1])"},
                             {5, R"([5])"},
                             {7, R"([7])"},
                             {1, R"([1])"}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, single_subquery_skip_and_produce) {
  auto helper = makeExecutorTestHelper<1, 2>();
  auto call = AqlCall{5};

  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {{5, R"([5])"}, {7, R"([7])"}, {1, R"([1])"}})
      .expectSkipped(5)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, single_subquery_skip_all) {
  auto helper = makeExecutorTestHelper<1, 2>();
  auto call = AqlCall{20};

  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {})
      .expectSkipped(8)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, single_subquery_fullcount) {
  auto helper = makeExecutorTestHelper<1, 2>();
  auto call = AqlCall{0, true, 0, AqlCall::LimitType::HARD};
  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {})
      .expectSkipped(8)
      .expectedState(ExecutionState::DONE)
      .run();
};

// NOTE: This test can be enabled if we can continue
// working on the second subquery without returning to consumer
TEST_P(SplicedSubqueryIntegrationTest, DISABLED_single_subquery_skip_produce_count) {
  auto helper = makeExecutorTestHelper<1, 2>();
  auto call = AqlCall{2, true, 2, AqlCall::LimitType::HARD};
  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {{5, R"([5])"}, {2, R"([2])"}})
      .expectSkipped(6)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, two_nested_subqueries_empty_input) {
  auto helper = makeExecutorTestHelper<1, 1>();
  auto call = AqlCall{};
  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList()
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, two_nested_subqueries) {
  auto helper = makeExecutorTestHelper<1, 1>();
  auto call = AqlCall{};
  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {{1}, {2}, {5}, {2}, {1}, {5}, {7}, {1}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, two_sequential_subqueries) {
  auto helper = makeExecutorTestHelper<1, 1>();
  auto call = AqlCall{};
  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {{1}, {2}, {5}, {2}, {1}, {5}, {7}, {1}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, do_nothing_in_subquery) {
  auto helper = makeExecutorTestHelper<1, 1>();
  auto call = AqlCall{};

  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<LambdaExe>(makeDoNothingRegisterInfos(), makeDoNothingExecutorInfos())
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {{1}, {2}, {5}, {2}, {1}, {5}, {7}, {1}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, check_call_passes_subquery) {
  auto helper = makeExecutorTestHelper<1, 1>();
  auto call = AqlCall{10};

  helper.addConsumer<LambdaExe>(makeAssertRegisterInfos(), makeAssertExecutorInfos(call))
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {})
      .expectSkipped(8)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, check_skipping_subquery) {
  auto helper = makeExecutorTestHelper<1, 1>();
  auto call = AqlCall{10};

  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<LambdaExe>(makeAssertRegisterInfos(), makeAssertExecutorInfos())
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {})
      .expectSkipped(8)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SplicedSubqueryIntegrationTest, check_soft_limit_subquery) {
  auto helper = makeExecutorTestHelper<1, 2>();
  auto call = AqlCall{0, false, 4, AqlCall::LimitType::SOFT};

  helper
      .addConsumer<SubqueryStartExecutor>(makeSubqueryStartRegisterInfos(),
                                          makeSubqueryStartExecutorInfos(),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<LambdaExe>(makeAssertRegisterInfos(), makeAssertExecutorInfos())
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {{1, R"([])"}, {2, R"([])"}, {5, R"([])"}, {2, R"([])"}})
      .expectSkipped(0)
      .expectedState(ExecutionState::HASMORE)
      .run();
};
