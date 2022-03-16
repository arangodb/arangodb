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
#include "Replication2/Mocks/FakeReplicatedState.h"

#include "StateMachines/MyStateMachine.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::test;

struct ReplicatedStateRecoveryTest;

namespace arangodb::replication2::test {
struct MyHelperLeaderState;
struct MyHelperFactory;
struct MyCoreType;

struct MyHelperState {
  using FactoryType = MyHelperFactory;
  using LeaderType = MyHelperLeaderState;
  using EntryType = MyEntryType;
  using FollowerType = EmptyFollowerType<MyHelperState>;
  using CoreType = MyCoreType;
};

struct MyHelperLeaderState : IReplicatedLeaderState<MyHelperState> {
  explicit MyHelperLeaderState(std::unique_ptr<MyCoreType> core)
      : _core(std::move(core)) {}

 protected:
  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override {
    TRI_ASSERT(recoveryTriggered == false);
    recoveryTriggered = true;
    return promise.getFuture();
  }

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<MyCoreType> override;
  std::unique_ptr<MyCoreType> _core;

 public:
  void runRecovery(Result res = {}) { promise.setValue(res); }

  bool recoveryTriggered = false;
  futures::Promise<Result> promise;
};

auto MyHelperLeaderState::resign() && noexcept -> std::unique_ptr<MyCoreType> {
  return std::move(_core);
}

struct MyHelperFactory {
  explicit MyHelperFactory(ReplicatedStateRecoveryTest& test) : test(test) {}
  auto constructLeader(std::unique_ptr<MyCoreType> core)
      -> std::shared_ptr<MyHelperLeaderState>;
  auto constructFollower(std::unique_ptr<MyCoreType> core)
      -> std::shared_ptr<EmptyFollowerType<MyHelperState>> {
    return std::make_shared<EmptyFollowerType<MyHelperState>>(std::move(core));
  }
  auto constructCore(GlobalLogIdentifier const&)
      -> std::unique_ptr<MyCoreType> {
    return std::make_unique<MyCoreType>();
  }
  ReplicatedStateRecoveryTest& test;
};

}  // namespace arangodb::replication2::test

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

struct ReplicatedStateRecoveryTest : test::ReplicatedLogTest {
  ReplicatedStateRecoveryTest() {
    feature->registerStateType<MyHelperState>("my-state", *this);
  }

  std::shared_ptr<MyHelperLeaderState> leaderState;
  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();
};

auto MyHelperFactory::constructLeader(std::unique_ptr<MyCoreType> core)
    -> std::shared_ptr<MyHelperLeaderState> {
  test.leaderState = std::make_shared<MyHelperLeaderState>(std::move(core));
  return test.leaderState;
}

TEST_F(ReplicatedStateRecoveryTest, trigger_recovery) {
  /*
   * This test creates a leader state and then checks if the recovery procedure
   * is called properly, and it's return value is awaited. We expect the status
   * to reflect those actions.
   *
   * The recovery returns successful, so we expect the service to start up.
   */

  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();
  auto replicatedState =
      std::dynamic_pointer_cast<ReplicatedState<MyHelperState>>(
          feature->createReplicatedState("my-state", leaderLog));
  ASSERT_NE(replicatedState, nullptr);
  ASSERT_EQ(leaderState, nullptr);

  replicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}));

  {
    auto status = replicatedState->getStatus().value();
    ASSERT_TRUE(
        std::holds_alternative<replicated_state::LeaderStatus>(status.variant));
    auto s = std::get<replicated_state::LeaderStatus>(status.variant);
    EXPECT_EQ(s.managerState.state,
              LeaderInternalState::kWaitingForLeadershipEstablished);
  }

  {
    // Leader state not yet reachable from replicated state object
    auto leaderStatePtr = replicatedState->getLeader();
    ASSERT_EQ(leaderStatePtr, nullptr);
  }

  ASSERT_EQ(leaderState, nullptr);

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  ASSERT_NE(leaderState, nullptr);
  EXPECT_TRUE(leaderState->recoveryTriggered);

  {
    auto status = replicatedState->getStatus().value();
    ASSERT_TRUE(
        std::holds_alternative<replicated_state::LeaderStatus>(status.variant));
    auto s = std::get<replicated_state::LeaderStatus>(status.variant);
    EXPECT_EQ(s.managerState.state, LeaderInternalState::kRecoveryInProgress);
  }

  {
    // Leader state not yet reachable from replicated state object
    auto leaderStatePtr = replicatedState->getLeader();
    ASSERT_EQ(leaderStatePtr, nullptr);
  }

  leaderState->runRecovery();

  {
    auto status = replicatedState->getStatus().value();
    ASSERT_TRUE(
        std::holds_alternative<replicated_state::LeaderStatus>(status.variant));
    auto s = std::get<replicated_state::LeaderStatus>(status.variant);
    EXPECT_EQ(s.managerState.state, LeaderInternalState::kServiceAvailable);
  }

  {
    // Now the leader state should be reachable through the replicated state
    // object
    auto leaderStatePtr = replicatedState->getLeader();
    ASSERT_NE(leaderStatePtr, nullptr);
  }
}

TEST_F(ReplicatedStateRecoveryTest, trigger_recovery_error_DeathTest) {
  /*
   * This test creates a leader state and then checks if the recovery procedure
   * is called properly, and it's return value is awaited. We expect the status
   * to reflect those actions.
   */

  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);
  leader->triggerAsyncReplication();
  auto replicatedState =
      std::dynamic_pointer_cast<ReplicatedState<MyHelperState>>(
          feature->createReplicatedState("my-state", leaderLog));
  ASSERT_NE(replicatedState, nullptr);
  ASSERT_EQ(leaderState, nullptr);

  replicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}));

  {
    auto status = replicatedState->getStatus().value();
    ASSERT_TRUE(
        std::holds_alternative<replicated_state::LeaderStatus>(status.variant));
    auto s = std::get<replicated_state::LeaderStatus>(status.variant);
    EXPECT_EQ(s.managerState.state,
              LeaderInternalState::kWaitingForLeadershipEstablished);
  }

  ASSERT_EQ(leaderState, nullptr);

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  ASSERT_NE(leaderState, nullptr);
  EXPECT_TRUE(leaderState->recoveryTriggered);

  {
    auto status = replicatedState->getStatus().value();
    ASSERT_TRUE(
        std::holds_alternative<replicated_state::LeaderStatus>(status.variant));
    auto s = std::get<replicated_state::LeaderStatus>(status.variant);
    EXPECT_EQ(s.managerState.state, LeaderInternalState::kRecoveryInProgress);
  }

  // failing recovery should result in a crash
  ASSERT_DEATH_IF_SUPPORTED(
      {
        leaderState->runRecovery(
            Result{TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT});
      },
      ".*");
  // this is here to resolve the promise in MyHelperLeaderState, which otherwise
  // will hold a reference to the MyHelperLeaderState and asan will be very sad
  leaderState->runRecovery(Result{TRI_ERROR_NO_ERROR});
}
