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
#include "Replication2/Mocks/ReplicatedStateMetricsMock.h"
#include "Replication2/Mocks/PersistedLog.h"
#include "Replication2/ReplicatedLog/TestHelper.h"
#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/Streams/LogMultiplexer.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

struct FollowerWaitForAppliedTest
    : testing::Test,
      tests::LogSuppressor<Logger::REPLICATED_STATE, LogLevel::TRACE> {
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
};

TEST_F(FollowerWaitForAppliedTest, wait_for_applied_future_test) {
  auto follower =
      std::make_shared<test::FakeFollower>("follower", "leader", LogTerm{1});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "A", .value = "a"});
  follower->updateCommitIndex(LogIndex{1});  // insert and commit index 1

  auto manager = std::make_shared<FollowerStateManager<State>>(
      loggerCtx, nullptr, follower, std::move(core),
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), factory,
      _metrics);
  manager->run();
  follower->triggerLeaderAcked();

  // complete snapshot transfer
  auto state = factory->getLatestFollower();
  state->acquire.resolveWith(Result{});

  // apply index 1
  state->apply.resolveWith(Result{});
  state->apply.reset();

  auto f1 = state->waitForApplied(LogIndex{1});
  ASSERT_TRUE(f1.isReady());

  auto f2 = state->waitForApplied(LogIndex{4});
  ASSERT_FALSE(f2.isReady());

  // insert more entries
  for (int i = 0; i < 5; i++) {
    follower->insertMultiplexedValue<State>(
        test::DefaultEntryType{.key = "A", .value = "a"});
  }
  follower->updateCommitIndex(LogIndex{5});  // commit index 5

  EXPECT_TRUE(state->apply.wasTriggered());
  state->apply.resolveWith({});
  EXPECT_TRUE(f2.isReady());
}

TEST_F(FollowerWaitForAppliedTest, wait_for_applied_resign_resolve) {
  auto follower =
      std::make_shared<test::FakeFollower>("follower", "leader", LogTerm{1});
  follower->insertMultiplexedValue<State>(
      test::DefaultEntryType{.key = "A", .value = "a"});
  follower->updateCommitIndex(LogIndex{1});  // insert and commit index 1

  auto manager = std::make_shared<FollowerStateManager<State>>(
      loggerCtx, nullptr, follower, std::move(core),
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), factory,
      _metrics);
  manager->run();
  follower->triggerLeaderAcked();

  // complete snapshot transfer
  auto state = factory->getLatestFollower();
  state->acquire.resolveWith(Result{});

  // apply index 1
  state->apply.resolveWith(Result{});
  state->apply.reset();

  auto f1 = state->waitForApplied(LogIndex{1});
  ASSERT_TRUE(f1.isReady());

  auto f2 = state->waitForApplied(LogIndex{4});
  ASSERT_FALSE(f2.isReady());
  auto [core, token, action] = std::move(*manager).resign();
  action.fire();

  // This is now fulfilled because we dropped the return value of resign
  ASSERT_TRUE(f2.isReady());
  ASSERT_TRUE(f2.hasException());
  ASSERT_THROW({ f2.get(); }, replicated_log::ParticipantResignedException);
}
