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

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"

#include "Aql/AqlItemBlockInputMatrix.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/MultiAqlItemBlockInputRange.h"

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
  // Used to holdData for InputMatrixTests
  AqlItemMatrix _matrix{1};
  // Picked a random number of dependencies for MultiInputRanges
  size_t _numberDependencies{3};

  auto buildRange(ExecutorState state, SharedAqlItemBlockPtr block) -> Range {
    if constexpr (std::is_same_v<Range, AqlItemBlockInputRange>) {
      return AqlItemBlockInputRange{state, 0, block, 0};
    }
    if constexpr (std::is_same_v<Range, AqlItemBlockInputMatrix>) {
      _matrix.clear();
      _matrix.addBlock(block);
      return AqlItemBlockInputMatrix(state, &_matrix);
    }
    if constexpr (std::is_same_v<Range, MultiAqlItemBlockInputRange>) {
      MultiAqlItemBlockInputRange res{state};
      res.resizeOnce(state, 0, _numberDependencies);
      size_t depNr = 0;
      std::unordered_map<size_t, std::vector<size_t>> chosenRows;
      chosenRows.reserve(_numberDependencies);
      for (size_t i = 0; i < _numberDependencies; ++i) {
        chosenRows.emplace(i, std::vector<size_t>{});
      }

      for (size_t i = 0; i < block->numRows(); ++i) {
        if (block->isShadowRow(i)) {
          // ShadowRows need to be added to all Clients
          for (auto& [key, value] : chosenRows) {
            value.emplace_back(i);
          }
        } else {
          // We alternate between dependendies to emplace rows
          chosenRows[depNr % _numberDependencies].emplace_back(i);
          ++depNr;
        }
      }
      for (auto const& [index, chosen] : chosenRows) {
        if (chosen.empty()) {
          res.setDependency(index, AqlItemBlockInputRange{state});
        } else {
          auto copiedBlock = block->slice(chosen, 0, chosen.size());
          if (index != 0) {
            // Simulate that shadowRows have been "moved"  by clearing their dataRegisters
            for (size_t i = 0; i < copiedBlock->numRows(); ++i) {
              if (copiedBlock->isShadowRow(i)) {
                for (RegisterId::value_t r = 0; r < copiedBlock->numRegisters(); ++r) {
                  copiedBlock->destroyValue(i, r);
                }

              }
            }
          }
          AqlItemBlockInputRange splitRange{state, 0, copiedBlock , 0};
          res.setDependency(index, splitRange);
        }
      }

      return res;
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto consumeData(Range& range) -> void {
    if constexpr (std::is_same_v<Range, AqlItemBlockInputRange>) {
      while (range.hasDataRow()) {
        std::ignore = range.nextDataRow();
      }
    } else if constexpr (std::is_same_v<Range, AqlItemBlockInputMatrix> ||
                         std::is_same_v<Range, MultiAqlItemBlockInputRange>) {
      std::ignore = range.skipAllRemainingDataRows();
    } else {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
  }
};

TYPED_TEST_CASE_P(InputRangeTest);

TYPED_TEST_P(InputRangeTest, test_default_initializer) {
  std::vector<ExecutorState> states{ExecutorState::DONE, ExecutorState::HASMORE};
  for (auto const& finalState : states) {
    if (std::is_same_v<AqlItemBlockInputMatrix, TypeParam> &&
        finalState == ExecutorState::DONE) {
      // The AqlItemBlockInputMatrix may not be instantiated with DONE
      continue;
    }
    SCOPED_TRACE("Testing state: " + stateToString(finalState));
    auto testee = std::invoke([&]() {
      if constexpr (std::is_same_v<TypeParam, AqlItemBlockInputMatrix>) {
        if (finalState == ExecutorState::HASMORE) {
          return TypeParam{finalState};
        } else {
          TRI_ASSERT(finalState == ExecutorState::DONE);
          // AqlItemBlockInputMatrix may not be instantiated with DONE and
          // without a matrix, thus this conditionals.
          return TypeParam{finalState, &this->_matrix};
        }
      } else {
        return TypeParam{finalState};
      }
    });
    // assert is just for documentation
    static_assert(std::is_same_v<decltype(testee), TypeParam>);
    if constexpr (std::is_same_v<decltype(testee), MultiAqlItemBlockInputRange>) {
      // Default has only 1 dependency
      EXPECT_EQ(testee.upstreamState(0), finalState);
    } else {
      EXPECT_EQ(testee.upstreamState(), finalState);
    }

    EXPECT_FALSE(testee.hasDataRow());
    EXPECT_FALSE(testee.hasShadowRow());

    // Required for expected Number Of Rows
    EXPECT_EQ(testee.finalState(), finalState);
    EXPECT_EQ(testee.countDataRows(), 0);
    EXPECT_EQ(testee.countShadowRows(), 0);

    {
      auto shadow = testee.peekShadowRow();
      EXPECT_FALSE(shadow.isInitialized());
    }
  }
}  // namespace arangodb::tests::aql

TYPED_TEST_P(InputRangeTest, test_block_only_datarows) {
  std::vector<ExecutorState> states{ExecutorState::DONE, ExecutorState::HASMORE};
  for (auto const& finalState : states) {
    SCOPED_TRACE("Testing state: " + stateToString(finalState));
    auto block = buildBlock<1>(this->manager(), {{1}, {2}, {3}});
    auto testee = this->buildRange(finalState, block);

    if constexpr (std::is_same_v<decltype(testee), AqlItemBlockInputMatrix>) {
      // Matrix is only done, if it has reached a shadowRow, or the end
      EXPECT_EQ(testee.upstreamState(), finalState);
    } else if constexpr (std::is_same_v<decltype(testee), MultiAqlItemBlockInputRange>) {
      EXPECT_GT(testee.numberDependencies(), 0);
      for (size_t i = 0; i < testee.numberDependencies(); ++i) {
        // We have enough rows for every depenendy to contain something
        EXPECT_EQ(testee.upstreamState(i), ExecutorState::HASMORE);
      }
    } else {
      EXPECT_EQ(testee.upstreamState(), ExecutorState::HASMORE);
    }

    if constexpr (std::is_same_v<decltype(testee), AqlItemBlockInputMatrix>) {
      // The AqlItemBlockInputMatrix may only report it has a data row when it
      // knows it has consumed all input (of the current subquery iteration, if
      // applicable).
      EXPECT_EQ(testee.hasDataRow(), finalState == ExecutorState::DONE);
    } else {
      EXPECT_TRUE(testee.hasDataRow());
    }
    EXPECT_FALSE(testee.hasShadowRow());

    // Required for expected Number Of Rows
    EXPECT_EQ(testee.finalState(), finalState);
    EXPECT_EQ(testee.countDataRows(), 3);
    EXPECT_EQ(testee.countShadowRows(), 0);

    {
      auto shadow = testee.peekShadowRow();
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
    if constexpr (std::is_same_v<decltype(testee), MultiAqlItemBlockInputRange>) {
      EXPECT_GT(testee.numberDependencies(), 0);
      for (size_t i = 0; i < testee.numberDependencies(); ++i) {
        // We have enough rows for every depenendy to contain something
        EXPECT_EQ(testee.upstreamState(i), ExecutorState::DONE);
      }
    } else {
      EXPECT_EQ(testee.upstreamState(), ExecutorState::DONE);
    }

    EXPECT_FALSE(testee.hasDataRow());
    EXPECT_TRUE(testee.hasShadowRow());

    // Required for expected Number Of Rows
    EXPECT_EQ(testee.finalState(), finalState);
    EXPECT_EQ(testee.countDataRows(), 0);
    EXPECT_EQ(testee.countShadowRows(), 3);

    {
      auto shadow = testee.peekShadowRow();
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
    if constexpr (std::is_same_v<decltype(testee), AqlItemBlockInputMatrix>) {
      // Matrix is only done, if it has reached a shadowRow, or the end
      EXPECT_EQ(testee.upstreamState(), ExecutorState::DONE);
    } else if constexpr (std::is_same_v<decltype(testee), MultiAqlItemBlockInputRange>) {
      EXPECT_GT(testee.numberDependencies(), 0);
      // We only have one Data Row. This is assigned to dependency 0
      EXPECT_EQ(testee.upstreamState(0), ExecutorState::HASMORE);
      for (size_t i = 1; i < testee.numberDependencies(); ++i) {
        // All others do not have data rows, and need to be done
        EXPECT_EQ(testee.upstreamState(i), ExecutorState::DONE);
      }
    } else {
      EXPECT_EQ(testee.upstreamState(), ExecutorState::HASMORE);
    }
    EXPECT_TRUE(testee.hasDataRow());
    EXPECT_FALSE(testee.hasShadowRow());

    // Required for expected Number Of Rows
    EXPECT_EQ(testee.finalState(), finalState);
    EXPECT_EQ(testee.countDataRows(), 2);
    EXPECT_EQ(testee.countShadowRows(), 2);

    {
      auto shadow = testee.peekShadowRow();
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

using RangeTypes =
    ::testing::Types<AqlItemBlockInputRange, AqlItemBlockInputMatrix, MultiAqlItemBlockInputRange>;
INSTANTIATE_TYPED_TEST_CASE_P(InputRangeTestInstance, InputRangeTest, RangeTypes);

}  // namespace arangodb::tests::aql

#endif
