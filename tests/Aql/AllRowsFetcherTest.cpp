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
/// @author Tobias Gödderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "DependencyProxyMock.h"
#include "gtest/gtest.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/InputAqlItemRow.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class AllRowsFetcherTest : public ::testing::Test {
 protected:
  ExecutionState state;
  AqlItemMatrix const* matrix = nullptr;
  VPackBuilder input;
  ResourceMonitor monitor;
  DependencyProxyMock<false> dependencyProxyMock{monitor, 1};
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};
};

TEST_F(AllRowsFetcherTest, no_blocks_upstream_the_producer_does_not_wait) {
  dependencyProxyMock.shouldReturn(ExecutionState::DONE, nullptr);

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix != nullptr);
    ASSERT_TRUE(matrix->empty());
    ASSERT_TRUE(matrix->size() == 0);

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix2 == matrix);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 1);
}

TEST_F(AllRowsFetcherTest, no_blocks_upstream_the_producer_waits) {
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(matrix == nullptr);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix != nullptr);
    ASSERT_TRUE(matrix->empty());
    ASSERT_TRUE(matrix->size() == 0);

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix2 == matrix);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(AllRowsFetcherTest, a_single_upstream_block_producer_returns_done_immediately) {
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix != nullptr);
    EXPECT_EQ(1, matrix->getNrRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(1, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    ASSERT_EQ(42, matrix->getRow(rowIndexes[0]).getValue(0).slice().getInt());

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix2 == matrix);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 1);
}

TEST_F(AllRowsFetcherTest, a_single_upstream_block_producer_returns_hasmore_then_done) {
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix != nullptr);
    EXPECT_EQ(1, matrix->getNrRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(1, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    EXPECT_EQ(42, matrix->getRow(rowIndexes[0]).getValue(0).slice().getInt());

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix2 == matrix);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(AllRowsFetcherTest, a_single_upstream_block_producer_waits_then_returns_done) {
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block));

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(matrix == nullptr);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix != nullptr);
    EXPECT_EQ(1, matrix->getNrRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(1, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    EXPECT_EQ(42, matrix->getRow(rowIndexes[0]).getValue(0).slice().getInt());

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix2 == matrix);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(AllRowsFetcherTest, a_single_upstream_block_producer_waits_returns_hasmore_then_done) {
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(matrix == nullptr);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix != nullptr);
    EXPECT_EQ(1, matrix->getNrRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(1, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    EXPECT_EQ(42, matrix->getRow(rowIndexes[0]).getValue(0).slice().getInt());

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix2 == matrix);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 3);
}

// TODO the following tests should be simplified, a simple output
// specification should be compared with the actual output.

TEST_F(AllRowsFetcherTest, multiple_blocks_upstream_producer_does_not_wait) {
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1 = buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3 = buildBlock<1>(itemBlockManager, {{{6}}});
  dependencyProxyMock.shouldReturn(ExecutionState::HASMORE, std::move(block1))
      .andThenReturn(ExecutionState::HASMORE, std::move(block2))
      .andThenReturn(ExecutionState::DONE, std::move(block3));

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix != nullptr);
    EXPECT_EQ(1, matrix->getNrRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(6, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    for (int64_t i = 0; i < 6; i++) {
      int64_t rowValue = i + 1;
      auto row = matrix->getRow(rowIndexes[i]);
      ASSERT_TRUE(row.isInitialized());
      EXPECT_EQ(rowValue, row.getValue(0).slice().getInt());
    }

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix2 == matrix);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 3);
}

TEST_F(AllRowsFetcherTest, multiple_blocks_upstream_producer_waits) {
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1 = buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3 = buildBlock<1>(itemBlockManager, {{{6}}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block1))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block2))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block3));

  {
    AllRowsFetcher testee(dependencyProxyMock);

    // wait when fetching the 1st and 2nd block
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(matrix == nullptr);
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(matrix == nullptr);
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(matrix == nullptr);

    // now get the matrix
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix != nullptr);
    EXPECT_EQ(1, matrix->getNrRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(6, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    for (int64_t i = 0; i < 6; i++) {
      int64_t rowValue = i + 1;
      auto row = matrix->getRow(rowIndexes[i]);
      EXPECT_EQ(rowValue, row.getValue(0).slice().getInt());
    }

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix2 == matrix);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 6);
}

TEST_F(AllRowsFetcherTest, multiple_blocks_upstream_producer_waits_and_does_not_return_done) {
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1 = buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3 = buildBlock<1>(itemBlockManager, {{{6}}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block1))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block2))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block3))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    AllRowsFetcher testee(dependencyProxyMock);

    // wait when fetching the 1st and 2nd block
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(matrix == nullptr);
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(matrix == nullptr);
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(matrix == nullptr);

    // now get the matrix
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix != nullptr);
    EXPECT_EQ(1, matrix->getNrRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(6, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    for (int64_t i = 0; i < 6; i++) {
      int64_t rowValue = i + 1;
      auto row = matrix->getRow(rowIndexes[i]);
      EXPECT_EQ(rowValue, row.getValue(0).slice().getInt());
    }

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(matrix2 == matrix);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 7);
}

TEST_F(AllRowsFetcherTest, handling_of_relevant_shadow_rows) {
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};
  {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 4, 1)};
    block->emplaceValue(0, 0, "a");
    block->setShadowRowDepth(1, AqlValue(AqlValueHintUInt(0ull)));
    block->emplaceValue(1, 0, "a");
    block->emplaceValue(2, 0, "b");
    block->setShadowRowDepth(3, AqlValue(AqlValueHintUInt(0ull)));
    block->emplaceValue(3, 0, "b");
    dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));
  }

  {
    AllRowsFetcher testee(dependencyProxyMock);
    AqlItemMatrix const* matrix = nullptr;
    std::tie(state, matrix) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    EXPECT_EQ(matrix->size(), 1);
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(rowIndexes.size(), 1);
    for (auto const& i : rowIndexes) {
      auto row = matrix->getRow(i);
      ASSERT_TRUE(row.isInitialized());
      EXPECT_TRUE(row.getValue(0).slice().isEqualString("a"));
    }

    // Will stay on done
    std::tie(state, matrix) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix, nullptr);

    // Can fetch shadow row
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("a"));
    EXPECT_TRUE(shadow.isRelevant());

    // Will stay on HASMORE
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    EXPECT_FALSE(shadow.isInitialized());

    std::tie(state, matrix) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    EXPECT_EQ(matrix->size(), 1);
    rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(rowIndexes.size(), 1);
    for (auto const& i : rowIndexes) {
      auto row = matrix->getRow(i);
      ASSERT_TRUE(row.isInitialized());
      EXPECT_TRUE(row.getValue(0).slice().isEqualString("b"));
    }

    // Will stay on done
    std::tie(state, matrix) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix, nullptr);

    // Can fetch shadow row
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
    EXPECT_TRUE(shadow.isRelevant());

    // Will stay on DONE
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(shadow.isInitialized());

    // Will not produce more data
    std::tie(state, matrix) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix, nullptr);

  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 1);
}
/*
TEST_F(AllRowsFetcherTest, handling_of_irrelevant_shadow_rows) {
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};
  {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 7, 1)};
    block->emplaceValue(0, 0, "a");
    block->setShadowRowDepth(1, AqlValue(AqlValueHintUInt(0ull)));
    block->emplaceValue(1, 0, "a");
    block->setShadowRowDepth(2, AqlValue(AqlValueHintUInt(1ull)));
    block->emplaceValue(2, 0, "a");
    // Back to top level
    block->emplaceValue(3, 0, "b");
    block->setShadowRowDepth(4, AqlValue(AqlValueHintUInt(0ull)));
    block->emplaceValue(4, 0, "b");
    block->setShadowRowDepth(5, AqlValue(AqlValueHintUInt(1ull)));
    block->emplaceValue(5, 0, "b");
    block->setShadowRowDepth(6, AqlValue(AqlValueHintUInt(2ull)));
    block->emplaceValue(6, 0, "b");

    dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));
  }

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row.isInitialized());
    EXPECT_TRUE(row.getValue(0).slice().isEqualString("a"));

    // Will stay on done
    std::tie(state, row) = testee.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(row.isInitialized());

    // Can fetch shadow row
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("a"));
    EXPECT_TRUE(shadow.isRelevant());

    // Can fetch shadow row
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("a"));
    EXPECT_FALSE(shadow.isRelevant());

    // Will stay on HASMORE
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    EXPECT_FALSE(shadow.isInitialized());

    std::tie(state, row) = testee.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row.isInitialized());
    EXPECT_TRUE(row.getValue(0).slice().isEqualString("b"));

    // Will stay on done
    std::tie(state, row) = testee.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(row.isInitialized());

    // Can fetch shadow row
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
    EXPECT_TRUE(shadow.isRelevant());

    // Can fetch shadow row
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
    EXPECT_FALSE(shadow.isRelevant());

    // Can fetch shadow row
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
    EXPECT_FALSE(shadow.isRelevant());

    // Will stay on DONE
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(shadow.isInitialized());

    // Will not produce data row
    std::tie(state, row) = testee.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(row.isInitialized());

  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 1);
}

TEST_F(AllRowsFetcherTest, handling_consecutive_shadowrows) {
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};
  {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 4, 1)};
    block->emplaceValue(0, 0, "a");
    block->setShadowRowDepth(0, AqlValue(AqlValueHintUInt(0ull)));
    block->emplaceValue(1, 0, "a");
    block->setShadowRowDepth(1, AqlValue(AqlValueHintUInt(1ull)));
    block->emplaceValue(2, 0, "b");
    block->setShadowRowDepth(2, AqlValue(AqlValueHintUInt(0ull)));
    block->emplaceValue(3, 0, "c");
    block->setShadowRowDepth(3, AqlValue(AqlValueHintUInt(0ull)));
    dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));
  }

  {
    AllRowsFetcher testee(dependencyProxyMock);

    // First no data row
    std::tie(state, row) = testee.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(row.isInitialized());

    // but shadow row
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("a"));
    EXPECT_TRUE(shadow.isRelevant());

    // Second, non-relevant shadow row
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("a"));
    EXPECT_FALSE(shadow.isRelevant());

    // Third, relevant shadow row.
    // We cannot continue fetching shadowrows now.
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_FALSE(shadow.isInitialized());

    // Require to fetch one data-row (done, empty)
    std::tie(state, row) = testee.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(row.isInitialized());

    // Now we can continue to fetch shadowrows
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
    EXPECT_TRUE(shadow.isRelevant());

    // We cannot continue fetching shadowrows now.
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::HASMORE);
    ASSERT_FALSE(shadow.isInitialized());

    // Require to fetch one data-row (done, empty)
    std::tie(state, row) = testee.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(row.isInitialized());

    // Now we can continue to fetch shadowrows
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("c"));
    EXPECT_TRUE(shadow.isRelevant());

    // Will stay on done
    std::tie(state, row) = testee.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(row.isInitialized());
    // Will stay on done
    std::tie(state, shadow) = testee.fetchShadowRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(shadow.isInitialized());
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 1);
}
*/
}  // namespace aql
}  // namespace tests
}  // namespace arangodb
