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
#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateMetrics.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Mocks/FakeReplicatedState.h"
#include "Replication2/Mocks/FakeLeader.h"

#include "Replication2/Mocks/ReplicatedStateMetricsMock.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::test;

struct ReplicatedStateLeaderResignTest : test::ReplicatedLogTest {
  struct State {
    using LeaderType = test::FakeLeaderType<State>;
    using FollowerType = test::EmptyFollowerType<State>;
    using EntryType = test::DefaultEntryType;
    using FactoryType = test::RecordingFactory<LeaderType, FollowerType>;
    using CoreType = test::TestCoreType;
    using CoreParameterType = void;
  };

  ReplicatedStateLeaderResignTest() {
    feature->registerStateType<State>("my-state");
  }
  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();

  std::shared_ptr<test::FakeLeader> logLeader =
      std::make_shared<test::FakeLeader>();
  std::shared_ptr<State::FactoryType> factory =
      std::make_shared<State::FactoryType>();
  std::unique_ptr<State::CoreType> core = std::make_unique<State::CoreType>();
  std::shared_ptr<State::LeaderType> leaderState;
  std::shared_ptr<ReplicatedStateMetrics> _metrics =
      std::make_shared<ReplicatedStateMetricsMock>("foo");
  LoggerContext const loggerCtx{Logger::REPLICATED_STATE};
  std::shared_ptr<replicated_state::LeaderStateManager<State>> manager =
      std::make_shared<replicated_state::LeaderStateManager<State>>(
          loggerCtx, nullptr, logLeader, std::move(core),
          std::make_unique<ReplicatedStateToken>(StateGeneration{1}), factory,
          _metrics);
};

TEST_F(ReplicatedStateLeaderResignTest, complete_run_without_resign) {
  auto index =
      logLeader->insertMultiplexedValue<State>(DefaultEntryType{"foo", "bar"});
  manager->run();
  logLeader->triggerLeaderEstablished(index);
  auto stateLeader = factory->getLatestLeader();
  stateLeader->resolveRecoveryOk();
}

TEST_F(ReplicatedStateLeaderResignTest,
       complete_run_with_resign_during_recovery) {
  auto index =
      logLeader->insertMultiplexedValue<State>(DefaultEntryType{"foo", "bar"});
  manager->run();
  logLeader->triggerLeaderEstablished(index);
  auto stateLeader = factory->getLatestLeader();
  auto [core, token, action] = std::move(*manager).resign();
  action.fire();
  stateLeader->resolveRecoveryOk();
}

TEST_F(ReplicatedStateLeaderResignTest,
       complete_run_with_resign_before_recovery) {
  auto index =
      logLeader->insertMultiplexedValue<State>(DefaultEntryType{"foo", "bar"});
  manager->run();
  auto [core, token, action] = std::move(*manager).resign();
  action.fire();
  logLeader->triggerLeaderEstablished(index);
}
