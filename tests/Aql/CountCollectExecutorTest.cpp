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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"
#include "TestLambdaExecutor.h"

#include "Aql/CountCollectExecutor.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Aql/SubqueryEndExecutor.h"
#include "Aql/SubqueryStartExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;

using LambdaExe = TestLambdaSkipExecutor;

namespace arangodb {
namespace tests {
namespace aql {

using CountCollectTestHelper = ExecutorTestHelper<1, 1>;
using CountCollectSplitType = CountCollectTestHelper::SplitType;
using CountCollectParamType = std::tuple<CountCollectSplitType>;

class CountCollectExecutorTest
    : public AqlExecutorTestCaseWithParam<CountCollectParamType, false> {
 protected:
  auto MakeInfos(RegisterId outReg) -> CountCollectExecutorInfos {
    return CountCollectExecutorInfos{
        outReg, outReg /*inputRegisters*/, outReg + 1 /*outputRegisters*/, {}, {}};
  }
  auto GetSplit() -> CountCollectSplitType {
    auto const& [split] = GetParam();
    return split;
  }

  auto MakeSubqueryStartInfos() -> SubqueryStartExecutor::Infos {
    auto inputRegisterSet = make_shared_unordered_set({0});
    auto outputRegisterSet = make_shared_unordered_set({});

    auto toKeepRegisterSet = std::unordered_set<RegisterId>{0};

    return SubqueryStartExecutor::Infos(inputRegisterSet, outputRegisterSet,
                                        inputRegisterSet->size(),
                                        inputRegisterSet->size() +
                                            outputRegisterSet->size(),
                                        {}, toKeepRegisterSet);
  }

  auto MakeSubqueryEndInfos(RegisterId inputRegister) -> SubqueryEndExecutor::Infos {
    auto const outputRegister = RegisterId{inputRegister + 1};
    auto inputRegisterSet = make_shared_unordered_set({});
    for (RegisterId r = 0; r <= inputRegister; ++r) {
      inputRegisterSet->emplace(r);
    }
    auto outputRegisterSet = make_shared_unordered_set({outputRegister});
    auto toKeepRegisterSet = std::unordered_set<RegisterId>{};

    return SubqueryEndExecutor::Infos(inputRegisterSet, outputRegisterSet,
                                      inputRegisterSet->size(),
                                      inputRegisterSet->size() +
                                          outputRegisterSet->size(),
                                      {}, toKeepRegisterSet, nullptr,
                                      inputRegister, outputRegister, false);
  }

  auto MakeRemoveAllLinesInfos() -> LambdaExe::Infos {
    auto numRegs = size_t{1};

    auto inRegisterList = make_shared_unordered_set({});
    auto outRegisterList = make_shared_unordered_set({});

    std::unordered_set<RegisterId> toKeep;

    for (RegisterId r = 0; r < numRegs; ++r) {
      toKeep.emplace(r);
    }

    ProduceCall prod = [](AqlItemBlockInputRange& input, OutputAqlItemRow& output)
        -> std::tuple<ExecutorState, LambdaExe::Stats, AqlCall> {
      EXPECT_TRUE(false) << "Should never be called";
      return {ExecutorState::DONE, NoStats{}, AqlCall{0, true, 0, AqlCall::LimitType::HARD}};
    };

    SkipCall skip = [](AqlItemBlockInputRange& input, AqlCall& call)
        -> std::tuple<ExecutorState, LambdaExe::Stats, size_t, AqlCall> {
      std::ignore = input.skipAll();
      return {input.upstreamState(), NoStats{}, 0,
              AqlCall{0, true, 0, AqlCall::LimitType::HARD}};
    };

    return LambdaExe::Infos(inRegisterList, outRegisterList, numRegs, numRegs,
                            {}, toKeep, prod, skip);
  }
};

template <size_t... vs>
const CountCollectSplitType splitIntoBlocks =
    CountCollectSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const CountCollectSplitType splitStep = CountCollectSplitType{step};

INSTANTIATE_TEST_CASE_P(CountCollectExecutor, CountCollectExecutorTest,
                        ::testing::Values(CountCollectSplitType{std::monostate()},
                                          splitStep<1>, splitIntoBlocks<2, 3>,
                                          splitStep<2>));

TEST_P(CountCollectExecutorTest, empty_input) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<CountCollectExecutor>(MakeInfos(1), ExecutionNode::COLLECT)
      .expectedStats(ExecutionStats{})
      .setInputSplitType(GetSplit())
      .setInputValue({})
      .expectedState(ExecutionState::DONE)
      .expectOutput({1}, {{0}})
      .expectSkipped(0)
      .setCall(AqlCall{})
      .run();
}

TEST_P(CountCollectExecutorTest, count_input) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<CountCollectExecutor>(MakeInfos(1), ExecutionNode::COLLECT)
      .expectedStats(ExecutionStats{})
      .setInputSplitType(GetSplit())
      .setInputValue({{0}, {1}, {2}, {3}})
      .expectedState(ExecutionState::DONE)
      .expectOutput({1}, {{4}})
      .expectSkipped(0)
      .setCall(AqlCall{})
      .run();
}

TEST_P(CountCollectExecutorTest, empty_input_skip) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<CountCollectExecutor>(MakeInfos(1), ExecutionNode::COLLECT)
      .expectedStats(ExecutionStats{})
      .setInputSplitType(GetSplit())
      .setInputValue({})
      .expectedState(ExecutionState::DONE)
      .expectOutput({1}, {})
      .expectSkipped(1)
      .setCall(AqlCall{10})
      .run();
}

TEST_P(CountCollectExecutorTest, count_input_skip) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<CountCollectExecutor>(MakeInfos(1), ExecutionNode::COLLECT)
      .expectedStats(ExecutionStats{})
      .setInputSplitType(GetSplit())
      .setInputValue({{0}, {1}, {2}, {3}})
      .expectedState(ExecutionState::DONE)
      .expectOutput({1}, {})
      .expectSkipped(1)
      .setCall(AqlCall{10})
      .run();
}

TEST_P(CountCollectExecutorTest, empty_input_fullCount) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<CountCollectExecutor>(MakeInfos(1), ExecutionNode::COLLECT)
      .expectedStats(ExecutionStats{})
      .setInputSplitType(GetSplit())
      .setInputValue({})
      .expectedState(ExecutionState::DONE)
      .expectOutput({1}, {})
      .expectSkipped(1)
      .setCall(AqlCall{0, true, 0, AqlCall::LimitType::HARD})
      .run();
}

TEST_P(CountCollectExecutorTest, count_input_fullCount) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<CountCollectExecutor>(MakeInfos(1), ExecutionNode::COLLECT)
      .expectedStats(ExecutionStats{})
      .setInputSplitType(GetSplit())
      .setInputValue({{0}, {1}, {2}, {3}})
      .expectedState(ExecutionState::DONE)
      .expectOutput({1}, {})
      .expectSkipped(1)
      .setCall(AqlCall{0, true, 0, AqlCall::LimitType::HARD})
      .run();
}

TEST_P(CountCollectExecutorTest, count_input_softlimit) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<CountCollectExecutor>(MakeInfos(1), ExecutionNode::COLLECT)
      .expectedStats(ExecutionStats{})
      .setInputSplitType(GetSplit())
      .setInputValue({{0}, {1}, {2}, {3}})
      .expectedState(ExecutionState::DONE)
      .expectOutput({1}, {{4}})
      .expectSkipped(0)
      .setCall(AqlCall{0, false, 1, AqlCall::LimitType::SOFT})
      .run();
}

TEST_P(CountCollectExecutorTest, count_in_empty_subquery) {
  auto helper = makeExecutorTestHelper<1, 1>();

  helper
      .addConsumer<SubqueryStartExecutor>(MakeSubqueryStartInfos(), ExecutionNode::SUBQUERY_START)
      .addConsumer<LambdaExe>(MakeRemoveAllLinesInfos(), ExecutionNode::FILTER)
      .addConsumer<CountCollectExecutor>(MakeInfos(1), ExecutionNode::COLLECT)
      .addConsumer<SubqueryEndExecutor>(MakeSubqueryEndInfos(1), ExecutionNode::SUBQUERY_END)
      .expectedStats(ExecutionStats{})
      .setInputSplitType(GetSplit())
      .setInputValue({{0}, {1}, {2}, {3}})
      .expectedState(ExecutionState::DONE)
      .expectOutput({2}, {{R"([0])"}, {R"([0])"}, {R"([0])"}, {R"([0])"}})
      .expectSkipped(0)
      .setCall(AqlCall{})
      .run();
}
TEST_P(CountCollectExecutorTest, count_in_subquery) {
  //   makeExecutorTestHelper<1, 1> helper();
  auto helper = makeExecutorTestHelper<1, 1>();

  helper
      .addConsumer<SubqueryStartExecutor>(MakeSubqueryStartInfos(), ExecutionNode::SUBQUERY_START)
      .addConsumer<CountCollectExecutor>(MakeInfos(1), ExecutionNode::COLLECT)
      .addConsumer<SubqueryEndExecutor>(MakeSubqueryEndInfos(1), ExecutionNode::SUBQUERY_END)
      .expectedStats(ExecutionStats{})
      .setInputSplitType(GetSplit())
      .setInputValue({{0}, {1}, {2}, {3}})
      .expectedState(ExecutionState::DONE)
      .expectOutput({2}, {{R"([1])"}, {R"([1])"}, {R"([1])"}, {R"([1])"}})
      .expectSkipped(0)
      .setCall(AqlCall{})
      .run();
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
