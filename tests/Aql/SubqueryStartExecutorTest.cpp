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

#include "AqlExecutorTestCase.h"
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

// We need to be backwards compatible, with version 3.6
// There we do not get a fullStack, but only a single entry.
// We have a compatibility mode Stack for this version
// These tests can be removed again in the branch for the version
// after 3.7.*
enum CompatibilityMode { VERSION36, VERSION37 };
class SubqueryStartExecutorTest
    : public AqlExecutorTestCaseWithParam<CompatibilityMode, false> {
 protected:
  auto GetCompatMode() const -> CompatibilityMode {
    auto const mode = GetParam();
    return mode;
  }

  auto queryStack(AqlCall fromSubqueryEnd, AqlCall insideSubquery) const -> AqlCallStack {
    if (GetCompatMode() == CompatibilityMode::VERSION36) {
      return AqlCallStack{insideSubquery, true};
    }
    AqlCallStack stack(fromSubqueryEnd);
    stack.pushCall(std::move(insideSubquery));
    return stack;
  }
};

INSTANTIATE_TEST_CASE_P(SubqueryStartExecutorTest, SubqueryStartExecutorTest,
                        ::testing::Values(CompatibilityMode::VERSION36,
                                          CompatibilityMode::VERSION37));

TEST_P(SubqueryStartExecutorTest, check_properties) {
  EXPECT_TRUE(SubqueryStartExecutor::Properties::preservesOrder)
      << "The block has no effect on ordering of elements, it adds additional "
         "rows only.";
  EXPECT_EQ(SubqueryStartExecutor::Properties::allowsBlockPassthrough, ::arangodb::aql::BlockPassthrough::Disable)
      << "The block cannot be passThrough, as it increases the number of rows.";
  EXPECT_TRUE(SubqueryStartExecutor::Properties::inputSizeRestrictsOutputSize)
      << "The block is restricted by input, it will atMost produce 2 times the "
         "input. (Might be less if input contains shadowRows";
}

TEST_P(SubqueryStartExecutorTest, empty_input_does_not_add_shadow_rows) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {})
      .expectSkipped(0, 0)
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run();
}

TEST_P(SubqueryStartExecutorTest, adds_a_shadowrow_after_single_input) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectSkipped(0, 0)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run();
}

// NOTE: The following two tests exclude each other.
// Right now we can only support 1 ShadowRow per request, we cannot do a look-ahead of
// calls. As soon as we can this test needs to re removed, and the one blow needs to be activated.
TEST_P(SubqueryStartExecutorTest,
       adds_only_one_shadowrow_even_if_more_input_is_available_in_single_pass) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{{R"("a")"}}, {{R"("b")"}}, {{R"("c")"}}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::HASMORE)
      .expectSkipped(0, 0)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run();
}

// NOTE: This test and the one right above do exclude each other.
// This is the behaviour we would like to have eventually
// As soon as we can support Call look-aheads we need to enable this test.
// and it needs to pass then
TEST_P(SubqueryStartExecutorTest, DISABLED_adds_a_shadowrow_after_every_input_line_in_single_pass) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{{R"("a")"}}, {{R"("b")"}}, {{R"("c")"}}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectSkipped(0, 0)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}, {R"("b")"}, {R"("b")"}, {R"("c")"}, {R"("c")"}},
                    {{1, 0}, {3, 0}, {5, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run();
}

// NOTE: As soon as the single_pass test is enabled this test is superflous.
// It will be identical to the one above
TEST_P(SubqueryStartExecutorTest, adds_a_shadowrow_after_every_input_line) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{{R"("a")"}}, {{R"("b")"}}, {{R"("c")"}}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectSkipped(0, 0)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}, {R"("b")"}, {R"("b")"}, {R"("c")"}, {R"("c")"}},
                    {{1, 0}, {3, 0}, {5, 0}})
      .setCallStack(queryStack(AqlCall{}, AqlCall{}))
      .run(true);
}

TEST_P(SubqueryStartExecutorTest, shadow_row_does_not_fit_in_current_block) {
  // NOTE: This test relies on batchSizes beeing handled correctly and we do not over-allocate memory
  // Also it tests, that ShadowRows go into place accounting of the output block (count as 1 line)

  // NOTE: Reduce batch size to 1, to enforce a too small output block
  ExecutionBlock::setDefaultBatchSize(1);
  TRI_DEFER(ExecutionBlock::setDefaultBatchSize(ExecutionBlock::ProductionDefaultBatchSize););
  {
    // First test: Validate that the shadowRow is not written
    // We only do a single call here
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectSkipped(0, 0)
        .expectOutput({0}, {{R"("a")"}}, {})
        .setCallStack(queryStack(AqlCall{}, AqlCall{}))
        .run();
  }
  {
    // Second test: Validate that the shadowRow is eventually written
    // if we call often enough
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::DONE)
        .expectSkipped(0, 0)
        .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
        .setCallStack(queryStack(AqlCall{}, AqlCall{}))
        .run(true);
  }
}

TEST_P(SubqueryStartExecutorTest, skip_in_subquery) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("a")"}}, {{0, 0}})
      .expectSkipped(0, 1)
      .setCallStack(queryStack(AqlCall{}, AqlCall{10, false}))
      .run();
}

TEST_P(SubqueryStartExecutorTest, fullCount_in_subquery) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .setInputValue({{R"("a")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("a")"}}, {{0, 0}})
      .expectSkipped(0, 1)
      .setCallStack(queryStack(AqlCall{}, AqlCall{0, true, 0, AqlCall::LimitType::HARD}))
      .run();
}

TEST_P(SubqueryStartExecutorTest, shadow_row_forwarding) {
  auto helper = makeExecutorTestHelper<1, 1>();

  AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
  stack.pushCall(AqlCall{});

  helper
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START);

  if (GetCompatMode() == CompatibilityMode::VERSION36) {
    // We will not get this infromation because the
    // query stack is too small on purpose
    helper.expectSkipped(0, 0);
  } else {
    helper.expectSkipped(0, 0, 0);
  }

  helper.setInputValue({{R"("a")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}, {R"("a")"}}, {{1, 0}, {2, 1}})
      .setCallStack(stack)
      .run();
}

TEST_P(SubqueryStartExecutorTest, shadow_row_forwarding_many_inputs_single_call) {
  auto helper = makeExecutorTestHelper<1, 1>();
  AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
  stack.pushCall(AqlCall{});

  helper
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START);

  if (GetCompatMode() == CompatibilityMode::VERSION36) {
    // We will not get this infromation because the
    // query stack is too small on purpose
    helper.expectSkipped(0, 0);
  } else {
    helper.expectSkipped(0, 0, 0);
  }

  helper.setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::HASMORE)
      .expectOutput({0}, {{R"("a")"}, {R"("a")"}, {R"("a")"}}, {{1, 0}, {2, 1}})
      .setCallStack(stack)
      .run();
}

TEST_P(SubqueryStartExecutorTest, shadow_row_forwarding_many_inputs_many_requests) {
  auto helper = makeExecutorTestHelper<1, 1>();
  AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
  stack.pushCall(AqlCall{});

  helper
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
      .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START);
  if (GetCompatMode() == CompatibilityMode::VERSION36) {
    // We will not get this infromation because the
    // query stack is too small on purpose
    helper.expectSkipped(0, 0);
  } else {
    helper.expectSkipped(0, 0, 0);
  }
  helper.setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput(
          {0},
          {{R"("a")"}, {R"("a")"}, {R"("a")"}, {R"("b")"}, {R"("b")"}, {R"("b")"}, {R"("c")"}, {R"("c")"}, {R"("c")"}},
          {{1, 0}, {2, 1}, {4, 0}, {5, 1}, {7, 0}, {8, 1}})
      .setCallStack(stack)
      .run(true);
}

TEST_P(SubqueryStartExecutorTest, shadow_row_forwarding_many_inputs_not_enough_space) {
  // NOTE: This test relies on batchSizes beeing handled correctly and we do not over-allocate memory
  // Also it tests, that ShadowRows go into place accounting of the output block (count as 1 line)

  // NOTE: Reduce batch size to 2, to enforce a too small output block, in between the shadow Rows
  ExecutionBlock::setDefaultBatchSize(2);
  TRI_DEFER(ExecutionBlock::setDefaultBatchSize(ExecutionBlock::ProductionDefaultBatchSize););
  {
    // First test: Validate that the shadowRow is not written
    // We only do a single call here
    auto helper = makeExecutorTestHelper<1, 1>();

    AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
    stack.pushCall(AqlCall{});

    helper
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START);

    if (GetCompatMode() == CompatibilityMode::VERSION36) {
      // We will not get this infromation because the
      // query stack is too small on purpose
      helper.expectSkipped(0, 0);
    } else {
      helper.expectSkipped(0, 0, 0);
    }
    helper.setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectOutput({0}, {{R"("a")"}, {R"("a")"}}, {{1, 0}})
        .setCallStack(stack)
        .run();
  }
  {
    // Second test: Validate that the shadowRow is eventually written
    // Wedo call as many times as we need to.
    auto helper = makeExecutorTestHelper<1, 1>();

    AqlCallStack stack = queryStack(AqlCall{}, AqlCall{});
    stack.pushCall(AqlCall{});

    helper
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START);

    if (GetCompatMode() == CompatibilityMode::VERSION36) {
      // We will not get this infromation because the
      // query stack is too small on purpose
      helper.expectSkipped(0, 0);
    } else {
      helper.expectSkipped(0, 0, 0);
    }

    helper.setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::DONE)
        .expectOutput(
            {0},
            {{R"("a")"}, {R"("a")"}, {R"("a")"}, {R"("b")"}, {R"("b")"}, {R"("b")"}, {R"("c")"}, {R"("c")"}, {R"("c")"}},
            {{1, 0}, {2, 1}, {4, 0}, {5, 1}, {7, 0}, {8, 1}})
        .setCallStack(stack)
        .run(true);
  }
}

TEST_P(SubqueryStartExecutorTest, skip_in_outer_subquery) {
  if (GetCompatMode() == CompatibilityMode::VERSION37) {
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}, {R"("b")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::DONE)
        .expectOutput({0}, {{R"("b")"}, {R"("b")"}}, {{1, 0}})
        .expectSkipped(1, 0)
        .setCallStack(queryStack(AqlCall{1, false, AqlCall::Infinity{}}, AqlCall{}))
        .run();
  } else {
    // The feature is not available in 3.6 or earlier.
  }
}

TEST_P(SubqueryStartExecutorTest, DISABLED_skip_only_in_outer_subquery) {
  if (GetCompatMode() == CompatibilityMode::VERSION37) {
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}, {R"("b")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::DONE)
        .expectOutput({0}, {})
        .expectSkipped(1, 0)
        .setCallStack(queryStack(AqlCall{1, false}, AqlCall{}))
        .run();
  } else {
    // The feature is not available in 3.7 or earlier.
  }
}

TEST_P(SubqueryStartExecutorTest, fullCount_in_outer_subquery) {
  if (GetCompatMode() == CompatibilityMode::VERSION37) {
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}, {R"("d")"}, {R"("e")"}, {R"("f")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::DONE)
        .expectOutput({0}, {})
        .expectSkipped(6, 0)
        .setCallStack(queryStack(AqlCall{0, true, 0, AqlCall::LimitType::HARD}, AqlCall{}))
        .run();
  } else {
    // The feature is not available in 3.7 or earlier.
  }
}

TEST_P(SubqueryStartExecutorTest, fastForward_in_inner_subquery) {
  if (GetCompatMode() == CompatibilityMode::VERSION37) {
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}, {R"("d")"}, {R"("e")"}, {R"("f")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectOutput({0}, {{R"("a")"}}, {{0, 0}})
        .expectSkipped(0, 0)
        .setCallStack(queryStack(AqlCall{0, false, AqlCall::Infinity{}},
                                 AqlCall{0, false, 0, AqlCall::LimitType::HARD}))
        .run();
  } else {
    // The feature is not available in 3.7 or earlier.
  }
}

TEST_P(SubqueryStartExecutorTest, skip_out_skip_in) {
  if (GetCompatMode() == CompatibilityMode::VERSION37) {
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}, {R"("d")"}, {R"("e")"}, {R"("f")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectOutput({0}, {{R"("c")"}}, {{0, 0}})
        .expectSkipped(2, 1)
        .setCallStack(queryStack(AqlCall{2, false, AqlCall::Infinity{}},
                                 AqlCall{10, false, AqlCall::Infinity{}}))
        .run();
  } else {
    // The feature is not available in 3.7 or earlier.
  }
}

TEST_P(SubqueryStartExecutorTest, fullbypass_in_outer_subquery) {
  if (GetCompatMode() == CompatibilityMode::VERSION37) {
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1), ExecutionNode::SUBQUERY_START)
        .setInputValue({{R"("a")"}, {R"("b")"}, {R"("c")"}, {R"("d")"}, {R"("e")"}, {R"("f")"}})
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::DONE)
        .expectOutput({0}, {})
        .expectSkipped(0, 0)
        .setCallStack(queryStack(AqlCall{0, false, 0, AqlCall::LimitType::HARD}, AqlCall{}))
        .run();
  } else {
    // The feature is not available in 3.7 or earlier.
  }
}
