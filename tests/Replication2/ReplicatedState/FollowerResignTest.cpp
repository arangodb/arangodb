////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateMetrics.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Mocks/FakeReplicatedState.h"
#include "Replication2/Mocks/FakeFollower.h"

#include "Replication2/Mocks/ReplicatedStateMetricsMock.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::test;

struct ReplicatedStateFollowerResignTest : test::ReplicatedLogTest {
  struct State {
    using LeaderType = test::EmptyLeaderType<State>;
    using FollowerType = test::FakeFollowerType<State>;
    using EntryType = test::DefaultEntryType;
    using FactoryType = test::RecordingFactory<LeaderType, FollowerType>;
    using CoreType = test::TestCoreType;
    using CoreParameterType = void;
  };

  std::shared_ptr<State::FactoryType> factory =
      std::make_shared<State::FactoryType>();
  std::unique_ptr<State::CoreType> core = std::make_unique<State::CoreType>();
  LoggerContext const loggerCtx{Logger::REPLICATED_STATE};
  std::shared_ptr<ReplicatedStateMetrics> _metrics =
      std::make_shared<ReplicatedStateMetricsMock>("foo");

  auto getFollowerAtState(FollowerInternalState const state)
      -> std::shared_ptr<test::FakeFollower> {
    auto follower =
        std::make_shared<test::FakeFollower>("follower", "leader", LogTerm{1});
    auto const logIndex = follower->insertMultiplexedValue<State>(
        test::DefaultEntryType{"foo", "bar"});
    auto manager = std::make_shared<FollowerStateManager<State>>(
        loggerCtx, nullptr, follower, std::move(core),
        std::make_unique<ReplicatedStateToken>(StateGeneration{1}), factory,
        _metrics);
    {
      auto status = *manager->getStatus().asFollowerStatus();
      EXPECT_EQ(status.managerState.state,
                FollowerInternalState::kUninitializedState);
    }
    if (state == FollowerInternalState::kUninitializedState) {
      return follower;
    }

    manager->run();
    {
      auto status = *manager->getStatus().asFollowerStatus();
      EXPECT_EQ(status.managerState.state,
                FollowerInternalState::kWaitForLeaderConfirmation);
    }
    if (state == FollowerInternalState::kWaitForLeaderConfirmation) {
      return follower;
    }

    follower->triggerLeaderAcked();
    {
      auto status = *manager->getStatus().asFollowerStatus();
      EXPECT_EQ(status.managerState.state,
                FollowerInternalState::kTransferSnapshot);
    }
    if (state == FollowerInternalState::kTransferSnapshot) {
      return follower;
    }
    auto followerState = factory->getLatestFollower();
    followerState->acquire.resolveWith(Result{});
    {
      auto status = *manager->getStatus().asFollowerStatus();
      EXPECT_EQ(status.managerState.state,
                FollowerInternalState::kWaitForNewEntries);
    }
    if (state == FollowerInternalState::kWaitForNewEntries) {
      return follower;
    }
    follower->updateCommitIndex(logIndex);
    {
      auto status = *manager->getStatus().asFollowerStatus();
      EXPECT_EQ(status.managerState.state,
                FollowerInternalState::kApplyRecentEntries);
    }
    if (state == FollowerInternalState::kApplyRecentEntries) {
      return follower;
    }

    // Note that kSnapshotTransferFailed currently is not supported: a way to
    // delay the "back-off-promise" being resolved would have to be implemented
    // first, because without that we end up back in kTransferSnapshot when
    // we get back here.
    ADB_PROD_ASSERT(false) << "unsupported state " << to_string(state);
    // silence warnings
    return nullptr;
  }
};

TEST_F(ReplicatedStateFollowerResignTest, resign_while_still_uninitialized) {
  auto follower =
      getFollowerAtState(FollowerInternalState::kUninitializedState);

  follower->resign();
}

TEST_F(ReplicatedStateFollowerResignTest, resign_while_waiting_for_leader) {
  auto follower =
      getFollowerAtState(FollowerInternalState::kWaitForLeaderConfirmation);

  follower->resign();
}

TEST_F(ReplicatedStateFollowerResignTest, resign_while_transferring_snapshot) {
  auto follower = getFollowerAtState(FollowerInternalState::kTransferSnapshot);

  follower->resign();
}

TEST_F(ReplicatedStateFollowerResignTest, resign_while_waiting_for_entries) {
  auto follower = getFollowerAtState(FollowerInternalState::kWaitForNewEntries);

  follower->resign();
}

TEST_F(ReplicatedStateFollowerResignTest, resign_while_applying_entries) {
  auto follower =
      getFollowerAtState(FollowerInternalState::kApplyRecentEntries);

  follower->resign();
}
