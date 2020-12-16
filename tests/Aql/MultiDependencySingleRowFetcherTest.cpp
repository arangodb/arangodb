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
#include "gtest/gtest.h"

#include "Aql/ExecutionBlock.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/RegisterInfos.h"
#include "Aql/ShadowAqlItemRow.h"
#include "Basics/ResourceUsage.h"

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
  auto buildFetcher(std::vector<std::deque<SharedAqlItemBlockPtr>> inputData,
                    size_t subqueryDepth) -> MultiDependencySingleRowFetcher {
    // We need at least 1 dependency!
    TRI_ASSERT(!inputData.empty());
    WaitingExecutionBlockMock::WaitingBehaviour waiting =
        doesWait() ? WaitingExecutionBlockMock::WaitingBehaviour::ONCE
                   : WaitingExecutionBlockMock::WaitingBehaviour::NEVER;
    for (auto blockDeque : inputData) {
      auto dep = std::make_unique<WaitingExecutionBlockMock>(fakedQuery->rootEngine(),
                                                             generateNodeDummy(),
                                                             std::move(blockDeque),
                                                             waiting, subqueryDepth);
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
    AqlCallStack stack{AqlCallList{AqlCall{}}};
    stack.popCall();
    return stack;
  }

  auto makeSameCallToAllDependencies(AqlCall call) -> AqlCallSet {
    AqlCallSet set{};
    for (size_t i = 0; i < _dependencies.size(); ++i) {
      set.calls.emplace_back(AqlCallSet::DepCallPair{i, AqlCallList{call}});
    }
    return set;
  }

  auto testWaiting(MultiDependencySingleRowFetcher& testee, AqlCallStack stack,
                   AqlCallSet const& set) {
    if (doesWait()) {
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
      auto row = testee.peekShadowRow();
      EXPECT_FALSE(row.isInitialized());
      ASSERT_EQ(rowIndexBefore, testee.getRowIndex())
          << "Skipped a non processed row.";
    }
    {
      auto [state, row] = testee.nextShadowRow();
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
      auto row = testee.peekShadowRow();
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
  // This vector is not read anywhere, it just serves as a data lake for the ExecutionBlocks generated by the tests
  // s.t. they are garbage collected after the test execution is done.
  std::vector<std::unique_ptr<ExecutionBlock>> _blocks;
  // The dependencies, they are referenced by _proxy, modifing this will modify the proxy
  std::vector<ExecutionBlock*> _dependencies{};
  RegIdSet inputRegister = RegIdSet{0};
  DependencyProxy<BlockPassthrough::Disable> _proxy{_dependencies, itemBlockManager,
                                                    inputRegister,
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

  auto testee = buildFetcher(data, 0);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  auto stack = makeStack();
  testWaiting(testee, stack, set);

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

  auto testee = buildFetcher(data, 0);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  auto stack = makeStack();
  testWaiting(testee, stack, set);

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
        buildBlock<1>(itemBlockManager, {
                                            {static_cast<int>(1 * (i + 1))},
                                            {static_cast<int>(2 * (i + 1))},
                                            {static_cast<int>(3 * (i + 1))},
                                        }));
    data.emplace_back(std::move(blockDeque));
  }

  auto testee = buildFetcher(data, 0);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  auto stack = makeStack();
  testWaiting(testee, stack, set);

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

  auto testee = buildFetcher(data, 0);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  auto stack = makeStack();
  testWaiting(testee, stack, set);

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
  for (int i = 0; i < static_cast<int>(numberDependencies()); ++i) {
    std::deque<SharedAqlItemBlockPtr> blockDeque{};
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{1 * (i + 1)}, {2 * (i + 1)}, {3 * (i + 1)}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{4 * (i + 1)}, {5 * (i + 1)}, {6 * (i + 1)}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{7 * (i + 1)}, {8 * (i + 1)}, {9 * (i + 1)}}));
    data.emplace_back(std::move(blockDeque));
  }

  auto testee = buildFetcher(data, 0);

  auto set = makeSameCallToAllDependencies(AqlCall{});
  auto stack = makeStack();
  testWaiting(testee, stack, set);

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
  for (int i = 0; i < static_cast<int>(numberDependencies()); ++i) {
    std::deque<SharedAqlItemBlockPtr> blockDeque{};
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{1 * (i + 1)}, {2 * (i + 1)}, {3 * (i + 1)}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{4 * (i + 1)}, {5 * (i + 1)}, {6 * (i + 1)}}));
    blockDeque.emplace_back(
        buildBlock<1>(itemBlockManager, {{7 * (i + 1)}, {8 * (i + 1)}, {9 * (i + 1)}}));
    data.emplace_back(std::move(blockDeque));
  }

  auto testee = buildFetcher(data, 0);
  auto stack = makeStack();
  for (size_t dep = 0; dep < numberDependencies(); ++dep) {
    AqlCallSet set{};
    set.calls.emplace_back(AqlCallSet::DepCallPair{dep, AqlCallList{AqlCall{}}});
    testWaiting(testee, stack, set);

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
  // NOTE: The fetcher does NOT care to synchronize the shadowRows between blocks.
  // This has to be done by the InputRange hold externally.
  // It has seperate tests
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (int i = 0; i < static_cast<int>(numberDependencies()); ++i) {
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

  auto testee = buildFetcher(data, 2);
  auto stack = makeStack();
  stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
  stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
  for (size_t dep = 0; dep < numberDependencies(); ++dep) {
    AqlCallSet set{};
    set.calls.emplace_back(AqlCallSet::DepCallPair{dep, AqlCallList{AqlCall{}}});
    testWaiting(testee, stack, set);

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

TEST_P(MultiDependencySingleRowFetcherTest,
       many_blocks_upstream_all_deps_differ_sequentially_using_shadowRows_no_callList_reverse_order) {
  // NOTE: The fetcher does NOT care to synchronize the shadowRows between blocks.
  // This has to be done by the InputRange hold externally.
  // It has seperate tests
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (int i = 0; i < static_cast<int>(numberDependencies()); ++i) {
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

  auto testee = buildFetcher(data, 2);
  auto stack = makeStack();
  stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
  stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
  for (size_t depCounter = numberDependencies(); depCounter > 0; --depCounter) {
    TRI_ASSERT(depCounter > 0);
    auto dep = depCounter - 1;
    AqlCallSet set{};
    set.calls.emplace_back(AqlCallSet::DepCallPair{dep, AqlCallList{AqlCall{}}});
    testWaiting(testee, stack, set);

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
      if (dep == 0) {
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
       many_blocks_upstream_all_deps_differ_sequentially_using_shadowRows_no_callList_offset) {
  // NOTE: The fetcher does NOT care to synchronize the shadowRows between blocks.
  // This has to be done by the InputRange hold externally.
  // It has seperate tests
  std::vector<std::deque<SharedAqlItemBlockPtr>> data;
  for (int i = 0; i < static_cast<int>(numberDependencies()); ++i) {
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

  auto testee = buildFetcher(data, 2);
  auto stack = makeStack();
  stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
  stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});

  for (size_t dep = 0; dep < numberDependencies(); ++dep) {
    AqlCallSet set{};
    // offset 10
    set.calls.emplace_back(AqlCallSet::DepCallPair{dep, AqlCallList{AqlCall{10}}});
    testWaiting(testee, stack, set);

    {
      // First Block, split after shadowRow, we cannot overfetch
      auto [state, skipped, ranges] = testee.execute(stack, set);
      EXPECT_EQ(state, ExecutionState::HASMORE);
      EXPECT_FALSE(skipped.nothingSkipped());
      EXPECT_EQ(skipped.getSkipCount(), 2);
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are non empty
        validateNextIsShadowRow(range, ExecutorState::HASMORE, 0, 0);
      }
    }

    {
      // Second Block, split after the higher depth shadow row
      auto [state, skipped, ranges] = testee.execute(stack, set);
      EXPECT_EQ(state, ExecutionState::HASMORE);
      EXPECT_FALSE(skipped.nothingSkipped());
      // We skip 1 from firstBlock part 2 and 2 in this block
      EXPECT_EQ(skipped.getSkipCount(), 3);
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are non empty
        validateNextIsShadowRow(range, ExecutorState::HASMORE, 1, 0);
        validateNextIsShadowRow(range, ExecutorState::HASMORE, 2, 1);
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

      // We skip 1 from secondBlock part 2 and 3 in this block
      EXPECT_EQ(skipped.getSkipCount(), 4);
      EXPECT_EQ(ranges.size(), set.size());
      for (auto [dep, range] : ranges) {
        // All Ranges are empty
        EXPECT_FALSE(range.hasShadowRow());
        EXPECT_FALSE(range.hasDataRow());
        EXPECT_EQ(range.upstreamState(), ExecutorState::DONE);
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
