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
                                                            static_cast<size_t>(3))));

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

class MultiDependencySingleRowFetcherTestOld : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  ExecutionState state;

  MultiDependencySingleRowFetcherTestOld()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS) {}
};

TEST_F(MultiDependencySingleRowFetcherTestOld,
       single_upstream_block_with_a_single_row_single_dependency_the_producer_returns_done_immediately) {
  VPackBuilder input;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.getDependencyMock(0).shouldReturn(ExecutionState::DONE,
                                                        std::move(block));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTestOld,
       single_upstream_block_with_a_single_row_single_dependency_the_producer_returns_hasmore_then_done_with_a_nullptr) {
  VPackBuilder input;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2);
}

TEST_F(MultiDependencySingleRowFetcherTestOld,
       single_upstream_block_with_a_single_row_single_dependency_the_producer_waits_then_returns_done) {
  VPackBuilder input;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);

    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTestOld,
       single_upstream_block_with_a_single_row_single_dependency_the_producer_waits_returns_hasmore_then_done) {
  VPackBuilder input;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), 42);

    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTestOld,
       multiple_blocks_upstream_single_dependency_the_producer_doesnt_wait) {
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1 = buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3 = buildBlock<1>(itemBlockManager, {{{6}}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::HASMORE, std::move(block1))
      .andThenReturn(ExecutionState::HASMORE, std::move(block2))
      .andThenReturn(ExecutionState::DONE, std::move(block3));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    int64_t rowIdxAndValue;
    for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTestOld,
       multiple_blocks_upstream_single_dependency_the_producer_waits) {
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1 = buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3 = buildBlock<1>(itemBlockManager, {{{6}}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block1))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block2))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block3));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    int64_t rowIdxAndValue;
    for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
      if (rowIdxAndValue == 1 || rowIdxAndValue == 4) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(0);
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTestOld,
       multiple_blocks_upstream_single_dependency_the_producer_the_producer_waits_and_doesnt_return_done_asap) {
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1 = buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3 = buildBlock<1>(itemBlockManager, {{{6}}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block1))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block2))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block3))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    for (int64_t rowIdxAndValue = 1; rowIdxAndValue <= 6; rowIdxAndValue++) {
      if (rowIdxAndValue == 1 || rowIdxAndValue == 4 || rowIdxAndValue == 6) {
        // wait at the beginning of the 1st, 2nd and 3rd block
        std::tie(state, row) = testee.fetchRowForDependency(0);
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 7);
}

/*********************
 *  Multi Dependencies
 *********************/

TEST_F(MultiDependencySingleRowFetcherTestOld,
       no_blocks_upstream_multiple_dependencies_the_producers_dont_wait) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{
      monitor, 0, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  for (size_t i = 0; i < numDeps; ++i) {
    dependencyProxyMock.getDependencyMock(i).shouldReturn(ExecutionState::DONE, nullptr);
  }

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::DONE);
      ASSERT_FALSE(row);
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTestOld,
       no_blocks_upstream_multiple_dependencies_the_producers_wait) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{
      monitor, 0, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  for (size_t i = 0; i < numDeps; ++i) {
    dependencyProxyMock.getDependencyMock(i)
        .shouldReturn(ExecutionState::WAITING, nullptr)
        .andThenReturn(ExecutionState::DONE, nullptr);
  }

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::WAITING);
      ASSERT_FALSE(row);
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::DONE);
      ASSERT_FALSE(row);
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2 * numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTestOld,
       single_upstream_block_with_a_single_row_multi_dependency_the_producer_returns_done_immediately) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{
      monitor, 1, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr blockDep1 = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockDep2 = buildBlock<1>(itemBlockManager, {{23}});
  SharedAqlItemBlockPtr blockDep3 = buildBlock<1>(itemBlockManager, {{1337}});
  dependencyProxyMock.getDependencyMock(0).shouldReturn(ExecutionState::DONE,
                                                        std::move(blockDep1));
  dependencyProxyMock.getDependencyMock(1).shouldReturn(ExecutionState::DONE,
                                                        std::move(blockDep2));
  dependencyProxyMock.getDependencyMock(2).shouldReturn(ExecutionState::DONE,
                                                        std::move(blockDep3));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::DONE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      if (i == 0) {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 42);
      } else if (i == 1) {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 23);
      } else {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 1337);
      }
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTestOld,
       single_upstream_block_with_a_single_row_multi_dependency_the_producer_returns_hasmore_then_done_with_a_nullptr) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{
      monitor, 1, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr blockDep1 = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockDep2 = buildBlock<1>(itemBlockManager, {{23}});
  SharedAqlItemBlockPtr blockDep3 = buildBlock<1>(itemBlockManager, {{1337}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::HASMORE, std::move(blockDep1))
      .andThenReturn(ExecutionState::DONE, nullptr);
  dependencyProxyMock.getDependencyMock(1)
      .shouldReturn(ExecutionState::HASMORE, std::move(blockDep2))
      .andThenReturn(ExecutionState::DONE, nullptr);
  dependencyProxyMock.getDependencyMock(2)
      .shouldReturn(ExecutionState::HASMORE, std::move(blockDep3))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      if (i == 0) {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 42);
      } else if (i == 1) {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 23);
      } else {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 1337);
      }
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::DONE);
      ASSERT_FALSE(row);
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2 * numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTestOld,
       single_upstream_block_with_a_single_row_multi_dependency_the_producer_waits_then_returns_done) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{
      monitor, 1, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr blockDep1 = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockDep2 = buildBlock<1>(itemBlockManager, {{23}});
  SharedAqlItemBlockPtr blockDep3 = buildBlock<1>(itemBlockManager, {{1337}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(blockDep1));
  dependencyProxyMock.getDependencyMock(1)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(blockDep2));
  dependencyProxyMock.getDependencyMock(2)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(blockDep3));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::WAITING);
      ASSERT_FALSE(row);
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::DONE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      if (i == 0) {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 42);
      } else if (i == 1) {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 23);
      } else {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 1337);
      }
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 2 * numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTestOld,
       single_upstream_block_with_a_single_row_multi_dependency_the_producer_waits_returns_more_then_done) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{
      monitor, 1, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr blockDep1 = buildBlock<1>(itemBlockManager, {{42}});
  SharedAqlItemBlockPtr blockDep2 = buildBlock<1>(itemBlockManager, {{23}});
  SharedAqlItemBlockPtr blockDep3 = buildBlock<1>(itemBlockManager, {{1337}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(blockDep1))
      .andThenReturn(ExecutionState::DONE, nullptr);
  dependencyProxyMock.getDependencyMock(1)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(blockDep2))
      .andThenReturn(ExecutionState::DONE, nullptr);
  dependencyProxyMock.getDependencyMock(2)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(blockDep3))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::WAITING);
      ASSERT_FALSE(row);
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      if (i == 0) {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 42);
      } else if (i == 1) {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 23);
      } else {
        ASSERT_EQ(row.getValue(0).slice().getInt(), 1337);
      }
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_EQ(state, ExecutionState::DONE);
      ASSERT_FALSE(row);
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 3 * numDeps);
}

// TODO the following tests should be simplified, a simple output
// specification should be compared with the actual output.

TEST_F(MultiDependencySingleRowFetcherTestOld,
       multiple_blocks_upstream_multiple_dependencies_the_producer_does_not_wait) {
  size_t numDeps = 3;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{
      monitor, 1, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1Dep1 =
                            buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2Dep1 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3Dep1 = buildBlock<1>(itemBlockManager, {{{6}}});
  // two 1-column matrices with 1 and 2 rows, respectively
  SharedAqlItemBlockPtr block1Dep2 = buildBlock<1>(itemBlockManager, {{{7}}}),
                        block2Dep2 = buildBlock<1>(itemBlockManager, {{{8}}, {{9}}});
  // single 1-column matrices with 2 rows
  SharedAqlItemBlockPtr block1Dep3 = buildBlock<1>(itemBlockManager, {{{10}}, {{11}}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::HASMORE, std::move(block1Dep1))
      .andThenReturn(ExecutionState::HASMORE, std::move(block2Dep1))
      .andThenReturn(ExecutionState::DONE, std::move(block3Dep1));
  dependencyProxyMock.getDependencyMock(1)
      .shouldReturn(ExecutionState::HASMORE, std::move(block1Dep2))
      .andThenReturn(ExecutionState::DONE, std::move(block2Dep2));
  dependencyProxyMock.getDependencyMock(2).shouldReturn(ExecutionState::DONE,
                                                        std::move(block1Dep3));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    int64_t rowIdxAndValue;
    for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);

    for (rowIdxAndValue = 7; rowIdxAndValue <= 8; rowIdxAndValue++) {
      std::tie(state, row) = testee.fetchRowForDependency(1);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 9;
    std::tie(state, row) = testee.fetchRowForDependency(1);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);

    for (rowIdxAndValue = 10; rowIdxAndValue <= 10; rowIdxAndValue++) {
      std::tie(state, row) = testee.fetchRowForDependency(2);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 11;
    std::tie(state, row) = testee.fetchRowForDependency(2);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 3 + 2 + 1);
}

TEST_F(MultiDependencySingleRowFetcherTestOld,
       multiple_blocks_upstream_multiple_dependencies_the_producer_waits) {
  size_t numDeps = 3;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{
      monitor, 1, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1Dep1 =
                            buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2Dep1 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3Dep1 = buildBlock<1>(itemBlockManager, {{{6}}});
  // two 1-column matrices with 1 and 2 rows, respectively
  SharedAqlItemBlockPtr block1Dep2 = buildBlock<1>(itemBlockManager, {{{7}}}),
                        block2Dep2 = buildBlock<1>(itemBlockManager, {{{8}}, {{9}}});
  // single 1-column matrices with 2 rows
  SharedAqlItemBlockPtr block1Dep3 = buildBlock<1>(itemBlockManager, {{{10}}, {{11}}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep1))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block2Dep1))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block3Dep1));
  dependencyProxyMock.getDependencyMock(1)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep2))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block2Dep2));
  dependencyProxyMock.getDependencyMock(2)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block1Dep3));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    int64_t rowIdxAndValue;
    for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
      if (rowIdxAndValue == 1 || rowIdxAndValue == 4) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(0);
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);

    for (rowIdxAndValue = 7; rowIdxAndValue <= 8; rowIdxAndValue++) {
      if (rowIdxAndValue == 7 || rowIdxAndValue == 8) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(1);
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(1);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 9;
    std::tie(state, row) = testee.fetchRowForDependency(1);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);

    for (rowIdxAndValue = 10; rowIdxAndValue <= 10; rowIdxAndValue++) {
      if (rowIdxAndValue == 10) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(2);
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(2);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 11;
    std::tie(state, row) = testee.fetchRowForDependency(2);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 12);
}

TEST_F(MultiDependencySingleRowFetcherTestOld,
       multiple_blocks_upstream_multiple_dependencies_the_producer_waits_and_doesnt_return_done_asap) {
  size_t numDeps = 3;
  MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable> dependencyProxyMock{
      monitor, 1, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  // three 1-column matrices with 3, 2 and 1 rows, respectively
  SharedAqlItemBlockPtr block1Dep1 =
                            buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                        block2Dep1 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                        block3Dep1 = buildBlock<1>(itemBlockManager, {{{6}}});
  // two 1-column matrices with 1 and 2 rows, respectively
  SharedAqlItemBlockPtr block1Dep2 = buildBlock<1>(itemBlockManager, {{{7}}}),
                        block2Dep2 = buildBlock<1>(itemBlockManager, {{{8}}, {{9}}});
  // single 1-column matrices with 2 rows
  SharedAqlItemBlockPtr block1Dep3 = buildBlock<1>(itemBlockManager, {{{10}}, {{11}}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep1))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block2Dep1))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block3Dep1))
      .andThenReturn(ExecutionState::DONE, nullptr);
  dependencyProxyMock.getDependencyMock(1)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep2))
      .andThenReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block2Dep2))
      .andThenReturn(ExecutionState::DONE, nullptr);
  dependencyProxyMock.getDependencyMock(2)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep3))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    int64_t rowIdxAndValue;
    for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
      if (rowIdxAndValue == 1 || rowIdxAndValue == 4) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(0);
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_FALSE(row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);

    for (rowIdxAndValue = 7; rowIdxAndValue <= 8; rowIdxAndValue++) {
      if (rowIdxAndValue == 7 || rowIdxAndValue == 8) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(1);
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(1);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 9;
    std::tie(state, row) = testee.fetchRowForDependency(1);
    ASSERT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    std::tie(state, row) = testee.fetchRowForDependency(1);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);

    for (rowIdxAndValue = 10; rowIdxAndValue <= 10; rowIdxAndValue++) {
      if (rowIdxAndValue == 10) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(2);
        ASSERT_EQ(state, ExecutionState::WAITING);
        ASSERT_FALSE(row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(2);
      ASSERT_EQ(state, ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_EQ(row.getNrRegisters(), 1);
      ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    }
    rowIdxAndValue = 11;
    std::tie(state, row) = testee.fetchRowForDependency(2);
    ASSERT_EQ(state, ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_EQ(row.getNrRegisters(), 1);
    ASSERT_EQ(row.getValue(0).slice().getInt(), rowIdxAndValue);
    std::tie(state, row) = testee.fetchRowForDependency(2);
    ASSERT_EQ(state, ExecutionState::DONE);
    ASSERT_FALSE(row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_EQ(dependencyProxyMock.numFetchBlockCalls(), 15);
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
  // Get a shadow row pointing to a row with the specified value, and specified
  // shadow row depth, in the only register in an anonymous block.
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
