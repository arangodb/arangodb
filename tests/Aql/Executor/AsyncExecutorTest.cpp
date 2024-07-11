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
#include "Mocks/FakeScheduler.h"

#include "Aql/Executor/AsyncExecutor.h"
#include "Aql/SharedQueryState.h"

#include <random>
#include <stdexcept>
#include <thread>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;

struct AsyncExecutorTest : AqlExecutorTestCase<false> {
  AsyncExecutorTest()
      : AqlExecutorTestCase(&scheduler), scheduler(_server->server()) {}

  FakeScheduler scheduler;
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
    while (!scheduler.queueEmpty()) {
      scheduler.runOnce();
    }
    while (wakeupsQueued > 0) {
      --wakeupsQueued;
      testHelper.executeOnce();
    }
  });

  while (wakeupsQueued > 0 || !scheduler.queueEmpty()) {
    while (wakeupsQueued > 0) {
      --wakeupsQueued;
      testHelper.executeOnce();
    }
    if (!scheduler.queueEmpty()) {
      scheduler.runOnce();
    }
  };

  EXPECT_EQ(0, wakeupsQueued);
  EXPECT_TRUE(scheduler.queueEmpty());

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
    return !scheduler.queueEmpty() or wakeupsQueued > 0;
  };

  auto const doSomething = [&](bool everything) {
    while (somethingToDo()) {
      auto const queueSize = scheduler.queueSize();
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
        scheduler.runOne(actionIdx);
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
  EXPECT_TRUE(scheduler.queueEmpty());

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

  while (wakeupsQueued > 0 || !scheduler.queueEmpty()) {
    while (wakeupsQueued > 0) {
      --wakeupsQueued;
      testHelper.executeOnce();
    }
    if (!scheduler.queueEmpty()) {
      scheduler.runOnce();
    }
  };

  EXPECT_EQ(0, wakeupsQueued);
  EXPECT_TRUE(scheduler.queueEmpty());

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
    while (wakeupsQueued > 0 || !scheduler.queueEmpty()) {
      while (wakeupsQueued > 0) {
        --wakeupsQueued;
        testHelper.executeOnce();
      }
      if (!scheduler.queueEmpty()) {
        scheduler.runOnce();
      }
    };
    FAIL() << "expected test exception to be thrown";
  } catch (std::runtime_error const& e) {
    EXPECT_EQ(e.what(), std::string("test exception"));
  }

  EXPECT_EQ(0, wakeupsQueued);
  EXPECT_TRUE(scheduler.queueEmpty());
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

  std::thread schedulerThread([&]() {
    taskActive.wait(false);

    ASSERT_EQ(scheduler.queueSize(), 1);
    scheduler.runOnce();

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
    EXPECT_EQ(scheduler.queueSize(), 1);

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

  schedulerThread.join();

  ASSERT_TRUE(scheduler.queueEmpty());

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
    ASSERT_EQ(scheduler.queueSize(), 1);
  }

  // run the scheduled task from async node1. This should trigger a wakeup that
  // schedules yet another task
  scheduler.runOnce();

  ASSERT_EQ(scheduler.queueSize(), 1);

  // run scheduled task which should execute the previously set wakeup handler
  scheduler.runOnce();
  ASSERT_EQ(gatherState, ExecutionState::DONE);
}
