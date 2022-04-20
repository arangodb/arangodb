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

  ParticipantFlags const flagsSnapshotComplete{};
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

TEST_F(ReplicatedStateSupervisionTest, check_snapshot_complete) {
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetConfig(defaultConfig);

  state.setPlanParticipants("A", "B", "C").setSnapshotCompleteFor("A");
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
  EXPECT_TRUE(
      std::holds_alternative<UpdateParticipantFlagsAction>(ctx.getAction()));
  auto& action = std::get<UpdateParticipantFlagsAction>(ctx.getAction());
  EXPECT_EQ(action.participant, "A");
  EXPECT_EQ(action.flags, flagsSnapshotComplete);

  auto statusReport = ctx.getReport();
  ASSERT_EQ(statusReport.size(), 2);

  std::unordered_set<ParticipantId> participants;

  for (int i = 0; i < 2; i++) {
    auto& message = statusReport[i];
    EXPECT_EQ(message.code, RSA::StatusCode::kServerSnapshotMissing);
    if (message.participant) {
      participants.insert(*message.participant);
    }
  }

  EXPECT_EQ(participants, (std::unordered_set<ParticipantId>{"B", "C"}));
}

TEST_F(ReplicatedStateSupervisionTest, check_all_snapshot_complete) {
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C")
      .setTargetVersion(12)
      .setTargetConfig(defaultConfig);

  state.setPlanParticipants("A", "B", "C").setAllSnapshotsComplete();
  state.setCurrentVersion(12);

  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotComplete)
      .setTargetParticipant("B", flagsSnapshotComplete)
      .setTargetParticipant("C", flagsSnapshotComplete);

  log.setPlanParticipant("A", flagsSnapshotComplete)
      .setPlanParticipant("B", flagsSnapshotComplete)
      .setPlanParticipant("C", flagsSnapshotComplete);

  replicated_state::SupervisionContext ctx;
  ctx.enableErrorReporting();
  replicated_state::checkReplicatedState(ctx, log.get(), state.get());

  EXPECT_FALSE(ctx.hasUpdates());
}

TEST_F(ReplicatedStateSupervisionTest, check_add_participant_1) {
  // Server "D" was added to target
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C", "D")
      .setTargetVersion(12)
      .setTargetConfig(defaultConfig);

  state.setPlanParticipants("A", "B", "C").setAllSnapshotsComplete();
  state.setCurrentVersion(5);

  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotComplete)
      .setTargetParticipant("B", flagsSnapshotComplete)
      .setTargetParticipant("C", flagsSnapshotComplete);

  log.setPlanParticipant("A", flagsSnapshotComplete)
      .setPlanParticipant("B", flagsSnapshotComplete)
      .setPlanParticipant("C", flagsSnapshotComplete);

  replicated_state::SupervisionContext ctx;
  ctx.enableErrorReporting();
  replicated_state::checkReplicatedState(ctx, log.get(), state.get());

  EXPECT_TRUE(ctx.hasUpdates());
  EXPECT_TRUE(std::holds_alternative<AddParticipantAction>(ctx.getAction()));
  auto& action = std::get<AddParticipantAction>(ctx.getAction());
  EXPECT_EQ(action.participant, "D");
}

TEST_F(ReplicatedStateSupervisionTest, check_add_participant_2) {
  // Server "D" is now in State/Plan and Log/Target
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C", "D")
      .setTargetVersion(12)
      .setTargetConfig(defaultConfig);

  state.setPlanParticipants("A", "B", "C")
      .setAllSnapshotsComplete()
      .addPlanParticipant("D");
  state.setCurrentVersion(5);

  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotComplete)
      .setTargetParticipant("B", flagsSnapshotComplete)
      .setTargetParticipant("C", flagsSnapshotComplete)
      .setTargetParticipant("D", flagsSnapshotIncomplete);

  log.setPlanParticipant("A", flagsSnapshotComplete)
      .setPlanParticipant("B", flagsSnapshotComplete)
      .setPlanParticipant("C", flagsSnapshotComplete);

  replicated_state::SupervisionContext ctx;
  ctx.enableErrorReporting();
  replicated_state::checkReplicatedState(ctx, log.get(), state.get());

  EXPECT_TRUE(ctx.hasUpdates());
  EXPECT_TRUE(std::holds_alternative<EmptyAction>(ctx.getAction()));
  auto& report = ctx.getReport();
  ASSERT_EQ(report.size(), 1);
  EXPECT_EQ(report[0].code, RSA::StatusCode::kServerSnapshotMissing);
}

TEST_F(ReplicatedStateSupervisionTest, check_add_participant_3_1) {
  // Server "D" is now in Log/Current committed
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C", "D")
      .setTargetVersion(12)
      .setTargetConfig(defaultConfig);

  state.setPlanParticipants("A", "B", "C")
      .setAllSnapshotsComplete()
      .addPlanParticipant("D");
  state.setCurrentVersion(5);

  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotComplete)
      .setTargetParticipant("B", flagsSnapshotComplete)
      .setTargetParticipant("C", flagsSnapshotComplete)
      .setTargetParticipant("D", flagsSnapshotIncomplete);

  log.setPlanParticipant("A", flagsSnapshotComplete)
      .setPlanParticipant("B", flagsSnapshotComplete)
      .setPlanParticipant("C", flagsSnapshotComplete)
      .setPlanParticipant("D", flagsSnapshotIncomplete);

  replicated_state::SupervisionContext ctx;
  ctx.enableErrorReporting();
  replicated_state::checkReplicatedState(ctx, log.get(), state.get());

  EXPECT_TRUE(ctx.hasUpdates());
  EXPECT_TRUE(std::holds_alternative<EmptyAction>(ctx.getAction()));
  auto& report = ctx.getReport();
  ASSERT_EQ(report.size(), 1);
  EXPECT_EQ(report[0].code, RSA::StatusCode::kServerSnapshotMissing);
}

TEST_F(ReplicatedStateSupervisionTest, check_add_participant_3_2) {
  // Server "D" is not yet in Log/Current but the snapshot is completed
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C", "D")
      .setTargetVersion(12)
      .setTargetConfig(defaultConfig);

  state.setPlanParticipants("A", "B", "C", "D").setAllSnapshotsComplete();
  state.setCurrentVersion(5);

  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotComplete)
      .setTargetParticipant("B", flagsSnapshotComplete)
      .setTargetParticipant("C", flagsSnapshotComplete)
      .setTargetParticipant("D", flagsSnapshotIncomplete);

  log.setPlanParticipant("A", flagsSnapshotComplete)
      .setPlanParticipant("B", flagsSnapshotComplete)
      .setPlanParticipant("C", flagsSnapshotComplete)
      .setPlanParticipant("D", flagsSnapshotIncomplete);

  replicated_state::SupervisionContext ctx;
  ctx.enableErrorReporting();
  replicated_state::checkReplicatedState(ctx, log.get(), state.get());

  EXPECT_TRUE(ctx.hasUpdates());
  EXPECT_TRUE(
      std::holds_alternative<UpdateParticipantFlagsAction>(ctx.getAction()));
  auto& action = std::get<UpdateParticipantFlagsAction>(ctx.getAction());
  EXPECT_EQ(action.participant, "D");
  EXPECT_EQ(action.flags, flagsSnapshotComplete);
}

TEST_F(ReplicatedStateSupervisionTest, check_add_participant_4) {
  AgencyStateBuilder state;
  AgencyLogBuilder log;
  state.setId(logId)
      .setTargetParticipants("A", "B", "C", "D")
      .setTargetVersion(12)
      .setTargetConfig(defaultConfig);

  state.setPlanParticipants("A", "B", "C", "D").setAllSnapshotsComplete();
  state.setCurrentVersion(5);

  log.setId(logId)
      .setTargetParticipant("A", flagsSnapshotComplete)
      .setTargetParticipant("B", flagsSnapshotComplete)
      .setTargetParticipant("C", flagsSnapshotComplete)
      .setTargetParticipant("D", flagsSnapshotComplete);

  log.setPlanParticipant("A", flagsSnapshotComplete)
      .setPlanParticipant("B", flagsSnapshotComplete)
      .setPlanParticipant("C", flagsSnapshotComplete)
      .setPlanParticipant("D", flagsSnapshotIncomplete);

  replicated_state::SupervisionContext ctx;
  ctx.enableErrorReporting();
  replicated_state::checkReplicatedState(ctx, log.get(), state.get());

  EXPECT_TRUE(ctx.hasUpdates());
  EXPECT_TRUE(std::holds_alternative<CurrentConvergedAction>(ctx.getAction()));
  auto& action = std::get<CurrentConvergedAction>(ctx.getAction());
  EXPECT_EQ(action.version, 12);
}
