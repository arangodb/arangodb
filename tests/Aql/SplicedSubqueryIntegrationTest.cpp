////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Aql/AqlItemBlockHelper.h"
#include "Aql/Executor/AqlExecutorTestCase.h"
#include "Aql/Executor/TestEmptyExecutorHelper.h"
#include "Aql/Executor/TestLambdaExecutor.h"
#include "Aql/Executor/TestLambdaExecutor.h"
#include "Aql/WaitingExecutionBlockMock.h"
#include "Aql/WaitingExecutionBlockMock.h"
#include "Mocks/Servers.h"
#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/Executor/LimitExecutor.h"
#include "Aql/Executor/ReturnExecutor.h"
#include "Aql/Executor/SubqueryEndExecutor.h"
#include "Aql/Executor/SubqueryStartExecutor.h"
#include "Aql/Query.h"
#include "Basics/ScopeGuard.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::basics;

using SubqueryExecutorParamType = std::tuple<SplitType>;

using RegisterSet = std::unordered_set<RegisterId>;
using LambdaExePassThrough = TestLambdaExecutor;
using LambdaExe = TestLambdaSkipExecutor;

class SplicedSubqueryIntegrationTest
    : public AqlExecutorTestCaseWithParam<SubqueryExecutorParamType, false> {
 protected:
  auto makeSubqueryStartRegisterInfos() -> RegisterInfos {
    auto inputRegisterSet = RegIdSet{0};
    auto outputRegisterSet = RegIdSet{};

    auto toKeepRegisterSet =
        RegIdSetStack{RegIdSet{0}, RegIdSet{0}, RegIdSet{0}};

    auto nrInputRegisters = static_cast<RegisterCount>(inputRegisterSet.size());
    auto nrOutputRegisters = static_cast<RegisterCount>(
        inputRegisterSet.size() + outputRegisterSet.size());
    return RegisterInfos(inputRegisterSet, outputRegisterSet, nrInputRegisters,
                         nrOutputRegisters, {}, toKeepRegisterSet);
  }
  auto makeSubqueryStartExecutorInfos() -> SubqueryStartExecutor::Infos {
    auto inputRegisterSet = RegIdSet{0};
    auto outputRegisterSet = RegIdSet{};

    auto toKeepRegisterSet =
        RegIdSetStack{RegIdSet{0}, RegIdSet{0}, RegIdSet{0}};

    auto nrInputRegisters = static_cast<RegisterCount>(inputRegisterSet.size());
    auto nrOutputRegisters = static_cast<RegisterCount>(
        inputRegisterSet.size() + outputRegisterSet.size());
    return SubqueryStartExecutor::Infos(inputRegisterSet, outputRegisterSet,
                                        nrInputRegisters, nrOutputRegisters, {},
                                        toKeepRegisterSet);
  }

  auto makeSubqueryEndRegisterInfos(RegisterId inputRegister) -> RegisterInfos {
    auto inputRegisterSet = RegIdSet{inputRegister};
    auto const outputRegister =
        RegisterId{static_cast<RegisterId::value_t>(inputRegister.value() + 1)};
    for (RegisterId::value_t r = 0; r <= inputRegister.value(); ++r) {
      inputRegisterSet.emplace(r);
    }
    auto outputRegisterSet = RegIdSet{outputRegister};
    auto toKeepRegisterSet =
        RegIdSetStack{RegIdSet{inputRegisterSet}, RegIdSet{inputRegisterSet},
                      RegIdSet{inputRegisterSet}};

    auto nrInputRegisters = static_cast<RegisterCount>(inputRegisterSet.size());
    auto nrOutputRegisters = static_cast<RegisterCount>(
        inputRegisterSet.size() + outputRegisterSet.size());

    return RegisterInfos(inputRegisterSet, outputRegisterSet, nrInputRegisters,
                         nrOutputRegisters, {}, toKeepRegisterSet);
  }

  auto makeSubqueryEndExecutorInfos(RegisterId inputRegister)
      -> SubqueryEndExecutor::Infos {
    auto const outputRegister =
        RegisterId{static_cast<RegisterId::value_t>(inputRegister.value() + 1)};

    return SubqueryEndExecutor::Infos(nullptr, monitor, inputRegister,
                                      outputRegister);
  }

  auto makeDoNothingRegisterInfos() -> RegisterInfos {
    auto numRegs = size_t{1};

    RegIdSet prototype{};
    for (RegisterId::value_t r = 0; r < numRegs; ++r) {
      prototype.emplace(r);
    }

    return RegisterInfos(RegIdSet{0}, RegIdSet{1}, 1, 2, {},
                         {prototype, prototype, prototype});
  }

  auto makeDoNothingExecutorInfos() -> LambdaExe::Infos {
    return LambdaExe::Infos(createProduceCall(), createSkipCall());
  }

  auto makeAssertRegisterInfos() -> RegisterInfos {
    auto numRegs = size_t{1};
    RegIdSet prototype{};
    for (RegisterId::value_t r = 0; r < numRegs; ++r) {
      prototype.emplace(r);
    }

    return RegisterInfos(RegIdSet{0}, RegIdSet{1}, 1, 2, {},
                         {{prototype}, {prototype}});
  }

  auto makeAssertExecutorInfos() -> LambdaExe::Infos {
    return LambdaExe::Infos(createAssertCall(), createSkipCall());
  }

  auto makeAssertExecutorInfos(AqlCall call) -> LambdaExe::Infos {
    return LambdaExe::Infos(createAssertCallCall(call), createSkipCall());
  }

  auto createProduceCall() -> ProduceCall {
    return [](AqlItemBlockInputRange& input, OutputAqlItemRow& output)
               -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
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
    return [](AqlItemBlockInputRange& input, AqlCall& call)
               -> std::tuple<ExecutorState, LambdaExe::Stats, size_t, AqlCall> {
      while (call.needSkipMore() && input.skippedInFlight() > 0) {
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
      while (input.hasDataRow() && call.needSkipMore()) {
        auto const& [state, inputRow] = input.nextDataRow();
        EXPECT_TRUE(inputRow.isInitialized());
        call.didSkip(1);
      }
      auto upstreamCall = AqlCall{call};
      return {input.upstreamState(), NoStats{}, call.getSkipCount(),
              upstreamCall};
    };
  };

  // Asserts if called. This is to check that when we use skip to
  // skip over a subquery, the subquery's produce is not invoked
  // with data
  auto createAssertCall() -> ProduceCall {
    return [](AqlItemBlockInputRange& input, OutputAqlItemRow& output)
               -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      EXPECT_FALSE(input.hasDataRow());
      NoStats stats{};
      AqlCall call{};

      return {ExecutorState::DONE, stats, call};
    };
  }

  auto createAssertCallCall(AqlCall call) -> ProduceCall {
    return [call](AqlItemBlockInputRange& input, OutputAqlItemRow& output)
               -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
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
  auto getSplit() -> SplitType {
    auto [split] = GetParam();
    return split;
  }
};

template<size_t... vs>
const SplitType splitIntoBlocks = SplitType{std::vector<std::size_t>{vs...}};
template<size_t step>
const SplitType splitStep = SplitType{step};

INSTANTIATE_TEST_CASE_P(SplicedSubqueryIntegrationTest,
                        SplicedSubqueryIntegrationTest,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<2>,
                                          splitStep<1>));

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
TEST_P(SplicedSubqueryIntegrationTest,
       DISABLED_single_subquery_skip_produce_count) {
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

// Regression test for https://github.com/arangodb/arangodb/issues/16451
TEST_P(SplicedSubqueryIntegrationTest, nested_subqueries_inner_limit) {
  auto helper = makeExecutorTestHelper<1, 1>();
  auto call = AqlCall{};
  ExecutionBlock::setDefaultBatchSize(2);
  auto sg = arangodb::scopeGuard([&]() noexcept {
    ExecutionBlock::setDefaultBatchSize(
        ExecutionBlock::ProductionDefaultBatchSize);
  });
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
      .addConsumer<LimitExecutor>(
          RegisterInfos(RegIdSet{}, RegIdSet{}, RegisterCount{2},
                        RegisterCount{2}, {},
                        RegIdSetStack{RegIdSet{RegisterId{0}, RegisterId{0},
                                               RegisterId{0}}}),
          LimitExecutor::Infos(0, 1 /* limit */, false), ExecutionNode::LIMIT)
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {{1}})
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
      .addConsumer<LambdaExe>(makeDoNothingRegisterInfos(),
                              makeDoNothingExecutorInfos())
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

  helper
      .addConsumer<LambdaExe>(makeAssertRegisterInfos(),
                              makeAssertExecutorInfos(call))
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
      .addConsumer<LambdaExe>(makeAssertRegisterInfos(),
                              makeAssertExecutorInfos())
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
      .addConsumer<LambdaExe>(makeAssertRegisterInfos(),
                              makeAssertExecutorInfos())
      .addConsumer<SubqueryEndExecutor>(makeSubqueryEndRegisterInfos(0),
                                        makeSubqueryEndExecutorInfos(0),
                                        ExecutionNode::SUBQUERY_END)
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1},
                    {{1, R"([])"}, {2, R"([])"}, {5, R"([])"}, {2, R"([])"}})
      .expectSkipped(0)
      .expectedState(ExecutionState::HASMORE)
      .run();
};
