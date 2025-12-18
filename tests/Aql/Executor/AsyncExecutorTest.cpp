////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Aql/ExecutionState.h"
#include "Aql/Executor/AqlExecutorTestCase.h"
#include "Aql/Executor/EmptyExecutorInfos.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/Executor/MutexExecutor.h"
#include "Aql/ExecutionNode/DistributeConsumerNode.h"
#include "Aql/Executor/ParallelUnsortedGatherExecutor.h"
#include "Aql/MultiDependencySingleRowFetcher.cpp"
#include "Mocks/AcceptanceQueue/FakeAcceptanceQueue.h"

#include "Aql/Executor/AsyncExecutor.h"
#include "Aql/SharedQueryState.h"

#include <random>
#include <stdexcept>
#include <thread>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;

namespace {
void consume_and_check_rows(ExecutionBlock* consumer, size_t expectedRows,
                            ExecutionState expectedState) {
  AqlCallStack callstack{AqlCallList{AqlCall{}}};
  auto const [state, skipped, block] = consumer->execute(callstack);
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->numRows(), expectedRows);
  for (size_t i = 0; i < expectedRows; ++i) {
    auto const& value = block->getValueReference(i, 0);
    EXPECT_TRUE(value.isObject());
  }
  EXPECT_EQ(state, expectedState);
}

void sendHardLimit(ExecutionBlock* consumer) {
  AqlCallStack callstack{
      AqlCallList{AqlCall{0, false, 0, AqlCall::LimitType::HARD}}};
  auto const [state, skipped, block] = consumer->execute(callstack);
  ASSERT_EQ(block, nullptr);
  EXPECT_EQ(state, ExecutionState::DONE);
}

}  // anonymous namespace

struct MutexTestSetup {
  std::unique_ptr<WaitingExecutionBlockMock> waitingBlock;
  std::unique_ptr<ExecutionBlockImpl<MutexExecutor>> mutexExecutor;
  std::vector<std::unique_ptr<ExecutionBlock>> consumers;
};

struct AsyncExecutorTest : AqlExecutorTestCase<false> {
  AsyncExecutorTest() : AqlExecutorTestCase(&acceptanceQueue) {}

  FakeAcceptanceQueue acceptanceQueue;

 protected:
  MutexTestSetup create_mutex_consumers(
      arangodb::aql::AqlItemBlockManager& blockManager, size_t nrOfInputRows,
      size_t nrOfConsumers) {
    // set up the query nodes:
    //
    //                WaitingBlock (nrOfInputRows rows)
    //                      |
    //                 MutexExecutor
    //                   /    \
    //           CONSUMER1   CONSUMER2 (n Consumers based on Input)
    //
    // Both CONSUMER1 and CONSUMER2 are IdExecutors connected to the same
    // MutexExecutor, which is fed by a WaitingBlockMock with nrOfInputRows
    // rows.
    auto registerInfos = RegisterInfos(RegIdSet{}, RegIdSet{}, 1, 1,
                                       RegIdFlatSet{}, RegIdFlatSetStack{{0}});

    // Create input rows in blocks of at most 1000
    std::deque<arangodb::aql::SharedAqlItemBlockPtr> blockDeque;
    size_t rowsLeft = nrOfInputRows;
    std::string dummyJson = "{\"dummy\": \"dummy\"}";
    while (rowsLeft > 0) {
      size_t blockSize = std::min(rowsLeft, static_cast<size_t>(1000));
      tests::aql::MatrixBuilder<1> matrix;
      for (size_t i = 0; i < blockSize; ++i) {
        matrix.push_back({dummyJson.c_str()});
      }
      auto inputBlock =
          tests::aql::buildBlock<1>(blockManager, std::move(matrix));
      blockDeque.push_back(inputBlock);
      rowsLeft -= blockSize;
    }

    auto waitingBlock = std::make_unique<WaitingExecutionBlockMock>(
        fakedQuery->rootEngine(), generateNodeDummy(), std::move(blockDeque),
        WaitingExecutionBlockMock::WaitingBehaviour::NEVER);

    // Create client IDs
    std::vector<std::string> clientIds;
    for (size_t i = 1; i <= nrOfConsumers; ++i) {
      clientIds.push_back(std::to_string(i));
    }

    // Create MutexExecutor
    auto mutexExecutor = std::make_unique<ExecutionBlockImpl<MutexExecutor>>(
        fakedQuery->rootEngine(), generateMutexNodeDummy(), registerInfos,
        MutexExecutorInfos(clientIds));
    mutexExecutor->addDependency(waitingBlock.get());

    // Create consumers
    std::vector<std::unique_ptr<ExecutionBlock>> consumers;
    for (size_t i = 1; i <= nrOfConsumers; ++i) {
      auto consumer = std::make_unique<ExecutionBlockImpl<
          IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>>(
          fakedQuery->rootEngine(),
          generateDistributeConsumerNode(std::to_string(i)), registerInfos,
          IdExecutorInfos(false, RegisterId(0), std::to_string(i), false));
      consumer->addDependency(mutexExecutor.get());
      consumers.push_back(std::move(consumer));
    }

    MutexTestSetup setup;
    setup.waitingBlock = std::move(waitingBlock);
    setup.mutexExecutor = std::move(mutexExecutor);
    setup.consumers = std::move(consumers);
    return setup;
  }
};

// Regression test for https://arangodb.atlassian.net/browse/BTS-1325.
// See https://github.com/arangodb/arangodb/pull/18729 for details.
TEST_F(AsyncExecutorTest, sleepingBeauty) {
  auto registerInfos = RegisterInfos(RegIdSet{}, RegIdSet{}, 1, 1,
                                     RegIdFlatSet{}, RegIdFlatSetStack{{0}});

  auto testHelper = makeExecutorTestHelper();
  testHelper
      .addDependency<AsyncExecutor>(registerInfos, {}, ExecutionNode::ASYNC)
      .addDependency<AsyncExecutor>(registerInfos, {}, ExecutionNode::ASYNC)
      .setInputFromRowNum(1)
      .setWaitingBehaviour(WaitingExecutionBlockMock::WaitingBehaviour::ALWAYS)
      .setCall(AqlCall{0u, AqlCall::Infinity{}, AqlCall::Infinity{}, false});

  auto* asyncBlock0 = dynamic_cast<ExecutionBlockImpl<AsyncExecutor>*>(
      testHelper.pipeline().get().at(0).get());
  // Having the nodes in a certain order (i.e. pipeline[0].id() == 0, and
  // pipeline[1].id() == 1), makes reading profiles less confusing.
  ASSERT_EQ(asyncBlock0->getPlanNode()->id().id(), 0);

  // one initial "wakeup" to start execution
  auto wakeupsQueued = 1;
  auto wakeupHandler = [&wakeupsQueued]() noexcept {
    ++wakeupsQueued;
    return true;
  };
  testHelper.setWakeupHandler(wakeupHandler);
  testHelper.setWakeupCallback(wakeupHandler);
  testHelper.prepareInput();

  // The postAsyncExecuteCallback is called _after_ the AsyncNode has called
  // execute() on its dependency, but _before_ the results of that call is
  // stored.  So the node is still "inProgress".  By making sure all tasks and
  // wakeups are processed before leaving the callback, we simulate the thread
  // being slow.
  asyncBlock0->setPostAsyncExecuteCallback([&](ExecutionState) {
    while (!acceptanceQueue.queueEmpty()) {
      acceptanceQueue.runOnce();
    }
    while (wakeupsQueued > 0) {
      --wakeupsQueued;
      testHelper.executeOnce();
    }
  });

  while (wakeupsQueued > 0 || !acceptanceQueue.queueEmpty()) {
    while (wakeupsQueued > 0) {
      --wakeupsQueued;
      testHelper.executeOnce();
    }
    if (!acceptanceQueue.queueEmpty()) {
      acceptanceQueue.runOnce();
    }
  };

  EXPECT_EQ(0, wakeupsQueued);
  EXPECT_TRUE(acceptanceQueue.queueEmpty());

  testHelper.expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{0}})
      .expectSkipped(0)
      .checkExpectations();

  ASSERT_TRUE(testHelper.sharedState()->noTasksRunning());
}

// Regression test for https://arangodb.atlassian.net/browse/BTS-1325.
// See https://github.com/arangodb/arangodb/pull/18729 for details.
// A randomized variant of the test above, that might be able to find other
// kinds of errors, but only sometimes catches the original bug.
TEST_F(AsyncExecutorTest, sleepingBeautyRandom) {
  auto seed = std::random_device{}();
  auto gen = std::mt19937();
  gen.seed(seed);
  // Make the seed available when the test fails, so a failure can be reproduced
  // deterministically.
  RecordProperty("seed", seed);
  SCOPED_TRACE(fmt::format("seed={}", seed));

  auto registerInfos = RegisterInfos(RegIdSet{}, RegIdSet{}, 1, 1,
                                     RegIdFlatSet{}, RegIdFlatSetStack{{0}});

  auto testHelper = makeExecutorTestHelper();
  testHelper
      .addDependency<AsyncExecutor>(registerInfos, {}, ExecutionNode::ASYNC)
      .addDependency<AsyncExecutor>(registerInfos, {}, ExecutionNode::ASYNC)
      .setInputFromRowNum(1)
      .setWaitingBehaviour(WaitingExecutionBlockMock::WaitingBehaviour::ALWAYS)
      .setCall(AqlCall{0u, AqlCall::Infinity{}, AqlCall::Infinity{}, false});

  auto* asyncBlock0 = dynamic_cast<ExecutionBlockImpl<AsyncExecutor>*>(
      testHelper.pipeline().get().at(0).get());
  auto* asyncBlock1 = dynamic_cast<ExecutionBlockImpl<AsyncExecutor>*>(
      testHelper.pipeline().get().at(1).get());

  // Having the nodes in a certain order (i.e. pipeline[0].id() == 0, and
  // pipeline[1].id() == 1), makes reading profiles less confusing.
  ASSERT_EQ(asyncBlock0->getPlanNode()->id().id(), 0);
  ASSERT_EQ(asyncBlock1->getPlanNode()->id().id(), 1);

  // one initial "wakeup" to start execution
  auto wakeupsQueued = 1;
  auto wakeupHandler = [&wakeupsQueued]() noexcept {
    ++wakeupsQueued;
    return true;
  };
  testHelper.setWakeupHandler(wakeupHandler);
  testHelper.setWakeupCallback(wakeupHandler);
  testHelper.prepareInput();

  auto const somethingToDo = [&] {
    return !acceptanceQueue.queueEmpty() or wakeupsQueued > 0;
  };

  auto const doSomething = [&](bool everything) {
    while (somethingToDo()) {
      auto const queueSize = acceptanceQueue.queueSize();
      auto max = queueSize;
      if (wakeupsQueued > 0) {
        ++max;
      }
      TRI_ASSERT(max > 0) << "error in test logic";
      // [0, queueSize - 1] means run a scheduler task;
      // if wakeupsQueued > 0, [queueSize] means wake up the "rest handler";
      // [max] means stop doing anything (but can not be reached if
      //       `everything` is true).
      auto dist = std::uniform_int_distribution<std::size_t>(
          0, everything ? max - 1 : max);
      auto actionIdx = dist(gen);

      if (actionIdx < queueSize) {
        acceptanceQueue.runOne(actionIdx);
      } else if (actionIdx == max) {
        TRI_ASSERT(!everything);
        return;
      } else {
        TRI_ASSERT(actionIdx == queueSize);
        TRI_ASSERT(wakeupsQueued > 0);
        --wakeupsQueued;
        testHelper.executeOnce();
      }
    }
  };

  // get the "lower" of the two async blocks
  asyncBlock0->setPostAsyncExecuteCallback(
      [&](ExecutionState) { doSomething(false); });
  // get the "upper" of the two async blocks
  asyncBlock1->setPostAsyncExecuteCallback(
      [&](ExecutionState) { doSomething(false); });

  doSomething(true);

  EXPECT_EQ(0, wakeupsQueued);
  EXPECT_TRUE(acceptanceQueue.queueEmpty());

  testHelper.expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{0}})
      .expectSkipped(0)
      .checkExpectations();

  ASSERT_TRUE(testHelper.sharedState()->noTasksRunning());
}

TEST_F(AsyncExecutorTest, WAITING_result_should_not_trigger_wakeup) {
  auto registerInfos = RegisterInfos(RegIdSet{}, RegIdSet{}, 1, 1,
                                     RegIdFlatSet{}, RegIdFlatSetStack{{0}});

  auto testHelper = makeExecutorTestHelper();
  testHelper
      .addDependency<AsyncExecutor>(registerInfos, {}, ExecutionNode::ASYNC)
      .setInputFromRowNum(1)
      .setWaitingBehaviour(WaitingExecutionBlockMock::WaitingBehaviour::ONCE)
      .setCall(AqlCall{0u, AqlCall::Infinity{}, AqlCall::Infinity{}, false});

  auto* asyncBlock0 = dynamic_cast<ExecutionBlockImpl<AsyncExecutor>*>(
      testHelper.pipeline().get().at(0).get());
  // Having the nodes in a certain order (i.e. pipeline[0].id() == 0, and
  // pipeline[1].id() == 1), makes reading profiles less confusing.
  ASSERT_EQ(asyncBlock0->getPlanNode()->id().id(), 0);

  // one initial "wakeup" to start execution
  auto wakeupsQueued = 1;
  auto totalWakeups = 0;
  auto wakeupHandler = [&wakeupsQueued, &totalWakeups]() noexcept {
    ++wakeupsQueued;
    ++totalWakeups;
    // we are woken up once from the WaitingExecutionBlockMock and once from the
    // scheduler
    EXPECT_LE(totalWakeups, 2);
    return true;
  };
  testHelper.setWakeupHandler(wakeupHandler);
  testHelper.setWakeupCallback(wakeupHandler);
  testHelper.prepareInput();

  while (wakeupsQueued > 0 || !acceptanceQueue.queueEmpty()) {
    while (wakeupsQueued > 0) {
      --wakeupsQueued;
      testHelper.executeOnce();
    }
    if (!acceptanceQueue.queueEmpty()) {
      acceptanceQueue.runOnce();
    }
  };

  EXPECT_EQ(0, wakeupsQueued);
  EXPECT_TRUE(acceptanceQueue.queueEmpty());

  testHelper.expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{0}})
      .expectSkipped(0)
      .checkExpectations();

  ASSERT_TRUE(testHelper.sharedState()->noTasksRunning());
}

TEST_F(AsyncExecutorTest,
       WAITING_result_should_trigger_wakeup_in_case_of_exception) {
  auto registerInfos = RegisterInfos(RegIdSet{}, RegIdSet{}, 1, 1,
                                     RegIdFlatSet{}, RegIdFlatSetStack{{0}});

  auto testHelper = makeExecutorTestHelper();
  testHelper
      .addDependency<AsyncExecutor>(registerInfos, {}, ExecutionNode::ASYNC)
      .setInputFromRowNum(1)
      .setWaitingBehaviour(WaitingExecutionBlockMock::WaitingBehaviour::ALWAYS)
      .setCall(AqlCall{0u, AqlCall::Infinity{}, AqlCall::Infinity{}, false});

  auto* asyncBlock0 = dynamic_cast<ExecutionBlockImpl<AsyncExecutor>*>(
      testHelper.pipeline().get().at(0).get());
  // Having the nodes in a certain order (i.e. pipeline[0].id() == 0, and
  // pipeline[1].id() == 1), makes reading profiles less confusing.
  ASSERT_EQ(asyncBlock0->getPlanNode()->id().id(), 0);

  // one initial "wakeup" to start execution
  auto wakeupsQueued = 1;
  auto wakeupHandler = [&wakeupsQueued]() noexcept {
    ++wakeupsQueued;
    return true;
  };
  testHelper.setWakeupHandler(wakeupHandler);
  testHelper.setWakeupCallback(wakeupHandler);

  auto executeCalls = 0;
  testHelper.setExecuteCallback([&executeCalls]() {
    ASSERT_LE(executeCalls, 1);
    ++executeCalls;
    if (executeCalls > 1) {
      throw std::runtime_error("test exception");
    }
  });
  testHelper.prepareInput();

  try {
    while (wakeupsQueued > 0 || !acceptanceQueue.queueEmpty()) {
      while (wakeupsQueued > 0) {
        --wakeupsQueued;
        testHelper.executeOnce();
      }
      if (!acceptanceQueue.queueEmpty()) {
        acceptanceQueue.runOnce();
      }
    };
    FAIL() << "expected test exception to be thrown";
  } catch (std::runtime_error const& e) {
    EXPECT_EQ(e.what(), std::string("test exception"));
  }

  EXPECT_EQ(0, wakeupsQueued);
  EXPECT_TRUE(acceptanceQueue.queueEmpty());
  EXPECT_EQ(2, executeCalls);

  ASSERT_TRUE(testHelper.sharedState()->noTasksRunning());
}

TEST_F(AsyncExecutorTest, AsyncNode_does_not_return_stored_WAITING) {
  auto registerInfos = RegisterInfos(RegIdSet{}, RegIdSet{}, 1, 1,
                                     RegIdFlatSet{}, RegIdFlatSetStack{{0}});
  GlobalResourceMonitor globalMonitor;
  arangodb::ResourceMonitor monitor(globalMonitor);
  arangodb::aql::AqlItemBlockManager blockManager(monitor);

  // set up the query nodes:
  //
  //           ASYNC1 - CONSUMER1
  //          /                  \
  //    GATHER                    MUTEX - WaitingBlock
  //          \                  /
  //           ASYNC2 - CONSUMER2

  auto gather =
      std::make_unique<ExecutionBlockImpl<ParallelUnsortedGatherExecutor>>(
          fakedQuery->rootEngine(),
          this->generateNodeDummy(ExecutionNode::GATHER), registerInfos,
          EmptyExecutorInfos());

  auto async1 = std::make_unique<ExecutionBlockImpl<AsyncExecutor>>(
      fakedQuery->rootEngine(), this->generateNodeDummy(ExecutionNode::ASYNC));

  auto async2 = std::make_unique<ExecutionBlockImpl<AsyncExecutor>>(
      fakedQuery->rootEngine(), this->generateNodeDummy(ExecutionNode::ASYNC));

  gather->addDependency(async1.get());
  gather->addDependency(async2.get());

  auto consumer1 = std::make_unique<ExecutionBlockImpl<
      IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>>(
      fakedQuery->rootEngine(), generateDistributeConsumerNode("client1"),
      registerInfos, IdExecutorInfos(false, RegisterId(0), "client1", false));
  async1->addDependency(consumer1.get());

  auto consumer2 = std::make_unique<ExecutionBlockImpl<
      IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>>(
      fakedQuery->rootEngine(), generateDistributeConsumerNode("client2"),
      registerInfos, IdExecutorInfos(false, RegisterId(0), "client2", false));
  async2->addDependency(consumer2.get());

  auto mutex = std::make_unique<ExecutionBlockImpl<MutexExecutor>>(
      fakedQuery->rootEngine(), this->generateMutexNodeDummy(), registerInfos,
      // note: we use client ids in reverse order, because we want the async
      // node1 to perform the first call, but the async node2 to receive the
      // only result block
      MutexExecutorInfos{{"client2", "client1"}});

  consumer1->addDependency(mutex.get());
  consumer2->addDependency(mutex.get());

  std::deque<arangodb::aql::SharedAqlItemBlockPtr> blockDeque;
  auto block = blockManager.requestBlock(1, 1);
  blockDeque.emplace_back(std::move(block));
  auto waitingBlock = std::make_unique<WaitingExecutionBlockMock>(
      fakedQuery->rootEngine(), generateNodeDummy(), std::move(blockDeque),
      WaitingExecutionBlockMock::WaitingBehaviour::ONCE);

  mutex->addDependency(waitingBlock.get());

  // we have two async nodes, but set the max tasks to one to make the second
  // async node execute _synchronously_
  fakedQuery->rootEngine()->sharedState()->setMaxTasks(1);

  // we use a call with a soft limit of 1 to simulate a full block
  AqlCallStack callstack{AqlCallList{AqlCall{0, AqlCall::Limit{1ul}}}};

  // we set the wakeup handler to simulate the existence of a RestHandler.
  // This is necessary so that the AsyncExecutor can trigger a wakeup and
  // schedule the next task.
  fakedQuery->rootEngine()->sharedState()->setWakeupHandler([]() {
    throw std::runtime_error("should not be called");
    return true;
  });

  std::atomic<bool> taskActive{false};

  auto switchToAsync1Task = [&]() {
    ASSERT_FALSE(taskActive.load());
    taskActive.store(true);
    taskActive.notify_one();
    taskActive.wait(true);
  };

  auto switchToMainTask = [&]() {
    ASSERT_TRUE(taskActive.load());
    taskActive.store(false);
    taskActive.notify_one();
    // wait for the task to finish
    taskActive.wait(false);
  };

  std::thread acceptanceQueueThread([&]() {
    taskActive.wait(false);

    ASSERT_EQ(acceptanceQueue.queueSize(), 1);
    acceptanceQueue.runOnce();

    taskActive.store(false);
    taskActive.notify_one();
  });

  // we set a post async call back for the first async node that should return
  // WAITING, but before the result is stored we switch back to the main task to
  // run the second async node
  async1->setPostAsyncExecuteCallback([&](ExecutionState state) {
    EXPECT_TRUE(taskActive.load());
    EXPECT_EQ(state, ExecutionState::WAITING);

    // we simulate that the task is suspended/slow _after_ we have performed the
    // upstream call, but _before_ we store the result
    switchToMainTask();
    // have
    // // called the first async node and receive a DONE. Since the gather node
    // has
    // // one more client but already received a full block, it should return
    // // HASMORE _without_ calling the second async node.
    // ASSERT_EQ(gatherState, ExecutionState::HASMORE);
  });

  async2->setBeforeAsyncExecuteCallback([&]() {
    // the first async node should have scheduled a task. We want to run that
    // task, but _not_ store the result before the second async node is run.
    EXPECT_EQ(acceptanceQueue.queueSize(), 1);

    // let the first part of he async1 node task run (perform the call to the
    // upstream), but do _not_ yet store the result, before we perform our call
    switchToAsync1Task();

    // let the remaining part of the query run
  });
  bool async2Called = false;
  async2->setPostAsyncExecuteCallback([&async2Called](ExecutionState state) {
    // async node2 should be called only once!
    EXPECT_FALSE(async2Called);
    async2Called = true;
    EXPECT_EQ(state, ExecutionState::DONE);
  });

  {
    auto const [state, skipped, result] = gather->execute(callstack);
    ASSERT_EQ(state, ExecutionState::HASMORE);
  }

  // we now reset the wakeup handler to simulate that the RestHandler is done
  // this will prevent a wakeup scheduled by async1 to schedule another task
  fakedQuery->rootEngine()->sharedState()->resetWakeupHandler();

  // now let the async1 task finish - this would trigger a wakeup, but since the
  // RestHandler is gone and the wakeupHandler has been reset, this wakeup is
  // swallowed
  switchToAsync1Task();

  acceptanceQueueThread.join();

  ASSERT_TRUE(acceptanceQueue.queueEmpty());

  // set another wakeup handler to simulate the existence of a new
  // RestHandler.
  ExecutionState gatherState{-1};
  fakedQuery->rootEngine()->sharedState()->setWakeupHandler(
      [&gather, &callstack, &gatherState]() {
        auto const [state, skipped, result] = gather->execute(callstack);
        gatherState = state;
        return true;
      });

  {
    async1->setPostAsyncExecuteCallback(nullptr);
    // run the gather node - this will ask only the first async node (the second
    // one is already done), which has stored WAITING, but should not return it
    // and instead trigger a new task
    auto const [state, skipped, result] = gather->execute(callstack);
    ASSERT_EQ(state, ExecutionState::WAITING);
    ASSERT_EQ(acceptanceQueue.queueSize(), 1);
  }

  // run the scheduled task from async node1. This should trigger a wakeup that
  // schedules yet another task
  acceptanceQueue.runOnce();

  ASSERT_EQ(acceptanceQueue.queueSize(), 1);

  // run scheduled task which should execute the previously set wakeup handler
  acceptanceQueue.runOnce();
  ASSERT_EQ(gatherState, ExecutionState::DONE);
}

TEST_F(AsyncExecutorTest, two_consumers_receive_rows_from_mutex_executor) {
  GlobalResourceMonitor globalMonitor;
  arangodb::ResourceMonitor monitor(globalMonitor);
  arangodb::aql::AqlItemBlockManager blockManager(monitor);
  auto setup = create_mutex_consumers(blockManager, 3000, 2);
  auto& inputBlock = setup.waitingBlock;
  auto& mutexBlock = setup.mutexExecutor;

  // Create two consumers (IdExecutors) that depend on the same MutexExecutor
  auto& consumer1 = setup.consumers[0];
  auto& consumer2 = setup.consumers[1];

  // Test if we can read data one block after the other
  // Note we do get 500 rows each time, as the MutexExecutor does round robin
  // splitting of a single 1000 lines input. If we add a third consumer we would
  // get 333 or 334 lines each time.
  consume_and_check_rows(consumer1.get(), 500, ExecutionState::HASMORE);
  consume_and_check_rows(consumer1.get(), 500, ExecutionState::HASMORE);
  consume_and_check_rows(consumer1.get(), 500, ExecutionState::DONE);

  EXPECT_EQ(inputBlock->remainingRows(), 0)
      << "One consumer fetched everything. The block now needs to be empty";
  EXPECT_NE(inputBlock->getLastCall().getLimit(), 0)
      << "No hardlimit was asked";

  // Note: The second consumer will see 1000 rows on the first run, as it's
  // block has been filled to completion by the other task
  consume_and_check_rows(consumer2.get(), 1000, ExecutionState::HASMORE);
  consume_and_check_rows(consumer2.get(), 500, ExecutionState::DONE);
  EXPECT_EQ(mutexBlock->remainingRowsForClient("1"), 0)
      << "Client 1 should have no rows left in flight";
  EXPECT_EQ(mutexBlock->remainingRowsForClient("2"), 0)
      << "Client 2 should have no rows left in flight";
}

TEST_F(AsyncExecutorTest, two_consumers_one_early_abort) {
  GlobalResourceMonitor globalMonitor;
  arangodb::ResourceMonitor monitor(globalMonitor);
  arangodb::aql::AqlItemBlockManager blockManager(monitor);
  auto setup = create_mutex_consumers(blockManager, 3000, 2);
  auto& inputBlock = setup.waitingBlock;
  auto& mutexBlock = setup.mutexExecutor;

  // Create two consumers (IdExecutors) that depend on the same MutexExecutor
  auto& consumer1 = setup.consumers[0];
  auto& consumer2 = setup.consumers[1];

  // Test first block sends a hard limit 0, basically it does not need anything.
  sendHardLimit(consumer1.get());
  EXPECT_EQ(inputBlock->remainingRows(), 3000)
      << "No rows are asked for, hardLimit should not trigger an upstream "
         "request. But we cannot yet discard rows";
  EXPECT_NE(inputBlock->getLastCall().getLimit(), 0)
      << "We asked for a hard limit, but only on one consumer, it cannot be "
         "forwarded to the input block";
  EXPECT_EQ(mutexBlock->remainingRowsForClient("1"), 0)
      << "Client 1 should have no rows left in flight";

  // Note: The second consumer will still see all it's input
  consume_and_check_rows(consumer2.get(), 500, ExecutionState::HASMORE);
  EXPECT_EQ(mutexBlock->remainingRowsForClient("1"), 0)
      << "Client 1 should have no rows left in flight";
  consume_and_check_rows(consumer2.get(), 500, ExecutionState::HASMORE);
  EXPECT_EQ(mutexBlock->remainingRowsForClient("1"), 0)
      << "Client 1 should have no rows left in flight";
  consume_and_check_rows(consumer2.get(), 500, ExecutionState::DONE);
  EXPECT_EQ(mutexBlock->remainingRowsForClient("1"), 0)
      << "Client 1 should have no rows left in flight";
  EXPECT_EQ(mutexBlock->remainingRowsForClient("2"), 0)
      << "Client 2 should have no rows left in flight";
}

TEST_F(AsyncExecutorTest, two_consumers_both_early_abort) {
  GlobalResourceMonitor globalMonitor;
  arangodb::ResourceMonitor monitor(globalMonitor);
  arangodb::aql::AqlItemBlockManager blockManager(monitor);
  auto setup = create_mutex_consumers(blockManager, 3000, 2);
  auto& inputBlock = setup.waitingBlock;
  auto& mutexBlock = setup.mutexExecutor;

  // Create two consumers (IdExecutors) that depend on the same MutexExecutor
  auto& consumer1 = setup.consumers[0];
  auto& consumer2 = setup.consumers[1];

  // Test first block consumes some data, then sends hard limit 0
  consume_and_check_rows(consumer1.get(), 500, ExecutionState::HASMORE);
  sendHardLimit(consumer1.get());
  EXPECT_EQ(inputBlock->remainingRows(), 2000)
      << "We asked for 500 rows but rows are split equally between two "
         "consumer, so 2000 rows should be left";
  EXPECT_NE(inputBlock->getLastCall().getLimit(), 0)
      << "We asked for a hard limit, but only on one consumer, it cannot be "
         "forwarded to the input block";

  // Note: The second consumer can still get some input, but also request Hard
  // limit afterwards
  consume_and_check_rows(consumer2.get(), 500, ExecutionState::HASMORE);
  EXPECT_EQ(inputBlock->remainingRows(), 2000)
      << "We asked for 500 more rows but rows are split equally between two "
         "consumer, so 2000 rows should be left";
  EXPECT_NE(inputBlock->getLastCall().getLimit(), 0)
      << "We asked for a hard limit, but only on one consumer, it cannot be "
         "forwarded to the input block";

  sendHardLimit(consumer2.get());
  EXPECT_EQ(inputBlock->remainingRows(), 0)
      << "Hardlimit should consume all rows";
  EXPECT_EQ(inputBlock->getLastCall().getLimit(), 0)
      << "Now both consumers are on hardlimit, so it can be forwarded to the "
         "input block";
  EXPECT_EQ(mutexBlock->remainingRowsForClient("1"), 0)
      << "Client 1 should have no rows left in flight";
  EXPECT_EQ(mutexBlock->remainingRowsForClient("2"), 0)
      << "Client 2 should have no rows left in flight";
}

TEST_F(AsyncExecutorTest,
       two_consumers_second_early_abort_first_can_reach_limit) {
  GlobalResourceMonitor globalMonitor;
  arangodb::ResourceMonitor monitor(globalMonitor);
  arangodb::aql::AqlItemBlockManager blockManager(monitor);
  auto setup = create_mutex_consumers(blockManager, 3000, 2);
  auto& inputBlock = setup.waitingBlock;
  auto& mutexBlock = setup.mutexExecutor;

  // Create two consumers (IdExecutors) that depend on the same MutexExecutor
  auto& consumer1 = setup.consumers[0];
  auto& consumer2 = setup.consumers[1];

  consume_and_check_rows(consumer1.get(), 500, ExecutionState::HASMORE);
  consume_and_check_rows(consumer2.get(), 500, ExecutionState::HASMORE);
  EXPECT_EQ(inputBlock->remainingRows(), 2000)
      << "We asked for 1000 rows equally between two consumer, so 2000 rows "
         "should be left";
  EXPECT_NE(inputBlock->getLastCall().getLimit(), 0)
      << "We did not ask for a hard limit, so the last call should not be a "
         "hard limit";
  // Now stop consumer 2
  sendHardLimit(consumer2.get());
  EXPECT_EQ(inputBlock->remainingRows(), 2000)
      << "HardLimit 0, should not trigger a pull from the input block";
  EXPECT_NE(inputBlock->getLastCall().getLimit(), 0)
      << "Only one consumer asked for a hard limit, this cannot be forwarded "
         "to the input block";

  // Let consumer 1 continue until end
  consume_and_check_rows(consumer1.get(), 500, ExecutionState::HASMORE);
  consume_and_check_rows(consumer1.get(), 500, ExecutionState::DONE);
  EXPECT_EQ(inputBlock->remainingRows(), 0)
      << "All rows should have been consumed";
  EXPECT_NE(inputBlock->getLastCall().getLimit(), 0)
      << "The other consumer consumed all input, no hardlimit can be send";
  EXPECT_EQ(mutexBlock->remainingRowsForClient("1"), 0)
      << "Client 1 should have no rows left in flight";
  EXPECT_EQ(mutexBlock->remainingRowsForClient("2"), 0)
      << "Client 2 should have no rows left in flight";
}