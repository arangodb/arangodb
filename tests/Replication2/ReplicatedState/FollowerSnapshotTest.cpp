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

#include <utility>

#include "Logger/LogMacros.h"
#include "LogLevels.h"

#include "Replication2/Mocks/FakeFollower.h"
#include "Replication2/Mocks/FakeReplicatedState.h"
#include "Replication2/Mocks/PersistedLog.h"
#include "Replication2/ReplicatedLog/TestHelper.h"
#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/Streams/LogMultiplexer.h"

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
  };

  std::shared_ptr<State::FactoryType> factory =
      std::make_shared<State::FactoryType>();
  std::unique_ptr<ReplicatedStateCore> core =
      std::make_unique<ReplicatedStateCore>();
};


TEST_F(FollowerSnapshotTest, basic_follower_manager_test) {
  auto follower =
      std::make_shared<test::FakeFollower>("follower", "leader", LogTerm{1});
  follower->insertMultiplexedValue<State>(test::DefaultEntryType{.key = "A", .value = "a"});
  follower->insertMultiplexedValue<State>(test::DefaultEntryType{.key = "B", .value = "b"});
  follower->insertMultiplexedValue<State>(test::DefaultEntryType{.key = "C", .value = "c"});
  follower->insertMultiplexedValue<State>(test::DefaultEntryType{.key = "D", .value = "d"});

  auto manager = std::make_shared<FollowerStateManager<State>>(
      nullptr, follower, std::move(core), factory);
  manager->run();
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.state.state,
              FollowerInternalState::kWaitForLeaderConfirmation);
  }

  // required for leader to become established
  follower->triggerLeaderAcked();

  // we expect a snapshot to be requested, because the snapshot state was
  // uninitialized
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.state.state, FollowerInternalState::kTransferSnapshot);
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
  ASSERT_ANY_THROW(manager->getFollowerState())
      << "follower state not yet available";

  // notify the manager that the state transfer was successfully completed
  state->acquire.resolveWith(Result{});

  // since the log is empty, we should be good
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.state.state, FollowerInternalState::kNothingToApply);
  }
  ASSERT_NO_THROW(manager->getFollowerState())
      << "follower state should be available";
  EXPECT_FALSE(state->apply.wasTriggered());

  follower->updateCommitIndex(LogIndex{3});
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.state.state, FollowerInternalState::kApplyRecentEntries);
  }
  EXPECT_TRUE(state->apply.wasTriggered());
  EXPECT_EQ(state->apply.inspectValue()->range(),
            LogRange(LogIndex{1}, LogIndex{4}));

  state->apply.resolveWith(Result{}); // resolve with ok
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.state.state, FollowerInternalState::kNothingToApply);
  }
}

TEST_F(FollowerSnapshotTest, follower_resign_before_leadership_acked) {
  auto follower =
      std::make_shared<test::FakeFollower>("follower", "leader", LogTerm{1});
  follower->insertMultiplexedValue<State>(test::DefaultEntryType{.key = "A", .value = "a"});
  follower->insertMultiplexedValue<State>(test::DefaultEntryType{.key = "B", .value = "b"});
  follower->insertMultiplexedValue<State>(test::DefaultEntryType{.key = "C", .value = "c"});
  follower->insertMultiplexedValue<State>(test::DefaultEntryType{.key = "D", .value = "d"});

  auto manager = std::make_shared<FollowerStateManager<State>>(
      nullptr, follower, std::move(core), factory);
  manager->run();
  {
    auto status = *manager->getStatus().asFollowerStatus();
    EXPECT_EQ(status.state.state,
              FollowerInternalState::kWaitForLeaderConfirmation);
  }

  // follower resign
  follower->resign();
}
