////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"

#if GTEST_HAS_TYPED_TEST_P

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

namespace {
std::string const stateToString(ExecutorState state) {
  switch (state) {
    case ExecutorState::DONE:
      return "DONE";
    case ExecutorState::HASMORE:
      return "HASMORE";
    default:
      // just to suppress a warning ..
      return "UNKNOWN";
  }
}
}  // namespace

template <typename Range>
class InputRangeTest : public AqlExecutorTestCase<> {
 protected:
  auto buildRange(ExecutorState state, SharedAqlItemBlockPtr block) -> Range {
    if constexpr (std::is_same_v<Range, AqlItemBlockInputRange>) {
      return AqlItemBlockInputRange{state, 0, block, 0};
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto consumeData(Range& range) -> void {
    if constexpr (std::is_same_v<Range, AqlItemBlockInputRange>) {
      while (range.hasDataRow()) {
        std::ignore = range.nextDataRow();
      }
    } else {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
  }
};

TYPED_TEST_CASE_P(InputRangeTest);

TYPED_TEST_P(InputRangeTest, test_default_initializer) {
  std::vector<ExecutorState> states{ExecutorState::DONE, ExecutorState::HASMORE};
  for (auto const& finalState : states) {
    SCOPED_TRACE("Testing state: " + stateToString(finalState));
    TypeParam testee{finalState};
    EXPECT_EQ(testee.upstreamState(), finalState);
    EXPECT_FALSE(testee.hasDataRow());
    EXPECT_FALSE(testee.hasShadowRow());

    // Required for expected Number Of Rows
    EXPECT_EQ(testee.finalState(), finalState);
    EXPECT_EQ(testee.countDataRows(), 0);
    EXPECT_EQ(testee.countShadowRows(), 0);

    {
      auto [state, shadow] = testee.peekShadowRowAndState();
      EXPECT_EQ(state, finalState);
      EXPECT_FALSE(shadow.isInitialized());
    }
  }
}

TYPED_TEST_P(InputRangeTest, test_block_only_datarows) {
  std::vector<ExecutorState> states{ExecutorState::DONE, ExecutorState::HASMORE};
  for (auto const& finalState : states) {
    SCOPED_TRACE("Testing state: " + stateToString(finalState));
    auto block = buildBlock<1>(this->manager(), {{1}, {2}, {3}});
    auto testee = this->buildRange(finalState, block);
    EXPECT_EQ(testee.upstreamState(), ExecutorState::HASMORE);
    EXPECT_TRUE(testee.hasDataRow());
    EXPECT_FALSE(testee.hasShadowRow());

    // Required for expected Number Of Rows
    EXPECT_EQ(testee.finalState(), finalState);
    EXPECT_EQ(testee.countDataRows(), 3);
    EXPECT_EQ(testee.countShadowRows(), 0);

    {
      auto [state, shadow] = testee.peekShadowRowAndState();
      EXPECT_EQ(state, ExecutorState::HASMORE);
      EXPECT_FALSE(shadow.isInitialized());
    }
  }
}

TYPED_TEST_P(InputRangeTest, test_block_only_shadowrows) {
  std::vector<ExecutorState> states{ExecutorState::DONE, ExecutorState::HASMORE};
  for (auto const& finalState : states) {
    SCOPED_TRACE("Testing state: " + stateToString(finalState));
    auto block =
        buildBlock<1>(this->manager(), {{1}, {2}, {3}}, {{0, 0}, {1, 1}, {2, 0}});
    auto testee = this->buildRange(finalState, block);
    EXPECT_EQ(testee.upstreamState(), ExecutorState::DONE);
    EXPECT_FALSE(testee.hasDataRow());
    EXPECT_TRUE(testee.hasShadowRow());

    // Required for expected Number Of Rows
    EXPECT_EQ(testee.finalState(), finalState);
    EXPECT_EQ(testee.countDataRows(), 0);
    EXPECT_EQ(testee.countShadowRows(), 3);

    {
      auto [state, shadow] = testee.peekShadowRowAndState();
      EXPECT_EQ(state, ExecutorState::HASMORE);
      EXPECT_TRUE(shadow.isInitialized());
    }
  }
}

TYPED_TEST_P(InputRangeTest, test_block_mixed_rows) {
  std::vector<ExecutorState> states{ExecutorState::DONE, ExecutorState::HASMORE};
  for (auto const& finalState : states) {
    SCOPED_TRACE("Testing state: " + stateToString(finalState));
    auto block = buildBlock<1>(this->manager(), {{1}, {2}, {3}, {4}}, {{1, 0}, {3, 0}});
    auto testee = this->buildRange(finalState, block);
    EXPECT_EQ(testee.upstreamState(), ExecutorState::HASMORE);
    EXPECT_TRUE(testee.hasDataRow());
    EXPECT_FALSE(testee.hasShadowRow());

    // Required for expected Number Of Rows
    EXPECT_EQ(testee.finalState(), finalState);
    EXPECT_EQ(testee.countDataRows(), 2);
    EXPECT_EQ(testee.countShadowRows(), 2);

    {
      auto [state, shadow] = testee.peekShadowRowAndState();
      EXPECT_EQ(state, ExecutorState::HASMORE);
      EXPECT_FALSE(shadow.isInitialized());
    }
  }
}

TYPED_TEST_P(InputRangeTest, test_block_continuous_walk_only_relevant_rows) {
  std::vector<ExecutorState> states{ExecutorState::DONE, ExecutorState::HASMORE};
  for (auto const& finalState : states) {
    SCOPED_TRACE("Testing state: " + stateToString(finalState));
    auto block =
        buildBlock<1>(this->manager(),
                      {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}},
                      {{3, 0}, {6, 0}, {11, 0}});
    auto testee = this->buildRange(finalState, block);
    {
      // First subquery
      // Required for expected Number Of Rows
      EXPECT_EQ(testee.finalState(), finalState);
      EXPECT_EQ(testee.countDataRows(), 9);
      EXPECT_EQ(testee.countShadowRows(), 3);
      EXPECT_TRUE(testee.hasDataRow());
      EXPECT_FALSE(testee.hasShadowRow());

      // Consume all DataRows.
      this->consumeData(testee);
      EXPECT_FALSE(testee.hasDataRow());
      EXPECT_TRUE(testee.hasShadowRow());

      EXPECT_EQ(testee.countDataRows(), 6);
      EXPECT_EQ(testee.countShadowRows(), 3);

      auto [state, shadow] = testee.nextShadowRow();
      EXPECT_EQ(state, ExecutorState::HASMORE);
      EXPECT_TRUE(shadow.isInitialized());

      EXPECT_EQ(testee.countDataRows(), 6);
      EXPECT_EQ(testee.countShadowRows(), 2);
    }

    {
      // Second subquery
      EXPECT_TRUE(testee.hasDataRow());
      EXPECT_FALSE(testee.hasShadowRow());

      // Consume all DataRows.
      this->consumeData(testee);
      EXPECT_FALSE(testee.hasDataRow());
      EXPECT_TRUE(testee.hasShadowRow());

      EXPECT_EQ(testee.countDataRows(), 4);
      EXPECT_EQ(testee.countShadowRows(), 2);

      auto [state, shadow] = testee.nextShadowRow();
      EXPECT_EQ(state, ExecutorState::HASMORE);
      EXPECT_TRUE(shadow.isInitialized());

      EXPECT_EQ(testee.countDataRows(), 4);
      EXPECT_EQ(testee.countShadowRows(), 1);
    }

    {
      // Third subquery
      EXPECT_TRUE(testee.hasDataRow());
      EXPECT_FALSE(testee.hasShadowRow());

      // Consume all DataRows.
      this->consumeData(testee);
      EXPECT_FALSE(testee.hasDataRow());
      EXPECT_TRUE(testee.hasShadowRow());

      EXPECT_EQ(testee.countDataRows(), 0);
      EXPECT_EQ(testee.countShadowRows(), 1);

      auto [state, shadow] = testee.nextShadowRow();
      EXPECT_EQ(state, finalState);
      EXPECT_TRUE(shadow.isInitialized());

      EXPECT_EQ(testee.countDataRows(), 0);
      EXPECT_EQ(testee.countShadowRows(), 0);
    }
  }
}

REGISTER_TYPED_TEST_CASE_P(InputRangeTest, test_default_initializer, test_block_only_datarows,
                           test_block_only_shadowrows, test_block_mixed_rows,
                           test_block_continuous_walk_only_relevant_rows);

using RangeTypes = ::testing::Types<AqlItemBlockInputRange>;
INSTANTIATE_TYPED_TEST_CASE_P(InputRangeTestInstance, InputRangeTest, RangeTypes);

}  // namespace arangodb::tests::aql

#endif