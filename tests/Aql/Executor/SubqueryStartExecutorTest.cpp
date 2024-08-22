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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Executor/AqlExecutorTestCase.h"
#include "Aql/FixedOutputExecutionBlockMock.h"
#include "Aql/RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockHelper.h"
#include "Aql/Executor/SubqueryStartExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Basics/ScopeGuard.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

namespace {
RegisterInfos MakeBaseInfos(RegisterCount numRegs, size_t subqueryDepth = 2) {
  RegIdSet prototype{};
  for (RegisterId::value_t r = 0; r < numRegs; ++r) {
    prototype.emplace(r);
  }
  RegIdSetStack regsToKeep{};
  for (size_t i = 0; i <= subqueryDepth; ++i) {
    regsToKeep.push_back({prototype});
  }
  return RegisterInfos({}, {}, numRegs, numRegs, {}, regsToKeep);
}
}  // namespace

class SubqueryStartExecutorTest
    : public AqlExecutorTestCaseWithParam<std::tuple<SplitType>, false> {
 protected:
  auto GetSplit() const -> SplitType {
    auto const [split] = GetParam();
    return split;
  }

  auto queryStack(AqlCall fromSubqueryEnd, AqlCall insideSubquery) const
      -> AqlCallStack {
    AqlCallList list =
        insideSubquery.getOffset() == 0 && !insideSubquery.needsFullCount()
            ? AqlCallList{insideSubquery, insideSubquery}
            : AqlCallList{insideSubquery};
    AqlCallStack stack(AqlCallList{fromSubqueryEnd});
    stack.pushCall(list);
    return stack;
  }
};

template<size_t... vs>
const SplitType splitIntoBlocks = SplitType{std::vector<std::size_t>{vs...}};
template<size_t step>
const SplitType splitStep = SplitType{step};

INSTANTIATE_TEST_CASE_P(SubqueryStartExecutorTest, SubqueryStartExecutorTest,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<2>));

TEST_P(SubqueryStartExecutorTest, check_properties) {
  EXPECT_TRUE(SubqueryStartExecutor::Properties::preservesOrder)
      << "The block has no effect on ordering of elements, it adds additional "
         "rows only.";
  EXPECT_EQ(SubqueryStartExecutor::Properties::allowsBlockPassthrough,
            ::arangodb::aql::BlockPassthrough::Disable)
      << "The block cannot be passThrough, as it increases the number of rows.";
}

TEST_P(SubqueryStartExecutorTest, empty_input_does_not_add_shadow_rows) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {})
      .expectSkipped(0, 0)
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest, adds_a_shadowrow_after_single_input) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectSkipped(0, 0)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest,
       adds_a_shadowrow_after_every_input_line_in_single_pass) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{{R"("a")"}}, {{R"("b")"}}, {{R"("c")"}}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectSkipped(0, 0)
      .expectOutput({0},
                    {{R"("a")"},
                     {R"("a")"},
                     {R"("b")"},
                     {R"("b")"},
                     {R"("c")"},
                     {R"("c")"}},
                    {{1, 0}, {3, 0}, {5, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .setInputSplitType(GetSplit())
      .run();
}

// NOTE: As soon as the single_pass test is enabled this test is superflous.
// It will be identical to the one above
TEST_P(SubqueryStartExecutorTest, adds_a_shadowrow_after_every_input_line) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{{R"("a")"}}, {{R"("b")"}}, {{R"("c")"}}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectSkipped(0, 0)
      .expectOutput({0},
                    {{R"("a")"},
                     {R"("a")"},
                     {R"("b")"},
                     {R"("b")"},
                     {R"("c")"},
                     {R"("c")"}},
                    {{1, 0}, {3, 0}, {5, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .setInputSplitType(GetSplit())
      .run(true);
}

TEST_P(SubqueryStartExecutorTest, shadow_row_does_not_fit_in_current_block) {
  // NOTE: This test relies on batchSizes being handled correctly and we do not
  // over-allocate memory Also it tests, that ShadowRows go into place
  // accounting of the output block (count as 1 line)

  // NOTE: Reduce batch size to 1, to enforce a too small output block
  ExecutionBlock::setDefaultBatchSize(1);
  auto sg = arangodb::scopeGuard([&]() noexcept {
    ExecutionBlock::setDefaultBatchSize(
        ExecutionBlock::ProductionDefaultBatchSize);
  });
  {
    // First test: Validate that the shadowRow is not written
    // We only do a single call here
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                            ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectSkipped(0, 0)
        .expectOutput({0}, {{R"("a")"}}, {})
        .setCallStack(queryStack(AqlCall{}, AqlCall{}))
        .setInputSplitType(GetSplit())
        .run();
  }
  {
    // Second test: Validate that the shadowRow is eventually written
    // if we call often enough
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                            ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::DONE)
        .expectSkipped(0, 0)
        .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
        .setCallStack(queryStack(AqlCall{}, AqlCall{}))
        .setInputSplitType(GetSplit())
        .run(true);
  }
}

TEST_P(SubqueryStartExecutorTest, skip_in_subquery) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("a")"}}, {{0, 0}})
      .expectSkipped(0, 1)
      .setCallStack(queryStack(AqlCall{}, AqlCall{10, false}))
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest, fullCount_in_subquery) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("a")"}}, {{0, 0}})
      .expectSkipped(0, 1)
      .setCallStack(
          queryStack(AqlCall{}, AqlCall{0, true, 0, AqlCall::LimitType::HARD}))
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest, shadow_row_forwarding) {
  auto helper = makeExecutorTestHelper<1, 1>();

  AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
  stack.pushCall(AqlCallList{AqlCall{}});

  helper
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START);

  helper.expectSkipped(0, 0, 0);

  helper.setInputValue({{R"("a")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}, {R"("a")"}}, {{1, 0}, {2, 1}})
      .setCallStack(stack)
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest,
       shadow_row_forwarding_many_inputs_single_call) {
  auto helper = makeExecutorTestHelper<1, 1>();
  AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
  stack.pushCall(AqlCallList{AqlCall{}});

  helper
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START);

  helper.expectSkipped(0, 0, 0);

  helper.setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::HASMORE)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}, {R"("a")"}}, {{1, 0}, {2, 1}})
      .setCallStack(stack)
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest,
       shadow_row_forwarding_many_inputs_many_requests) {
  auto helper = makeExecutorTestHelper<1, 1>();
  AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
  stack.pushCall(AqlCallList{AqlCall{}});

  helper
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START);

  helper.expectSkipped(0, 0, 0);

  helper.setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0},
                    {{R"("a")"},
                     {R"("a")"},
                     {R"("a")"},
                     {R"("b")"},
                     {R"("b")"},
                     {R"("b")"},
                     {R"("c")"},
                     {R"("c")"},
                     {R"("c")"}},
                    {{1, 0}, {2, 1}, {4, 0}, {5, 1}, {7, 0}, {8, 1}})
      .setCallStack(stack)
      .setInputSplitType(GetSplit())
      .run(true);
}

TEST_P(SubqueryStartExecutorTest,
       shadow_row_forwarding_many_inputs_not_enough_space) {
  // NOTE: This test relies on batchSizes being handled correctly and we do not
  // over-allocate memory Also it tests, that ShadowRows go into place
  // accounting of the output block (count as 1 line)

  // NOTE: Reduce batch size to 2, to enforce a too small output block, in
  // between the shadow Rows
  ExecutionBlock::setDefaultBatchSize(2);
  auto sg = arangodb::scopeGuard([&]() noexcept {
    ExecutionBlock::setDefaultBatchSize(
        ExecutionBlock::ProductionDefaultBatchSize);
  });
  {
    // First test: Validate that the shadowRow is not written
    // We only do a single call here
    auto helper = makeExecutorTestHelper<1, 1>();

    AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
    stack.pushCall(AqlCallList{AqlCall{}});

    helper
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                            ExecutionNode::SUBQUERY_START)
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                            ExecutionNode::SUBQUERY_START);

    helper.expectSkipped(0, 0, 0);

    helper.setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
        .setCallStack(stack)
        .setInputSplitType(GetSplit())
        .run();
  }
  {
    // Second test: Validate that the shadowRow is eventually written
    // Wedo call as many times as we need to.
    auto helper = makeExecutorTestHelper<1, 1>();

    AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
    stack.pushCall(AqlCallList{AqlCall{}});

    helper
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                            ExecutionNode::SUBQUERY_START)
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                            ExecutionNode::SUBQUERY_START);

    helper.expectSkipped(0, 0, 0);

    helper.setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::DONE)
        .expectOutput({0},
                      {{R"("a")"},
                       {R"("a")"},
                       {R"("a")"},
                       {R"("b")"},
                       {R"("b")"},
                       {R"("b")"},
                       {R"("c")"},
                       {R"("c")"},
                       {R"("c")"}},
                      {{1, 0}, {2, 1}, {4, 0}, {5, 1}, {7, 0}, {8, 1}})
        .setCallStack(stack)
        .setInputSplitType(GetSplit())
        .run(true);
  }
}

TEST_P(SubqueryStartExecutorTest, skip_in_outer_subquery) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"}, {R"("b")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("b")"}, {R"("b")"}}, {{1, 0}})
      .expectSkipped(1, 0)
      .setCallStack(
          queryStack(AqlCall{1, false, AqlCall::Infinity{}}, AqlCall{}))
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest, DISABLED_skip_only_in_outer_subquery) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"}, {R"("b")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {})
      .expectSkipped(1, 0)
      .setCallStack(queryStack(AqlCall{1, false}, AqlCall{}))
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest, fullCount_in_outer_subquery) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"},
                      {R"("b")"},
                      {R"("c")"},
                      {R"("d")"},
                      {R"("e")"},
                      {R"("f")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {})
      .expectSkipped(6, 0)
      .setCallStack(
          queryStack(AqlCall{0, true, 0, AqlCall::LimitType::HARD}, AqlCall{}))
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest, fastForward_in_inner_subquery) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"},
                      {R"("b")"},
                      {R"("c")"},
                      {R"("d")"},
                      {R"("e")"},
                      {R"("f")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0},
                    {{R"("a")"},
                     {R"("b")"},
                     {R"("c")"},
                     {R"("d")"},
                     {R"("e")"},
                     {R"("f")"}},
                    {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}})
      .expectSkipped(0, 0)
      .setCallStack(queryStack(AqlCall{0, false, AqlCall::Infinity{}},
                               AqlCall{0, false, 0, AqlCall::LimitType::HARD}))
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest, skip_out_skip_in) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"},
                      {R"("b")"},
                      {R"("c")"},
                      {R"("d")"},
                      {R"("e")"},
                      {R"("f")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::HASMORE)
      .expectOutput({0}, {{R"("c")"}}, {{0, 0}})
      .expectSkipped(2, 1)
      .setCallStack(queryStack(AqlCall{2, false, AqlCall::Infinity{}},
                               AqlCall{10, false, AqlCall::Infinity{}}))
      .setInputSplitType(GetSplit())
      .run();
}

TEST_P(SubqueryStartExecutorTest, fullbypass_in_outer_subquery) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), MakeBaseInfos(1),
                                          ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"},
                      {R"("b")"},
                      {R"("c")"},
                      {R"("d")"},
                      {R"("e")"},
                      {R"("f")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {})
      .expectSkipped(0, 0)
      .setCallStack(
          queryStack(AqlCall{0, false, 0, AqlCall::LimitType::HARD}, AqlCall{}))
      .setInputSplitType(GetSplit())
      .run();
}

class SubqueryStartSpecficTest : public AqlExecutorTestCase<false> {};

TEST_F(SubqueryStartSpecficTest, hard_limit_nested_subqueries) {
  // NOTE: This is a regression test for DEVSUP-899, the below is
  // a partial execution of the query where the issue got triggered
  std::deque<arangodb::aql::SharedAqlItemBlockPtr> inputData{};

  // The issue under test is a split after a datarow, but before the
  // shadowRow (entry 5)
  // This caused the SubqueryStartExecutor to not reset that it has returned
  // done.
  inputData.push_back(buildBlock<2>(manager(),
                                    {{1, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {2, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {3, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {4, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {5, NoneEntry{}}},
                                    {{1, 0}, {3, 0}, {5, 0}, {7, 0}}));

  inputData.push_back(buildBlock<2>(manager(),
                                    {{NoneEntry{}, NoneEntry{}},
                                     {6, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {7, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}}},
                                    {{0, 0}, {2, 0}, {4, 0}}));

  inputData.push_back(buildBlock<2>(
      manager(),
      {{8, NoneEntry{}}, {NoneEntry{}, NoneEntry{}}, {9, NoneEntry{}}},
      {{1, 0}}));

  inputData.push_back(buildBlock<2>(manager(),
                                    {
                                        {NoneEntry{}, NoneEntry{}},
                                    },
                                    {{0, 0}}));

  MockTypedNode inputNode{fakedQuery->plan(), ExecutionNodeId{1},
                          ExecutionNode::FILTER};
  FixedOutputExecutionBlockMock dependency{fakedQuery->rootEngine(), &inputNode,
                                           std::move(inputData)};
  MockTypedNode sqNode{fakedQuery->plan(), ExecutionNodeId{42},
                       ExecutionNode::SUBQUERY_START};
  ExecutionBlockImpl<SubqueryStartExecutor> testee{
      fakedQuery->rootEngine(), &sqNode, MakeBaseInfos(2), MakeBaseInfos(2)};
  testee.addDependency(&dependency);
  // MainQuery (HardLimit 10)
  AqlCallStack callStack{
      AqlCallList{AqlCall{0, false, 10, AqlCall::LimitType::HARD}}};
  // outer subquery (Hardlimit 1)
  callStack.pushCall(
      AqlCallList{AqlCall{0, false, 1, AqlCall::LimitType::HARD},
                  AqlCall{0, false, 1, AqlCall::LimitType::HARD}});
  // InnerSubquery (Produce all)
  callStack.pushCall(AqlCallList{AqlCall{0}, AqlCall{0}});

  auto [state, skipped, block] = testee.execute(callStack);
  // We will always get 9 times 3 rows
  ASSERT_EQ(block->numRows(), 3 * 9);
  // Two of the 3 rows are Shadows
  ASSERT_EQ(block->numShadowRows(), 2 * 9);

  for (size_t i = 0; i < 9; ++i) {
    // First is relevant
    EXPECT_FALSE(block->isShadowRow(i * 3 + 0));
    // Second is Depth 0
    ASSERT_TRUE(block->isShadowRow(i * 3 + 1));
    ShadowAqlItemRow second(block, i * 3 + 1);
    EXPECT_EQ(second.getDepth(), 0);
    // Third is Depth 1
    ASSERT_TRUE(block->isShadowRow(i * 3 + 2));
    ShadowAqlItemRow third(block, i * 3 + 2);
    EXPECT_EQ(third.getDepth(), 1);
  }
  EXPECT_EQ(state, ExecutionState::DONE);
}

TEST_F(SubqueryStartSpecficTest, count_shadow_rows_test) {
  // NOTE: This is a regression test for BTS-673
  std::deque<arangodb::aql::SharedAqlItemBlockPtr> inputData{};

  // The issue under test is to return too few results to SubqueryStartExecutor
  // including higher level shadow rows,which forces the SubqueryStartExecutor
  // to correctly count the returned rows.
  inputData.push_back(buildBlock<2>(
      manager(),
      {{1, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {2, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {3, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {4, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {5, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}},
       {6, NoneEntry{}},
       {NoneEntry{}, NoneEntry{}}},
      {{1, 0}, {2, 1}, {4, 0}, {6, 0}, {7, 1}, {9, 0}, {11, 0}, {13, 0}}));
  // After this block we have returned 2 level 1 shadowrows, and 3 level 0
  // shadowrows.

  inputData.push_back(buildBlock<2>(manager(),
                                    {{NoneEntry{}, NoneEntry{}},
                                     {6, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {7, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}}},
                                    {{0, 1}, {2, 0}, {4, 0}, {5, 1}}));

  MockTypedNode inputNode{fakedQuery->plan(), ExecutionNodeId{1},
                          ExecutionNode::FILTER};
  FixedOutputExecutionBlockMock dependency{fakedQuery->rootEngine(), &inputNode,
                                           std::move(inputData)};
  MockTypedNode sqNode{fakedQuery->plan(), ExecutionNodeId{42},
                       ExecutionNode::SUBQUERY_START};
  ExecutionBlockImpl<SubqueryStartExecutor> testee{fakedQuery->rootEngine(),
                                                   &sqNode, MakeBaseInfos(2, 3),
                                                   MakeBaseInfos(2, 3)};
  testee.addDependency(&dependency);
  size_t mainQuerySoftLimit = 100;
  // MainQuery (SoftLimit 100)
  AqlCallStack callStack{AqlCallList{
      AqlCall{0, false, mainQuerySoftLimit, AqlCall::LimitType::SOFT}}};
  // outer subquery (SoftLimit 10)
  size_t subQuerySoftLimit = 10;
  callStack.pushCall(AqlCallList{
      AqlCall{0, false, subQuerySoftLimit, AqlCall::LimitType::SOFT},
      AqlCall{0, false, subQuerySoftLimit, AqlCall::LimitType::SOFT}});
  // InnerSubquery (Produce all)
  callStack.pushCall(AqlCallList{AqlCall{0}, AqlCall{0}});
  callStack.pushCall(AqlCallList{AqlCall{0}, AqlCall{0}});
  size_t numCalls = 0;

  dependency.setExecuteEnterHook(
      [&numCalls, mainQuerySoftLimit,
       subQuerySoftLimit](AqlCallStack const& stack) {
        auto mainQCall = stack.getCallAtDepth(2);
        auto subQCall = stack.getCallAtDepth(1);
        ASSERT_FALSE(mainQCall.needSkipMore());
        ASSERT_FALSE(subQCall.needSkipMore());
        if (numCalls == 0) {
          // Call with the original limits, SubqueryStart does not reduce it.
          ASSERT_EQ(mainQCall.getLimit(), mainQuerySoftLimit);
          ASSERT_EQ(subQCall.getLimit(), subQuerySoftLimit);
        } else if (numCalls == 1) {
          // We have returned some rows of each in the block before. They need
          // to be accounted
          ASSERT_EQ(mainQCall.getLimit(), mainQuerySoftLimit - 2);
          ASSERT_EQ(subQCall.getLimit(), subQuerySoftLimit - 3);
        } else {
          // Should not be called thrice.
          ASSERT_TRUE(false);
        }
        numCalls++;
      });

  auto [state, skipped, block] = testee.execute(callStack);

  ASSERT_EQ(numCalls, 2);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(block->numRows(), 28);
}

TEST_F(SubqueryStartSpecficTest, handle_non_continue_call_on_outer_subqueries) {
  // NOTE: This is a regression test for BTS-673
  std::deque<arangodb::aql::SharedAqlItemBlockPtr> inputData{};

  // The issue under test here is that the SubqueryStart needs to return
  // if it does not have a continue call for a completed outer subquery
  inputData.push_back(buildBlock<2>(manager(),
                                    {{1, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {2, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {3, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}}},
                                    {{1, 0}, {3, 0}, {5, 0}}));
  // Split to enforce two internal calls to upstream
  inputData.push_back(buildBlock<2>(manager(),
                                    {{4, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {5, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}},
                                     {NoneEntry{}, NoneEntry{}}},
                                    {{1, 0}, {3, 0}, {4, 1}}));
  // Split again.
  // This block should NOT be fetched on first go, otherwise the Executor logic
  // failed However we need it to return "HASMORE"
  inputData.push_back(
      buildBlock<2>(manager(), {{"\"INVALID\"", "\"INVALID\""}}, {}));

  MockTypedNode inputNode{fakedQuery->plan(), ExecutionNodeId{1},
                          ExecutionNode::FILTER};
  FixedOutputExecutionBlockMock dependency{fakedQuery->rootEngine(), &inputNode,
                                           std::move(inputData)};
  MockTypedNode sqNode{fakedQuery->plan(), ExecutionNodeId{42},
                       ExecutionNode::SUBQUERY_START};
  ExecutionBlockImpl<SubqueryStartExecutor> testee{fakedQuery->rootEngine(),
                                                   &sqNode, MakeBaseInfos(2, 3),
                                                   MakeBaseInfos(2, 3)};
  testee.addDependency(&dependency);
  size_t mainQuerySoftLimit = 100;
  // MainQuery (SoftLimit 100)
  AqlCallStack callStack{AqlCallList{
      AqlCall{0, false, mainQuerySoftLimit, AqlCall::LimitType::SOFT}}};
  // outer subquery (SoftLimit 10)
  size_t subQuerySoftLimit = 10;
  // Only add one call, no continue call, the SubqueryEnd needs to return as
  // soon as the first higher (main query) shadowrow is seen.
  callStack.pushCall(AqlCallList{
      AqlCall{0, false, subQuerySoftLimit, AqlCall::LimitType::SOFT}});
  // InnerSubquery (Produce all)
  callStack.pushCall(AqlCallList{AqlCall{0}, AqlCall{0}});
  callStack.pushCall(AqlCallList{AqlCall{0}, AqlCall{0}});
  size_t numCalls = 0;

  dependency.setExecuteEnterHook(
      [&numCalls, mainQuerySoftLimit,
       subQuerySoftLimit](AqlCallStack const& stack) {
        auto mainQCall = stack.getCallAtDepth(2);
        auto subQCall = stack.getCallAtDepth(1);
        ASSERT_FALSE(mainQCall.needSkipMore());
        ASSERT_FALSE(subQCall.needSkipMore());
        if (numCalls == 0) {
          // Call with the original limits, SubqueryStart does not reduce it.
          ASSERT_EQ(mainQCall.getLimit(), mainQuerySoftLimit);
          ASSERT_EQ(subQCall.getLimit(), subQuerySoftLimit);
        } else if (numCalls == 1) {
          // We have not returned a mainQuery ShadowRow
          ASSERT_EQ(mainQCall.getLimit(), mainQuerySoftLimit);
          // We have not returned 3 subQuery ShadowRows on the first go
          ASSERT_EQ(subQCall.getLimit(), subQuerySoftLimit - 3);
        } else {
          // Should not be called thrice.
          // The call before had to figure out that we cannot continue after the
          // first Subquery is completed
          ASSERT_TRUE(false);
        }
        numCalls++;
      });

  auto [state, skipped, block] = testee.execute(callStack);

  ASSERT_EQ(numCalls, 2);
  EXPECT_EQ(state, ExecutionState::HASMORE);
  EXPECT_EQ(block->numRows(), 16);
}
