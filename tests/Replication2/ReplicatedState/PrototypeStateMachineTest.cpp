////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Streams/Streams.h"

#include "Replication2/StateMachines/Prototype/PrototypeStateMachine.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;
using namespace arangodb::replication2::test;

struct ReplicatedStateRecoveryTest;

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

struct PrototypeStateMachineTest : test::ReplicatedLogTest {
  PrototypeStateMachineTest() {
    feature->registerStateType<prototype::PrototypeState>("prototype-state");
  }

  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();
};

TEST_F(PrototypeStateMachineTest, set_remove_get) {
  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();
  auto replicatedState =
      std::dynamic_pointer_cast<ReplicatedState<PrototypeState>>(
          feature->createReplicatedState("prototype-state", leaderLog));
  ASSERT_NE(replicatedState, nullptr);

  replicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}));

  LOG_DEVEL << "before appendentries";
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }
  LOG_DEVEL << "after appendentries";

  auto leaderState = replicatedState->getLeader();
  LOG_DEVEL << "got leader state";
  ASSERT_NE(leaderState, nullptr);
  LOG_DEVEL << "before set";
  auto result = leaderState->set("foo", "bar");
  LOG_DEVEL << "after set";

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }
  ASSERT_TRUE(result.get().ok());

  auto result2 = leaderState->get("foo");
  ASSERT_EQ(result2, "bar");
  auto result3 = leaderState->get("baz");
  ASSERT_EQ(result3, std::nullopt);
}