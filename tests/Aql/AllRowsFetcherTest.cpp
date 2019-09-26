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
#include "Aql/AqlItemMatrix.h"
#include "Aql/InputAqlItemRow.h"

#include "Logger/LogMacros.h"

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

/// SECTION: Shadow row combination tests. Split the input block into diverse
/// parts to securely test all borders

namespace {

/// Helper method that splits a single baseBlock into multiple AqlItemBlocks.
/// Where to split is defined by the piecesBitMap handed it.
/// If the `n-th` bit piecesBitMap is set, we will add a split after Row `n`.
/// e.g. we will now have a block from 0 -> n and a block from n+1 -> end
/// we can apply multiple of these splits, ulimate case, split block into single line blocks.
std::vector<std::pair<arangodb::aql::ExecutionState, arangodb::aql::SharedAqlItemBlockPtr>> CutMyBlockIntoPieces(
    SharedAqlItemBlockPtr baseBlock, uint64_t piecesBitMap) {
  std::vector<std::pair<arangodb::aql::ExecutionState, arangodb::aql::SharedAqlItemBlockPtr>> toReturn{};
  size_t from = 0;
  for (size_t to = 0; to < baseBlock->size(); ++to) {
    if (((piecesBitMap) >> (to)) & 1) {
      // We split blocks if the corresponding bit is set.
      ExecutionState state = to == baseBlock->size() - 1 ? ExecutionState::DONE
                                                         : ExecutionState::HASMORE;
      toReturn.emplace_back(std::make_pair(state, baseBlock->slice(from, to + 1)));
      from = to + 1;
    }
  }
  if (from < baseBlock->size()) {
    toReturn.emplace_back(std::make_pair(ExecutionState::DONE,
                                         baseBlock->slice(from, baseBlock->size())));
  }
  return toReturn;
}

/// helper method to assert that next up we will produce exactly the given set
/// of DataRows For simplicity we only test string values here, that AqlValues
/// of different types work is tested somewhere else.
template <class Fetcher>
void PullAndAssertDataRows(Fetcher& testee, std::vector<std::string> dataResults);

/// Helper method to assert that we can now pull all ShadowRows at once, and stop at the next data row
/// Also asserts that we will never leave the finalState (either HASMORE or DONE)
template <class Fetcher>
void PullAndAssertShadowRows(Fetcher& testee,
                             std::vector<std::pair<uint64_t, std::string>> shadowResults,
                             ExecutionState finalState);

template <class Fetcher>
void StaysConstantAfterDone(Fetcher& testee) {
  PullAndAssertDataRows(testee, {});
  PullAndAssertShadowRows(testee, {}, ExecutionState::DONE);
}

template <>
void PullAndAssertDataRows<AllRowsFetcher>(AllRowsFetcher& testee,
                                           std::vector<std::string> dataResults) {
  AqlItemMatrix const* matrix = nullptr;
  ExecutionState state = ExecutionState::HASMORE;

  // Fetch all rows until done
  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
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

  // Now assert that we will forever stay in the DONE state and do not move on.
  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(matrix, nullptr);
}

template <>
void PullAndAssertShadowRows<AllRowsFetcher>(AllRowsFetcher& testee,
                                             std::vector<std::pair<uint64_t, std::string>> shadowResults,
                                             ExecutionState finalState) {
  ExecutionState state = ExecutionState::HASMORE;
  ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};
  // For each entry in shadowResults.
  // We can fetch 1 shadow row matching it.
  for (auto const& it : shadowResults) {
    std::tie(state, shadow) = testee.fetchShadowRow();
    if (it == shadowResults.back()) {
      // The last element will reach the final state
      EXPECT_EQ(state, finalState);
    } else {
      EXPECT_EQ(state, ExecutionState::HASMORE);
    }
    // We are awaiting a shadow row now
    ASSERT_TRUE(shadow.isInitialized());
    // Assert the data
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString(it.second));
    EXPECT_EQ(shadow.getDepth(), it.first);
  }

  // Will stay on FinalState
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, finalState);
  EXPECT_FALSE(shadow.isInitialized());
}

}  // namespace

// Section First Pattern, alternating Input / Shadow Rows, 1 higher level shadow row

class AllRowsFetcherPattern1Test : public testing::TestWithParam<uint64_t> {
 public:
  static size_t const NrRows = 7;

 protected:
  ResourceMonitor monitor;
  DependencyProxyMock<false> dependencyProxyMock{monitor, 1};
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  void SetUp() override {
    SharedAqlItemBlockPtr baseBlock =
        buildBlock<1>(itemBlockManager,
                      {{{"a"}}, {{"a"}}, {{"b"}}, {{"b"}}, {{"c"}}, {{"c"}}, {{"c"}}},
                      {{1, 0}, {3, 0}, {5, 0}, {6, 0}});
    // prepare the proxy
    uint64_t splits = GetParam();
    ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));
    auto toReturn = CutMyBlockIntoPieces(baseBlock, splits);
    dependencyProxyMock.shouldReturn(toReturn);
  }
};

TEST_P(AllRowsFetcherPattern1Test, handle_shadow_rows) {
  // The result should be always identical, it does not matter how the blocks are splitted.
  AllRowsFetcher testee(dependencyProxyMock);

  // Fetch the input data
  PullAndAssertDataRows(testee, {"a"});

  // Fetch the shadow row
  PullAndAssertShadowRows(testee, {{0, "a"}}, ExecutionState::HASMORE);

  // Fetch the input data
  PullAndAssertDataRows(testee, {"b"});

  // Fetch the shadow row
  PullAndAssertShadowRows(testee, {{0, "b"}}, ExecutionState::HASMORE);

  // Fetch the input data
  PullAndAssertDataRows(testee, {"c"});

  // Fetch the shadow row and the higher level shadow row
  PullAndAssertShadowRows(testee, {{0, "c"}, {1, "c"}}, ExecutionState::DONE);

  // Done check
  StaysConstantAfterDone(testee);
}

INSTANTIATE_TEST_CASE_P(AllRowsFetcherPattern1Instanciated, AllRowsFetcherPattern1Test,
                        testing::Range(static_cast<uint64_t>(0),
                                       static_cast<uint64_t>(std::pow(2, AllRowsFetcherPattern1Test::NrRows -
                                                                             1))));

// Section Second Pattern, two consecutive relevant shadow rows, , 1 higher level shadow row

class AllRowsFetcherPattern2Test : public testing::TestWithParam<uint64_t> {
 public:
  static size_t const NrRows = 4;

 protected:
  ResourceMonitor monitor;
  DependencyProxyMock<false> dependencyProxyMock{monitor, 1};
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  void SetUp() override {
    SharedAqlItemBlockPtr baseBlock{new AqlItemBlock(itemBlockManager, NrRows, 1)};
    {  // fill data into the baseBlock
      baseBlock->emplaceValue(0, 0, "a");
      baseBlock->emplaceValue(1, 0, "a");
      baseBlock->setShadowRowDepth(1, AqlValue(AqlValueHintUInt(0ull)));
      baseBlock->emplaceValue(2, 0, "b");
      baseBlock->setShadowRowDepth(2, AqlValue(AqlValueHintUInt(0ull)));
      baseBlock->emplaceValue(3, 0, "b");
      baseBlock->setShadowRowDepth(3, AqlValue(AqlValueHintUInt(1ull)));
    }
    // prepare the proxy
    uint64_t splits = GetParam();
    ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));
    auto toReturn = CutMyBlockIntoPieces(baseBlock, splits);
    dependencyProxyMock.shouldReturn(toReturn);
  }
};

TEST_P(AllRowsFetcherPattern2Test, handle_shadow_rows) {
  AllRowsFetcher testee(dependencyProxyMock);

  // Fetch the input data
  PullAndAssertDataRows(testee, {"a"});

  AqlItemMatrix const* matrix = nullptr;
  ExecutionState state = ExecutionState::HASMORE;
  ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};

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

  // Now fetch an empty matrix and shadowRow "b", and higher level shadowRow "b"
  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(matrix, nullptr);

  // Will stay on done
  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(matrix, nullptr);

  // Can fetch shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
  EXPECT_TRUE(shadow.isRelevant());

  // Can fetch higher level shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
  EXPECT_FALSE(shadow.isRelevant());
  EXPECT_EQ(shadow.getDepth(), 1);

  // Will stay on done
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_FALSE(shadow.isInitialized());

  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(matrix, nullptr);
}

INSTANTIATE_TEST_CASE_P(AllRowsFetcherPattern2Instanciated, AllRowsFetcherPattern2Test,
                        testing::Range(static_cast<uint64_t>(0),
                                       static_cast<uint64_t>(std::pow(2, AllRowsFetcherPattern2Test::NrRows -
                                                                             1))));

// Section Third Pattern, 1 input, and alternating relevant irrelvant shadow rows

class AllRowsFetcherPattern3Test : public testing::TestWithParam<uint64_t> {
 public:
  static size_t const NrRows = 5;

 protected:
  ResourceMonitor monitor;
  DependencyProxyMock<false> dependencyProxyMock{monitor, 1};
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  void SetUp() override {
    SharedAqlItemBlockPtr baseBlock{new AqlItemBlock(itemBlockManager, NrRows, 1)};
    {  // fill data into the baseBlock
      baseBlock->emplaceValue(0, 0, "a");
      baseBlock->emplaceValue(1, 0, "a");
      baseBlock->setShadowRowDepth(1, AqlValue(AqlValueHintUInt(0ull)));
      baseBlock->emplaceValue(2, 0, "a");
      baseBlock->setShadowRowDepth(2, AqlValue(AqlValueHintUInt(1ull)));

      baseBlock->emplaceValue(3, 0, "b");
      baseBlock->setShadowRowDepth(3, AqlValue(AqlValueHintUInt(0ull)));
      baseBlock->emplaceValue(4, 0, "b");
      baseBlock->setShadowRowDepth(4, AqlValue(AqlValueHintUInt(1ull)));
    }
    // prepare the proxy
    uint64_t splits = GetParam();
    ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));
    auto toReturn = CutMyBlockIntoPieces(baseBlock, splits);
    dependencyProxyMock.shouldReturn(toReturn);
  }
};

TEST_P(AllRowsFetcherPattern3Test, handle_shadow_rows) {
  // The result should be always identical, it does not matter how the blocks are splitted.
  AllRowsFetcher testee(dependencyProxyMock);

  // Fetch first data row
  PullAndAssertDataRows(testee, {"a"});

  // Fetch shadow rows
  PullAndAssertShadowRows(testee, {{0, "a"}, {1, "a"}}, ExecutionState::HASMORE);

  // Now we need to fetch an empty list of Data rows
  PullAndAssertDataRows(testee, {});

  // Fetch "b" on two levels
  PullAndAssertShadowRows(testee, {{0, "b"}, {1, "b"}}, ExecutionState::DONE);

  // Assert we cannot get any more
  StaysConstantAfterDone(testee);
}

INSTANTIATE_TEST_CASE_P(AllRowsFetcherPattern3Instanciated, AllRowsFetcherPattern3Test,
                        testing::Range(static_cast<uint64_t>(0),
                                       static_cast<uint64_t>(std::pow(2, AllRowsFetcherPattern3Test::NrRows -
                                                                             1))));

// Section Foruth Pattern, 1 input, and alternating relevant irrelvant shadow rows

class AllRowsFetcherPattern4Test : public testing::TestWithParam<uint64_t> {
 public:
  static size_t const NrRows = 5;

 protected:
  ResourceMonitor monitor;
  DependencyProxyMock<false> dependencyProxyMock{monitor, 1};
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  void SetUp() override {
    SharedAqlItemBlockPtr baseBlock{new AqlItemBlock(itemBlockManager, NrRows, 1)};
    {  // fill data into the baseBlock
      baseBlock->emplaceValue(0, 0, "a");
      baseBlock->setShadowRowDepth(0, AqlValue(AqlValueHintUInt(0ull)));
      baseBlock->emplaceValue(1, 0, "b");
      baseBlock->emplaceValue(2, 0, "c");
      baseBlock->emplaceValue(3, 0, "d");
      baseBlock->emplaceValue(4, 0, "d");
      baseBlock->setShadowRowDepth(4, AqlValue(AqlValueHintUInt(0ull)));
    }
    // prepare the proxy
    uint64_t splits = GetParam();
    ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));
    auto toReturn = CutMyBlockIntoPieces(baseBlock, splits);
    dependencyProxyMock.shouldReturn(toReturn);
  }
};

TEST_P(AllRowsFetcherPattern4Test, handle_shadow_rows) {
  // The result should be always identical, it does not matter how the blocks are splitted.
  AllRowsFetcher testee(dependencyProxyMock);
  AqlItemMatrix const* matrix = nullptr;
  ExecutionState state = ExecutionState::HASMORE;
  ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};

  // We start with an empty matrix
  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(matrix, nullptr);

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

  // Now fetch the data block
  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_NE(matrix, nullptr);
  EXPECT_EQ(matrix->size(), 3);
  auto rowIndexes = matrix->produceRowIndexes();
  ASSERT_EQ(rowIndexes.size(), 3);
  size_t count = 0;
  for (auto const& i : rowIndexes) {
    auto row = matrix->getRow(i);
    ASSERT_TRUE(row.isInitialized());
    switch (count) {
      case 0:
        EXPECT_TRUE(row.getValue(0).slice().isEqualString("b"));
        break;
      case 1:
        EXPECT_TRUE(row.getValue(0).slice().isEqualString("c"));
        break;
      case 2:
        EXPECT_TRUE(row.getValue(0).slice().isEqualString("d"));
        break;
      default:
        EXPECT_TRUE(false) << "This code is unreachable";
    }
    ++count;
  }

  // Will stay on done
  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(matrix, nullptr);

  // Can fetch shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("d"));
  EXPECT_TRUE(shadow.isRelevant());

  // Will stay on done
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_FALSE(shadow.isInitialized());

  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(matrix, nullptr);
}

INSTANTIATE_TEST_CASE_P(AllRowsFetcherPattern4Instanciated, AllRowsFetcherPattern4Test,
                        testing::Range(static_cast<uint64_t>(0),
                                       static_cast<uint64_t>(std::pow(2, AllRowsFetcherPattern4Test::NrRows -
                                                                             1))));

// Section fifth Pattern, 1 input, 1 relevant, a set of irrelevant shadow rows
// followed by another input and 1 relevant, 1 irrelevant shadow row.

class AllRowsFetcherPattern5Test : public testing::TestWithParam<uint64_t> {
 public:
  static size_t const NrRows = 10;

 protected:
  ResourceMonitor monitor;
  DependencyProxyMock<false> dependencyProxyMock{monitor, 1};
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  void SetUp() override {
    SharedAqlItemBlockPtr baseBlock{new AqlItemBlock(itemBlockManager, NrRows, 1)};
    {  // fill data into the baseBlock
      baseBlock->emplaceValue(0, 0, "a");
      baseBlock->emplaceValue(1, 0, "b");
      baseBlock->setShadowRowDepth(1, AqlValue(AqlValueHintUInt(0ull)));
      baseBlock->emplaceValue(2, 0, "c");
      baseBlock->setShadowRowDepth(2, AqlValue(AqlValueHintUInt(1ull)));
      baseBlock->emplaceValue(3, 0, "d");
      baseBlock->setShadowRowDepth(3, AqlValue(AqlValueHintUInt(2ull)));
      baseBlock->emplaceValue(4, 0, "e");
      baseBlock->setShadowRowDepth(4, AqlValue(AqlValueHintUInt(1ull)));
      baseBlock->emplaceValue(5, 0, "f");
      baseBlock->setShadowRowDepth(5, AqlValue(AqlValueHintUInt(2ull)));
      baseBlock->emplaceValue(6, 0, "g");
      baseBlock->emplaceValue(7, 0, "h");
      baseBlock->setShadowRowDepth(7, AqlValue(AqlValueHintUInt(0ull)));
      baseBlock->emplaceValue(8, 0, "i");
      baseBlock->setShadowRowDepth(8, AqlValue(AqlValueHintUInt(1ull)));
      baseBlock->emplaceValue(9, 0, "j");
      baseBlock->setShadowRowDepth(9, AqlValue(AqlValueHintUInt(2ull)));
    }
    // prepare the proxy
    uint64_t splits = GetParam();
    ASSERT_LE(splits, (std::pow)(2, baseBlock->size() - 1));
    auto toReturn = CutMyBlockIntoPieces(baseBlock, splits);
    dependencyProxyMock.shouldReturn(toReturn);
  }
};

TEST_P(AllRowsFetcherPattern5Test, handle_shadow_rows) {
  // The result should be always identical, it does not matter how the blocks are splitted.
  AllRowsFetcher testee(dependencyProxyMock);
  AqlItemMatrix const* matrix = nullptr;
  ExecutionState state = ExecutionState::HASMORE;
  ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};

  // We start with our single data row
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
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
  EXPECT_TRUE(shadow.isRelevant());

  // Can fetch another irrelevant shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("c"));
  EXPECT_FALSE(shadow.isRelevant());
  EXPECT_EQ(shadow.getDepth(), 1);

  // Can fetch another irrelevant shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("d"));
  EXPECT_FALSE(shadow.isRelevant());
  EXPECT_EQ(shadow.getDepth(), 2);

  // Can fetch another irrelevant shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("e"));
  EXPECT_FALSE(shadow.isRelevant());
  EXPECT_EQ(shadow.getDepth(), 1);

  // Can fetch another irrelevant shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("f"));
  EXPECT_FALSE(shadow.isRelevant());
  EXPECT_EQ(shadow.getDepth(), 2);

  // Will stay on HASMORE
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::HASMORE);
  EXPECT_FALSE(shadow.isInitialized());

  // Now fetch the data block
  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_NE(matrix, nullptr);
  EXPECT_EQ(matrix->size(), 1);
  rowIndexes = matrix->produceRowIndexes();
  ASSERT_EQ(rowIndexes.size(), 1);
  for (auto const& i : rowIndexes) {
    auto row = matrix->getRow(i);
    ASSERT_TRUE(row.isInitialized());
    EXPECT_TRUE(row.getValue(0).slice().isEqualString("g"));
  }

  // Will stay on done
  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(matrix, nullptr);

  // Can fetch shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("h"));
  EXPECT_TRUE(shadow.isRelevant());

  // Can fetch another irrelevant shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("i"));
  EXPECT_FALSE(shadow.isRelevant());
  EXPECT_EQ(shadow.getDepth(), 1);

  // Can fetch another irrelevant shadow row
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(shadow.isInitialized());
  EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("j"));
  EXPECT_FALSE(shadow.isRelevant());
  EXPECT_EQ(shadow.getDepth(), 2);

  // Will stay on done
  std::tie(state, shadow) = testee.fetchShadowRow();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_FALSE(shadow.isInitialized());

  std::tie(state, matrix) = testee.fetchAllRows();
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(matrix, nullptr);
}

INSTANTIATE_TEST_CASE_P(AllRowsFetcherPattern5Instanciated, AllRowsFetcherPattern5Test,
                        testing::Range(static_cast<uint64_t>(0),
                                       static_cast<uint64_t>(std::pow(2, AllRowsFetcherPattern5Test::NrRows -
                                                                             1))));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
