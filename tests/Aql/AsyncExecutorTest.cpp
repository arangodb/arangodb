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

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;

struct AsyncExecutorTest : AqlExecutorTestCase<false> {
  AsyncExecutorTest()
      : AqlExecutorTestCase(&scheduler), scheduler(_server->server()) {}

  FakeScheduler scheduler;
};

TEST_F(AsyncExecutorTest, sleepingBeauty) {
  auto registerInfos = RegisterInfos(RegIdSet{}, RegIdSet{}, 1, 1,
                                     RegIdFlatSet{}, RegIdFlatSetStack{{0}});

  TRI_AddFailurePointDebugging("AsyncExecutor::SleepWhenWaiting");

  auto testHelper = makeExecutorTestHelper();
  testHelper.addConsumer<AsyncExecutor>(registerInfos, {}, ExecutionNode::ASYNC)
      .addConsumer<AsyncExecutor>(registerInfos, {}, ExecutionNode::ASYNC)
      .setInputFromRowNum(1)
      .setWaitingBehaviour(WaitingExecutionBlockMock::WaitingBehaviour::ALWAYS)
      .setCall(AqlCall{0u, AqlCall::Infinity{}, AqlCall::Infinity{}, false});

  // one initial "wakeup" to start execution
  auto wakeupsQueued = 1;
  auto wakeupHandler = [&wakeupsQueued]() noexcept {
    ++wakeupsQueued;
    return true;
  };
  testHelper.setWakeupHandler(wakeupHandler);
  testHelper.setWakeupCallback(wakeupHandler);
  testHelper.prepareInput();

  auto* block = testHelper.pipeline().get()[0].get();
  auto* asyncBlock = dynamic_cast<ExecutionBlockImpl<AsyncExecutor>*>(block);
  asyncBlock->setFailureCallback([&] {
    while (!scheduler.queueEmpty()) {
      scheduler.runOnce();
    }
    while (wakeupsQueued > 0) {
      --wakeupsQueued;
      testHelper.executeOnce();
    }
  });
  block = testHelper.pipeline().get()[1].get();
  asyncBlock = dynamic_cast<ExecutionBlockImpl<AsyncExecutor>*>(block);
  asyncBlock->setFailureCallback([] {});

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
