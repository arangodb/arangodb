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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "ExecutorTestHelper.h"
#include "RowFetcherHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockHelper.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/SubqueryStartExecutor.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/LogMacros.h"
using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

namespace {
ExecutorInfos MakeBaseInfos(RegisterId numRegs) {
  auto emptyRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
      std::initializer_list<RegisterId>{});
  std::unordered_set<RegisterId> toKeep;
  for (RegisterId r = 0; r < numRegs; ++r) {
    toKeep.emplace(r);
  }
  return ExecutorInfos(emptyRegisterList, emptyRegisterList, numRegs, numRegs, {}, toKeep);
}
}  // namespace

class SubqueryStartExecutorTest : public AqlExecutorTestCase<true> {
 protected:
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher{
      itemBlockManager, VPackParser::fromJson("[]")->steal(), false};
  AqlCall call{};

  auto queryStack(AqlCall fromSubqueryEnd, AqlCall insideSubquery) const -> AqlCallStack {
    AqlCallStack stack(fromSubqueryEnd);
    stack.pushCall(std::move(insideSubquery));
    return stack;
  }
};

TEST_F(SubqueryStartExecutorTest, check_properties) {
  EXPECT_TRUE(SubqueryStartExecutor::Properties::preservesOrder)
      << "The block has no effect on ordering of elements, it adds additional "
         "rows only.";
  EXPECT_EQ(SubqueryStartExecutor::Properties::allowsBlockPassthrough, ::arangodb::aql::BlockPassthrough::Disable)
      << "The block cannot be passThrough, as it increases the number of rows.";
  EXPECT_TRUE(SubqueryStartExecutor::Properties::inputSizeRestrictsOutputSize)
      << "The block is restricted by input, it will atMost produce 2 times the "
         "input. (Might be less if input contains shadowRows";
}

TEST_F(SubqueryStartExecutorTest, empty_input_does_not_add_shadow_rows) {
  ExecutorTestHelper<1, 1>(*fakedQuery)
      .setExecBlock<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run();
}

TEST_F(SubqueryStartExecutorTest, adds_a_shadowrow_after_single_input) {
  ExecutorTestHelper<1, 1>(*fakedQuery)
      .setExecBlock<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run();
}

// NOTE: The following two tests exclude each other.
// Right now we can only support 1 ShadowRow per request, we cannot do a look-ahead of
// calls. As soon as we can this test needs to re removed, and the one blow needs to be activated.
TEST_F(SubqueryStartExecutorTest,
       adds_only_one_shadowrow_even_if_more_input_is_available_in_single_pass) {
  ExecutorTestHelper<1, 1>(*fakedQuery)
      .setExecBlock<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{{R"("a")"}}, {{R"("b")"}}, {{R"("c")"}}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::HASMORE)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run();
}

// NOTE: This test and the one right above do exclude each other.
// This is the behaviour we would like to have eventually
// As soon as we can support Call look-aheads we need to enable this test.
// and it needs to pass then
TEST_F(SubqueryStartExecutorTest, DISABLED_adds_a_shadowrow_after_every_input_line_in_single_pass) {
  ExecutorTestHelper<1, 1>(*fakedQuery)
      .setExecBlock<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{{R"("a")"}}, {{R"("b")"}}, {{R"("c")"}}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}, {R"("b")"}, {R"("b")"}, {R"("c")"}, {R"("c")"}},
                    {{1, 0}, {3, 0}, {5, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run();
}

// NOTE: As soon as the single_pass test is enabled this test is superflous.
// It will be identical to the one above
TEST_F(SubqueryStartExecutorTest, adds_a_shadowrow_after_every_input_line) {
  ExecutorTestHelper<1, 1>(*fakedQuery)
      .setExecBlock<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{{R"("a")"}}, {{R"("b")"}}, {{R"("c")"}}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}, {R"("b")"}, {R"("b")"}, {R"("c")"}, {R"("c")"}},
                    {{1, 0}, {3, 0}, {5, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run(true);
}

TEST_F(SubqueryStartExecutorTest, shadow_row_does_not_fit_in_current_block) {
  // NOTE: This test relies on batchSizes beeing handled correctly and we do not over-allocate memory
  // Also it tests, that ShadowRows go into place accounting of the output block (count as 1 line)

  // NOTE: Reduce batch size to 1, to enforce a too small output block
  ExecutionBlock::setDefaultBatchSize(1);
  TRI_DEFER(ExecutionBlock::setDefaultBatchSize(ExecutionBlock::DefaultBatchSize);) {
    // First test: Validate that the shadowRow is not written
    // We only do a single call here
    ExecutorTestHelper<1, 1>(*fakedQuery)
        .setExecBlock<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectOutput({0}, {{R"("a")"}}, {})
        .setCallStack(queryStack(AqlCall{}, AqlCall{}))
        .run();
  }
  {
    // Second test: Validate that the shadowRow is eventually written
    // if we call often enough
    ExecutorTestHelper<1, 1>(*fakedQuery)
        .setExecBlock<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::DONE)
        .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
        .setCallStack(queryStack(AqlCall{}, AqlCall{}))
        .run(true);
  }
}
