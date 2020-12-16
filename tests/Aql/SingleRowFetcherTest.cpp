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

#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/FilterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StringUtils.h"

#include "FetcherTestHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// TODO check that blocks are not returned to early (e.g. not before the nextDataRow row
//      is fetched)

// TODO check that, for SingleRowFetcher<true>, blocks are reposited (passed through) immediately
//      after they have been fetched

class SingleRowFetcherTestPassBlocks : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  ExecutionState state;
  static constexpr ::arangodb::aql::BlockPassthrough passBlocksThrough =
      ::arangodb::aql::BlockPassthrough::Enable;
  SingleRowFetcherTestPassBlocks()
      : itemBlockManager(monitor, SerializationFormat::SHADOWROWS) {}

  void validateInputRange(AqlItemBlockInputRange& input,
                          std::vector<std::string> const& result) {
    for (auto const& value : result) {
      SCOPED_TRACE("Checking for value: " + value);
      // We need more rows
      ASSERT_TRUE(input.hasDataRow());
      EXPECT_FALSE(input.hasShadowRow());

      auto [state, row] = input.nextDataRow();

      if (value == result.back()) {
        EXPECT_EQ(state, ExecutorState::DONE);
      } else {
        EXPECT_EQ(state, ExecutorState::HASMORE);
      }
      ASSERT_TRUE(row.isInitialized());
      auto const& inputVal = row.getValue(0);
      ASSERT_TRUE(inputVal.isString());
      EXPECT_TRUE(inputVal.slice().isEqualString(value))
          << inputVal.slice().toJson() << " should be equal to \"" << value << "\"";
    }
    // We always fetch to the end
    EXPECT_FALSE(input.hasDataRow());
  }

  void validateShadowRange(AqlItemBlockInputRange& input,
                           std::vector<std::pair<uint64_t, std::string>> const& result) {
    for (auto const& [depth, value] : result) {
      SCOPED_TRACE("Checking for depth " + basics::StringUtils::itoa(depth) +
                   " with value: " + value);
      // We need more rows
      ASSERT_TRUE(input.hasShadowRow());
      EXPECT_FALSE(input.hasDataRow());

      auto [state, row] = input.nextShadowRow();
      EXPECT_EQ(state, ExecutorState::HASMORE);
      ASSERT_TRUE(row.isInitialized());
      auto const& inputVal = row.getValue(0);

      ASSERT_TRUE(inputVal.isString());
      EXPECT_TRUE(inputVal.slice().isEqualString(value))
          << inputVal.slice().toJson() << " should be equal to \"" << value << "\"";
      EXPECT_EQ(row.getDepth(), depth);
    }
  }
};

class SingleRowFetcherTestDoNotPassBlocks : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  ExecutionState state;
  static constexpr ::arangodb::aql::BlockPassthrough passBlocksThrough =
      ::arangodb::aql::BlockPassthrough::Disable;
  SingleRowFetcherTestDoNotPassBlocks()
      : itemBlockManager(monitor, SerializationFormat::SHADOWROWS) {}
};

TEST_F(SingleRowFetcherTestPassBlocks, there_are_no_blocks_upstream_the_producer_doesnt_wait) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};

  dependencyProxyMock.shouldReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 1);
}

TEST_F(SingleRowFetcherTestPassBlocks, there_are_blocks_upstream_the_producer_waits) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};

  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks, there_are_blocks_upstream_the_producer_waits) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};

  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks, handling_of_relevant_shadow_rows) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

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
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
    EXPECT_TRUE(shadow.isRelevant());

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

TEST_F(SingleRowFetcherTestDoNotPassBlocks, handling_of_irrelevant_shadow_rows) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

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

TEST_F(SingleRowFetcherTestDoNotPassBlocks, handling_consecutive_shadowrows) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

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

TEST_F(SingleRowFetcherTestPassBlocks,
       single_upstream_block_with_a_single_row_producer_returns_done_immediately) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 1);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       single_upstream_block_with_a_single_row_producer_returns_done_immediately) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 1);
}

TEST_F(SingleRowFetcherTestPassBlocks,
       single_upstream_block_with_a_single_row_producer_returns_hasmore_then_done_with_a_nullptr) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       single_upstream_block_with_a_single_row_producer_returns_hasmore_then_done_with_a_nullptr) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(SingleRowFetcherTestPassBlocks,
       single_upstream_block_with_a_single_row_producer_waits_then_returns_done) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       single_upstream_block_with_a_single_row_producer_waits_then_returns_done) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block));

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(SingleRowFetcherTestPassBlocks,
       single_upstream_bock_with_a_single_row_producer_waits_returns_hasmore_then_done) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 3);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       single_upstream_bock_with_a_single_row_producer_waits_returns_hasmore_then_done) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);

    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 3);
}

// TODO the following tests should be simplified, a simple output
// specification should be compared with the actual output.

TEST_F(SingleRowFetcherTestPassBlocks, multiple_blocks_upstream_producer_doesnt_wait) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 3);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks, multiple_blocks_upstream_producer_doesnt_wait) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 3);
}

TEST_F(SingleRowFetcherTestPassBlocks, multiple_blocks_upstream_producer_waits) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRow();
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 6);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks, multiple_blocks_upstream_producer_waits) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRow();
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 6);
}

TEST_F(SingleRowFetcherTestPassBlocks,
       multiple_blocks_upstream_producer_waits_and_does_not_return_done) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRow();
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 7);
}

TEST_F(SingleRowFetcherTestDoNotPassBlocks,
       multiple_blocks_upstream_producer_waits_and_does_not_return_done) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRow();
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    std::tie(state, row) = testee.fetchRow();
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 7);
}

TEST_F(SingleRowFetcherTestPassBlocks, handling_of_relevant_shadow_rows) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

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
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(shadow.isInitialized());
    EXPECT_TRUE(shadow.getValue(0).slice().isEqualString("b"));
    EXPECT_TRUE(shadow.isRelevant());

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

TEST_F(SingleRowFetcherTestPassBlocks, handling_of_irrelevant_shadow_rows) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

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

TEST_F(SingleRowFetcherTestPassBlocks, handling_consecutive_shadowrows) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
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
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);

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

TEST_F(SingleRowFetcherTestPassBlocks, handling_shadowrows_in_execute_oneAndDone) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};

  {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 7, 1)};
    block->emplaceValue(0, 0, "a");
    block->emplaceValue(1, 0, "b");
    block->emplaceValue(2, 0, "c");
    block->emplaceValue(3, 0, "d");
    block->emplaceValue(4, 0, "e");  // first shadowrow
    block->setShadowRowDepth(4, AqlValue(AqlValueHintUInt(0ull)));
    block->emplaceValue(5, 0, "f");
    block->setShadowRowDepth(5, AqlValue(AqlValueHintUInt(1ull)));
    block->emplaceValue(6, 0, "g");
    block->setShadowRowDepth(6, AqlValue(AqlValueHintUInt(0ull)));
    dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));
  }

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);
    AqlCall call;
    auto stack = AqlCallStack{AqlCallList{call}};

    // First no data row
    auto [state, skipped, input] = testee.execute(stack);
    EXPECT_EQ(input.getRowIndex(), 0);
    EXPECT_EQ(skipped.getSkipCount(), 0);
    EXPECT_EQ(state, ExecutionState::DONE);
  }  // testee is destroyed here
}

TEST_F(SingleRowFetcherTestPassBlocks, handling_shadowrows_in_execute_twoAndHasMore) {
  RegIdSet inputRegister;
  DependencyProxyMock<passBlocksThrough> dependencyProxyMock{monitor, inputRegister, 0};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  ShadowAqlItemRow shadow{CreateInvalidShadowRowHint{}};

  {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 9, 1)};
    block->emplaceValue(0, 0, "a");
    block->emplaceValue(1, 0, "b");
    block->emplaceValue(2, 0, "c");
    block->emplaceValue(3, 0, "d");
    block->emplaceValue(4, 0, "e");  // first shadowrow
    block->setShadowRowDepth(4, AqlValue(AqlValueHintUInt(0ull)));
    block->emplaceValue(5, 0, "f");
    block->setShadowRowDepth(5, AqlValue(AqlValueHintUInt(1ull)));
    block->emplaceValue(6, 0, "g");
    block->setShadowRowDepth(6, AqlValue(AqlValueHintUInt(0ull)));
    block->emplaceValue(7, 0, "h");
    block->emplaceValue(8, 0, "i");
    dependencyProxyMock.shouldReturn(ExecutionState::DONE, std::move(block));
  }

  {
    SingleRowFetcher<passBlocksThrough> testee(dependencyProxyMock);
    AqlCall call;
    auto stack = AqlCallStack{AqlCallList{call}};

    auto [state, skipped, input] = testee.execute(stack);
    // We only have one block, no more calls to execute necessary
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(skipped.getSkipCount(), 0);
    EXPECT_EQ(input.getRowIndex(), 0);

    // Now validate the input range
    validateInputRange(input, std::vector<std::string>{"a", "b", "c", "d"});
    validateShadowRange(input, std::vector<std::pair<uint64_t, std::string>>{
                                   {0ull, "e"}, {1ull, "f"}});
    validateShadowRange(input, std::vector<std::pair<uint64_t, std::string>>{
                                   {0ull, "g"}});
    validateInputRange(input, std::vector<std::string>{"h", "i"});

  }  // testee is destroyed here
}

class SingleRowFetcherWrapper
    : public fetcherHelper::PatternTestWrapper<SingleRowFetcher<::arangodb::aql::BlockPassthrough::Disable>> {
 public:
  SingleRowFetcherWrapper() : PatternTestWrapper() {}

  void PullAndAssertDataRows(std::vector<std::string> const& dataResults) override {
    InputAqlItemRow row{CreateInvalidInputRowHint{}};
    ExecutionState state = ExecutionState::HASMORE;

    // Fetch all rows until done
    for (auto const& it : dataResults) {
      std::tie(state, row) = _fetcher.fetchRow();
      if (it != dataResults.back()) {
        EXPECT_EQ(state, ExecutionState::HASMORE);
      }
      // We cannot guarantee the DONE case on end, as we potentially need to fetch from upstream
      ASSERT_TRUE(row.isInitialized());
      EXPECT_TRUE(row.getValue(0).slice().isEqualString(it));
    }
    // Now assert that we will forever stay in the DONE state and do not move on.
    std::tie(state, row) = _fetcher.fetchRow();
    EXPECT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row.isInitialized());
  }
};

TEST_SHADOWROW_PATTERN_1(SingleRowFetcherWrapper, SingleRowFetcherPattern1Test);
TEST_SHADOWROW_PATTERN_2(SingleRowFetcherWrapper, SingleRowFetcherPattern2Test);
TEST_SHADOWROW_PATTERN_3(SingleRowFetcherWrapper, SingleRowFetcherPattern3Test);
TEST_SHADOWROW_PATTERN_4(SingleRowFetcherWrapper, SingleRowFetcherPattern4Test);
TEST_SHADOWROW_PATTERN_5(SingleRowFetcherWrapper, SingleRowFetcherPattern5Test);
TEST_SHADOWROW_PATTERN_6(SingleRowFetcherWrapper, SingleRowFetcherPattern6Test);

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
