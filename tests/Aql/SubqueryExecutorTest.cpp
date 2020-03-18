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
#include "TestExecutorHelper.h"
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
#include "Aql/SubqueryExecutor.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include "Aql/ExecutorTestHelper.h"
#include "Aql/TestLambdaExecutor.h"
#include "Aql/WaitingExecutionBlockMock.h"

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

// This will maintain another deque of ExecutionBlocks
// Which represents the content of the Subquery.
// Note this cannot be concated with any other pipeline.
struct SubqueryPipeline : public Pipeline {
  explicit SubqueryPipeline(ExecBlock subquery, ExecBlock singleton)
      : Pipeline(std::move(subquery)),
        _subquery(get().front().get() /* == subquery */) {
    // must be of type SubqueryExecutor
    TRI_ASSERT(dynamic_cast<SubqueryExecutor<true>*>(_subquery) != nullptr ||
               dynamic_cast<SubqueryExecutor<false>*>(_subquery) != nullptr);
    // We always need to start with a singleton
    TRI_ASSERT(dynamic_cast<IdExecutor<ConstFetcher>*>(singleton.get()) != nullptr);
    _subqueryPipeline.emplace_back(std::move(singleton));
  }

  Pipeline& addSubqueryConsumer(ExecBlock&& consumer) {
    TRI_ASSERT(!_subqueryPipeline.empty());
    consumer->addDependency(_subqueryPipeline.front().get());
    _subqueryPipeline.emplace_front(std::move(consumer));

    return *this;
  }

  ExecutionBlock& getSubquery() const { return *_subquery; }

 private:
  ExecutionBlock* _subquery;
  PipelineStorage _subqueryPipeline;
};

class SubqueryExecutorIntegrationTest
    : public AqlExecutorTestCaseWithParam<SubqueryExecutorParamType, false> {
 protected:
  ExecutorTestHelper<1, 1> executorTestHelper;

  SubqueryExecutorIntegrationTest() : executorTestHelper(*fakedQuery) {}

  // returns a new pipeline that contains body as a subquery
  auto createSubquery(Pipeline body) -> Pipeline {

    /*
        auto subqueryEnd = createSubqueryEndExecutionBlock();
        if (!body.empty()) {
          subqueryEnd->addDependency(body.get().front().get());
        }
        body.get().emplace_front(std::move(subqueryEnd));

        auto subqueryStart = createSubqueryStartExecutionBlock();
        // This exists because we at least added the subqueryEnd
        body.get().back()->addDependency(subqueryStart.get());

        body.get().emplace_back(std::move(subqueryStart));
    */
    return body;
  }

  auto createSubquery() -> Pipeline { return createSubquery(Pipeline()); }

  auto createDoNothingPipeline() -> Pipeline {
    auto numRegs = size_t{1};
    auto emptyRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});

    auto inRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{0});
    auto outRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{1});

    std::unordered_set<RegisterId> toKeep;

    for (RegisterId r = 0; r < numRegs; ++r) {
      toKeep.emplace(r);
    }

    auto infos = LambdaExe::Infos(inRegisterList, outRegisterList, 1, 2, {},
                                  toKeep, createProduceCall(), createSkipCall());

    return Pipeline(executorTestHelper.createExecBlock<LambdaExe>(std::move(infos)));
  }

  auto createAssertPipeline() -> Pipeline {
    auto numRegs = size_t{1};
    auto emptyRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});

    auto inRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{0});
    auto outRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{1});

    std::unordered_set<RegisterId> toKeep;

    for (RegisterId r = 0; r < numRegs; ++r) {
      toKeep.emplace(r);
    }

    auto infos = LambdaExe::Infos(inRegisterList, outRegisterList, 1, 2, {},
                                  toKeep, createAssertCall(), createSkipCall());

    return Pipeline(executorTestHelper.createExecBlock<LambdaExe>(std::move(infos)));
  }

  auto createCallAssertPipeline(AqlCall call) -> Pipeline {
    auto numRegs = size_t{1};
    auto emptyRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});

    auto inRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{0});
    auto outRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{1});

    std::unordered_set<RegisterId> toKeep;

    for (RegisterId r = 0; r < numRegs; ++r) {
      toKeep.emplace(r);
    }

    auto infos = LambdaExe::Infos(inRegisterList, outRegisterList, 1, 2, {}, toKeep,
                                  createAssertCallCall(call), createSkipCall());

    return Pipeline(executorTestHelper.createExecBlock<LambdaExe>(std::move(infos)));
  }
#if 0
  auto createSubqueryStartExecutionBlock() -> ExecBlock {
    // Subquery start executor does not care about input or output registers?
    // TODO: talk about registers & register planning

    auto inputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{0});
    auto outputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{});
    auto toKeepRegisterSet = RegisterSet{0};

    auto infos = SubqueryStartExecutor::Infos(inputRegisterSet, outputRegisterSet,
                                              inputRegisterSet->size(),
                                              inputRegisterSet->size() +
                                                  outputRegisterSet->size(),
                                              {}, toKeepRegisterSet);

    return executorTestHelper.createExecBlock<SubqueryStartExecutor>(std::move(infos),
                                                                     ExecutionNode::SUBQUERY_START);
  }

  // Subquery end executor has an input and an output register,
  // but only the output register is used, remove input reg?
  auto createSubqueryEndExecutionBlock() -> ExecBlock {
    auto const inputRegister = RegisterId{0};
    auto const outputRegister = RegisterId{1};
    auto inputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{inputRegister});
    auto outputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{outputRegister});
    auto toKeepRegisterSet = RegisterSet{0};

    auto infos =
        SubqueryEndExecutor::Infos(inputRegisterSet, outputRegisterSet,
                                   inputRegisterSet->size(),
                                   inputRegisterSet->size() + outputRegisterSet->size(),
                                   {}, toKeepRegisterSet, nullptr,
                                   inputRegister, outputRegister, false);

    return executorTestHelper.createExecBlock<SubqueryEndExecutor>(std::move(infos),
                                                                   ExecutionNode::SUBQUERY_END);
  }
#endif
  auto createReturnExecutionBlock() -> ExecBlock {
    auto const inputRegister = RegisterId{0};
    auto const outputRegister = RegisterId{0};
    auto inputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{inputRegister});
    auto outputRegisterSet =
        std::make_shared<RegisterSet>(std::initializer_list<RegisterId>{outputRegister});
    auto toKeepRegisterSet = RegisterSet{0};

    auto infos = ReturnExecutor::Infos(inputRegister, 1, 1, false);

    return executorTestHelper.createExecBlock<ReturnExecutor>(std::move(infos),
                                                              ExecutionNode::RETURN);
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
      auto skipped = size_t{0};
      while (input.hasDataRow() && call.shouldSkip()) {
        auto const& [state, inputRow] = input.nextDataRow();
        EXPECT_TRUE(inputRow.isInitialized());
        call.didSkip(1);
        skipped++;
      }
      auto upstreamCall = AqlCall{call};
      return {input.upstreamState(), NoStats{}, skipped, upstreamCall};
    };
  };

  // Asserts if called. This is to check that when we use skip to
  // skip over a subquery, the subquery's produce is not invoked
  auto createAssertCall() -> ProduceCall {
    return [](AqlItemBlockInputRange& input,
              OutputAqlItemRow& output) -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      EXPECT_TRUE(false);
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

/*
INSTANTIATE_TEST_CASE_P(SubqueryExecutorIntegrationTest, SubqueryExecutorIntegrationTest,
                        ::testing::Values(splitIntoBlocks<2, 3>, splitIntoBlocks<3, 4>,
                                          splitStep<2>, splitStep<1>));
*/

TEST_P(SubqueryExecutorIntegrationTest, single_subquery_empty_input) {
  auto call = AqlCall{};
  auto pipeline = createSubquery();
  executorTestHelper.setPipeline(std::move(pipeline))
      .setInputValueList()
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, single_subquery) {
  auto call = AqlCall{};
  auto pipeline = createSubquery();
  ExecutorTestHelper<1, 2>{*fakedQuery}
      .setPipeline(std::move(pipeline))
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

TEST_P(SubqueryExecutorIntegrationTest, DISABLED_single_subquery_skip_and_produce) {
  auto call = AqlCall{5};
  auto pipeline = createSubquery();
  ExecutorTestHelper<1, 2>{*fakedQuery}
      .setPipeline(std::move(pipeline))
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {{5, R"([5])"}, {7, R"([7])"}, {1, R"([1])"}})
      .expectSkipped(5)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, DISABLED_single_subquery_skip_all) {
  auto call = AqlCall{20};
  auto pipeline = createSubquery();
  ExecutorTestHelper<1, 2>{*fakedQuery}
      .setPipeline(std::move(pipeline))
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {})
      .expectSkipped(8)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, DISABLED_single_subquery_fullcount) {
  auto call = AqlCall{0, true, 0, AqlCall::LimitType::HARD};
  auto pipeline = createSubquery();
  ExecutorTestHelper<1, 2>{*fakedQuery}
      .setPipeline(std::move(pipeline))
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {})
      .expectSkipped(8)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, DISABLED_single_subquery_skip_produce_count) {
  auto call = AqlCall{2, true, 2, AqlCall::LimitType::HARD};
  auto pipeline = createSubquery();
  ExecutorTestHelper<1, 2>{*fakedQuery}
      .setPipeline(std::move(pipeline))
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0, 1}, {{5, R"([5])"}, {2, R"([2])"}})
      .expectSkipped(6)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, two_nested_subqueries_empty_input) {
  auto call = AqlCall{};
  auto pipeline = createSubquery(createSubquery());
  executorTestHelper.setPipeline(std::move(pipeline))
      .setInputValueList()
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, two_nested_subqueries) {
  auto call = AqlCall{};
  auto pipeline = createSubquery(createSubquery());
  executorTestHelper.setPipeline(std::move(pipeline))
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {{1}, {2}, {5}, {2}, {1}, {5}, {7}, {1}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, two_sequential_subqueries) {
  auto call = AqlCall{};
  auto pipeline = concatPipelines(createSubquery(), createSubquery());
  executorTestHelper.setPipeline(std::move(pipeline))
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {{1}, {2}, {5}, {2}, {1}, {5}, {7}, {1}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, do_nothing_in_subquery) {
  auto call = AqlCall{};
  auto pipeline = createSubquery(createDoNothingPipeline());

  executorTestHelper.setPipeline(std::move(pipeline))
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {{1}, {2}, {5}, {2}, {1}, {5}, {7}, {1}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, DISABLED_check_call_passes_subquery) {
  auto call = AqlCall{10};
  auto pipeline = concatPipelines(createCallAssertPipeline(call), createSubquery());

  executorTestHelper.setPipeline(std::move(pipeline))
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {})
      .expectSkipped(8)
      .expectedState(ExecutionState::DONE)
      .run();
};

TEST_P(SubqueryExecutorIntegrationTest, DISABLED_check_skipping_subquery) {
  auto call = AqlCall{10};
  auto pipeline = createSubquery(createAssertPipeline());

  executorTestHelper.setPipeline(std::move(pipeline))
      .setInputValueList(1, 2, 5, 2, 1, 5, 7, 1)
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({0}, {})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
};
