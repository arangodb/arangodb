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

#include "AqlExecutorTestCase.h"
#include "AqlItemBlockHelper.h"
#include "AqlItemRowPrinter.h"
#include "DependencyProxyMock.h"
#include "ExecutorTestHelper.h"
#include "MultiDepFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/ResourceUsage.h"
#include "Aql/ShadowAqlItemRow.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class MultiDependencySingleRowFetcherTest
    : public AqlExecutorTestCaseWithParam<std::tuple<bool, size_t>> {
 protected:
  auto doesWait() -> bool {
    auto const [wait, deps] = GetParam();
    return wait;
  }

  auto numberDependencies() -> size_t {
    auto const [wait, deps] = GetParam();
    return deps;
  }

  // This will create inputData.size() many dependecies.
  // Each will be initialized with the given deque of blocks
  // Note: Caller needs to make sure that ShadowRows are present in correct
  // order and correct amount in all deques
  auto buildFetcher(std::vector<std::deque<SharedAqlItemBlockPtr>> inputData)
      -> MultiDependencySingleRowFetcher {
    // We need at least 1 dependency!
    TRI_ASSERT(!inputData.empty());
    WaitingExecutionBlockMock::WaitingBehaviour waiting =
        doesWait() ? WaitingExecutionBlockMock::WaitingBehaviour::ONCE
                   : WaitingExecutionBlockMock::WaitingBehaviour::NEVER;
    for (auto blockDeque : inputData) {
      auto dep = std::make_unique<WaitingExecutionBlockMock>(fakedQuery->engine(),
                                                             generateNodeDummy(),
                                                             std::move(blockDeque), waiting);
      _dependencies.emplace_back(dep.get());
      _blocks.emplace_back(std::move(dep));
    }

    MultiDependencySingleRowFetcher testee{_proxy};
    testee.init();
    return testee;
  }

  auto makeStack() -> AqlCallStack {
    // We need a stack for the API, an empty one will do.
    // We are not testing subqueries here
    AqlCallStack stack{AqlCall{}};
    stack.popCall();
    return stack;
  }

  auto makeSameCallToAllDependencies(AqlCall call) -> AqlCallSet {
    AqlCallSet set{};
    for (size_t i = 0; i < _dependencies.size(); ++i) {
      set.calls.emplace_back(AqlCallSet::DepCallPair{i, call});
    }
    return set;
  }

  auto testWaiting(MultiDependencySingleRowFetcher& testee, AqlCallSet const& set) {
    if (doesWait()) {
      auto stack = makeStack();
      auto [state, skipped, ranges] = testee.execute(stack, set);
      EXPECT_EQ(state, ExecutionState::WAITING);
      EXPECT_TRUE(skipped.nothingSkipped());
      EXPECT_EQ(ranges.size(), set.size());
      for (auto const& [dep, range] : ranges) {
        EXPECT_FALSE(range.hasDataRow());
        EXPECT_FALSE(range.hasShadowRow());
        EXPECT_EQ(range.upstreamState(), ExecutorState::HASMORE);
      }
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
      auto [state, row] = testee.peekShadowRowAndState();
      EXPECT_EQ(state, ExecutorState::DONE);
      EXPECT_FALSE(row.isInitialized());
      ASSERT_EQ(rowIndexBefore, testee.getRowIndex())
          << "Skipped a non processed row.";
    }
    {
      auto [state, row] = testee.nextShadowRow();
      EXPECT_EQ(state, ExecutorState::DONE);
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
      auto [state, row] = testee.peekShadowRowAndState();
      EXPECT_EQ(state, expectedState);
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

 private:
  // This is memory responsibility for the blocks only
  std::vector<std::unique_ptr<ExecutionBlock>> _blocks;
  // The dependencies, they are referenced by _proxy, modifing this will modify the proxy
  std::vector<ExecutionBlock*> _dependencies{};
  DependencyProxy<BlockPassthrough::Disable> _proxy{_dependencies, itemBlockManager,
                                                    make_shared_unordered_set({0}),
                                                    1, nullptr};
};

INSTANTIATE_TEST_CASE_P(MultiDependencySingleRowFetcherTest, MultiDependencySingleRowFetcherTest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Range(static_cast<size_t>(1),
                                                            static_cast<size_t>(4))));

TEST_P(MultiDependencySingleRowFetcherTest, no_blocks_upstream) {
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (size_t i = 0; i < numberDependencies(); ++i) {
    data.emplace_back(std::deque<SharedAqlItemBlockPtr>{});
  }

  auto testee = buildFetcher(data);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  testWaiting(testee, set);

  auto stack = makeStack();
  auto [state, skipped, ranges] = testee.execute(stack, set);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_TRUE(skipped.nothingSkipped());
  EXPECT_EQ(ranges.size(), set.size());
  for (auto const& [dep, range] : ranges) {
    // All Ranges are empty
    EXPECT_FALSE(range.hasDataRow());
    EXPECT_FALSE(range.hasShadowRow());
    EXPECT_EQ(range.upstreamState(), ExecutorState::DONE);
  }
}

TEST_P(MultiDependencySingleRowFetcherTest, one_block_upstream_all_deps_equal) {
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (size_t i = 0; i < numberDependencies(); ++i) {
    std::deque<SharedAqlItemBlockPtr> blockDeque{};
    blockDeque.emplace_back(buildBlock<1>(itemBlockManager, {{1}, {2}, {3}}));
    data.emplace_back(std::move(blockDeque));
  }

  auto testee = buildFetcher(data);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  testWaiting(testee, set);

  auto stack = makeStack();
  auto [state, skipped, ranges] = testee.execute(stack, set);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_TRUE(skipped.nothingSkipped());
  EXPECT_EQ(ranges.size(), set.size());
  for (auto [dep, range] : ranges) {
    // All Ranges are non empty
    validateNextIsDataRow(range, ExecutorState::HASMORE, 1);
    validateNextIsDataRow(range, ExecutorState::HASMORE, 2);
    validateNextIsDataRow(range, ExecutorState::DONE, 3);
  }
}

TEST_P(MultiDependencySingleRowFetcherTest, one_block_upstream_all_deps_differ) {
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (size_t i = 0; i < numberDependencies(); ++i) {
    std::deque<SharedAqlItemBlockPtr> blockDeque{};
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{1 * (i + 1)}, {2 * (i + 1)}, {3 * (i + 1)}}));
    data.emplace_back(std::move(blockDeque));
  }

  auto testee = buildFetcher(data);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  testWaiting(testee, set);

  auto stack = makeStack();
  auto [state, skipped, ranges] = testee.execute(stack, set);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_TRUE(skipped.nothingSkipped());
  EXPECT_EQ(ranges.size(), set.size());
  for (auto [dep, range] : ranges) {
    // All Ranges are non empty
    validateNextIsDataRow(range, ExecutorState::HASMORE, 1 * (dep + 1));
    validateNextIsDataRow(range, ExecutorState::HASMORE, 2 * (dep + 1));
    validateNextIsDataRow(range, ExecutorState::DONE, 3 * (dep + 1));
  }
}

TEST_P(MultiDependencySingleRowFetcherTest, many_blocks_upstream_all_deps_equal) {
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (size_t i = 0; i < numberDependencies(); ++i) {
    std::deque<SharedAqlItemBlockPtr> blockDeque{};
    blockDeque.emplace_back(buildBlock<1>(itemBlockManager, {{1}, {2}, {3}}));
    blockDeque.emplace_back(buildBlock<1>(itemBlockManager, {{4}, {5}, {6}}));
    blockDeque.emplace_back(buildBlock<1>(itemBlockManager, {{7}, {8}, {9}}));
    data.emplace_back(std::move(blockDeque));
  }

  auto testee = buildFetcher(data);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  testWaiting(testee, set);

  auto stack = makeStack();
  {
    // First Block
    auto [state, skipped, ranges] = testee.execute(stack, set);
    EXPECT_EQ(state, ExecutionState::HASMORE);
    EXPECT_TRUE(skipped.nothingSkipped());
    EXPECT_EQ(ranges.size(), set.size());
    for (auto [dep, range] : ranges) {
      // All Ranges are non empty
      validateNextIsDataRow(range, ExecutorState::HASMORE, 1);
      validateNextIsDataRow(range, ExecutorState::HASMORE, 2);
      validateNextIsDataRow(range, ExecutorState::HASMORE, 3);
    }
  }

  {
    // Second Block
    auto [state, skipped, ranges] = testee.execute(stack, set);
    EXPECT_EQ(state, ExecutionState::HASMORE);
    EXPECT_TRUE(skipped.nothingSkipped());
    EXPECT_EQ(ranges.size(), set.size());
    for (auto [dep, range] : ranges) {
      // All Ranges are non empty
      validateNextIsDataRow(range, ExecutorState::HASMORE, 4);
      validateNextIsDataRow(range, ExecutorState::HASMORE, 5);
      validateNextIsDataRow(range, ExecutorState::HASMORE, 6);
    }
  }

  {
    // Third Block
    auto [state, skipped, ranges] = testee.execute(stack, set);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_TRUE(skipped.nothingSkipped());
    EXPECT_EQ(ranges.size(), set.size());
    for (auto [dep, range] : ranges) {
      // All Ranges are non empty
      validateNextIsDataRow(range, ExecutorState::HASMORE, 7);
      validateNextIsDataRow(range, ExecutorState::HASMORE, 8);
      validateNextIsDataRow(range, ExecutorState::DONE, 9);
    }
  }
}

TEST_P(MultiDependencySingleRowFetcherTest, many_blocks_upstream_all_deps_differ) {
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (size_t i = 0; i < numberDependencies(); ++i) {
    std::deque<SharedAqlItemBlockPtr> blockDeque{};
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{1 * (i + 1)}, {2 * (i + 1)}, {3 * (i + 1)}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{4 * (i + 1)}, {5 * (i + 1)}, {6 * (i + 1)}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{7 * (i + 1)}, {8 * (i + 1)}, {9 * (i + 1)}}));
    data.emplace_back(std::move(blockDeque));
  }

  auto testee = buildFetcher(data);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  testWaiting(testee, set);

  auto stack = makeStack();
  {
    // First Block
    auto [state, skipped, ranges] = testee.execute(stack, set);
    EXPECT_EQ(state, ExecutionState::HASMORE);
    EXPECT_TRUE(skipped.nothingSkipped());
    EXPECT_EQ(ranges.size(), set.size());
    for (auto [dep, range] : ranges) {
      // All Ranges are non empty
      validateNextIsDataRow(range, ExecutorState::HASMORE, 1 * (dep + 1));
      validateNextIsDataRow(range, ExecutorState::HASMORE, 2 * (dep + 1));
      validateNextIsDataRow(range, ExecutorState::HASMORE, 3 * (dep + 1));
    }
  }

  {
    // Second Block
    auto [state, skipped, ranges] = testee.execute(stack, set);
    EXPECT_EQ(state, ExecutionState::HASMORE);
    EXPECT_TRUE(skipped.nothingSkipped());
    EXPECT_EQ(ranges.size(), set.size());
    for (auto [dep, range] : ranges) {
      // All Ranges are non empty
      validateNextIsDataRow(range, ExecutorState::HASMORE, 4 * (dep + 1));
      validateNextIsDataRow(range, ExecutorState::HASMORE, 5 * (dep + 1));
      validateNextIsDataRow(range, ExecutorState::HASMORE, 6 * (dep + 1));
    }
  }

  {
    // Third Block
    auto [state, skipped, ranges] = testee.execute(stack, set);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_TRUE(skipped.nothingSkipped());
    EXPECT_EQ(ranges.size(), set.size());
    for (auto [dep, range] : ranges) {
      // All Ranges are non empty
      validateNextIsDataRow(range, ExecutorState::HASMORE, 7 * (dep + 1));
      validateNextIsDataRow(range, ExecutorState::HASMORE, 8 * (dep + 1));
      validateNextIsDataRow(range, ExecutorState::DONE, 9 * (dep + 1));
    }
  }
}

TEST_P(MultiDependencySingleRowFetcherTest, many_blocks_upstream_all_deps_differ_sequentially) {
  // Difference to the test above is that we first fetch all from Dep1, then Dep2 then Dep3
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (size_t i = 0; i < numberDependencies(); ++i) {
    std::deque<SharedAqlItemBlockPtr> blockDeque{};
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{1 * (i + 1)}, {2 * (i + 1)}, {3 * (i + 1)}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{4 * (i + 1)}, {5 * (i + 1)}, {6 * (i + 1)}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{7 * (i + 1)}, {8 * (i + 1)}, {9 * (i + 1)}}));
    data.emplace_back(std::move(blockDeque));
  }

  auto testee = buildFetcher(data);
  auto stack = makeStack();
  for (size_t dep = 0; dep < numberDependencies(); ++dep) {
    AqlCallSet set{};
    set.calls.emplace_back(AqlCallSet::DepCallPair{dep, AqlCall{}});
    testWaiting(testee, set);

    {
      // First Block
      auto [state, skipped, ranges] = testee.execute(stack, set);
      EXPECT_EQ(state, ExecutionState::HASMORE);
      EXPECT_TRUE(skipped.nothingSkipped());
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are non empty
        validateNextIsDataRow(range, ExecutorState::HASMORE, 1 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::HASMORE, 2 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::HASMORE, 3 * (dep + 1));
      }
    }

    {
      // Second Block
      auto [state, skipped, ranges] = testee.execute(stack, set);
      EXPECT_EQ(state, ExecutionState::HASMORE);
      EXPECT_TRUE(skipped.nothingSkipped());
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are non empty
        validateNextIsDataRow(range, ExecutorState::HASMORE, 4 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::HASMORE, 5 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::HASMORE, 6 * (dep + 1));
      }
    }

    {
      // Third Block
      auto [state, skipped, ranges] = testee.execute(stack, set);
      if (dep + 1 == numberDependencies()) {
        // Only the last dependency reports a global DONE
        EXPECT_EQ(state, ExecutionState::DONE);
      } else {
        // All others still report HASMORE on the other parts
        EXPECT_EQ(state, ExecutionState::HASMORE);
      }

      EXPECT_TRUE(skipped.nothingSkipped());
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are non empty
        validateNextIsDataRow(range, ExecutorState::HASMORE, 7 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::HASMORE, 8 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::DONE, 9 * (dep + 1));
      }
    }
  }
}

TEST_P(MultiDependencySingleRowFetcherTest,
       many_blocks_upstream_all_deps_differ_sequentially_using_shadowRows_no_callList) {
  // NOTE: The fetcher does NOT care to snychronize the shadowRows between blocks.
  // This has to be done by the InputRange hold externally.
  // It has seperate tests
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (size_t i = 0; i < numberDependencies(); ++i) {
    std::deque<SharedAqlItemBlockPtr> blockDeque{};
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager,
                      {{1 * (i + 1)}, {2 * (i + 1)}, {0}, {3 * (i + 1)}}, {{2, 0}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager,
                      {{4 * (i + 1)}, {5 * (i + 1)}, {1}, {2}, {6 * (i + 1)}},
                      {{2, 0}, {3, 1}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{7 * (i + 1)}, {8 * (i + 1)}, {9 * (i + 1)}}));
    data.emplace_back(std::move(blockDeque));
  }

  auto testee = buildFetcher(data);
  auto stack = makeStack();
  for (size_t dep = 0; dep < numberDependencies(); ++dep) {
    AqlCallSet set{};
    set.calls.emplace_back(AqlCallSet::DepCallPair{dep, AqlCall{}});
    testWaiting(testee, set);

    {
      // First Block, split after shadowRow, we cannot overfetch
      auto [state, skipped, ranges] = testee.execute(stack, set);
      EXPECT_EQ(state, ExecutionState::HASMORE);
      EXPECT_TRUE(skipped.nothingSkipped());
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are non empty
        validateNextIsDataRow(range, ExecutorState::HASMORE, 1 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::DONE, 2 * (dep + 1));
        validateNextIsShadowRow(range, ExecutorState::HASMORE, 0, 0);
      }
    }

    {
      // First Block, part 2
      auto [state, skipped, ranges] = testee.execute(stack, set);
      EXPECT_EQ(state, ExecutionState::HASMORE);
      EXPECT_TRUE(skipped.nothingSkipped());
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        validateNextIsDataRow(range, ExecutorState::HASMORE, 3 * (dep + 1));
      }
    }

    {
      // Second Block, split after the higher depth shadow row
      auto [state, skipped, ranges] = testee.execute(stack, set);
      EXPECT_EQ(state, ExecutionState::HASMORE);
      EXPECT_TRUE(skipped.nothingSkipped());
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are non empty
        validateNextIsDataRow(range, ExecutorState::HASMORE, 4 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::DONE, 5 * (dep + 1));
        validateNextIsShadowRow(range, ExecutorState::HASMORE, 1, 0);
        validateNextIsShadowRow(range, ExecutorState::HASMORE, 2, 1);
      }
    }

    {
      // Second Block, part 2
      auto [state, skipped, ranges] = testee.execute(stack, set);
      EXPECT_EQ(state, ExecutionState::HASMORE);
      EXPECT_TRUE(skipped.nothingSkipped());
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are non empty
        validateNextIsDataRow(range, ExecutorState::HASMORE, 6 * (dep + 1));
      }
    }

    {
      // Third Block
      auto [state, skipped, ranges] = testee.execute(stack, set);
      if (dep + 1 == numberDependencies()) {
        // Only the last dependency reports a global DONE
        EXPECT_EQ(state, ExecutionState::DONE);
      } else {
        // All others still report HASMORE on the other parts
        EXPECT_EQ(state, ExecutionState::HASMORE);
      }

      EXPECT_TRUE(skipped.nothingSkipped());
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are non empty
        validateNextIsDataRow(range, ExecutorState::HASMORE, 7 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::HASMORE, 8 * (dep + 1));
        validateNextIsDataRow(range, ExecutorState::DONE, 9 * (dep + 1));
      }
    }
  }
}

using CutAt = uint64_t;

class MultiDependencySingleRowFetcherShadowRowTest
    : public AqlExecutorTestCaseWithParam<CutAt, true> {
 protected:
  MultiDependencySingleRowFetcherShadowRowTest() {}

  uint64_t cutAt() const { return GetParam(); }  // namespace aql

  auto blockAlternatingDataAndShadowRows(std::vector<int> const& values)
      -> std::unique_ptr<ExecutionBlock> {
    std::deque<SharedAqlItemBlockPtr> blockDeque;
    std::set<size_t> splits{};
    if (cutAt() != 0) {
      splits.emplace(cutAt());
    }
    {
      // Build the result
      MatrixBuilder<1> matrixBuilder;
      for (auto const& val : values) {
        matrixBuilder.emplace_back(RowBuilder<1>{{val}});
      }
      SharedAqlItemBlockPtr block =
          buildBlock<1>(itemBlockManager, std::move(matrixBuilder));

      for (size_t row = 0; row < block->size(); ++row) {
        if (row % 2 == 1) {
          block->setShadowRowDepth(row, AqlValue{AqlValueHintUInt{0}});
          size_t splitAt = row + 1;
          if (splits.find(splitAt) == splits.end()) {
            splits.emplace(splitAt);
          }
        }
      }
      size_t lastSplit = 0;

      for (auto const s : splits) {
        if (s >= block->size()) {
          SharedAqlItemBlockPtr block1 = block->slice(lastSplit, block->size());
          blockDeque.emplace_back(block1);
          break;
        } else {
          SharedAqlItemBlockPtr block1 = block->slice(lastSplit, s);
          lastSplit = s;
          blockDeque.emplace_back(block1);
        }
      }
    }

    auto execBlock = std::make_unique<WaitingExecutionBlockMock>(
        fakedQuery->engine(), generateNodeDummy(), std::move(blockDeque),
        WaitingExecutionBlockMock::WaitingBehaviour::NEVER);
    return execBlock;
  }

  std::vector<std::pair<ExecutionState, SharedAqlItemBlockPtr>> alternatingDataAndShadowRows(
      std::vector<int> const& values) {
    MatrixBuilder<1> matrixBuilder;
    for (auto const& val : values) {
      matrixBuilder.emplace_back(RowBuilder<1>{{val}});
    }
    SharedAqlItemBlockPtr block =
        buildBlock<1>(itemBlockManager, std::move(matrixBuilder));

    for (size_t row = 0; row < block->size(); ++row) {
      if (row % 2 == 1) {
        block->setShadowRowDepth(row, AqlValue{AqlValueHintUInt{0}});
      }
    }

    std::vector<std::pair<ExecutionState, SharedAqlItemBlockPtr>> result;

    if (cutAt() != 0 && cutAt() < block->size()) {
      SharedAqlItemBlockPtr block1 = block->slice(0, cutAt());
      SharedAqlItemBlockPtr block2 = block->slice(cutAt(), block->size());
      result.emplace_back(ExecutionState::HASMORE, block1);
      result.emplace_back(ExecutionState::DONE, block2);
    } else {
      result.emplace_back(ExecutionState::DONE, block);
    }

    return result;
  }

  std::vector<std::pair<ExecutionState, SharedAqlItemBlockPtr>> onlyShadowRows(
      std::vector<int> const& values) {
    MatrixBuilder<1> matrixBuilder;
    for (auto const& val : values) {
      matrixBuilder.emplace_back(RowBuilder<1>{{val}});
    }
    SharedAqlItemBlockPtr block =
        buildBlock<1>(itemBlockManager, std::move(matrixBuilder));

    for (size_t row = 0; row < block->size(); ++row) {
      block->setShadowRowDepth(1, AqlValue{AqlValueHintUInt{0}});
    }

    // TODO cut block into pieces
    return {{ExecutionState::DONE, std::move(block)}};
  }

  // Get a row pointing to a row with the specified value in the only register
  // in an anonymous block.
  InputAqlItemRow inputRow(int value) {
    SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{{value}}});
    return InputAqlItemRow{block, 0};
  }
  // Get a shadow row pointing to a row with the specified value, and
  // specified shadow row depth, in the only register in an anonymous block.
  ShadowAqlItemRow shadowRow(int value, uint64_t depth = 0) {
    SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{{value}}});
    block->setShadowRowDepth(0, AqlValue{AqlValueHintUInt{depth}});
    return ShadowAqlItemRow{block, 0};
  }

  InputAqlItemRow invalidInputRow() const {
    return InputAqlItemRow{CreateInvalidInputRowHint{}};
  }

  ShadowAqlItemRow invalidShadowRow() const {
    return ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
  }
};  // namespace tests

INSTANTIATE_TEST_CASE_P(MultiDependencySingleRowFetcherShadowRowTestInstance,
                        MultiDependencySingleRowFetcherShadowRowTest,
                        testing::Range(static_cast<uint64_t>(0), static_cast<uint64_t>(4)));

TEST_P(MultiDependencySingleRowFetcherShadowRowTest, simple_fetch_shadow_row_test) {
  auto waitingBlock = blockAlternatingDataAndShadowRows({0, 1, 2, 3});
  auto inputRegs = make_shared_unordered_set({0});
  std::vector<ExecutionBlock*> deps{waitingBlock.get()};
  DependencyProxy<BlockPassthrough::Disable> dependencyProxy{deps, itemBlockManager,
                                                             inputRegs, 1, nullptr};

  MultiDependencySingleRowFetcher testee{dependencyProxy};
  testee.initDependencies();

  auto ioPairs = std::vector<FetcherIOPair>{};
  auto add = [&ioPairs](auto call, auto result) {
    ioPairs.emplace_back(std::make_pair(call, result));
  };

  if (cutAt() == 1) {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(0)});
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(0)});
  }
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(1)});
  if (cutAt() == 3) {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(2)});
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(2)});
  }
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(3)});
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::DONE, invalidShadowRow()});

  runFetcher(testee, ioPairs);
}

TEST_P(MultiDependencySingleRowFetcherShadowRowTest, fetch_shadow_rows_2_deps) {
  auto waitingBlockFirst = blockAlternatingDataAndShadowRows({0, 1, 2, 3});
  auto waitingBlockSecond = blockAlternatingDataAndShadowRows({4, 1, 6, 3});
  auto inputRegs = make_shared_unordered_set({0});
  std::vector<ExecutionBlock*> deps{waitingBlockFirst.get(), waitingBlockSecond.get()};
  DependencyProxy<BlockPassthrough::Disable> dependencyProxy{deps, itemBlockManager,
                                                             inputRegs, 1, nullptr};

  MultiDependencySingleRowFetcher testee{dependencyProxy};
  testee.initDependencies();

  auto ioPairs = std::vector<FetcherIOPair>{};
  auto add = [&ioPairs](auto call, auto result) {
    ioPairs.emplace_back(std::make_pair(call, result));
  };

  // fetch dep 1
  if (cutAt() == 1) {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(0)});
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(0)});
  }
  // dep 1 should stay done
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // fetching the shadow row should not yet be possible
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()});
  // dep 1 should stay done
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // fetch dep 2
  if (cutAt() == 1) {
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(4)});
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(4)});
  }
  // dep 2 should stay done
  add(FetchRowForDependency{1, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // dep 1 should stay done
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // Fetch the first shadow row.
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(1)});
  // fetch dep 1 again
  if (cutAt() == 3) {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(2)});
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(2)});
  }
  // dep 1 should stay done
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // fetching the shadow row should not yet be possible
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()});
  // dep 1 should stay done
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // fetch dep 2 again
  if (cutAt() == 3) {
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(6)});
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(6)});
  }
  // dep 2 should stay done
  add(FetchRowForDependency{1, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // dep 1 should stay done
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // Fetch the second shadow row.
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(3)});
  // We're now done.
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::DONE, invalidShadowRow()});

  runFetcher(testee, ioPairs);
}

TEST_P(MultiDependencySingleRowFetcherShadowRowTest, fetch_shadow_rows_2_deps_reverse_pull) {
  auto waitingBlockFirst = blockAlternatingDataAndShadowRows({0, 1, 2, 3});
  auto waitingBlockSecond = blockAlternatingDataAndShadowRows({4, 1, 6, 3});
  auto inputRegs = make_shared_unordered_set({0});
  std::vector<ExecutionBlock*> deps{waitingBlockFirst.get(), waitingBlockSecond.get()};
  DependencyProxy<BlockPassthrough::Disable> dependencyProxy{deps, itemBlockManager,
                                                             inputRegs, 1, nullptr};

  MultiDependencySingleRowFetcher testee{dependencyProxy};
  testee.initDependencies();

  auto ioPairs = std::vector<FetcherIOPair>{};
  auto add = [&ioPairs](auto call, auto result) {
    ioPairs.emplace_back(std::make_pair(call, result));
  };

  // fetch dep 2
  if (cutAt() == 1) {
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(4)});
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(4)});
  }
  // dep 2 should stay done
  add(FetchRowForDependency{1, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // fetching the shadow row should not yet be possible
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()});
  // dep 2 should stay done
  add(FetchRowForDependency{1, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // fetch dep 1
  if (cutAt() == 1) {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(0)});
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(0)});
  }
  // dep 1 should stay done
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // dep 2 should stay done
  add(FetchRowForDependency{1, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // Fetch the first shadow row.
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(1)});
  // fetch dep 2 again
  if (cutAt() == 3) {
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(6)});
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{1, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(6)});
  }
  // dep 2 should stay done
  add(FetchRowForDependency{1, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // fetching the shadow row should not yet be possible
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()});
  // dep 2 should stay done
  add(FetchRowForDependency{1, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // fetch dep 1 again
  if (cutAt() == 3) {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::HASMORE, inputRow(2)});
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  } else {
    add(FetchRowForDependency{0, 1000},
        FetchRowForDependency::Result{ExecutionState::DONE, inputRow(2)});
  }
  // dep 2 should stay done
  add(FetchRowForDependency{1, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // dep 1 should stay done
  add(FetchRowForDependency{0, 1000},
      FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()});
  // Fetch the second shadow row.
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(3)});
  // We're now done.
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::DONE, invalidShadowRow()});

  runFetcher(testee, ioPairs);
}

TEST_P(MultiDependencySingleRowFetcherShadowRowTest, simple_skip_shadow_row_test) {
  auto waitingBlock = blockAlternatingDataAndShadowRows({0, 1, 2, 3});
  auto inputRegs = make_shared_unordered_set({0});
  std::vector<ExecutionBlock*> deps{waitingBlock.get()};
  DependencyProxy<BlockPassthrough::Disable> dependencyProxy{deps, itemBlockManager,
                                                             inputRegs, 1, nullptr};

  MultiDependencySingleRowFetcher testee{dependencyProxy};
  testee.initDependencies();

  auto ioPairs = std::vector<FetcherIOPair>{};
  auto add = [&ioPairs](auto call, auto result) {
    ioPairs.emplace_back(std::make_pair(call, result));
  };

  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::HASMORE, 1});
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::HASMORE, 0});
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(1)});
  if (cutAt() == 3) {
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::HASMORE, 1});
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  } else {
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 1});
  }
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(3)});
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::DONE, invalidShadowRow()});

  runFetcher(testee, ioPairs);
}

TEST_P(MultiDependencySingleRowFetcherShadowRowTest, skip_shadow_rows_2_deps) {
  auto waitingBlockFirst = blockAlternatingDataAndShadowRows({0, 1, 2, 3});
  auto waitingBlockSecond = blockAlternatingDataAndShadowRows({4, 1, 6, 3});
  auto inputRegs = make_shared_unordered_set({0});
  std::vector<ExecutionBlock*> deps{waitingBlockFirst.get(), waitingBlockSecond.get()};
  DependencyProxy<BlockPassthrough::Disable> dependencyProxy{deps, itemBlockManager,
                                                             inputRegs, 1, nullptr};

  MultiDependencySingleRowFetcher testee{dependencyProxy};
  testee.initDependencies();

  auto ioPairs = std::vector<FetcherIOPair>{};
  auto add = [&ioPairs](auto call, auto result) {
    ioPairs.emplace_back(std::make_pair(call, result));
  };

  // fetch dep 1
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 1});
  // dep 1 should stay done
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // fetching the shadow row should not yet be possible
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()});
  // dep 1 should stay done
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // fetch dep 2
  if (cutAt() == 1) {
    add(SkipRowsForDependency{1, 1000},
        SkipRowsForDependency::Result{ExecutionState::HASMORE, 1});
    add(SkipRowsForDependency{1, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  } else {
    add(SkipRowsForDependency{1, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 1});
  }
  // dep 2 should stay done
  add(SkipRowsForDependency{1, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // dep 1 should stay done
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // Fetch the first shadow row.
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(1)});
  // fetch dep 1 again
  if (cutAt() == 3) {
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::HASMORE, 1});
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  } else {
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 1});
  }
  // dep 1 should stay done
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // fetching the shadow row should not yet be possible
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()});
  // dep 1 should stay done
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // fetch dep 2 again
  if (cutAt() == 3) {
    add(SkipRowsForDependency{1, 1000},
        SkipRowsForDependency::Result{ExecutionState::HASMORE, 1});
    add(SkipRowsForDependency{1, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  } else {
    add(SkipRowsForDependency{1, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 1});
  }
  // dep 2 should stay done
  add(SkipRowsForDependency{1, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // dep 1 should stay done
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // Fetch the second shadow row.
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(3)});
  // We're now done.
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::DONE, invalidShadowRow()});

  runFetcher(testee, ioPairs);
}

TEST_P(MultiDependencySingleRowFetcherShadowRowTest, skip_shadow_rows_2_deps_reverse_pull) {
  auto waitingBlockFirst = blockAlternatingDataAndShadowRows({0, 1, 2, 3});
  auto waitingBlockSecond = blockAlternatingDataAndShadowRows({4, 1, 6, 3});
  auto inputRegs = make_shared_unordered_set({0});
  std::vector<ExecutionBlock*> deps{waitingBlockFirst.get(), waitingBlockSecond.get()};
  DependencyProxy<BlockPassthrough::Disable> dependencyProxy{deps, itemBlockManager,
                                                             inputRegs, 1, nullptr};

  MultiDependencySingleRowFetcher testee{dependencyProxy};
  testee.initDependencies();

  auto ioPairs = std::vector<FetcherIOPair>{};
  auto add = [&ioPairs](auto call, auto result) {
    ioPairs.emplace_back(std::make_pair(call, result));
  };

  // fetch dep 2
  add(SkipRowsForDependency{1, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 1});
  // dep 2 should stay done
  add(SkipRowsForDependency{1, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // fetching the shadow row should not yet be possible
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()});
  // dep 2 should stay done
  add(SkipRowsForDependency{1, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // fetch dep 1
  if (cutAt() == 1) {
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::HASMORE, 1});
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  } else {
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 1});
  }
  // dep 1 should stay done
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // dep 2 should stay done
  add(SkipRowsForDependency{1, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // Fetch the first shadow row.
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(1)});
  // fetch dep 2 again
  if (cutAt() == 3) {
    add(SkipRowsForDependency{1, 1000},
        SkipRowsForDependency::Result{ExecutionState::HASMORE, 1});
    add(SkipRowsForDependency{1, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  } else {
    add(SkipRowsForDependency{1, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 1});
  }
  // dep 2 should stay done
  add(SkipRowsForDependency{1, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // fetching the shadow row should not yet be possible
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()});
  // dep 2 should stay done
  add(SkipRowsForDependency{1, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // fetch dep 1 again
  if (cutAt() == 3) {
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::HASMORE, 1});
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  } else {
    add(SkipRowsForDependency{0, 1000},
        SkipRowsForDependency::Result{ExecutionState::DONE, 1});
  }
  // dep 2 should stay done
  add(SkipRowsForDependency{1, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // dep 1 should stay done
  add(SkipRowsForDependency{0, 1000},
      SkipRowsForDependency::Result{ExecutionState::DONE, 0});
  // Fetch the second shadow row.
  add(FetchShadowRow{1000}, FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(3)});
  // We're now done.
  add(FetchShadowRow{1000},
      FetchShadowRow::Result{ExecutionState::DONE, invalidShadowRow()});

  runFetcher(testee, ioPairs);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
