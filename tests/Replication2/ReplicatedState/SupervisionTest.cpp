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
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/Supervision.h"
#include "Replication2/Helper/AgencyStateBuilder.h"
#include "Replication2/Helper/AgencyLogBuilder.h"

using namespace arangodb;
using namespace arangodb::test;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
namespace RLA = arangodb::replication2::agency;
namespace RSA = arangodb::replication2::replicated_state::agency;

struct ReplicatedStateSupervisionTest : ::testing::Test {
  LogConfig const defaultConfig = {2, 2, 3, false};
  LogId const logId{12};

  ParticipantFlags const flagsSnapshotIncomplete{.allowedInQuorum = false,
                                                 .allowedAsLeader = false};
};

TEST_F(ReplicatedStateSupervisionTest, check_state_and_log) {
  AgencyStateBuilder state;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetConfig(defaultConfig);

  replicated_state::SupervisionContext ctx;
  replicated_state::checkReplicatedState(ctx, std::nullopt, state.get());

  EXPECT_TRUE(ctx.hasUpdates());
  EXPECT_TRUE(std::holds_alternative<AddStateToPlanAction>(ctx.getAction()));
  auto const& action = std::get<AddStateToPlanAction>(ctx.getAction());
  EXPECT_EQ(action.statePlan.id, logId);
  EXPECT_EQ(action.logTarget.id, logId);
}

TEST_F(ReplicatedStateSupervisionTest, check_wait_current) {
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetConfig(defaultConfig);

  state.makePlan();

  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotIncomplete)
      .setTargetParticipant("B", flagsSnapshotIncomplete)
      .setTargetParticipant("C", flagsSnapshotIncomplete);

  replicated_state::SupervisionContext ctx;
  ctx.enableErrorReporting();
  replicated_state::checkReplicatedState(ctx, log.get(), state.get());

  EXPECT_TRUE(ctx.hasUpdates());
  EXPECT_TRUE(std::holds_alternative<EmptyAction>(ctx.getAction()));
  auto statusReport = ctx.getReport();
  ASSERT_EQ(statusReport.size(), 1);
  auto& message = statusReport[0];
  EXPECT_EQ(message.code, RSA::StatusCode::kLogCurrentNotAvailable);
}

TEST_F(ReplicatedStateSupervisionTest, check_wait_log_plan) {
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetConfig(defaultConfig);

  state.setPlanParticipants("A", "B", "C");
  state.makeCurrent();

  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotIncomplete)
      .setTargetParticipant("B", flagsSnapshotIncomplete)
      .setTargetParticipant("C", flagsSnapshotIncomplete);

  replicated_state::SupervisionContext ctx;
  ctx.enableErrorReporting();
  replicated_state::checkReplicatedState(ctx, log.get(), state.get());

  EXPECT_TRUE(ctx.hasUpdates());
  EXPECT_TRUE(std::holds_alternative<EmptyAction>(ctx.getAction()));
  auto statusReport = ctx.getReport();
  ASSERT_EQ(statusReport.size(), 1);
  auto& message = statusReport[0];
  EXPECT_EQ(message.code, RSA::StatusCode::kLogPlanNotAvailable);
}

TEST_F(ReplicatedStateSupervisionTest, check_wait_snapshot) {
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetConfig(defaultConfig);

  state.setPlanParticipants("A", "B", "C");
  state.makeCurrent();

  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotIncomplete)
      .setTargetParticipant("B", flagsSnapshotIncomplete)
      .setTargetParticipant("C", flagsSnapshotIncomplete);

  log.setPlanParticipant("A", flagsSnapshotIncomplete)
      .setPlanParticipant("B", flagsSnapshotIncomplete)
      .setPlanParticipant("C", flagsSnapshotIncomplete);

  replicated_state::SupervisionContext ctx;
  ctx.enableErrorReporting();
  replicated_state::checkReplicatedState(ctx, log.get(), state.get());

  EXPECT_TRUE(ctx.hasUpdates());
  EXPECT_TRUE(std::holds_alternative<EmptyAction>(ctx.getAction()))
      << std::visit([](auto const& a) { return typeid(a).name(); },
                    ctx.getAction());
  auto statusReport = ctx.getReport();
  ASSERT_EQ(statusReport.size(), 3);

  std::unordered_set<ParticipantId> participants;

  for (int i = 0; i < 3; i++) {
    auto& message = statusReport[i];
    EXPECT_EQ(message.code, RSA::StatusCode::kServerSnapshotMissing);
    if (message.participant) {
      participants.insert(*message.participant);
    }
  }

  EXPECT_EQ(participants, (std::unordered_set<ParticipantId>{"A", "B", "C"}));
}
