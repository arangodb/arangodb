////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Aql/AqlExecutorTestCase.h"
#include "Mocks/FakeScheduler.h"

#include "Aql/AsyncExecutor.h"

#include <random>

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

  asyncBlock0->setFailureCallback([&] {
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
  asyncBlock0->setFailureCallback([&] { doSomething(false); });
  // get the "upper" of the two async blocks
  asyncBlock1->setFailureCallback([&] { doSomething(false); });

  doSomething(true);

  EXPECT_EQ(0, wakeupsQueued);
  EXPECT_TRUE(scheduler.queueEmpty());

  testHelper.expectedState(ExecutionState::DONE)
      .expectOutput({0}, {{0}})
      .expectSkipped(0)
      .checkExpectations();

  ASSERT_TRUE(testHelper.sharedState()->noTasksRunning());
}
