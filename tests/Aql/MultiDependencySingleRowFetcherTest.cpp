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
#include "AqlItemRowPrinter.h"
#include "DependencyProxyMock.h"
#include "MultiDepFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/ExecutionBlock.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/ResourceUsage.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class MultiDependencySingleRowFetcherTest : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  ExecutionState state;

  MultiDependencySingleRowFetcherTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS) {}
};

TEST_F(MultiDependencySingleRowFetcherTest,
       no_blocks_upstream_single_dependency_the_producer_doesnt_wait) {
  VPackBuilder input;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 0, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  dependencyProxyMock.getDependencyMock(0).shouldReturn(ExecutionState::DONE, nullptr);

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 1);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       no_blocks_upstream_single_dependency_the_producer_waits) {
  VPackBuilder input;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 0, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       single_upstream_block_with_a_single_row_single_dependency_the_producer_returns_done_immediately) {
  VPackBuilder input;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.getDependencyMock(0).shouldReturn(ExecutionState::DONE,
                                                        std::move(block));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTest,
       single_upstream_block_with_a_single_row_single_dependency_the_producer_returns_hasmore_then_done_with_a_nullptr) {
  VPackBuilder input;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::HASMORE, std::move(block))
      .andThenReturn(ExecutionState::DONE, nullptr);

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       single_upstream_block_with_a_single_row_single_dependency_the_producer_waits_then_returns_done) {
  VPackBuilder input;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, 1};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
  dependencyProxyMock.getDependencyMock(0)
      .shouldReturn(ExecutionState::WAITING, nullptr)
      .andThenReturn(ExecutionState::DONE, std::move(block));

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);

    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTest,
       single_upstream_block_with_a_single_row_single_dependency_the_producer_waits_returns_hasmore_then_done) {
  VPackBuilder input;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, 1};
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
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);

    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);

    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTest,
       multiple_blocks_upstream_single_dependency_the_producer_doesnt_wait) {
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, 1};
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
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTest,
       multiple_blocks_upstream_single_dependency_the_producer_waits) {
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, 1};
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
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRowForDependency(0);
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

TEST_F(MultiDependencySingleRowFetcherTest,
       multiple_blocks_upstream_single_dependency_the_producer_the_producer_waits_and_doesnt_return_done_asap) {
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, 1};
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
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 7);
}

/*********************
 *  Multi Dependencies
 *********************/

TEST_F(MultiDependencySingleRowFetcherTest,
       no_blocks_upstream_multiple_dependencies_the_producers_dont_wait) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 0, numDeps};
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  for (size_t i = 0; i < numDeps; ++i) {
    dependencyProxyMock.getDependencyMock(i).shouldReturn(ExecutionState::DONE, nullptr);
  }

  {
    MultiDependencySingleRowFetcher testee(dependencyProxyMock);

    testee.initDependencies();

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_TRUE(state == ExecutionState::DONE);
      ASSERT_TRUE(!row);
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       no_blocks_upstream_multiple_dependencies_the_producers_wait) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 0, numDeps};
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
      ASSERT_TRUE(state == ExecutionState::WAITING);
      ASSERT_TRUE(!row);
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_TRUE(state == ExecutionState::DONE);
      ASSERT_TRUE(!row);
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2 * numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       single_upstream_block_with_a_single_row_multi_dependency_the_producer_returns_done_immediately) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};
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
      ASSERT_TRUE(state == ExecutionState::DONE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      if (i == 0) {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);
      } else if (i == 1) {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 23);
      } else {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 1337);
      }
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       single_upstream_block_with_a_single_row_multi_dependency_the_producer_returns_hasmore_then_done_with_a_nullptr) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};
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
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      if (i == 0) {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);
      } else if (i == 1) {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 23);
      } else {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 1337);
      }
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_TRUE(state == ExecutionState::DONE);
      ASSERT_TRUE(!row);
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2 * numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       single_upstream_block_with_a_single_row_multi_dependency_the_producer_waits_then_returns_done) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};
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
      ASSERT_TRUE(state == ExecutionState::WAITING);
      ASSERT_TRUE(!row);
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_TRUE(state == ExecutionState::DONE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      if (i == 0) {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);
      } else if (i == 1) {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 23);
      } else {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 1337);
      }
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 2 * numDeps);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       single_upstream_block_with_a_single_row_multi_dependency_the_producer_waits_returns_more_then_done) {
  VPackBuilder input;
  size_t numDeps = 3;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};
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
      ASSERT_TRUE(state == ExecutionState::WAITING);
      ASSERT_TRUE(!row);
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      if (i == 0) {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 42);
      } else if (i == 1) {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 23);
      } else {
        ASSERT_TRUE(row.getValue(0).slice().getInt() == 1337);
      }
    }

    for (size_t i = 0; i < numDeps; ++i) {
      std::tie(state, row) = testee.fetchRowForDependency(i);
      ASSERT_TRUE(state == ExecutionState::DONE);
      ASSERT_TRUE(!row);
    }
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 3 * numDeps);
}

// TODO the following tests should be simplified, a simple output
// specification should be compared with the actual output.

TEST_F(MultiDependencySingleRowFetcherTest,
       multiple_blocks_upstream_multiple_dependencies_the_producer_does_not_wait) {
  size_t numDeps = 3;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};
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
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);

    for (rowIdxAndValue = 7; rowIdxAndValue <= 8; rowIdxAndValue++) {
      std::tie(state, row) = testee.fetchRowForDependency(1);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 9;
    std::tie(state, row) = testee.fetchRowForDependency(1);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);

    for (rowIdxAndValue = 10; rowIdxAndValue <= 10; rowIdxAndValue++) {
      std::tie(state, row) = testee.fetchRowForDependency(2);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 11;
    std::tie(state, row) = testee.fetchRowForDependency(2);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 3 + 2 + 1);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       multiple_blocks_upstream_multiple_dependencies_the_producer_waits) {
  size_t numDeps = 3;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};
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
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);

    for (rowIdxAndValue = 7; rowIdxAndValue <= 8; rowIdxAndValue++) {
      if (rowIdxAndValue == 7 || rowIdxAndValue == 8) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(1);
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(1);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 9;
    std::tie(state, row) = testee.fetchRowForDependency(1);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);

    for (rowIdxAndValue = 10; rowIdxAndValue <= 10; rowIdxAndValue++) {
      if (rowIdxAndValue == 10) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(2);
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(2);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 11;
    std::tie(state, row) = testee.fetchRowForDependency(2);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 12);
}

TEST_F(MultiDependencySingleRowFetcherTest,
       multiple_blocks_upstream_multiple_dependencies_the_producer_waits_and_doesnt_return_done_asap) {
  size_t numDeps = 3;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};
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
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(0);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 6;
    // wait at the beginning of the 3rd block
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::WAITING);
    ASSERT_TRUE(!row);
    // last row and DONE
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    std::tie(state, row) = testee.fetchRowForDependency(0);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);

    for (rowIdxAndValue = 7; rowIdxAndValue <= 8; rowIdxAndValue++) {
      if (rowIdxAndValue == 7 || rowIdxAndValue == 8) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(1);
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(1);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 9;
    std::tie(state, row) = testee.fetchRowForDependency(1);
    ASSERT_TRUE(state == ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    std::tie(state, row) = testee.fetchRowForDependency(1);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);

    for (rowIdxAndValue = 10; rowIdxAndValue <= 10; rowIdxAndValue++) {
      if (rowIdxAndValue == 10) {
        // wait at the beginning of the 1st and 2nd block
        std::tie(state, row) = testee.fetchRowForDependency(2);
        ASSERT_TRUE(state == ExecutionState::WAITING);
        ASSERT_TRUE(!row);
      }
      std::tie(state, row) = testee.fetchRowForDependency(2);
      ASSERT_TRUE(state == ExecutionState::HASMORE);
      ASSERT_TRUE(row);
      ASSERT_TRUE(row.getNrRegisters() == 1);
      ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    }
    rowIdxAndValue = 11;
    std::tie(state, row) = testee.fetchRowForDependency(2);
    ASSERT_TRUE(state == ExecutionState::HASMORE);
    ASSERT_TRUE(row);
    ASSERT_TRUE(row.getNrRegisters() == 1);
    ASSERT_TRUE(row.getValue(0).slice().getInt() == rowIdxAndValue);
    std::tie(state, row) = testee.fetchRowForDependency(2);
    ASSERT_TRUE(state == ExecutionState::DONE);
    ASSERT_TRUE(!row);
  }  // testee is destroyed here
  // testee must be destroyed before verify, because it may call returnBlock
  // in the destructor
  ASSERT_TRUE(dependencyProxyMock.allBlocksFetched());
  ASSERT_TRUE(dependencyProxyMock.numFetchBlockCalls() == 15);
}

using CutAt = uint64_t;

 class MultiDependencySingleRowFetcherShadowRowTest : public testing::TestWithParam<CutAt> {
 protected:
  ResourceMonitor monitor{};
  AqlItemBlockManager itemBlockManager;

  MultiDependencySingleRowFetcherShadowRowTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS) {}

  uint64_t cutAt() const { return GetParam(); }

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
          block->setShadowRowDepth(1, AqlValue{AqlValueHintUInt{0}});
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
};

INSTANTIATE_TEST_CASE_P(MultiDependencySingleRowFetcherShadowRowTestInstance,
                        MultiDependencySingleRowFetcherShadowRowTest,
                        testing::Range(static_cast<uint64_t>(0),
                                       static_cast<uint64_t>(4)));

TEST_P(MultiDependencySingleRowFetcherShadowRowTest, simple_fetch_shadow_row_test) {
  constexpr size_t numDeps = 1;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};

  dependencyProxyMock.getDependencyMock(0).shouldReturn(alternatingDataAndShadowRows({0, 1, 2, 3}));

  MultiDependencySingleRowFetcher testee{dependencyProxyMock};
  testee.initDependencies();

  runFetcher(
      testee,
      {
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(0)}),
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(1)}),
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(2)}),
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(3)}),
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::DONE, invalidShadowRow()}),
      });
}

TEST_P(MultiDependencySingleRowFetcherShadowRowTest, shadow_rows_2_deps) {
  constexpr size_t numDeps = 2;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};

  dependencyProxyMock.getDependencyMock(0).shouldReturn(alternatingDataAndShadowRows({0, 1, 2, 3}));
  dependencyProxyMock.getDependencyMock(1).shouldReturn(alternatingDataAndShadowRows({4, 1, 6, 3}));

  MultiDependencySingleRowFetcher testee{dependencyProxyMock};
  testee.initDependencies();

  runFetcher(
      testee,
      {
          // fetch dep 1
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(0)}),
          // dep 1 should stay done
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // fetching the shadow row should not yet be possible
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()}),
          // dep 1 should stay done
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // fetch dep 2
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(4)}),
          // dep 2 should stay done
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // dep 1 should stay done
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // Fetch the first shadow row.
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(1)}),
          // fetch dep 1 again
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(2)}),
          // dep 1 should stay done
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // fetching the shadow row should not yet be possible
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()}),
          // dep 1 should stay done
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // fetch dep 2 again
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(6)}),
          // dep 2 should stay done
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // dep 1 should stay done
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // Fetch the second shadow row.
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(3)}),
          // We're now done.
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::DONE, invalidShadowRow()}),
      });
}

TEST_P(MultiDependencySingleRowFetcherShadowRowTest, shadow_rows_2_deps_reverse_pull) {
  constexpr size_t numDeps = 2;
  MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};

  dependencyProxyMock.getDependencyMock(0).shouldReturn(alternatingDataAndShadowRows({0, 1, 2, 3}));
  dependencyProxyMock.getDependencyMock(1).shouldReturn(alternatingDataAndShadowRows({4, 1, 6, 3}));

  MultiDependencySingleRowFetcher testee{dependencyProxyMock};
  testee.initDependencies();

  runFetcher(
      testee,
      {
          // fetch dep 2
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(4)}),
          // dep 2 should stay done
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // fetching the shadow row should not yet be possible
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()}),
          // dep 2 should stay done
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // fetch dep 1
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(0)}),
          // dep 1 should stay done
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // dep 2 should stay done
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // Fetch the first shadow row.
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(1)}),
          // fetch dep 2 again
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(6)}),
          // dep 2 should stay done
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // fetching the shadow row should not yet be possible
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, invalidShadowRow()}),
          // dep 2 should stay done
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // fetch dep 1 again
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, inputRow(2)}),
          // dep 2 should stay done
          std::make_pair(FetchRowForDependency{1, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // dep 1 should stay done
          std::make_pair(FetchRowForDependency{0, 1000},
                         FetchRowForDependency::Result{ExecutionState::DONE, invalidInputRow()}),
          // Fetch the second shadow row.
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::HASMORE, shadowRow(3)}),
          // We're now done.
          std::make_pair(FetchShadowRow{1000},
                         FetchShadowRow::Result{ExecutionState::DONE, invalidShadowRow()}),
      });
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
