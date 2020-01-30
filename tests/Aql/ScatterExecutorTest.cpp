////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AqlCall.h"
#include "Aql/AqlCallStack.h"
#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/Query.h"
#include "Aql/ScatterExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

class ScatterExecutionBlockTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server{};
  ResourceMonitor monitor{};
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};
  std::unique_ptr<arangodb::aql::Query> fakedQuery{server.createFakeQuery()};
  std::vector<std::unique_ptr<ExecutionNode>> _execNodes;
  velocypack::Options vpackOptions;

  ScatterExecutionBlockTest() {
    auto engine =
        std::make_unique<ExecutionEngine>(*fakedQuery, SerializationFormat::SHADOWROWS);
    fakedQuery->setEngine(engine.release());
  }

  /**
   * @brief Creates and manages a ExecutionNode.
   *        These nodes can be used to create the Executors
   *        Caller does not need to manage the memory.
   *
   * @return ExecutionNode* Pointer to a dummy ExecutionNode. Memory is managed, do not delete.
   */
  auto generateNodeDummy() -> ExecutionNode* {
    auto dummy = std::make_unique<SingletonNode>(fakedQuery->plan(), _execNodes.size());
    auto res = dummy.get();
    _execNodes.emplace_back(std::move(dummy));
    return res;
  }

  auto generateScatterNode() -> ScatterNode* {
    auto dummy = std::make_unique<ScatterNode>(fakedQuery->plan(), _execNodes.size(),
                                               ScatterNode::ScatterType::SHARD);
    auto res = dummy.get();
    _execNodes.emplace_back(std::move(dummy));
    return res;
  }

  auto generateInfos() const -> ExecutorInfos {
    auto inputRegs = make_shared_unordered_set({0});
    auto outputRegs = make_shared_unordered_set({});
    return {inputRegs, outputRegs, 1, 1, {}, {0}};
  }
};

TEST_F(ScatterExecutionBlockTest, all_clients_should_get_the_block) {
  auto inputBlock = buildBlock<1>(itemBlockManager, {{0}, {1}, {2}});
  std::deque<SharedAqlItemBlockPtr> blockDeque;
  blockDeque.push_back(inputBlock);

  WaitingExecutionBlockMock producer(fakedQuery->engine(), generateNodeDummy(),
                                     std::move(blockDeque),
                                     WaitingExecutionBlockMock::WaitingBehaviour::NEVER);
  std::vector<std::string> clientIds{"a", "b", "c"};
  ExecutionBlockImpl<ScatterExecutor> testee{fakedQuery->engine(), generateScatterNode(),
                                             generateInfos(), clientIds};
  testee.addDependency(&producer);

  for (auto const& client : clientIds) {
    SCOPED_TRACE("Testing client " + client);
    AqlCall call{};  // DefaultCall
    AqlCallStack stack{call};
    auto const [state, skipped, block] = testee.executeForClient(stack, client);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(skipped, 0);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(block->size(), inputBlock->size());
    EXPECT_EQ(block->getNrRegs(), 1);
    for (size_t i = 0; i < block->size(); ++i) {
      auto const& x = block->getValueReference(i, 0);
      auto const& y = inputBlock->getValueReference(i, 0);
      EXPECT_TRUE(AqlValue::Compare(&vpackOptions, x, y, true) == 0)
          << "Row " << i << " Column " << 0 << " do not agree";
    }
  }
}

// TODO:
// [] Add a test that includes subqueries
// [] Add a test that looks after non-relevant subquery forwarding.

}  // namespace arangodb::tests::aql