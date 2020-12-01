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
#include "Mocks/Servers.h"
#include "WaitingExecutionBlockMock.h"

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

static_assert(GTEST_HAS_TYPED_TEST, "We need typed tests for the following:");

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// Right now we use the following Executors:
//  FilterExecutor => SingleRowFetcher, non-passthrough
// IdExecutor => SingleRowFetcher, passthrough
using ExecutorsToTest = ::testing::Types<FilterExecutor, IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>;

template <class ExecutorType>
class AqlSharedExecutionBlockImplTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server{};
  ResourceMonitor monitor{};
  std::unique_ptr<arangodb::aql::Query> fakedQuery{server.createFakeQuery()};
  std::vector<std::unique_ptr<ExecutionNode>> _execNodes;

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
   * @brief Creates Register Infos. As we do not actually test the Node, those should be good
   *        for everything under test.
   *
   * @return RegisterInfo
   */
  auto buildRegisterInfos() -> RegisterInfos {
    return RegisterInfos(RegIdSet{0}, {}, 1, 1, {}, {RegIdSet{0}});
  }

  auto emptyProducer() -> WaitingExecutionBlockMock {
    return WaitingExecutionBlockMock{fakedQuery->rootEngine(), generateNodeDummy(),
                                     std::deque<SharedAqlItemBlockPtr>{},
                                     WaitingExecutionBlockMock::WaitingBehaviour::NEVER};
  }

  auto buildBlock() -> ExecutionBlockImpl<ExecutorType> {
    if constexpr(std::is_same_v<ExecutorType, FilterExecutor>) {
      FilterExecutorInfos execInfos{0};
      return ExecutionBlockImpl<ExecutorType>{fakedQuery->rootEngine(),
                                              generateNodeDummy(), std::move(buildRegisterInfos()),
                                              std::move(execInfos)};
    }
    if constexpr(std::is_same_v<ExecutorType, IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>) {
      IdExecutorInfos execInfos{false};
      return ExecutionBlockImpl<ExecutorType>{fakedQuery->rootEngine(),
                                              generateNodeDummy(), std::move(buildRegisterInfos()),
                                              std::move(execInfos)};
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto runLeftoverTest(ExecutionBlockImpl<ExecutorType>& testee, SharedAqlItemBlockPtr leftoverBlock,
               AqlCallStack stack, SkipResult expectedSkip) -> void {
    if constexpr (ExecutorType::Properties::allowsBlockPassthrough == BlockPassthrough::Enable) {
      // Passthrough Blocks cannot leave this situation behind
      return;
    }
    auto prod = emptyProducer();
    testee.addDependency(&prod);

    AqlItemBlockInputRange fakedInternalRange{ExecutorState::DONE, 0, leftoverBlock, 0};
    SkipResult skip{};
    for (size_t i = 1; i < stack.subqueryLevel(); ++i) {
      skip.incrementSubquery();
    }
    testee.testInjectInputRange(std::move(fakedInternalRange), std::move(skip));

    auto [state, skipped, block] = testee.execute(stack);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(block, nullptr);
    EXPECT_EQ(skipped, expectedSkip);
  }
};

TYPED_TEST_CASE(AqlSharedExecutionBlockImplTest, ExecutorsToTest);

TYPED_TEST(AqlSharedExecutionBlockImplTest, hardlimit_main_query_one_row) {
  auto testee = this->buildBlock();

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
  auto testee = this->buildBlock();

 auto leftoverBlock = buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
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
  auto testee = this->buildBlock();

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
  auto testee = this->buildBlock();

 auto leftoverBlock = buildBlock<1>(this->fakedQuery->rootEngine()->itemBlockManager(),
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


}  // namespace aql
}  // namespace tests
}  // namespace arangodb