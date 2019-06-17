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
#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/FilterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// TODO check that blocks are not returned to early (e.g. not before the next row
//      is fetched)

// TODO check that, for SingleRowFetcher<true>, blocks are reposited (passed through) immediately
//      after they have been fetched

class SingleRowFetcherTestPassBlocks : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  ExecutionState state;
  static constexpr bool passBlocksThrough = true;
  SingleRowFetcherTestPassBlocks() : itemBlockManager(&monitor) {}
};

class SingleRowFetcherTestDoNotPassBlocks : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  ExecutionState state;
  static constexpr bool passBlocksThrough = false;
  SingleRowFetcherTestDoNotPassBlocks() : itemBlockManager(&monitor) {}
};

TEST_F(SingleRowFetcherTestPassBlocks, there_are_no_blocks_upstream_the_producer_doesnt_wait) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};

  dependencyProxyMock.shouldReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 1);
}

TEST_F(SingleRowFetcherTestPassBlocks, there_are_blocks_upstream_the_producer_waits) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};

  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks, there_are_blocks_upstream_the_producer_waits) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};

  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(SingleRowFetcherTestPassBlocks,
       single_upstream_block_with_a_single_row_producer_returns_done_immediately) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, MatrixBuilder<1>(42));
  dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 1);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       single_upstream_block_with_a_single_row_producer_returns_done_immediately) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, MatrixBuilder<1>(42));
  dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 1);
}

TEST_F(SingleRowFetcherTestPassBlocks,
       single_upstream_block_with_a_single_row_producer_returns_hasmore_then_done_with_a_nullptr) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, MatrixBuilder<1>(42));
  dependencyProxyMock.shouldReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       single_upstream_block_with_a_single_row_producer_returns_hasmore_then_done_with_a_nullptr) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, MatrixBuilder<1>(42));
  dependencyProxyMock.shouldReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(SingleRowFetcherTestPassBlocks,
       single_upstream_block_with_a_single_row_producer_waits_then_returns_done) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, MatrixBuilder<1>(42));
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       single_upstream_block_with_a_single_row_producer_waits_then_returns_done) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, MatrixBuilder<1>(42));
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(SingleRowFetcherTestPassBlocks,
       single_upstream_bock_with_a_single_row_producer_waits_returns_hasmore_then_done) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, MatrixBuilder<1>(42));
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 3);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       single_upstream_bock_with_a_single_row_producer_waits_returns_hasmore_then_done) {
  VPackBuilder input;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, MatrixBuilder<1>(42));
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 3);
}

// TODO the following tests should be simplified, a simple output
// specification should be compared with the actual output.

TEST_F(SingleRowFetcherTestPassBlocks, multiple_blocks_upstream_producer_doesnt_wait) {
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1 = buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3 = buildBlock<1>(itemBlockManager, {{{6}}});
  dependencyProxyMock.shouldReturn(ExecutionState::HASMORE, std::move(block1))
      .andThenReturn(ExecutionState::HASMORE, std::move(block2))
      .andThenReturn(ExecutionState::DONE, std::move(block3));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    int64_t rowIdxAndValue;
    for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
      std::tie(state, row) = testee.fetchRow();
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 3);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks, multiple_blocks_upstream_producer_doesnt_wait) {
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1 = buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3 = buildBlock<1>(itemBlockManager, {{{6}}});
  dependencyProxyMock.shouldReturn(ExecutionState::HASMORE, std::move(block1))
      .andThenReturn(ExecutionState::HASMORE, std::move(block2))
      .andThenReturn(ExecutionState::DONE, std::move(block3));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    int64_t rowIdxAndValue;
    for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
      std::tie(state, row) = testee.fetchRow();
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 3);
}

TEST_F(SingleRowFetcherTestPassBlocks, multiple_blocks_upstream_producer_waits) {
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    int rowIdxAndValue;
    for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
      if (rowIdxAndValue == 1 || rowIdxAndValue == 4) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRow();
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRow();
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 6);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks, multiple_blocks_upstream_producer_waits) {
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    int rowIdxAndValue;
    for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
      if (rowIdxAndValue == 1 || rowIdxAndValue == 4) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRow();
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRow();
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 6);
}

TEST_F(SingleRowFetcherTestPassBlocks,
       multiple_blocks_upstream_producer_waits_and_does_not_return_done) {
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    for (int rowIdxAndValue = 1; rowIdxAndValue <= 6; rowIdxAndValue++) {
      if (rowIdxAndValue == 1 || rowIdxAndValue == 4 || rowIdxAndValue == 6) {
        // wait at the beginning of the 1st, 2nd and 3rd block
        std::tie(state, row) = testee.fetchRow();
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRow();
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 7);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       multiple_blocks_upstream_producer_waits_and_does_not_return_done) {
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    for (int rowIdxAndValue = 1; rowIdxAndValue <= 6; rowIdxAndValue++) {
      if (rowIdxAndValue == 1 || rowIdxAndValue == 4 || rowIdxAndValue == 6) {
        // wait at the beginning of the 1st, 2nd and 3rd block
        std::tie(state, row) = testee.fetchRow();
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRow();
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    std::tie(state, row) = testee.fetchRow();
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 7);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
