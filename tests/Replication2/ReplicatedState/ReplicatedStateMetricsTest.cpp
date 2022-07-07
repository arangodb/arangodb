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

#include "Metrics/Gauge.h"
#include "Mocks/LogLevels.h"
#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/Streams/Streams.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"

#include "StateMachines/MyStateMachine.h"
#include "Replication2/Mocks/ReplicatedStateMetricsMock.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::test;

struct ReplicatedStateMetricsTest : test::ReplicatedLogTest {
  std::shared_ptr<ReplicatedStateMetrics> metrics =
      std::make_shared<ReplicatedStateMetricsMock>("my-state>");
  std::shared_ptr<MyFactory> factory = std::make_shared<MyFactory>();

  LoggerContext const loggerCtx{Logger::REPLICATED_STATE};
};

TEST_F(ReplicatedStateMetricsTest, count_replicated_states) {
  auto log = makeReplicatedLog(LogId{1});
  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
  auto numberBefore = metrics->replicatedStateNumber->load();
  auto state = std::make_shared<ReplicatedState<MyState>>(log, factory,
                                                          loggerCtx, metrics);
  auto numberBetween = metrics->replicatedStateNumber->load();
  EXPECT_EQ(numberBefore + 1, numberBetween);
  state.reset();
  auto numberAfter = metrics->replicatedStateNumber->load();
  EXPECT_EQ(numberBefore, numberAfter);
}

TEST_F(ReplicatedStateMetricsTest, count_replicated_states_follower) {
  auto log = makeReplicatedLog(LogId{1});
  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
  auto numberBefore = metrics->replicatedStateNumberFollowers->load();
  auto state = std::make_shared<ReplicatedState<MyState>>(log, factory,
                                                          loggerCtx, metrics);
  state->start(std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
               std::nullopt);
  auto numberBetween = metrics->replicatedStateNumberFollowers->load();
  EXPECT_EQ(numberBefore + 1, numberBetween);
  state.reset();
  auto numberAfter = metrics->replicatedStateNumberFollowers->load();
  EXPECT_EQ(numberBefore, numberAfter);
}

TEST_F(ReplicatedStateMetricsTest, count_replicated_states_leader) {
  auto log = makeReplicatedLog(LogId{1});
  auto leader = log->becomeLeader("leader", LogTerm{1}, {}, 1);
  auto numberBefore = metrics->replicatedStateNumberLeaders->load();
  auto state = std::make_shared<ReplicatedState<MyState>>(log, factory,
                                                          loggerCtx, metrics);
  state->start(std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
               std::nullopt);
  auto numberBetween = metrics->replicatedStateNumberLeaders->load();
  EXPECT_EQ(numberBefore + 1, numberBetween);
  state.reset();
  auto numberAfter = metrics->replicatedStateNumberLeaders->load();
  EXPECT_EQ(numberBefore, numberAfter);
}
