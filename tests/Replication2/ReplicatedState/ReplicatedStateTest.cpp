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

#include "Mocks/LogLevels.h"
#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/Streams/Streams.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"

#include "StateMachines/MyStateMachine.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::test;

struct ReplicatedStateTest : test::ReplicatedLogTest {
  ReplicatedStateTest() { feature->registerStateType<MyState>("my-state"); }
  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();
};

TEST_F(ReplicatedStateTest, simple_become_follower_test) {
  auto log = makeReplicatedLog(LogId{1});
  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
  auto state = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
      feature->createReplicatedState("my-state", log));
  ASSERT_NE(state, nullptr);

  state->flush();

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  auto mux = streams::LogMultiplexer<
      replicated_state::ReplicatedStateStreamSpec<MyState>>::construct(leader);
  auto inputStream = mux->getStreamById<1>();

  inputStream->insert({.key = "hello", .value = "world"});
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto followerState = state->getFollower();
  ASSERT_NE(followerState, nullptr);
  auto& store = followerState->store;
  EXPECT_EQ(store.size(), 1);
  EXPECT_EQ(store["hello"], "world");
}

TEST_F(ReplicatedStateTest, recreate_follower_on_new_term) {
  auto log = makeReplicatedLog(LogId{1});
  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
  auto state = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
      feature->createReplicatedState("my-state", log));
  ASSERT_NE(state, nullptr);

  // create a leader in term 1
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  auto mux =
      streams::LogMultiplexer<ReplicatedStateStreamSpec<MyState>>::construct(
          leader);
  auto inputStream = mux->getStreamById<1>();
  inputStream->insert({.key = "hello", .value = "world"});

  state->flush();

  // recreate follower
  follower = log->becomeFollower("follower", LogTerm{2}, "leader");

  // create a leader in term 2
  leader = leaderLog->becomeLeader("leader", LogTerm{2}, {follower}, 2);
  leader->triggerAsyncReplication();

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto followerState = state->getFollower();
  ASSERT_NE(followerState, nullptr);
  auto& store = followerState->store;
  EXPECT_EQ(store.size(), 1);
  EXPECT_EQ(store["hello"], "world");
}

TEST_F(ReplicatedStateTest, simple_become_leader_test) {
  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto log = makeReplicatedLog(LogId{1});
  auto leader = log->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();
  auto state = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
      feature->createReplicatedState("my-state", log));
  ASSERT_NE(state, nullptr);
  state->flush();
  {
    auto status = state->getStatus();
    ASSERT_TRUE(
        std::holds_alternative<replicated_state::LeaderStatus>(status.variant));
    auto s = std::get<replicated_state::LeaderStatus>(status.variant);
    EXPECT_EQ(s.state.state,
              LeaderInternalState::kWaitingForLeadershipEstablished);
  }

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto leaderState = state->getLeader();
  ASSERT_NE(leaderState, nullptr);
  EXPECT_TRUE(leaderState->wasRecoveryRun());

  {
    auto status = state->getStatus();
    ASSERT_TRUE(
        std::holds_alternative<replicated_state::LeaderStatus>(status.variant));
    auto s = std::get<replicated_state::LeaderStatus>(status.variant);
    EXPECT_EQ(s.state.state, LeaderInternalState::kServiceAvailable);
  }
}

TEST_F(ReplicatedStateTest, simple_become_leader_recovery_test) {
  auto log = makeReplicatedLog(LogId{1});
  auto leaderLog = makeReplicatedLog(LogId{1});

  // First insert an entry on the leader log
  {
    auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
    auto state = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
        feature->createReplicatedState("my-state", log));
    ASSERT_NE(state, nullptr);

    state->flush();
    {
      auto status = state->getStatus();
      ASSERT_TRUE(std::holds_alternative<replicated_state::FollowerStatus>(
          status.variant));
      auto s = std::get<replicated_state::FollowerStatus>(status.variant);
      EXPECT_EQ(s.state.state,
                FollowerInternalState::kWaitForLeaderConfirmation);
    }

    auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);
    auto mux =
        streams::LogMultiplexer<ReplicatedStateStreamSpec<MyState>>::construct(
            leader);
    auto inputStream = mux->getStreamById<1>();

    inputStream->insert({.key = "hello", .value = "world"});
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
    }

    {
      auto status = state->getStatus();
      ASSERT_TRUE(std::holds_alternative<replicated_state::FollowerStatus>(
          status.variant));
      auto s = std::get<replicated_state::FollowerStatus>(status.variant);
      EXPECT_EQ(s.state.state, FollowerInternalState::kNothingToApply);
    }
  }

  // then let the follower log become the leader
  // and check that old entries are recovered.
  {
    auto follower = leaderLog->becomeFollower("follower", LogTerm{2}, "leader");
    auto leader = log->becomeLeader("leader", LogTerm{2}, {follower}, 2);

    leader->triggerAsyncReplication();
    auto state = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
        feature->createReplicatedState("my-state", log));
    ASSERT_NE(state, nullptr);

    state->flush();
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
    }

    auto leaderState = state->getLeader();
    ASSERT_NE(leaderState, nullptr);
    ASSERT_TRUE(leaderState->wasRecoveryRun());
    {
      auto status = state->getStatus();
      ASSERT_TRUE(std::holds_alternative<replicated_state::LeaderStatus>(
          status.variant));
      auto s = std::get<replicated_state::LeaderStatus>(status.variant);
      EXPECT_EQ(s.state.state, LeaderInternalState::kServiceAvailable);
    }

    {
      auto& store = leaderState->store;
      EXPECT_EQ(store.size(), 1);
      EXPECT_EQ(store["hello"], "world");
    }
  }
}

TEST_F(ReplicatedStateTest, stream_test) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{1});

  auto follower = followerLog->becomeFollower("B", LogTerm{1}, "A");
  auto leader = leaderLog->becomeLeader("A", LogTerm{1}, {follower}, 2);
  leader->triggerAsyncReplication();

  auto leaderState = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
      feature->createReplicatedState("my-state", leaderLog));
  leaderState->flush();

  auto followerState = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
      feature->createReplicatedState("my-state", followerLog));
  followerState->flush();

  // make sure we do recovery
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  // now leader machine should be available
  auto leaderMachine = leaderState->getLeader();
  ASSERT_NE(leaderMachine, nullptr);

  for (int i = 0; i < 200; i++) {
    leaderMachine->set(std::to_string(i),
                       std::string{"value"} + std::to_string(i));
  }

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto followerMachine = followerState->getFollower();
  ASSERT_NE(followerMachine, nullptr);

  for (int i = 0; i < 200; i++) {
    EXPECT_EQ(followerMachine->store[std::to_string(i)],
              std::string{"value"} + std::to_string(i));
  }
}
