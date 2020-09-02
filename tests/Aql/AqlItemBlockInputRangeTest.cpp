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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AqlItemBlockInputRange.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ShadowAqlItemRow.h"
#include "AqlItemBlockHelper.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace arangodb {
namespace tests {
namespace aql {

// Test empty constructor
class InputRangeTest : public ::testing::TestWithParam<ExecutorState> {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  AqlItemBlockInputRange createEmpty() {
    return AqlItemBlockInputRange{GetParam()};
  }

  AqlItemBlockInputRange createFromBlock(arangodb::aql::SharedAqlItemBlockPtr& block) {
    return AqlItemBlockInputRange(GetParam(), 0, block, 0);
  }

  void validateEndReached(AqlItemBlockInputRange& testee) {
    EXPECT_EQ(GetParam(), testee.upstreamState());
    // Test Data rows
    EXPECT_FALSE(testee.hasDataRow());
    {
      auto const [state, row] = testee.peekDataRow();
      EXPECT_EQ(GetParam(), state);
      EXPECT_FALSE(row.isInitialized());
    }
    {
      auto const [state, row] = testee.nextDataRow();
      EXPECT_EQ(GetParam(), state);
      EXPECT_FALSE(row.isInitialized());
    }
    // Test Shadow Rows
    EXPECT_FALSE(testee.hasShadowRow());
    {
      auto const& row = testee.peekShadowRow();
      EXPECT_FALSE(row.isInitialized());
    }
    {
      auto const [state, row] = testee.nextShadowRow();
      EXPECT_EQ(GetParam(), state);
      EXPECT_FALSE(row.isInitialized());
    }
  }

  void validateNextIsDataRow(AqlItemBlockInputRange& testee,
                             ExecutorState expectedState, int64_t value) {
    EXPECT_TRUE(testee.hasDataRow());
    EXPECT_FALSE(testee.hasShadowRow());
    // We have the next row
    EXPECT_EQ(testee.upstreamState(), ExecutorState::HASMORE);
    auto rowIndexBefore = testee.getRowIndex();
    // Validate that shadowRowAPI does not move on
    {
      auto const& row = testee.peekShadowRow();
      EXPECT_FALSE(row.isInitialized());
      ASSERT_EQ(rowIndexBefore, testee.getRowIndex())
          << "Skipped a non processed row.";
    }
    {
      auto [state, row] = testee.nextShadowRow();
      // EXPECT_EQ(state, ExecutorState::HASMORE);
      EXPECT_FALSE(row.isInitialized());
      ASSERT_EQ(rowIndexBefore, testee.getRowIndex())
          << "Skipped a non processed row.";
    }
    // Validate Data Row API
    {
      auto [state, row] = testee.peekDataRow();
      EXPECT_EQ(state, expectedState);
      EXPECT_TRUE(row.isInitialized());
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), value);
      ASSERT_EQ(rowIndexBefore, testee.getRowIndex())
          << "Skipped a non processed row.";
    }

    {
      auto [state, row] = testee.nextDataRow();
      EXPECT_EQ(state, expectedState);
      EXPECT_TRUE(row.isInitialized());
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), value);
      ASSERT_NE(rowIndexBefore, testee.getRowIndex())
          << "Did not go to next row.";
    }
    EXPECT_EQ(expectedState, testee.upstreamState());
  }

  void validateNextIsShadowRow(AqlItemBlockInputRange& testee, ExecutorState expectedState,
                               int64_t value, uint64_t depth) {
    EXPECT_TRUE(testee.hasShadowRow());
    // The next is a ShadowRow, the state shall be done
    EXPECT_EQ(testee.upstreamState(), ExecutorState::DONE);

    auto rowIndexBefore = testee.getRowIndex();
    // Validate that inputRowAPI does not move on
    {
      auto [state, row] = testee.peekDataRow();
      EXPECT_EQ(state, ExecutorState::DONE);
      EXPECT_FALSE(row.isInitialized());
      ASSERT_EQ(rowIndexBefore, testee.getRowIndex())
          << "Skipped a non processed row.";
    }
    {
      auto [state, row] = testee.nextDataRow();
      EXPECT_EQ(state, ExecutorState::DONE);
      EXPECT_FALSE(row.isInitialized());
      ASSERT_EQ(rowIndexBefore, testee.getRowIndex())
          << "Skipped a non processed row.";
    }
    // Validate ShadowRow API
    {
      auto const& row = testee.peekShadowRow();
      EXPECT_TRUE(row.isInitialized());
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), value);
      EXPECT_EQ(row.getDepth(), depth);
      ASSERT_EQ(rowIndexBefore, testee.getRowIndex())
          << "Skipped a non processed row.";
    }
    {
      auto [state, row] = testee.nextShadowRow();
      EXPECT_EQ(state, expectedState);
      EXPECT_TRUE(row.isInitialized());
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), value);
      EXPECT_EQ(row.getDepth(), depth);
      ASSERT_NE(rowIndexBefore, testee.getRowIndex())
          << "Did not go to next row.";
    }
  }
};

TEST_P(InputRangeTest, empty_returns_given_state) {
  auto testee = createEmpty();
  EXPECT_EQ(GetParam(), testee.upstreamState());
}

TEST_P(InputRangeTest, empty_does_not_have_more) {
  auto testee = createEmpty();
  EXPECT_FALSE(testee.hasDataRow());
}

TEST_P(InputRangeTest, empty_peek_is_empty) {
  auto testee = createEmpty();
  auto const [state, row] = testee.peekDataRow();
  EXPECT_EQ(GetParam(), state);
  EXPECT_FALSE(row.isInitialized());
}

TEST_P(InputRangeTest, empty_next_is_empty) {
  auto testee = createEmpty();
  auto const [state, row] = testee.nextDataRow();
  EXPECT_EQ(GetParam(), state);
  EXPECT_FALSE(row.isInitialized());
}

TEST_P(InputRangeTest, empty_does_not_have_more_shadow_rows) {
  auto testee = createEmpty();
  EXPECT_FALSE(testee.hasShadowRow());
}

TEST_P(InputRangeTest, empty_peek_shadow_is_empty) {
  auto testee = createEmpty();
  auto const& row = testee.peekShadowRow();
  EXPECT_FALSE(row.isInitialized());
}

TEST_P(InputRangeTest, empty_next_shadow_is_empty) {
  auto testee = createEmpty();
  auto const [state, row] = testee.nextShadowRow();
  EXPECT_EQ(GetParam(), state);
  EXPECT_FALSE(row.isInitialized());
}

TEST_P(InputRangeTest, no_shadow_rows_in_block) {
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}, {{4}}}, {});
  auto testee = createFromBlock(inputBlock);

  // We have 3 internal rows
  validateNextIsDataRow(testee, ExecutorState::HASMORE, 1);
  validateNextIsDataRow(testee, ExecutorState::HASMORE, 2);
  validateNextIsDataRow(testee, ExecutorState::HASMORE, 3);
  // Last Row needs to return upstream State
  validateNextIsDataRow(testee, GetParam(), 4);
  validateEndReached(testee);
}

TEST_P(InputRangeTest, level_0_shadow_rows_in_block) {
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}, {{4}}}, {{1, 0}, {3, 0}});
  auto testee = createFromBlock(inputBlock);

  validateNextIsDataRow(testee, ExecutorState::DONE, 1);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 2, 0);
  validateNextIsDataRow(testee, ExecutorState::DONE, 3);
  // Last Row needs to return upstream State
  validateNextIsShadowRow(testee, GetParam(), 4, 0);
  validateEndReached(testee);
}

TEST_P(InputRangeTest, multi_level_shadow_rows_in_block) {
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}}, {{7}}},
                    {{3, 0}, {4, 1}, {5, 2}});
  auto testee = createFromBlock(inputBlock);

  validateNextIsDataRow(testee, ExecutorState::HASMORE, 1);
  validateNextIsDataRow(testee, ExecutorState::HASMORE, 2);
  validateNextIsDataRow(testee, ExecutorState::DONE, 3);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 4, 0);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 5, 1);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 6, 2);

  // Last Row needs to return upstream State
  validateNextIsDataRow(testee, GetParam(), 7);
  validateEndReached(testee);
}

TEST_P(InputRangeTest, multi_shadow_rows_batches_in_block) {
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}}, {{7}}},
                    {{3, 0}, {4, 1}, {5, 0}, {6, 1}});
  auto testee = createFromBlock(inputBlock);

  validateNextIsDataRow(testee, ExecutorState::HASMORE, 1);
  validateNextIsDataRow(testee, ExecutorState::HASMORE, 2);
  validateNextIsDataRow(testee, ExecutorState::DONE, 3);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 4, 0);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 5, 1);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 6, 0);

  // Last Row needs to return upstream State
  validateNextIsShadowRow(testee, GetParam(), 7, 1);
  validateEndReached(testee);
}

TEST_P(InputRangeTest, multi_shadow_rows_batches_with_skip) {
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}}, {{7}}},
                    {{3, 0}, {4, 1}, {5, 0}, {6, 1}});
  auto testee = createFromBlock(inputBlock);

  validateNextIsDataRow(testee, ExecutorState::HASMORE, 1);
  validateNextIsDataRow(testee, ExecutorState::HASMORE, 2);
  validateNextIsDataRow(testee, ExecutorState::DONE, 3);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 4, 0);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 5, 1);
  validateNextIsShadowRow(testee, ExecutorState::HASMORE, 6, 0);

  // Last Row needs to return upstream State
  validateNextIsShadowRow(testee, GetParam(), 7, 1);
  validateEndReached(testee);
}

INSTANTIATE_TEST_CASE_P(AqlItemBlockInputRangeTest, InputRangeTest,
                        ::testing::Values(ExecutorState::DONE, ExecutorState::HASMORE));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
