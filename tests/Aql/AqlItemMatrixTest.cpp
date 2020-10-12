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

#include "Aql/AqlItemMatrix.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {
class AqlItemMatrixTest : public AqlExecutorTestCase<> {};

TEST_F(AqlItemMatrixTest, should_forward_number_of_regs) {
  for (RegisterCount c = 1; c < 3; c++) {
    AqlItemMatrix testee(c);
    EXPECT_EQ(testee.getNumRegisters(), c);
  }
}

TEST_F(AqlItemMatrixTest, expose_size_of_data_only) {
  auto& manager = this->manager();

  AqlItemMatrix testee(1);
  EXPECT_TRUE(testee.blocksEmpty());
  {
    // 12
    auto block =
        buildBlock<1>(manager,
                      {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}});
    testee.addBlock(block);
  }
  EXPECT_FALSE(testee.blocksEmpty());
  ASSERT_EQ(testee.size(), 12);
  {
    // 8
    auto block = buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}});
    testee.addBlock(block);
  }
  EXPECT_FALSE(testee.blocksEmpty());
  ASSERT_EQ(testee.size(), 20);

  {
    // 9
    auto block = buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}});
    testee.addBlock(block);
  }
  EXPECT_FALSE(testee.blocksEmpty());
  ASSERT_EQ(testee.size(), 29);
}

TEST_F(AqlItemMatrixTest, count_number_of_blocks) {
  auto& manager = this->manager();

  AqlItemMatrix testee(1);
  ASSERT_EQ(testee.numberOfBlocks(), 0);
  {
    auto block =
        buildBlock<1>(manager,
                      {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}});
    testee.addBlock(block);
  }
  ASSERT_EQ(testee.numberOfBlocks(), 1);
  {
    auto block = buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}});
    testee.addBlock(block);
  }
  ASSERT_EQ(testee.numberOfBlocks(), 2);

  {
    auto block = buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}});
    testee.addBlock(block);
  }
  ASSERT_EQ(testee.numberOfBlocks(), 3);
}

// Section ShadowRow behaviour

TEST_F(AqlItemMatrixTest, size_with_shadow_row_in_first_block) {
  auto& manager = this->manager();

  AqlItemMatrix testee(1);
  ASSERT_EQ(testee.size(), 0);
  {
    auto block =
        buildBlock<1>(manager,
                      {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}},
                      {{5, 0}});
    testee.addBlock(block);
  }
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.size(), 5);

  testee.popShadowRow();
  EXPECT_FALSE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.size(), 6);
}

TEST_F(AqlItemMatrixTest, size_with_shadow_row_in_multiple_blocks) {
  auto& manager = this->manager();

  AqlItemMatrix testee(1);
  ASSERT_EQ(testee.size(), 0);
  testee.addBlock(
      buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}},
                    {{5, 0}}));

  EXPECT_TRUE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.size(), 5);

  testee.popShadowRow();

  EXPECT_FALSE(testee.stoppedOnShadowRow());
  // We can only add more blocks, after we have removed the last shadowRow

  testee.addBlock(
      buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}}));
  testee.addBlock(
      buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}},
                    {{2, 0}}));

  EXPECT_TRUE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.size(), 20);

  testee.popShadowRow();
  EXPECT_FALSE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.size(), 9);
}

TEST_F(AqlItemMatrixTest, size_with_multiple_shadow_row_in_first_block) {
  auto& manager = this->manager();

  AqlItemMatrix testee(1);
  ASSERT_EQ(testee.size(), 0);
  {
    auto block =
        buildBlock<1>(manager,
                      {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}},
                      {{3, 0}, {5, 0}, {11, 0}});
    testee.addBlock(block);
  }
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.size(), 3);

  testee.popShadowRow();
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.size(), 1);

  testee.popShadowRow();
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.size(), 5);

  testee.popShadowRow();
  EXPECT_FALSE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.size(), 0);
  EXPECT_EQ(testee.numberOfBlocks(), 0);
}

// Section ShadowRow rowIndexes

TEST_F(AqlItemMatrixTest, rowIndexes_with_shadow_row_in_first_block) {
  auto& manager = this->manager();

  AqlItemMatrix testee(1);
  ASSERT_EQ(testee.size(), 0);
  {
    auto block =
        buildBlock<1>(manager,
                      {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}},
                      {{5, 0}});
    testee.addBlock(block);
  }
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  {
    auto indexes = testee.produceRowIndexes();
    ASSERT_EQ(indexes.size(), 5);
    for (uint32_t i = 0; i < 5; ++i) {
      auto const& pair = indexes.at(i);
      EXPECT_EQ(pair.first, 0);
      EXPECT_EQ(pair.second, i);
    }
  }

  testee.popShadowRow();
  EXPECT_FALSE(testee.stoppedOnShadowRow());
  {
    auto indexes = testee.produceRowIndexes();
    ASSERT_EQ(indexes.size(), 6);
    for (uint32_t i = 0; i < 6; ++i) {
      auto const& pair = indexes.at(i);
      EXPECT_EQ(pair.first, 0);
      // The first 6 rows have been returned in the last call
      EXPECT_EQ(pair.second, i + 6);
    }
  }
}

TEST_F(AqlItemMatrixTest, rowIndexes_with_shadow_row_in_multiple_blocks) {
  auto& manager = this->manager();

  AqlItemMatrix testee(1);

  ASSERT_EQ(testee.size(), 0);
  testee.addBlock(
      buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}},
                    {{5, 0}}));

  EXPECT_TRUE(testee.stoppedOnShadowRow());
  {
    auto indexes = testee.produceRowIndexes();
    ASSERT_EQ(indexes.size(), 5);
    for (uint32_t i = 0; i < 5; ++i) {
      auto const& pair = indexes.at(i);
      EXPECT_EQ(pair.first, 0);
      EXPECT_EQ(pair.second, i);
    }
  }
  testee.popShadowRow();

  EXPECT_FALSE(testee.stoppedOnShadowRow());
  // We can only add more blocks, after we have removed the last shadowRow

  testee.addBlock(
      buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}}));
  testee.addBlock(
      buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}},
                    {{2, 0}}));

  EXPECT_TRUE(testee.stoppedOnShadowRow());

  {
    auto indexes = testee.produceRowIndexes();
    ASSERT_EQ(indexes.size(), 20);
    // The first 6 rows are from the first block.
    // Offset by 6 rows
    for (uint32_t i = 0; i < 6; ++i) {
      auto const& pair = indexes.at(i);
      EXPECT_EQ(pair.first, 0);
      EXPECT_EQ(pair.second, i + 6);
    }

    // The next 12 rows are from the second block.
    // Offset by 0 rows
    for (uint32_t i = 0; i < 12; ++i) {
      auto const& pair = indexes.at(i + 6);
      EXPECT_EQ(pair.first, 1);
      EXPECT_EQ(pair.second, i);
    }

    // The last 2 rows are from the last block.
    // Offset by 0 rows
    for (uint32_t i = 0; i < 2; ++i) {
      auto const& pair = indexes.at(i + 18);
      EXPECT_EQ(pair.first, 2);
      EXPECT_EQ(pair.second, i);
    }
  }

  testee.popShadowRow();
  EXPECT_FALSE(testee.stoppedOnShadowRow());
  {
    auto indexes = testee.produceRowIndexes();
    ASSERT_EQ(indexes.size(), 9);
    // The first 9 rows are from the first block.
    // Offset by 3 rows
    for (uint32_t i = 0; i < 9; ++i) {
      auto const& pair = indexes.at(i);
      EXPECT_EQ(pair.first, 0);
      EXPECT_EQ(pair.second, i + 3);
    }
  }
}

TEST_F(AqlItemMatrixTest, rowIndexes_with_multiple_shadow_row_in_first_block) {
  auto& manager = this->manager();

  AqlItemMatrix testee(1);
  ASSERT_EQ(testee.size(), 0);
  {
    auto block =
        buildBlock<1>(manager,
                      {{1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}, {1}, {2}, {3}, {4}},
                      {{3, 0}, {5, 0}, {11, 0}});
    testee.addBlock(block);
  }
  EXPECT_TRUE(testee.stoppedOnShadowRow());

  {
    auto indexes = testee.produceRowIndexes();
    ASSERT_EQ(indexes.size(), 3);
    for (uint32_t i = 0; i < 3; ++i) {
      auto const& pair = indexes.at(i);
      EXPECT_EQ(pair.first, 0);
      EXPECT_EQ(pair.second, i);
    }
  }

  testee.popShadowRow();
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  {
    auto indexes = testee.produceRowIndexes();
    ASSERT_EQ(indexes.size(), 1);
    // Offset by 4 rows
    for (uint32_t i = 0; i < 1; ++i) {
      auto const& pair = indexes.at(i);
      EXPECT_EQ(pair.first, 0);
      EXPECT_EQ(pair.second, i + 4);
    }
  }

  testee.popShadowRow();
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  {
    auto indexes = testee.produceRowIndexes();
    ASSERT_EQ(indexes.size(), 5);
    // Offset by 6 rows
    for (uint32_t i = 0; i < 5; ++i) {
      auto const& pair = indexes.at(i);
      EXPECT_EQ(pair.first, 0);
      EXPECT_EQ(pair.second, i + 6);
    }
  }

  testee.popShadowRow();
  EXPECT_FALSE(testee.stoppedOnShadowRow());
  {
    auto indexes = testee.produceRowIndexes();
    ASSERT_EQ(indexes.size(), 0);
  }
  EXPECT_EQ(testee.numberOfBlocks(), 0);
}

// Section ShadowRow getBlock

TEST_F(AqlItemMatrixTest, getBlock_with_shadow_row_in_first_block) {
  auto& manager = this->manager();
  // We do not test skip and State logix for the Matrix, that
  // has to be handled by using Methods.
  ExecutorState unusedState = ExecutorState::DONE;
  size_t unusedSkip = 0;

  AqlItemMatrix testee(1);
  ASSERT_EQ(testee.size(), 0);
  {
    auto block =
        buildBlock<1>(manager,
                      {{1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11}, {12}},
                      {{5, 0}});
    testee.addBlock(block);
  }
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  {
    auto const [block, start] = testee.getBlock(0);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 0);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 1; i < 6; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches we need to stop at the shadowRow.
    EXPECT_TRUE(range.hasShadowRow());
  }

  testee.popShadowRow();
  EXPECT_FALSE(testee.stoppedOnShadowRow());
  {
    auto const [block, start] = testee.getBlock(0);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 6);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 7; i < 13; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches we need to stop at the shadowRow.
    EXPECT_FALSE(range.hasShadowRow());
    EXPECT_FALSE(range.hasDataRow());
  }
}

TEST_F(AqlItemMatrixTest, getBlock_with_shadow_row_in_multiple_blocks) {
  auto& manager = this->manager();
  // We do not test skip and State logix for the Matrix, that
  // has to be handled by using Methods.
  ExecutorState unusedState = ExecutorState::DONE;
  size_t unusedSkip = 0;

  AqlItemMatrix testee(1);

  ASSERT_EQ(testee.size(), 0);
  testee.addBlock(
      buildBlock<1>(manager, {{1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11}, {12}},
                    {{5, 0}}));

  EXPECT_TRUE(testee.stoppedOnShadowRow());
  {
    auto const [block, start] = testee.getBlock(0);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 0);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 1; i < 6; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches we need to stop at the shadowRow.
    EXPECT_TRUE(range.hasShadowRow());
  }
  testee.popShadowRow();

  EXPECT_FALSE(testee.stoppedOnShadowRow());
  // We can only add more blocks, after we have removed the last shadowRow

  testee.addBlock(buildBlock<1>(
      manager, {{13}, {14}, {15}, {16}, {17}, {18}, {19}, {20}, {21}, {22}, {23}, {24}}));
  testee.addBlock(buildBlock<1>(
      manager, {{25}, {26}, {27}, {28}, {29}, {30}, {31}, {32}, {33}, {34}, {35}, {36}},
      {{2, 0}}));

  EXPECT_TRUE(testee.stoppedOnShadowRow());

  {
    auto const [block, start] = testee.getBlock(0);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 6);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 7; i < 13; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches we need to stop at the shadowRow.
    EXPECT_FALSE(range.hasShadowRow());
    EXPECT_FALSE(range.hasDataRow());
  }

  {
    auto const [block, start] = testee.getBlock(1);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 0);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 13; i < 25; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches this range is at the end
    EXPECT_FALSE(range.hasShadowRow());
    EXPECT_FALSE(range.hasDataRow());
  }

  {
    auto const [block, start] = testee.getBlock(2);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 0);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 25; i < 27; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches this range is at the shadowRow
    EXPECT_TRUE(range.hasShadowRow());
  }

  testee.popShadowRow();
  EXPECT_FALSE(testee.stoppedOnShadowRow());
  {
    auto const [block, start] = testee.getBlock(0);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 3);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 28; i < 37; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches the range is at the end
    EXPECT_FALSE(range.hasShadowRow());
    EXPECT_FALSE(range.hasDataRow());
  }
}

TEST_F(AqlItemMatrixTest, getBlock_with_multiple_shadow_row_in_first_block) {
  auto& manager = this->manager();
  // We do not test skip and State logix for the Matrix, that
  // has to be handled by using Methods.
  ExecutorState unusedState = ExecutorState::DONE;
  size_t unusedSkip = 0;

  AqlItemMatrix testee(1);
  ASSERT_EQ(testee.size(), 0);
  {
    auto block =
        buildBlock<1>(manager,
                      {{1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11}, {12}},
                      {{3, 0}, {5, 0}, {11, 0}});
    testee.addBlock(block);
  }
  EXPECT_TRUE(testee.stoppedOnShadowRow());

  {
    auto const [block, start] = testee.getBlock(0);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 0);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 1; i < 4; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches we need to stop at the shadowRow.
    EXPECT_TRUE(range.hasShadowRow());
  }

  testee.popShadowRow();
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  {
    auto const [block, start] = testee.getBlock(0);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 4);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 5; i < 6; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches we need to stop at the shadowRow.
    EXPECT_TRUE(range.hasShadowRow());
  }

  testee.popShadowRow();
  EXPECT_TRUE(testee.stoppedOnShadowRow());
  {
    auto const [block, start] = testee.getBlock(0);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(start, 6);
    AqlItemBlockInputRange range(unusedState, unusedSkip, block, start);
    for (int64_t i = 7; i < 12; ++i) {
      ASSERT_TRUE(range.hasDataRow());
      auto [unused, row] = range.nextDataRow();
      auto val = row.getValue(0);
      ASSERT_TRUE(val.isNumber());
      EXPECT_EQ(val.toInt64(), i);
    }
    // After this fetches we need to stop at the shadowRow.
    EXPECT_TRUE(range.hasShadowRow());
  }

  testee.popShadowRow();
  EXPECT_FALSE(testee.stoppedOnShadowRow());
  EXPECT_EQ(testee.numberOfBlocks(), 0);
}

}  // namespace arangodb::tests::aql
