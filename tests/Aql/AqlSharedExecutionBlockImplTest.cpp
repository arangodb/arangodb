////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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

#include "AqlItemBlockHelper.h"
#include "ExecutorTestHelper.h"
#include "Mocks/Servers.h"
#include "WaitingExecutionBlockMock.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/FilterExecutor.h"
#include "Aql/IdExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SkipResult.h"
#include "Aql/SortExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/UnsortedGatherExecutor.h"

static_assert(GTEST_HAS_TYPED_TEST, "We need typed tests for the following:");

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// Right now we use the following Executors:
//   FilterExecutor => SingleRowFetcher, non-passthrough
//   IdExecutor => SingleRowFetcher, passthrough
//   SortExecutor => AllRowsFetcher;
//   UnsortedGatherExecutor => MultoDependencySingleRowFetcher
using ExecutorsToTest =
    ::testing::Types<FilterExecutor, IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>, SortExecutor, UnsortedGatherExecutor>;

template <class ExecutorType>
class AqlSharedExecutionBlockImplTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server{};
  ResourceMonitor monitor{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery{server.createFakeQuery()};
  std::vector<std::unique_ptr<ExecutionNode>> _execNodes;

  // Used for AllRowsFetcherCases
  std::unique_ptr<AqlItemMatrix> _aqlItemBlockMatrix;

  /**
   * @brief Creates and manages a ExecutionNode.
   *        These nodes can be used to create the Executors
   *        Caller does not need to manage the memory.
   *
   * @return ExecutionNode* Pointer to a dummy ExecutionNode. Memory is managed, do not delete.
   */
  ExecutionNode* generateNodeDummy() {
    auto dummy =
        std::make_unique<SingletonNode>(const_cast<ExecutionPlan*>(fakedQuery->plan()),
                                        ExecutionNodeId{_execNodes.size()});
    auto res = dummy.get();
    _execNodes.emplace_back(std::move(dummy));
    return res;
  }

  /**
   * @brief Creates Register Infos. As we do not actually test the Node, those
   * should be good for everything under test.
   *
   * @return RegisterInfo
   */
  auto buildRegisterInfos(size_t nestingLevel) -> RegisterInfos {
    RegIdSetStack regStack{};
    for (size_t i = 0; i <= nestingLevel; ++i) {
      regStack.emplace_back(RegIdSet{0});
    }
    return RegisterInfos(RegIdSet{0}, {}, 1, 1, {}, std::move(regStack));
  }

  auto emptyProducer() -> WaitingExecutionBlockMock {
    return WaitingExecutionBlockMock{fakedQuery->rootEngine(), generateNodeDummy(),
                                     std::deque<SharedAqlItemBlockPtr>{},
                                     WaitingExecutionBlockMock::WaitingBehaviour::NEVER};
  }

  auto buildExecBlock(size_t nestingLevel) -> ExecutionBlockImpl<ExecutorType> {
    if constexpr (std::is_same_v<ExecutorType, FilterExecutor>) {
      FilterExecutorInfos execInfos{0};
      return ExecutionBlockImpl<ExecutorType>{fakedQuery->rootEngine(),
                                              generateNodeDummy(),
                                              std::move(buildRegisterInfos(nestingLevel)),
                                              std::move(execInfos)};
    }
    if constexpr (std::is_same_v<ExecutorType, IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>) {
      IdExecutorInfos execInfos{false};
      return ExecutionBlockImpl<ExecutorType>{fakedQuery->rootEngine(),
                                              generateNodeDummy(),
                                              std::move(buildRegisterInfos(nestingLevel)),
                                              std::move(execInfos)};
    }
    if constexpr (std::is_same_v<ExecutorType, SortExecutor>) {
      std::vector<SortRegister> sortRegisters{};
      // We do not care for sorting, we skip anyways.
      sortRegisters.emplace_back(SortRegister{0, SortElement{nullptr, true}});
      SortExecutorInfos execInfos{1, 1, {}, std::move(sortRegisters), 0, fakedQuery->itemBlockManager(), nullptr, true};
      return ExecutionBlockImpl<ExecutorType>{fakedQuery->rootEngine(),
                                              generateNodeDummy(),
                                              std::move(buildRegisterInfos(nestingLevel)),
                                              std::move(execInfos)};
    }
    if constexpr (std::is_same_v<ExecutorType, UnsortedGatherExecutor>) {
      IdExecutorInfos execInfos{false};
      return ExecutionBlockImpl<ExecutorType>{fakedQuery->rootEngine(),
                                              generateNodeDummy(),
                                              std::move(buildRegisterInfos(nestingLevel)),
                                              std::move(execInfos)};
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto runLeftoverTest(ExecutionBlockImpl<ExecutorType>& testee,
                       SharedAqlItemBlockPtr leftoverBlock, AqlCallStack stack,
                       SkipResult expectedSkip,
                       SharedAqlItemBlockPtr expectedResult = nullptr) -> void {
    if constexpr (ExecutorType::Properties::allowsBlockPassthrough == BlockPassthrough::Enable) {
      // Passthrough Blocks cannot leave this situation behind
      return;
    }
    auto prod = emptyProducer();
    testee.addDependency(&prod);


    SkipResult skip{};
    for (size_t i = 1; i < stack.subqueryLevel(); ++i) {
      skip.incrementSubquery();
    }
    if constexpr (std::is_same_v<typename ExecutorType::Fetcher::DataRange, AqlItemBlockInputRange>) {
      AqlItemBlockInputRange fakedInternalRange{ExecutorState::DONE, 0, leftoverBlock, 0};
      testee.testInjectInputRange(std::move(fakedInternalRange), std::move(skip));
    }
    if constexpr (std::is_same_v<typename ExecutorType::Fetcher::DataRange, AqlItemBlockInputMatrix>) {
      _aqlItemBlockMatrix = std::make_unique<AqlItemMatrix>(1);
      _aqlItemBlockMatrix->addBlock(leftoverBlock);
      AqlItemBlockInputMatrix fakedInternalRange{ExecutorState::DONE, _aqlItemBlockMatrix.get()};
      testee.testInjectInputRange(std::move(fakedInternalRange), std::move(skip));
    }
    if constexpr (std::is_same_v<typename ExecutorType::Fetcher::DataRange, MultiAqlItemBlockInputRange>) {
      MultiAqlItemBlockInputRange fakedInternalRange{ExecutorState::DONE, 0, 1};
      AqlItemBlockInputRange tmpInternalRange{ExecutorState::DONE, 0, leftoverBlock, 0};
      fakedInternalRange.setDependency(0, tmpInternalRange);
      testee.testInjectInputRange(std::move(fakedInternalRange), std::move(skip));
    }

    auto [state, skipped, block] = testee.execute(stack);
    EXPECT_EQ(state, ExecutionState::DONE);
    if (expectedResult == nullptr) {
      EXPECT_EQ(block, nullptr);
    } else {
      asserthelper::ValidateBlocksAreEqual(block, expectedResult);
    }
    EXPECT_EQ(skipped, expectedSkip);
  }
};

TYPED_TEST_CASE(AqlSharedExecutionBlockImplTest, ExecutorsToTest);

TYPED_TEST(AqlSharedExecutionBlockImplTest, hardlimit_main_query_one_row) {
  auto testee = this->buildExecBlock(1);

  auto leftoverBlock = buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
                                     {{1}, {2}, {3}, {4}}, {{3, 0}});
  // MainQuery does a hardLimit 0
  AqlCall hardLimit{};
  hardLimit.offset = 0;
  hardLimit.hardLimit = 0u;
  hardLimit.fullCount = false;
  // Depth 1
  AqlCall call{};

  AqlCallStack stack{AqlCallList{hardLimit}};
  stack.pushCall(AqlCallList{call});

  SkipResult expectedSkip{};
  expectedSkip.incrementSubquery();

  this->runLeftoverTest(testee, leftoverBlock, stack, expectedSkip);
}

TYPED_TEST(AqlSharedExecutionBlockImplTest, hardlimit_main_query_many_rows) {
  auto testee = this->buildExecBlock(1);

  auto leftoverBlock =
      buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
                    {{1}, {2}, {3}, {4}, {5}, {6}}, {{2, 0}, {4, 0}, {5, 0}});
  // MainQuery does a hardLimit 0
  AqlCall hardLimit{};
  hardLimit.offset = 0;
  hardLimit.hardLimit = 0u;
  hardLimit.fullCount = false;
  // Depth 1
  AqlCall call{};

  AqlCallStack stack{AqlCallList{hardLimit}};
  stack.pushCall(AqlCallList{call});

  SkipResult expectedSkip{};
  expectedSkip.incrementSubquery();

  this->runLeftoverTest(testee, leftoverBlock, stack, expectedSkip);
}

TYPED_TEST(AqlSharedExecutionBlockImplTest, fullcount_main_query_one_row) {
  auto testee = this->buildExecBlock(1);

  auto leftoverBlock = buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
                                     {{1}, {2}, {3}, {4}}, {{3, 0}});
  // MainQuery does a hardLimit 0
  AqlCall hardLimit{};
  hardLimit.offset = 0;
  hardLimit.hardLimit = 0u;
  hardLimit.fullCount = true;
  // Depth 1
  AqlCall call{};

  AqlCallStack stack{AqlCallList{hardLimit}};
  stack.pushCall(AqlCallList{call});

  SkipResult expectedSkip{};
  expectedSkip.didSkip(1);
  expectedSkip.incrementSubquery();

  this->runLeftoverTest(testee, leftoverBlock, stack, expectedSkip);
}

TYPED_TEST(AqlSharedExecutionBlockImplTest, fullcount_main_query_many_rows) {
  auto testee = this->buildExecBlock(1);

  auto leftoverBlock =
      buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
                    {{1}, {2}, {3}, {4}, {5}, {6}}, {{2, 0}, {4, 0}, {5, 0}});
  // MainQuery does a hardLimit 0
  AqlCall hardLimit{};
  hardLimit.offset = 0;
  hardLimit.hardLimit = 0u;
  hardLimit.fullCount = true;
  // Depth 1
  AqlCall call{};

  AqlCallStack stack{AqlCallList{hardLimit}};
  stack.pushCall(AqlCallList{call});

  SkipResult expectedSkip{};
  expectedSkip.didSkip(3);
  expectedSkip.incrementSubquery();

  this->runLeftoverTest(testee, leftoverBlock, stack, expectedSkip);
}

TYPED_TEST(AqlSharedExecutionBlockImplTest, hardlimit_sub_query_one_row) {
  auto testee = this->buildExecBlock(2);

  auto leftoverBlock = buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
                                     {{1}, {2}, {3}, {4}, {5}}, {{3, 0}, {4, 1}});
  auto expectedBlock =
      buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(), {{5}}, {{0, 1}});
  // MainQuery does a hardLimit 0
  AqlCall hardLimit{};
  hardLimit.offset = 0;
  hardLimit.hardLimit = 0u;
  hardLimit.fullCount = false;
  // Depth 1
  AqlCall call{};

  AqlCallStack stack{AqlCallList{call}};
  stack.pushCall(AqlCallList{hardLimit});
  stack.pushCall(AqlCallList{call});

  SkipResult expectedSkip{};
  expectedSkip.incrementSubquery();
  expectedSkip.incrementSubquery();

  this->runLeftoverTest(testee, leftoverBlock, stack, expectedSkip, expectedBlock);
}

TYPED_TEST(AqlSharedExecutionBlockImplTest, hardlimit_sub_query_many_rows) {
  auto testee = this->buildExecBlock(2);

  auto leftoverBlock = buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
                                     {{1}, {2}, {3}, {4}, {5}, {6}, {7}},
                                     {{2, 0}, {4, 0}, {5, 0}, {6, 1}});
  auto expectedBlock =
      buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(), {{7}}, {{0, 1}});
  // MainQuery does a hardLimit 0
  AqlCall hardLimit{};
  hardLimit.offset = 0;
  hardLimit.hardLimit = 0u;
  hardLimit.fullCount = false;
  // Depth 1
  AqlCall call{};

  AqlCallStack stack{AqlCallList{call}};
  stack.pushCall(AqlCallList{hardLimit});
  stack.pushCall(AqlCallList{call});

  SkipResult expectedSkip{};
  expectedSkip.incrementSubquery();
  expectedSkip.incrementSubquery();

  this->runLeftoverTest(testee, leftoverBlock, stack, expectedSkip, expectedBlock);
}

TYPED_TEST(AqlSharedExecutionBlockImplTest, fullcount_sub_query_one_row) {
  auto testee = this->buildExecBlock(2);

  auto leftoverBlock = buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
                                     {{1}, {2}, {3}, {4}, {5}}, {{3, 0}, {4, 1}});
  auto expectedBlock =
      buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(), {{5}}, {{0, 1}});
  // MainQuery does a hardLimit 0
  AqlCall hardLimit{};
  hardLimit.offset = 0;
  hardLimit.hardLimit = 0u;
  hardLimit.fullCount = true;
  // Depth 1
  AqlCall call{};

  AqlCallStack stack{AqlCallList{call}};
  stack.pushCall(AqlCallList{hardLimit});
  stack.pushCall(AqlCallList{call});

  SkipResult expectedSkip{};
  expectedSkip.incrementSubquery();
  expectedSkip.didSkip(1);
  expectedSkip.incrementSubquery();

  this->runLeftoverTest(testee, leftoverBlock, stack, expectedSkip, expectedBlock);
}

TYPED_TEST(AqlSharedExecutionBlockImplTest, fullcount_sub_query_many_rows) {
  auto testee = this->buildExecBlock(2);

  auto leftoverBlock = buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
                                     {{1}, {2}, {3}, {4}, {5}, {6}, {7}},
                                     {{2, 0}, {4, 0}, {5, 0}, {6, 1}});
  auto expectedBlock =
      buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(), {{7}}, {{0, 1}});
  // MainQuery does a hardLimit 0
  AqlCall hardLimit{};
  hardLimit.offset = 0;
  hardLimit.hardLimit = 0u;
  hardLimit.fullCount = true;
  // Depth 1
  AqlCall call{};

  AqlCallStack stack{AqlCallList{call}};
  stack.pushCall(AqlCallList{hardLimit});
  stack.pushCall(AqlCallList{call});

  SkipResult expectedSkip{};
  expectedSkip.incrementSubquery();
  expectedSkip.didSkip(3);
  expectedSkip.incrementSubquery();

  this->runLeftoverTest(testee, leftoverBlock, stack, expectedSkip, expectedBlock);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb