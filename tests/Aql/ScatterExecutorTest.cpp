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
  std::vector<std::string> clientIds{"a", "b", "c"};

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

  auto createProducer(SharedAqlItemBlockPtr inputBlock) -> WaitingExecutionBlockMock {
    std::deque<SharedAqlItemBlockPtr> blockDeque;
    // TODO add input splicing
    blockDeque.push_back(inputBlock);
    return createProducer(blockDeque);
  }

  auto createProducer(std::deque<SharedAqlItemBlockPtr> blockDeque) -> WaitingExecutionBlockMock {
    // TODO add input splicing

    return WaitingExecutionBlockMock{fakedQuery->engine(), generateNodeDummy(),
                                     std::move(blockDeque),
                                     WaitingExecutionBlockMock::WaitingBehaviour::NEVER};
  }

  auto ValidateBlocksAreEqual(SharedAqlItemBlockPtr expected, SharedAqlItemBlockPtr actual) {
    ASSERT_NE(expected, nullptr);
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual->size(), expected->size());
    EXPECT_EQ(actual->getNrRegs(), 1);
    for (size_t i = 0; i < (std::min)(actual->size(), expected->size()); ++i) {
      auto const& x = actual->getValueReference(i, 0);
      auto const& y = expected->getValueReference(i, 0);
      EXPECT_TRUE(AqlValue::Compare(&vpackOptions, x, y, true) == 0)
          << "Row " << i << " Column " << 0 << " do not agree. "
          << x.slice().toJson(&vpackOptions) << " vs. "
          << y.slice().toJson(&vpackOptions);
    }
  }
};

TEST_F(ScatterExecutionBlockTest, all_clients_should_get_the_block) {
  auto inputBlock = buildBlock<1>(itemBlockManager, {{0}, {1}, {2}});
  auto producer = createProducer(inputBlock);

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
    ValidateBlocksAreEqual(inputBlock, block);
  }
}

TEST_F(ScatterExecutionBlockTest, all_clients_can_skip_the_block) {
  auto inputBlock = buildBlock<1>(itemBlockManager, {{0}, {1}, {2}});
  auto producer = createProducer(inputBlock);

  ExecutionBlockImpl<ScatterExecutor> testee{fakedQuery->engine(), generateScatterNode(),
                                             generateInfos(), clientIds};
  testee.addDependency(&producer);

  for (auto const& client : clientIds) {
    SCOPED_TRACE("Testing client " + client);
    AqlCall call{};
    call.offset = 10;
    AqlCallStack stack{call};
    auto const [state, skipped, block] = testee.executeForClient(stack, client);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(skipped, 3);
    EXPECT_EQ(block, nullptr);
  }
}

TEST_F(ScatterExecutionBlockTest, all_clients_can_fullcount_the_block) {
  auto inputBlock = buildBlock<1>(itemBlockManager, {{0}, {1}, {2}});
  auto expectedBlock = buildBlock<1>(itemBlockManager, {{0}});
  auto producer = createProducer(inputBlock);

  ExecutionBlockImpl<ScatterExecutor> testee{fakedQuery->engine(), generateScatterNode(),
                                             generateInfos(), clientIds};
  testee.addDependency(&producer);

  for (auto const& client : clientIds) {
    SCOPED_TRACE("Testing client " + client);
    AqlCall call{};
    call.hardLimit = 1;
    call.fullCount = true;
    AqlCallStack stack{call};
    auto const [state, skipped, block] = testee.executeForClient(stack, client);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_EQ(skipped, 2);
    ValidateBlocksAreEqual(block, expectedBlock);
  }
}

TEST_F(ScatterExecutionBlockTest, any_ordering_of_calls_is_fine) {
  std::deque<SharedAqlItemBlockPtr> blockDeque;
  std::unordered_map<std::string, std::pair<size_t, std::vector<SharedAqlItemBlockPtr>>> expected;
  std::vector<std::string> callOrder;
  for (auto const& c : clientIds) {
    expected[c] = std::make_pair(0, std::vector<SharedAqlItemBlockPtr>{});
  }

  {
    auto inputBlock = buildBlock<1>(itemBlockManager, {{0}, {1}, {2}});
    blockDeque.emplace_back(inputBlock);
    for (auto const& c : clientIds) {
      expected[c].second.emplace_back(inputBlock);
      callOrder.emplace_back(c);
    }
  }

  {
    auto inputBlock = buildBlock<1>(itemBlockManager, {{3}, {4}, {5}, {6}});
    blockDeque.emplace_back(inputBlock);
    for (auto const& c : clientIds) {
      expected[c].second.emplace_back(inputBlock);
      callOrder.emplace_back(c);
    }
  }

  {
    auto inputBlock = buildBlock<1>(itemBlockManager, {{7}, {8}, {9}});
    blockDeque.emplace_back(inputBlock);
    for (auto const& c : clientIds) {
      expected[c].second.emplace_back(inputBlock);
      callOrder.emplace_back(c);
    }
  }
  // Every client will ask every block alone.
  ASSERT_EQ(callOrder.size(), clientIds.size() * blockDeque.size());
  // Now we do all permuation of potentiall call ordering
  do {
    auto producer = createProducer(blockDeque);
    ExecutionBlockImpl<ScatterExecutor> testee{fakedQuery->engine(), generateScatterNode(),
                                               generateInfos(), clientIds};
    testee.addDependency(&producer);
    for (auto& [c, pair] : expected) {
      // Reset seen position
      pair.first = 0;
    }
    std::stringstream permutation;
    for (auto c : callOrder) {
      permutation << " " << c;
    }
    SCOPED_TRACE("Testing permutation: " + permutation.str());
    for (auto const& client : callOrder) {
      auto& [callNr, blocks] = expected[client];
      SCOPED_TRACE("Testing client " + client + " call number " + std::to_string(callNr));
      AqlCall call{};
      AqlCallStack stack{call};
      auto const [state, skipped, block] = testee.executeForClient(stack, client);
      if (callNr == 2) {
        EXPECT_EQ(state, ExecutionState::DONE);
      } else {
        EXPECT_EQ(state, ExecutionState::HASMORE);
      }
      EXPECT_EQ(skipped, 0);
      ASSERT_TRUE(callNr < blocks.size());
      ValidateBlocksAreEqual(blocks[callNr], block);
      callNr++;
    }
  } while (std::next_permutation(callOrder.begin(), callOrder.end()));
}

TEST_F(ScatterExecutionBlockTest, all_clients_can_have_different_calls) {
  auto inputBlock =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}});
  auto producer = createProducer(inputBlock);

  ExecutionBlockImpl<ScatterExecutor> testee{fakedQuery->engine(), generateScatterNode(),
                                             generateInfos(), clientIds};
  testee.addDependency(&producer);

  std::vector<std::string> callOrder = clientIds;
  do {
    auto producer = createProducer(inputBlock);

    ExecutionBlockImpl<ScatterExecutor> testee{fakedQuery->engine(), generateScatterNode(),
                                               generateInfos(), clientIds};
    testee.addDependency(&producer);

    for (auto const& client : callOrder) {
      SCOPED_TRACE("Testing client " + client);
      if (client == "a") {
        // Just produce all
        AqlCall call{};
        AqlCallStack stack{call};
        auto const [state, skipped, block] = testee.executeForClient(stack, client);
        EXPECT_EQ(state, ExecutionState::DONE);
        EXPECT_EQ(skipped, 0);
        ValidateBlocksAreEqual(block, inputBlock);
      } else if (client == "b") {
        AqlCall call{};
        call.offset = 2;
        call.hardLimit = 2;
        AqlCallStack stack{call};
        auto const [state, skipped, block] = testee.executeForClient(stack, client);
        EXPECT_EQ(state, ExecutionState::DONE);
        EXPECT_EQ(skipped, 2);
        auto expectedBlock = buildBlock<1>(itemBlockManager, {{2}, {3}});
        ValidateBlocksAreEqual(block, expectedBlock);
      } else if (client == "c") {
        {
          AqlCall call{};
          call.softLimit = 2;
          AqlCallStack stack{call};
          auto const [state, skipped, block] = testee.executeForClient(stack, client);
          EXPECT_EQ(state, ExecutionState::HASMORE);
          EXPECT_EQ(skipped, 0);
          auto expectedBlock = buildBlock<1>(itemBlockManager, {{0}, {1}});
          ValidateBlocksAreEqual(block, expectedBlock);
        }
        {
          // As we have softLimit we can simply call again
          AqlCall call{};
          call.offset = 1;
          call.softLimit = 2;
          AqlCallStack stack{call};
          auto const [state, skipped, block] = testee.executeForClient(stack, client);
          EXPECT_EQ(state, ExecutionState::HASMORE);
          EXPECT_EQ(skipped, 1);
          auto expectedBlock = buildBlock<1>(itemBlockManager, {{3}, {4}});
          ValidateBlocksAreEqual(block, expectedBlock);
        }
      }
    }
  } while (std::next_permutation(callOrder.begin(), callOrder.end()));
}

// TODO:
// [] Add a test that includes subqueries
// [] Add a test that looks after non-relevant subquery forwarding.

}  // namespace arangodb::tests::aql