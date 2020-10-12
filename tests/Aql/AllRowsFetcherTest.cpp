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
/// @author Tobias Gödderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "DependencyProxyMock.h"
#include "gtest/gtest.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/InputAqlItemRow.h"

#include "FetcherTestHelper.h"

#include <Containers/HashSet.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

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
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};
  RegIdSet inputRegisters{};
  DependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{monitor, inputRegisters, 1};
};

TEST_F(AllRowsFetcherTest, no_blocks_upstream_the_producer_does_not_wait) {
  dependencyProxyMock.shouldReturn(ExecutionState::DONE, nullptr);

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    ASSERT_TRUE(matrix->empty());
    ASSERT_EQ(matrix->size(), 0);

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix2, nullptr);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 1);
}

TEST_F(AllRowsFetcherTest, no_blocks_upstream_the_producer_waits) {
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(matrix, nullptr);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    ASSERT_TRUE(matrix->empty());
    ASSERT_EQ(matrix->size(), 0);

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix2, nullptr);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(AllRowsFetcherTest, a_single_upstream_block_producer_returns_done_immediately) {
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    EXPECT_EQ(1, matrix->getNumRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(1, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    ASSERT_EQ(42, matrix->getRow(rowIndexes[0]).getValue(0).slice().getInt());

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix2, nullptr);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 1);
}

TEST_F(AllRowsFetcherTest, a_single_upstream_block_producer_returns_hasmore_then_done) {
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    EXPECT_EQ(1, matrix->getNumRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(1, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    EXPECT_EQ(42, matrix->getRow(rowIndexes[0]).getValue(0).slice().getInt());

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix2, nullptr);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(AllRowsFetcherTest, a_single_upstream_block_producer_waits_then_returns_done) {
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block));

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(matrix, nullptr);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    EXPECT_EQ(1, matrix->getNumRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(1, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    EXPECT_EQ(42, matrix->getRow(rowIndexes[0]).getValue(0).slice().getInt());

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix2, nullptr);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(AllRowsFetcherTest, a_single_upstream_block_producer_waits_returns_hasmore_then_done) {
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    AllRowsFetcher testee(dependencyProxyMock);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(matrix, nullptr);

    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    EXPECT_EQ(1, matrix->getNumRegisters());
    EXPECT_FALSE(matrix->empty());
    EXPECT_EQ(1, matrix->size());
    auto rowIndexes = matrix->produceRowIndexes();
    ASSERT_EQ(matrix->size(), rowIndexes.size());
    EXPECT_EQ(42, matrix->getRow(rowIndexes[0]).getValue(0).slice().getInt());

    AqlItemMatrix const* matrix2 = nullptr;
    std::tie(state, matrix2) = testee.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix2, nullptr);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 3);
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
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    EXPECT_EQ(1, matrix->getNumRegisters());
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
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix2, nullptr);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 3);
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
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(matrix, nullptr);
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(matrix, nullptr);
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(matrix, nullptr);

    // now get the matrix
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    EXPECT_EQ(1, matrix->getNumRegisters());
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
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix2, nullptr);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 6);
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
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(matrix, nullptr);
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(matrix, nullptr);
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(matrix, nullptr);

    // now get the matrix
    std::tie(state, matrix) = testee.fetchAllRows();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_NE(matrix, nullptr);
    EXPECT_EQ(1, matrix->getNumRegisters());
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
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(matrix2, nullptr);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 7);
}

class AllRowsFetcherFetchRows : public fetcherHelper::PatternTestWrapper<AllRowsFetcher> {
 public:
  AllRowsFetcherFetchRows()
      : fetcherHelper::PatternTestWrapper<AllRowsFetcher>() {}

  void PullAndAssertDataRows(std::vector<std::string> const& dataResults) override {
    AqlItemMatrix const* matrix = nullptr;
    ExecutionState state = ExecutionState::HASMORE;

    // Fetch all rows until done
    std::tie(state, matrix) = _fetcher.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    if (!dataResults.empty() || matrix != nullptr) {
      ASSERT_NE(matrix, nullptr);

      // Assert that all rows come out in order and only these
      EXPECT_EQ(matrix->size(), dataResults.size());
      auto rowIndexes = matrix->produceRowIndexes();
      ASSERT_EQ(rowIndexes.size(), dataResults.size());

      for (size_t i = 0; i < rowIndexes.size(); ++i) {
        auto row = matrix->getRow(rowIndexes[i]);
        ASSERT_TRUE(row.isInitialized());
        EXPECT_TRUE(row.getValue(0).slice().isEqualString(dataResults[i]));
      }
    }

    AqlItemMatrix const* nextMatrix;
    // Now assert that we will forever stay in the DONE state and do not move on.
    std::tie(state, nextMatrix) = _fetcher.fetchAllRows();
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(nextMatrix, nullptr);
  }
};

TEST_SHADOWROW_PATTERN_1(AllRowsFetcherFetchRows, AllRowsFetcherPattern1Test);
TEST_SHADOWROW_PATTERN_2(AllRowsFetcherFetchRows, AllRowsFetcherPattern2Test);
TEST_SHADOWROW_PATTERN_3(AllRowsFetcherFetchRows, AllRowsFetcherPattern3Test);
TEST_SHADOWROW_PATTERN_4(AllRowsFetcherFetchRows, AllRowsFetcherPattern4Test);
TEST_SHADOWROW_PATTERN_5(AllRowsFetcherFetchRows, AllRowsFetcherPattern5Test);
TEST_SHADOWROW_PATTERN_6(AllRowsFetcherFetchRows, AllRowsFetcherPattern6Test);

class AllRowsFetcherFetchSingleRow
    : public fetcherHelper::PatternTestWrapper<AllRowsFetcher> {
 public:
  AllRowsFetcherFetchSingleRow()
      : fetcherHelper::PatternTestWrapper<AllRowsFetcher>() {}

  void PullAndAssertDataRows(std::vector<std::string> const& dataResults) override {
    InputAqlItemRow row{CreateInvalidInputRowHint{}};
    ExecutionState state = ExecutionState::HASMORE;

    // Fetch all rows until done
    for (auto const& it : dataResults) {
      std::tie(state, row) = _fetcher.fetchRow();
      if (it != dataResults.back()) {
        EXPECT_EQ(state, ExecutionState::HASMORE);
      } else {
        EXPECT_EQ(state, ExecutionState::DONE);
      }
      ASSERT_TRUE(row.isInitialized());
      EXPECT_TRUE(row.getValue(0).slice().isEqualString(it));
    }
    // Now assert that we will forever stay in the DONE state and do not move on.
    std::tie(state, row) = _fetcher.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row.isInitialized());
  }
};

TEST_SHADOWROW_PATTERN_1(AllRowsFetcherFetchSingleRow, AllRowsFetcherSingleRowPattern1Test);
TEST_SHADOWROW_PATTERN_2(AllRowsFetcherFetchSingleRow, AllRowsFetcherSingleRowPattern2Test);
TEST_SHADOWROW_PATTERN_3(AllRowsFetcherFetchSingleRow, AllRowsFetcherSingleRowPattern3Test);
TEST_SHADOWROW_PATTERN_4(AllRowsFetcherFetchSingleRow, AllRowsFetcherSingleRowPattern4Test);
TEST_SHADOWROW_PATTERN_5(AllRowsFetcherFetchSingleRow, AllRowsFetcherSingleRowPattern5Test);
TEST_SHADOWROW_PATTERN_6(AllRowsFetcherFetchSingleRow, AllRowsFetcherSingleRowPattern6Test);

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
