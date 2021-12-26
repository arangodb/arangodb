////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "TestLambdaExecutor.h"

#include "RowFetcherHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ConstrainedSortExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"
#include "Aql/SubqueryStartExecutor.h"
#include "Aql/Variable.h"
#include "Basics/ResourceUsage.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include "AqlItemBlockHelper.h"
#include "search/sort.hpp"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

using SortTestHelper = ExecutorTestHelper<1, 1>;
using SortSplitType = SortTestHelper::SplitType;
using SortInputParam = std::tuple<SortSplitType>;

class SortExecutorTest : public AqlExecutorTestCaseWithParam<SortInputParam> {
 protected:
  auto getSplit() -> SortSplitType {
    auto const& [split] = GetParam();
    return split;
  }

  auto makeRegisterInfos(size_t nestingLevel = 1) -> RegisterInfos {
    SortElement sl{&sortVar, true};
    SortRegister sortReg{0, sl};
    std::vector<SortRegister> sortRegisters;
    sortRegisters.emplace_back(std::move(sortReg));
    TRI_ASSERT(nestingLevel > 0);
    RegIdSetStack toKeepStack{};
    for (size_t i = 0; i < nestingLevel; ++i) {
      toKeepStack.emplace_back(RegIdSet{0});
    }
    return RegisterInfos(RegIdSet{sortReg.reg}, {}, 1, 1, {},
                         std::move(toKeepStack));
  }

  auto makeExecutorInfos() -> SortExecutorInfos {
    SortElement sl{&sortVar, true};
    SortRegister sortReg{0, sl};
    std::vector<SortRegister> sortRegisters;
    sortRegisters.emplace_back(std::move(sortReg));
    return SortExecutorInfos(1, 1, {}, std::move(sortRegisters),
                             /*limit (ignored for default sort)*/ 0, manager(),
                             vpackOptions, monitor, false);
  }

  auto makeSubqueryRegisterInfos(size_t nestingLevel) -> RegisterInfos {
    TRI_ASSERT(nestingLevel > 0);
    RegIdSetStack toKeepStack{};
    for (size_t i = 0; i < nestingLevel; ++i) {
      toKeepStack.emplace_back(RegIdSet{0});
    }
    return RegisterInfos(RegIdSet{0}, {}, 1, 1, {}, std::move(toKeepStack));
  }

  auto dropAllLambdaExecutorInfos() -> TestLambdaSkipExecutor::Infos {
    auto dropAll = [](AqlItemBlockInputRange& input, OutputAqlItemRow& output)
        -> std::tuple<ExecutorState, TestLambdaSkipExecutor::Stats, AqlCall> {
      while (input.hasDataRow() && !output.isFull()) {
        auto const [state, row] = input.nextDataRow();
        // Just drop!
      }
      NoStats stats{};
      // Fetch all
      AqlCall call{};
      return {input.upstreamState(), stats, call};
    };
    auto dropSkipAll = [](AqlItemBlockInputRange& input, AqlCall& inCall)
        -> std::tuple<ExecutorState, TestLambdaSkipExecutor::Stats, size_t,
                      AqlCall> {
      while (input.hasDataRow()) {
        auto const [state, row] = input.nextDataRow();
        // Just drop!
      }
      NoStats stats{};
      AqlCall call{};

      return {input.upstreamState(), stats, 0, call};
    };
    return TestLambdaSkipExecutor::Infos{dropAll, dropSkipAll};
  }

 private:
  velocypack::Options const* vpackOptions{&velocypack::Options::Defaults};
  Variable sortVar{"mySortVar", 0, false};
};

template<size_t... vs>
const SortSplitType splitIntoBlocks =
    SortSplitType{std::vector<std::size_t>{vs...}};
template<size_t step>
const SortSplitType splitStep = SortSplitType{step};

INSTANTIATE_TEST_CASE_P(SortExecutorTest, SortExecutorTest,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<1>,
                                          splitStep<2>));

TEST_P(SortExecutorTest, does_sort_all) {
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper()
      .addConsumer<SortExecutor>(makeRegisterInfos(), makeExecutorInfos(),
                                 ExecutionNode::SORT)
      .setInputSplitType(getSplit())
      .setInputValue({{5}, {3}, {1}, {2}, {4}})
      .expectOutput({0}, {{1}, {2}, {3}, {4}, {5}})
      .setCall(call)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(SortExecutorTest, no_input) {
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper()
      .addConsumer<SortExecutor>(makeRegisterInfos(), makeExecutorInfos(),
                                 ExecutionNode::SORT)
      .setInputSplitType(getSplit())
      .setInputValue({})
      .expectOutput({0}, {})
      .setCall(call)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(SortExecutorTest, skip) {
  AqlCall call{2};
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper()
      .addConsumer<SortExecutor>(makeRegisterInfos(), makeExecutorInfos(),
                                 ExecutionNode::SORT)
      .setInputSplitType(getSplit())
      .setInputValue({{5}, {3}, {1}, {2}, {4}})
      .expectOutput({0}, {{3}, {4}, {5}})
      .setCall(call)
      .expectSkipped(2)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(SortExecutorTest, hard_limit) {
  AqlCall call{0, false, 2, AqlCall::LimitType::HARD};
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper()
      .addConsumer<SortExecutor>(makeRegisterInfos(), makeExecutorInfos(),
                                 ExecutionNode::SORT)
      .setInputSplitType(getSplit())
      .setInputValue({{5}, {3}, {1}, {2}, {4}})
      .expectOutput({0}, {{1}, {2}})
      .setCall(call)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(SortExecutorTest, soft_limit) {
  AqlCall call{0, false, 2, AqlCall::LimitType::SOFT};
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper()
      .addConsumer<SortExecutor>(makeRegisterInfos(), makeExecutorInfos(),
                                 ExecutionNode::SORT)
      .setInputSplitType(getSplit())
      .setInputValue({{5}, {3}, {1}, {2}, {4}})
      .expectOutput({0}, {{1}, {2}})
      .setCall(call)
      .expectSkipped(0)
      .expectedState(ExecutionState::HASMORE)
      .run();
}

TEST_P(SortExecutorTest, fullcount) {
  AqlCall call{0, true, 2, AqlCall::LimitType::HARD};
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper()
      .addConsumer<SortExecutor>(makeRegisterInfos(), makeExecutorInfos(),
                                 ExecutionNode::SORT)
      .setInputSplitType(getSplit())
      .setInputValue({{5}, {3}, {1}, {2}, {4}})
      .expectOutput({0}, {{1}, {2}})
      .setCall(call)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(SortExecutorTest, skip_produce_fullcount) {
  AqlCall call{2, true, 2, AqlCall::LimitType::HARD};
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper()
      .addConsumer<SortExecutor>(makeRegisterInfos(), makeExecutorInfos(),
                                 ExecutionNode::SORT)
      .setInputSplitType(getSplit())
      .setInputValue({{5}, {3}, {1}, {2}, {4}})
      .expectOutput({0}, {{3}, {4}})
      .setCall(call)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(SortExecutorTest, skip_too_much) {
  AqlCall call{10, false};
  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper()
      .addConsumer<SortExecutor>(makeRegisterInfos(), makeExecutorInfos(),
                                 ExecutionNode::SORT)
      .setInputSplitType(getSplit())
      .setInputValue({{5}, {3}, {1}, {2}, {4}})
      .expectOutput({0}, {})
      .setCall(call)
      .expectSkipped(5)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(SortExecutorTest, skip_nested_subquery_no_data) {
  // Take a double nested subquery-fetch all Stack
  AqlCallStack callStack{AqlCallList{AqlCall{}}};
  callStack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
  callStack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});

  ExecutionStats stats{};  // No stats here
  makeExecutorTestHelper()
      .addConsumer<SubqueryStartExecutor>(makeSubqueryRegisterInfos(2),
                                          makeSubqueryRegisterInfos(2),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<TestLambdaSkipExecutor>(makeSubqueryRegisterInfos(2),
                                           dropAllLambdaExecutorInfos(),
                                           ExecutionNode::FILTER)
      .addConsumer<SubqueryStartExecutor>(makeSubqueryRegisterInfos(3),
                                          makeSubqueryRegisterInfos(3),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SortExecutor>(makeRegisterInfos(3), makeExecutorInfos(),
                                 ExecutionNode::SORT)
      .setInputValue({{1}})
      .expectOutput({0}, {{1}}, {{0, 1}})
      .setCallStack(callStack)
      .expectSkipped(0, 0, 0)
      .expectedState(ExecutionState::DONE)
      .run();
}
}  // namespace arangodb::tests::aql
