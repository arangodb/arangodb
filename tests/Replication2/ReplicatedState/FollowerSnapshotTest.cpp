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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <utility>

#include "Logger/LogMacros.h"
#include "LogLevels.h"
#include "Death_Test.h"

#include "Replication2/Mocks/FakeFollower.h"
#include "Replication2/Mocks/FakeReplicatedState.h"
#include "Replication2/Mocks/PersistedLog.h"
#include "Replication2/ReplicatedLog/TestHelper.h"
#include "Replication2/ReplicatedState/ReplicatedStateImpl.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Mocks/ReplicatedStateMetricsMock.h"
#include "Replication2/Mocks/MockStatePersistorInterface.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

struct FollowerSnapshotTest
    : testing::Test,
      tests::LogSuppressor<Logger::REPLICATED_STATE, LogLevel::TRACE> {
  struct State {
    using LeaderType = test::EmptyLeaderType<State>;
    using FollowerType = test::FakeFollowerType<State>;
    using EntryType = test::DefaultEntryType;
    using FactoryType = test::RecordingFactory<LeaderType, FollowerType>;
    using CoreType = test::TestCoreType;
    using CoreParameterType = void;
    using CleanupHandlerType = void;
  };

  std::shared_ptr<State::FactoryType> factory =
      std::make_shared<State::FactoryType>();
  std::unique_ptr<State::CoreType> core = std::make_unique<State::CoreType>();
  LoggerContext const loggerCtx{Logger::REPLICATED_STATE};
  std::shared_ptr<ReplicatedStateMetrics> _metrics =
      std::make_shared<ReplicatedStateMetricsMock>("foo");
  std::shared_ptr<test::MockStatePersistorInterface> _persistor =
      std::make_shared<test::MockStatePersistorInterface>();
};

using FollowerSnapshotDeathTest = FollowerSnapshotTest;

TEST_F(FollowerSnapshotDeathTest, basic_follower_manager_test) {
  auto follower =
      std::make_shared<test::FakeFollower>("follower", "leader", LogTerm{1});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "A", .value = "a"});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "B", .value = "b"});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "C", .value = "c"});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "D", .value = "d"});

  auto manager = std::make_shared<FollowerStateManager<State>>(
      loggerCtx, nullptr, follower, std::move(core),
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), factory,
      _metrics, _persistor);
  manager->run();
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kWaitForLeaderConfirmation);
    EXPECT_EQ(status.snapshot.status, SnapshotStatus::kUninitialized);
  }

  // required for leader to become established
  follower->triggerLeaderAcked();

  // we expect a snapshot to be requested, because the snapshot state was
  // uninitialized
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kTransferSnapshot);
    EXPECT_EQ(status.snapshot.status, SnapshotStatus::kInProgress);
  }

  // now here we expect that the state is internally created
  // but the user does not yet have access to it
  auto state = factory->getLatestFollower();
  ASSERT_NE(state, nullptr) << "expect state to be created";
  {
    ASSERT_TRUE(state->acquire.wasTriggered())
        << "expect snapshot to be requested";
    auto& value = state->acquire.inspectValue();
    EXPECT_EQ(value.first, "leader");
    EXPECT_EQ(value.second, LogIndex{0});
  }
  ASSERT_EQ(nullptr, manager->getFollowerState())
      << "follower state should not be available yet";

  // furthermore the state should not have access to the stream
  ASSERT_DEATH_CORE_FREE(std::ignore = state->getStream(), "");

  // first trigger an error
  state->acquire.resolveWithAndReset(
      Result{TRI_ERROR_HTTP_SERVICE_UNAVAILABLE});

  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kTransferSnapshot);
    EXPECT_EQ(status.snapshot.status, SnapshotStatus::kInProgress);
  }
  // we expect a retry
  {
    ASSERT_TRUE(state->acquire.wasTriggered())
        << "expect snapshot to be requested";
    auto& value = state->acquire.inspectValue();
    EXPECT_EQ(value.first, "leader");
    EXPECT_EQ(value.second, LogIndex{0});
  }
  ASSERT_EQ(nullptr, manager->getFollowerState())
      << "follower state should not be available yet";

  // notify the manager that the state transfer was successfully completed
  state->acquire.resolveWith(Result{});

  // since the log is empty, we should be good
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kWaitForNewEntries);
    EXPECT_EQ(status.snapshot.status, SnapshotStatus::kCompleted);
    EXPECT_EQ(status.lastAppliedIndex, LogIndex{0});
  }
  ASSERT_NE(nullptr, manager->getFollowerState())
      << "follower state should be available";
  EXPECT_FALSE(state->apply.wasTriggered());

  // furthermore the state should have access to the stream
  ASSERT_NE(state->getStream(), nullptr) << "stream is still nullptr";

  follower->updateCommitIndex(LogIndex{3});
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kApplyRecentEntries);
  }
  EXPECT_TRUE(state->apply.wasTriggered());
  EXPECT_EQ(state->apply.inspectValue()->range(),
            LogRange(LogIndex{1}, LogIndex{4}));

  state->apply.resolveWith(Result{});  // resolve with ok
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kWaitForNewEntries);
    EXPECT_EQ(status.lastAppliedIndex, LogIndex{3});
  }
}

TEST_F(FollowerSnapshotTest, follower_resign_before_leadership_acked) {
  auto follower =
      std::make_shared<test::FakeFollower>("follower", "leader", LogTerm{1});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "A", .value = "a"});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "B", .value = "b"});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "C", .value = "c"});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "D", .value = "d"});

  auto manager = std::make_shared<FollowerStateManager<State>>(
      loggerCtx, nullptr, follower, std::move(core),
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), factory,
      _metrics, _persistor);
  manager->run();
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kWaitForLeaderConfirmation);
  }

  // follower resign
  follower->resign();
}

TEST_F(FollowerSnapshotTest,
       basic_follower_manager_test_with_completed_snapshot) {
  auto follower =
      std::make_shared<test::FakeFollower>("follower", "leader", LogTerm{1});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "A", .value = "a"});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "B", .value = "b"});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "C", .value = "c"});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "D", .value = "d"});

  auto token = std::make_unique<ReplicatedStateToken>(
      ReplicatedStateToken::withExplicitSnapshotStatus(
          StateGeneration{1},
          SnapshotInfo{.status = SnapshotStatus::kCompleted,
                       .timestamp = SnapshotInfo::clock ::now(),
                       .error = std::nullopt}));
  auto manager = std::make_shared<FollowerStateManager<State>>(
      loggerCtx, nullptr, follower, std::move(core), std::move(token), factory,
      _metrics, _persistor);
  manager->run();
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kWaitForLeaderConfirmation);
    EXPECT_EQ(status.snapshot.status, SnapshotStatus::kCompleted);
  }

  // required for leader to become established
  follower->triggerLeaderAcked();

  // the snapshot is already available, we expect it to be complete
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kWaitForNewEntries);
    EXPECT_EQ(status.snapshot.status, SnapshotStatus::kCompleted);
  }

  // now here we expect that the state is internally created and available
  // to the user
  auto state = factory->getLatestFollower();
  ASSERT_NE(state, nullptr) << "expect state to be created";

  ASSERT_NE(nullptr, manager->getFollowerState())
      << "follower state should be available";
  EXPECT_FALSE(state->apply.wasTriggered());

  // furthermore the state should have access to the stream
  ASSERT_NE(state->getStream(), nullptr) << "stream is still nullptr";

  follower->updateCommitIndex(LogIndex{3});
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kApplyRecentEntries);
  }
  EXPECT_TRUE(state->apply.wasTriggered());
  EXPECT_EQ(state->apply.inspectValue()->range(),
            LogRange(LogIndex{1}, LogIndex{4}));

  state->apply.resolveWith(Result{});  // resolve with ok
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.managerState.state,
              FollowerInternalState::kWaitForNewEntries);
  }
}
