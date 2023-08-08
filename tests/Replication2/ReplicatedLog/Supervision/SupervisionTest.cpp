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
/// @author Markus Pfeiffer
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <variant>

#include <fmt/core.h>

#include "Replication2/Helper/AgencyLogBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"
#include "Replication2/ReplicatedLog/Supervision.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"

#include "Replication2/Mocks/MockOracle.h"

#include "Inspection/VPack.h"
#include "velocypack/Parser.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::test;

struct LeaderElectionCampaignTest : ::testing::Test {
  tests::MockCleanOracle mrProper{};
};

TEST_F(LeaderElectionCampaignTest, test_computeReason) {
  {
    auto r = computeReason(
        LogCurrentLocalState(LogTerm{1}, TermIndexPair{}, true, RebootId(1)),
        true, false, LogTerm{1});
    EXPECT_EQ(r, LogCurrentSupervisionElection::ErrorCode::OK);
  }

  {
    auto r = computeReason(
        LogCurrentLocalState(LogTerm{1}, TermIndexPair{}, true, RebootId(1)),
        false, false, LogTerm{1});
    EXPECT_EQ(r, LogCurrentSupervisionElection::ErrorCode::SERVER_NOT_GOOD);
  }

  {
    auto r = computeReason(
        LogCurrentLocalState(LogTerm{1}, TermIndexPair{}, true, RebootId(1)),
        true, false, LogTerm{3});
    EXPECT_EQ(r, LogCurrentSupervisionElection::ErrorCode::TERM_NOT_CONFIRMED);
  }

  {
    auto r = computeReason(
        LogCurrentLocalState(LogTerm{1}, TermIndexPair{}, true, RebootId(1)),
        true, true, LogTerm{3});
    EXPECT_EQ(r, LogCurrentSupervisionElection::ErrorCode::SERVER_EXCLUDED);
  }

  {
    auto r = computeReason(
        LogCurrentLocalState(LogTerm{1}, TermIndexPair{}, false, RebootId(1)),
        true, false, LogTerm{1});
    EXPECT_EQ(r, LogCurrentSupervisionElection::ErrorCode::SNAPSHOT_MISSING);
  }
}

TEST_F(LeaderElectionCampaignTest, test_runElectionCampaign_allElectible) {
  auto localStates = LogCurrentLocalStates{
      {"A",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(1)}},
      {"B",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(2)}},
      {"C",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(3)}},
  };

  auto health = ParticipantsHealth{._health{
      {"A", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"B", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"C", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}}}};

  auto config = ParticipantsConfig{
      .generation = 0,
      .participants = {
          {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}}}};

  auto campaign = runElectionCampaign(localStates, config, health, LogTerm{1},
                                      true, mrProper);

  EXPECT_EQ(campaign.participantsVoting, 3U) << campaign;
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{1}, LogIndex{1}}))
      << campaign;

  auto expectedElectible = std::map<ParticipantId, RebootId>{
      {"A", RebootId(1)}, {"B", RebootId(2)}, {"C", RebootId(3)}};
  auto electible = std::map<ParticipantId, RebootId>{};
  std::transform(std::begin(campaign.electibleLeaderSet),
                 std::end(campaign.electibleLeaderSet),
                 std::inserter(electible, std::begin(electible)),
                 [](auto&& server) {
                   return std::pair(server.serverId, server.rebootId);
                 });
  EXPECT_EQ(electible, expectedElectible);
}

TEST_F(LeaderElectionCampaignTest, test_runElectionCampaign_oneElectible) {
  auto localStates = LogCurrentLocalStates{
      {"A",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(1)}},
      {"B",
       {LogTerm{2}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(2)}},
      {"C",
       {LogTerm{2}, TermIndexPair{LogTerm{2}, LogIndex{1}}, true, RebootId(3)}},
  };

  auto health = ParticipantsHealth{._health{
      {"A", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = false}},
      {"B", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = false}},
      {"C", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}}}};

  auto config = ParticipantsConfig{
      .generation = 0,
      .participants = {
          {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}}}};

  auto campaign = runElectionCampaign(localStates, config, health, LogTerm{2},
                                      true, mrProper);

  EXPECT_EQ(campaign.participantsVoting, 1U);
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{2}, LogIndex{1}}));

  auto expectedElectible =
      std::map<ParticipantId, RebootId>{{"C", RebootId(3)}};
  auto electible = std::map<ParticipantId, RebootId>{};
  std::transform(std::begin(campaign.electibleLeaderSet),
                 std::end(campaign.electibleLeaderSet),
                 std::inserter(electible, std::begin(electible)),
                 [](auto&& server) {
                   return std::pair(server.serverId, server.rebootId);
                 });
  EXPECT_EQ(electible, expectedElectible);
}

TEST_F(LeaderElectionCampaignTest,
       test_runElectionCampaign_electible_not_in_plan) {
  // all servers have reported, but A has the longest log. However,
  // it is not in plan and should therefore not be elected.
  auto localStates = LogCurrentLocalStates{
      {"A",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{3}}, true, RebootId(1)}},
      {"B",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(2)}},
      {"C",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(3)}},
  };

  auto health = ParticipantsHealth{._health{
      {"A", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"B", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"C", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}}}};

  auto config = ParticipantsConfig{
      .generation = 0,
      .participants = {
          {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}}}};

  auto campaign = runElectionCampaign(localStates, config, health, LogTerm{1},
                                      true, mrProper);

  EXPECT_EQ(campaign.participantsVoting, 2U);
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{1}, LogIndex{1}}));

  auto expectedElectible =
      std::map<ParticipantId, RebootId>{{"B", RebootId(2)}, {"C", RebootId(3)}};
  auto electible = std::map<ParticipantId, RebootId>{};
  std::transform(std::begin(campaign.electibleLeaderSet),
                 std::end(campaign.electibleLeaderSet),
                 std::inserter(electible, std::begin(electible)),
                 [](auto&& server) {
                   return std::pair(server.serverId, server.rebootId);
                 });
  EXPECT_EQ(electible, expectedElectible);
}

TEST_F(LeaderElectionCampaignTest,
       test_runElectionCampaign_noneClean_allParticipantsAttending) {
  // No participant is clean, but all are attending, so all can vote.
  auto localStates = LogCurrentLocalStates{
      {"A",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(1)}},
      {"B",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(2)}},
      {"C",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(3)}},
  };

  auto health = ParticipantsHealth{._health{
      {"A", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"B", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"C", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
  }};

  auto config = ParticipantsConfig{
      .generation = 0,
      .participants = {
          {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      }};

  EXPECT_CALL(mrProper, serverIsCleanWfsFalse).Times(3);

  auto campaign = runElectionCampaign(localStates, config, health, LogTerm{1},
                                      false, mrProper);

  EXPECT_EQ(campaign.participantsVoting, 3U) << campaign;
  EXPECT_EQ(campaign.participantsAttending, 3U) << campaign;
  EXPECT_TRUE(campaign.allParticipantsAttending) << campaign;
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{1}, LogIndex{1}}))
      << campaign;

  auto expectedElectible = std::map<ParticipantId, RebootId>{
      {"A", RebootId(1)}, {"B", RebootId(2)}, {"C", RebootId(3)}};
  auto electible = std::map<ParticipantId, RebootId>{};
  std::transform(std::begin(campaign.electibleLeaderSet),
                 std::end(campaign.electibleLeaderSet),
                 std::inserter(electible, std::begin(electible)),
                 [](auto&& server) {
                   return std::pair(server.serverId, server.rebootId);
                 });
  EXPECT_EQ(electible, expectedElectible);
}

TEST_F(LeaderElectionCampaignTest,
       test_runElectionCampaign_oneDirty_oneMissing) {
  // B is dirty, C is missing, which means only A may vote.
  auto localStates = LogCurrentLocalStates{
      {"A",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(1)}},
      {"B",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(2)}},
      {"C",
       {LogTerm{0}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(3)}},
  };

  auto health = ParticipantsHealth{._health{
      {"A", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"B", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"C", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
  }};

  auto config = ParticipantsConfig{
      .generation = 0,
      .participants = {
          {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      }};

  EXPECT_CALL(mrProper, serverIsCleanWfsFalse)
      .Times(2)
      .WillRepeatedly([](auto const& participant) {
        if (participant.serverId == "A") {
          EXPECT_EQ(participant.rebootId, RebootId(1));
          return true;
        } else if (participant.serverId == "B") {
          EXPECT_EQ(participant.rebootId, RebootId(2));
          return false;
        } else {
          // shouldn't be called with C
          EXPECT_TRUE(false)
              << "Unexpected call with participant " << participant;
          return false;
        }
      });

  auto campaign = runElectionCampaign(localStates, config, health, LogTerm{1},
                                      false, mrProper);

  EXPECT_EQ(campaign.participantsVoting, 1U) << campaign;
  EXPECT_EQ(campaign.participantsAttending, 2U) << campaign;
  EXPECT_FALSE(campaign.allParticipantsAttending) << campaign;
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{1}, LogIndex{1}}))
      << campaign;

  auto expectedElectible = std::map<ParticipantId, RebootId>{
      {"A", RebootId(1)},
      {"B", RebootId(2)},
  };
  auto electible = std::map<ParticipantId, RebootId>{};
  std::transform(std::begin(campaign.electibleLeaderSet),
                 std::end(campaign.electibleLeaderSet),
                 std::inserter(electible, std::begin(electible)),
                 [](auto&& server) {
                   return std::pair(server.serverId, server.rebootId);
                 });
  EXPECT_EQ(electible, expectedElectible);
}

TEST_F(LeaderElectionCampaignTest,
       test_runElectionCampaign_dirty_is_electible) {
  // D is missing, A is dirty, but with the most recent spearhead. B, C, D
  // should be allowed to vote, and they should elect A.
  auto localStates = LogCurrentLocalStates{
      {"A",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{2}}, true, RebootId(7)}},
      {"B",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(1)}},
      {"C",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(1)}},
      {"D",
       {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(1)}},
      {"E",
       {LogTerm{0}, TermIndexPair{LogTerm{1}, LogIndex{1}}, true, RebootId(1)}},
  };

  auto health = ParticipantsHealth{._health{
      {"A", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"B", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"C", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"D", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
      {"E", ParticipantHealth{.rebootId = RebootId{0}, .notIsFailed = true}},
  }};

  auto config = ParticipantsConfig{
      .generation = 0,
      .participants = {
          {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"D", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
          {"E", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      }};

  EXPECT_CALL(mrProper, serverIsCleanWfsFalse)
      .Times(4)
      .WillRepeatedly([](auto const& participant) {
        if (participant.serverId == "A") {
          return false;
        } else {
          return true;
        }
      });

  auto campaign = runElectionCampaign(localStates, config, health, LogTerm{1},
                                      false, mrProper);

  EXPECT_EQ(campaign.participantsVoting, 3U) << campaign;
  EXPECT_EQ(campaign.participantsAttending, 4U) << campaign;
  EXPECT_FALSE(campaign.allParticipantsAttending) << campaign;
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{1}, LogIndex{2}}))
      << campaign;

  auto expectedElectible = std::map<ParticipantId, RebootId>{
      {"A", RebootId(7)},
  };
  auto electible = std::map<ParticipantId, RebootId>{};
  std::transform(std::begin(campaign.electibleLeaderSet),
                 std::end(campaign.electibleLeaderSet),
                 std::inserter(electible, std::begin(electible)),
                 [](auto&& server) {
                   return std::pair(server.serverId, server.rebootId);
                 });
  EXPECT_EQ(electible, expectedElectible);
}

struct SupervisionLogTest : ::testing::Test {};

TEST_F(SupervisionLogTest, test_log_created) {
  SupervisionContext ctx;

  auto const participants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto const log = Log{
      .target = LogTarget(LogId{44}, participants, LogTargetConfig(3, 2, true)),
      .plan = std::nullopt,
      .current = std::nullopt};

  checkReplicatedLog(ctx, log, ParticipantsHealth{});

  EXPECT_TRUE(ctx.hasAction());
  auto const& r = ctx.getAction();

  EXPECT_TRUE(std::holds_alternative<AddLogToPlanAction>(r));

  auto& action = std::get<AddLogToPlanAction>(r);
  EXPECT_EQ(action._participants, participants);
}

TEST_F(SupervisionLogTest, test_log_not_created) {
  SupervisionContext ctx;

  auto const participants = ParticipantsFlagsMap{
      {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto const log = Log{
      .target = LogTarget(LogId{44}, participants, LogTargetConfig(3, 2, true)),
      .plan = std::nullopt,
      .current = std::nullopt};

  checkReplicatedLog(ctx, log, ParticipantsHealth{});

  EXPECT_TRUE(ctx.hasAction());
  auto const& r = ctx.getAction();

  EXPECT_TRUE(std::holds_alternative<NoActionPossibleAction>(r));
}

struct LogSupervisionTest : ::testing::Test {
  LogId const logId = LogId{12};
  ParticipantFlags const defaultFlags{};
  LogTargetConfig const defaultConfig{2, 2, true};
  LogPlanConfig const defaultPlanConfig{2, true};
};

TEST_F(LogSupervisionTest, test_leader_not_failed) {
  // Leader is not failed and the reboot id is as expected
  auto const leader = ServerInstanceReference{"A", RebootId{1}};
  auto const health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = true}}}};

  auto r = isLeaderFailed(leader, health);
  EXPECT_FALSE(r);
}

TEST_F(LogSupervisionTest, test_leader_failed) {
  auto const leader = ServerInstanceReference{"A", RebootId{1}};
  auto const health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = false}}}};

  auto r = isLeaderFailed(leader, health);
  EXPECT_TRUE(r);
}

TEST_F(LogSupervisionTest, test_leader_wrong_reboot_id) {
  auto const leader = ServerInstanceReference{"A", RebootId{1}};
  auto const health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{15},
                                          .notIsFailed = false}}}};

  auto r = isLeaderFailed(leader, health);
  EXPECT_TRUE(r);
}

TEST_F(LogSupervisionTest, test_leader_not_known_in_health) {
  auto const leader = ServerInstanceReference{"A", RebootId{1}};
  auto const health = ParticipantsHealth{
      ._health = {{"B", ParticipantHealth{.rebootId = RebootId{15},
                                          .notIsFailed = false}}}};

  auto r = isLeaderFailed(leader, health);
  EXPECT_TRUE(r);
}

TEST_F(LogSupervisionTest, test_acceptable_leader_set) {
  auto const participants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"C", ParticipantFlags{.forced = false, .allowedAsLeader = false}},
      {"D", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"E", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto term = LogTerm{};
  auto localStates = std::unordered_map<ParticipantId, LogCurrentLocalState>{};
  localStates["A"].snapshotAvailable = true;
  localStates["B"].snapshotAvailable = true;
  localStates["C"].snapshotAvailable = true;
  localStates["D"].snapshotAvailable = true;

  auto r =
      getParticipantsAcceptableAsLeaders("A", term, participants, localStates);

  auto expectedAcceptable = std::set<ParticipantId>{"B", "D"};
  auto acceptable = std::set<ParticipantId>{};
  std::copy(std::begin(r), std::end(r),
            std::inserter(acceptable, std::begin(acceptable)));
  EXPECT_EQ(acceptable, expectedAcceptable);

  // IF the leader is changed via target, expect it to be forced first
  EXPECT_EQ(expectedAcceptable, acceptable);
}

TEST_F(LogSupervisionTest, test_remove_participant_action) {
  SupervisionContext ctx;

  auto const& logId = LogId{44};
  // Server D is missing in target
  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}}},
                LogTargetConfig(3, 3, true));

  ParticipantsFlagsMap participantsFlags{{"A", ParticipantFlags{}},
                                         {"B", ParticipantFlags{}},
                                         {"C", ParticipantFlags{}},
                                         {"D", ParticipantFlags{}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 1,
                         .participants = participantsFlags,
                         .config = LogPlanConfig(3, true)};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(LogTerm{1},
                               ServerInstanceReference{"A", RebootId{42}}),
      participantsConfig);

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfig,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};

  auto currentSupervision = LogCurrentSupervision{};
  currentSupervision.assumedWaitForSync = true;
  current.supervision.emplace(currentSupervision);
  current.supervision->assumedWriteConcern = 3;
  for (auto const& [id, _] : participantsFlags) {
    current.localState[id].term = LogTerm{1};
    current.localState[id].snapshotAvailable = true;
  }

  auto const& log = Log{.target = target, .plan = plan, .current = current};

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{42}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  checkReplicatedLog(ctx, log, health);

  EXPECT_TRUE(ctx.hasAction());
  auto const& r = ctx.getAction();

  // We expect a UpdateParticipantsFlagsAction to unset the allowedInQuorum
  // flag for d
  ASSERT_TRUE(std::holds_alternative<UpdateParticipantFlagsAction>(r))
      << fmt::format("{}", r);

  auto action = std::get<UpdateParticipantFlagsAction>(r);
  ASSERT_EQ(action._participant, "D");
  ASSERT_EQ(action._flags, (ParticipantFlags{.forced = false,
                                             .allowedInQuorum = false,
                                             .allowedAsLeader = true}));
}

TEST_F(LogSupervisionTest, test_remove_participant_action_missing_snapshot) {
  SupervisionContext ctx;

  auto const& logId = LogId{44};
  // Server D is missing in target, but B has no snapshot. Removing D
  // would violate the effective write concern.
  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}}},
                LogTargetConfig(3, 3, true));

  ParticipantsFlagsMap participantsFlags{{"A", ParticipantFlags{}},
                                         {"B", ParticipantFlags{}},
                                         {"C", ParticipantFlags{}},
                                         {"D", ParticipantFlags{}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 1,
                         .participants = participantsFlags,
                         .config = LogPlanConfig(3, true)};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(LogTerm{1},
                               ServerInstanceReference{"A", RebootId{42}}),
      participantsConfig);

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfig,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};

  auto currentSupervision = LogCurrentSupervision{};
  currentSupervision.assumedWaitForSync = true;
  current.supervision.emplace(currentSupervision);
  current.supervision->assumedWriteConcern = 3;
  for (auto const& [id, _] : participantsFlags) {
    current.localState[id].term = LogTerm{1};
    current.localState[id].snapshotAvailable = true;
  }
  current.localState["B"].snapshotAvailable = false;

  auto const& log = Log{.target = target, .plan = plan, .current = current};

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{42}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  checkReplicatedLog(ctx, log, health);

  // no action is expected.
  EXPECT_FALSE(ctx.hasAction()) << fmt::format("{}", ctx.getAction());
}

TEST_F(LogSupervisionTest, test_remove_participant_action_wait_for_committed) {
  SupervisionContext ctx;

  auto const& logId = LogId{44};

  // Server D is missing in target and has set the allowedInQuorum flag to
  // false but the config is not yet committed
  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}}},
                LogTargetConfig(3, 3, true));

  ParticipantsFlagsMap participantsFlags{
      {"A", ParticipantFlags{}},
      {"B", ParticipantFlags{}},
      {"C", ParticipantFlags{}},
      {"D", ParticipantFlags{.allowedInQuorum = false}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 2,
                         .participants = participantsFlags,
                         .config = LogPlanConfig(3, true)};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(LogTerm{1},
                               ServerInstanceReference{"A", RebootId{42}}),
      participantsConfig);

  ParticipantsFlagsMap participantsFlagsOld{{"A", ParticipantFlags{}},
                                            {"B", ParticipantFlags{}},
                                            {"C", ParticipantFlags{}},
                                            {"D", ParticipantFlags{}}};
  auto const& participantsConfigOld =
      ParticipantsConfig{.generation = 1, .participants = participantsFlagsOld};

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfigOld,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};
  current.supervision.emplace(LogCurrentSupervision{});
  for (auto const& [id, _] : participantsFlags) {
    current.localState[id].state = LocalStateMachineStatus::kOperational;
    current.localState[id].term = LogTerm{1};
    current.localState[id].snapshotAvailable = true;
  }

  auto const& log = Log{.target = target, .plan = plan, .current = current};

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{42}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  checkReplicatedLog(ctx, log, health);

  EXPECT_TRUE(ctx.hasAction());
  auto const& a = ctx.getAction();

  EXPECT_TRUE(std::holds_alternative<NoActionPossibleAction>(a))
      << fmt::format("{}", a);

  auto const& r = ctx.getReport();

  EXPECT_EQ(r.size(), 1U);

  EXPECT_TRUE(
      std::holds_alternative<LogCurrentSupervision::WaitingForConfigCommitted>(
          r[0]));
}

TEST_F(LogSupervisionTest, test_remove_participant_undo_exclude_from_quorum) {
  SupervisionContext ctx;

  auto const& logId = LogId{44};

  // Server D is supposed to be removed, but B has no snapshot. Removing
  // D would violate the effective write concern. If the allowedInQuorum = false
  // is already set, remove it.
  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}}},
                LogTargetConfig(3, 3, true));

  ParticipantsFlagsMap participantsFlags{
      {"A", ParticipantFlags{}},
      {"B", ParticipantFlags{}},
      {"C", ParticipantFlags{}},
      {"D", ParticipantFlags{.allowedInQuorum = false}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 2,
                         .participants = participantsFlags,
                         .config = LogPlanConfig(3, true)};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(LogTerm{1},
                               ServerInstanceReference{"A", RebootId{42}}),
      participantsConfig);

  ParticipantsFlagsMap participantsFlagsOld{{"A", ParticipantFlags{}},
                                            {"B", ParticipantFlags{}},
                                            {"C", ParticipantFlags{}},
                                            {"D", ParticipantFlags{}}};
  auto const& participantsConfigOld =
      ParticipantsConfig{.generation = 1, .participants = participantsFlagsOld};

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfigOld,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};
  current.supervision.emplace(LogCurrentSupervision{});
  for (auto const& [id, _] : participantsFlags) {
    current.localState[id].term = LogTerm{1};
    current.localState[id].snapshotAvailable = true;
  }
  current.localState["B"].snapshotAvailable = false;

  auto const& log = Log{.target = target, .plan = plan, .current = current};

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{42}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  checkReplicatedLog(ctx, log, health);

  EXPECT_TRUE(ctx.hasAction());
  auto const& a = ctx.getAction();

  ASSERT_TRUE(std::holds_alternative<UpdateParticipantFlagsAction>(a))
      << fmt::format("{}", a);

  auto action = std::get<UpdateParticipantFlagsAction>(a);
  ASSERT_EQ(action._participant, "D");
  ASSERT_EQ(action._flags, (ParticipantFlags{.forced = false,
                                             .allowedInQuorum = true,
                                             .allowedAsLeader = true}));
}

TEST_F(LogSupervisionTest, test_remove_participant_action_committed) {
  SupervisionContext ctx;

  auto const& logId = LogId{44};

  // Server D is missing in target and has set the allowedInQuorum flag to
  // false but the config is not yet committed
  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}}},
                LogTargetConfig(3, 3, true));

  ParticipantsFlagsMap participantsFlags{
      {"A", ParticipantFlags{}},
      {"B", ParticipantFlags{}},
      {"C", ParticipantFlags{}},
      {"D", ParticipantFlags{.allowedInQuorum = false}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 2,
                         .participants = participantsFlags,
                         .config = LogPlanConfig(3, true)};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(LogTerm{1},
                               ServerInstanceReference{"A", RebootId{42}}),
      participantsConfig);

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfig,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};
  auto currentSupervision = LogCurrentSupervision{};
  currentSupervision.assumedWaitForSync = true;
  current.supervision.emplace(currentSupervision);
  current.supervision->assumedWriteConcern = 3;
  for (auto const& [id, _] : participantsFlags) {
    current.localState[id].state = LocalStateMachineStatus::kOperational;
    current.localState[id].term = LogTerm{1};
    current.localState[id].snapshotAvailable = true;
  }

  auto const& log = Log{.target = target, .plan = plan, .current = current};

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{42}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  checkReplicatedLog(ctx, log, health);

  EXPECT_TRUE(ctx.hasAction());
  auto const& r = ctx.getAction();

  // We expect an RemoveParticipantFromPlanAction to finally remove D
  ASSERT_TRUE(std::holds_alternative<RemoveParticipantFromPlanAction>(r))
      << fmt::format("{}", r);

  auto action = std::get<RemoveParticipantFromPlanAction>(r);
  ASSERT_EQ(action._participant, "D");
}

TEST_F(LogSupervisionTest, test_write_empty_term) {
  SupervisionContext ctx;

  auto const& logId = LogId{44};

  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}},
                                     {"D", ParticipantFlags{}}},
                LogTargetConfig(3, 3, true));

  ParticipantsFlagsMap participantsFlags{
      {"A", ParticipantFlags{}},
      {"B", ParticipantFlags{}},
      {"C", ParticipantFlags{}},
      {"D", ParticipantFlags{.allowedInQuorum = false}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 2,
                         .participants = participantsFlags,
                         .config = LogPlanConfig(3, true)};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(LogTerm{2},
                               ServerInstanceReference{"A", RebootId{42}}),
      participantsConfig);

  ParticipantsFlagsMap participantsFlagsOld{{"A", ParticipantFlags{}},
                                            {"B", ParticipantFlags{}},
                                            {"C", ParticipantFlags{}},
                                            {"D", ParticipantFlags{}}};
  auto const& participantsConfigOld =
      ParticipantsConfig{.generation = 1, .participants = participantsFlagsOld};

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfigOld,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};
  current.localState = {
      {"A",
       LogCurrentLocalState(LogTerm(2), TermIndexPair(LogTerm(1), LogIndex(44)),
                            true, RebootId(1))},
      {"B",
       LogCurrentLocalState(LogTerm(2), TermIndexPair(LogTerm(1), LogIndex(44)),
                            true, RebootId(1))},
      {"C",
       LogCurrentLocalState(LogTerm(2), TermIndexPair(LogTerm(3), LogIndex(44)),
                            true, RebootId(1))},
      {"D",
       LogCurrentLocalState(LogTerm(2), TermIndexPair(LogTerm(1), LogIndex(44)),
                            true, RebootId(1))},
  };
  current.supervision.emplace(LogCurrentSupervision{});
  current.localState["A"].snapshotAvailable = true;
  current.localState["B"].snapshotAvailable = true;
  current.localState["C"].snapshotAvailable = true;
  current.localState["D"].snapshotAvailable = true;

  auto const& log = Log{.target = target, .plan = plan, .current = current};

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{44}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  checkReplicatedLog(ctx, log, health);

  EXPECT_TRUE(ctx.hasAction());
  auto const& r = ctx.getAction();

  // Since the leader is `A` and the rebootId in health is higher than the one
  // in plan, we need to write an empty term
  ASSERT_TRUE(std::holds_alternative<WriteEmptyTermAction>(r))
      << fmt::format("{}", r);

  auto writeEmptyTermAction = std::get<WriteEmptyTermAction>(r);

  ASSERT_EQ(writeEmptyTermAction.minTerm, LogTerm{3});
}

TEST_F(LogSupervisionTest, test_compute_effective_write_concern_correct_term) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig{3, 3, true})
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags);

  log.setPlanLeader("A");
  log.acknowledgeTerm("A").acknowledgeTerm("B").acknowledgeTerm("C");
  log.allSnapshotsTrue();

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});

  auto effectiveWriteConcern = computeEffectiveWriteConcern(
      log.get().target.config, log.get().current.value(),
      log.get().plan.value(), health);
  ASSERT_EQ(effectiveWriteConcern, 3U);
}

TEST_F(LogSupervisionTest, test_compute_effective_write_concern_wrong_term) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig{1, 3, true})
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags);

  log.setPlanLeader("A");
  auto& term = log.makeTerm();
  log.acknowledgeTerm("A").acknowledgeTerm("B").acknowledgeTerm("C");
  // C will be in the wrong term.
  term.term = LogTerm{2};
  log.acknowledgeTerm("A").acknowledgeTerm("B");
  log.allSnapshotsTrue();

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});

  auto effectiveWriteConcern = computeEffectiveWriteConcern(
      log.get().target.config, log.get().current.value(),
      log.get().plan.value(), health);
  ASSERT_EQ(effectiveWriteConcern, 2U);
}

TEST_F(LogSupervisionTest, test_compute_effective_write_concern_no_snapshot) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig{1, 3, true})
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags);

  log.setPlanLeader("A");
  // C remains without snapshot
  log.acknowledgeTerm("A").acknowledgeTerm("B").acknowledgeTerm("C");
  log.setSnapshotTrue("A").setSnapshotTrue("B");

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});

  auto effectiveWriteConcern = computeEffectiveWriteConcern(
      log.get().target.config, log.get().current.value(),
      log.get().plan.value(), health);
  ASSERT_EQ(effectiveWriteConcern, 2U);
}

TEST_F(LogSupervisionTest, test_compute_effective_write_concern) {
  auto const config = LogTargetConfig(3, 3, false);
  auto const current = LogCurrent();
  auto const participants = ParticipantsFlagsMap{{"A", {}}};
  auto const health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{44}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  auto effectiveWriteConcern =
      computeEffectiveWriteConcern(config, participants, health);
  ASSERT_EQ(effectiveWriteConcern, 3U);
}

TEST_F(LogSupervisionTest,
       test_compute_effective_write_concern_accepts_higher_soft_write_concern) {
  auto const config = LogTargetConfig(2, 5, false);
  auto const participants = ParticipantsFlagsMap{
      {"A", {}}, {"B", {}}, {"C", {}}, {"D", {}}, {"E", {}}};
  auto const health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{44}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = false}}}};

  auto effectiveWriteConcern =
      computeEffectiveWriteConcern(config, participants, health);
  ASSERT_EQ(effectiveWriteConcern, 3U);
}

TEST_F(LogSupervisionTest,
       test_compute_effective_write_concern_with_all_participants_failed) {
  auto const config = LogTargetConfig(2, 5, false);
  auto const participants = ParticipantsFlagsMap{
      {"A", {}}, {"B", {}}, {"C", {}}, {"D", {}}, {"E", {}}};
  auto const health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{44}, .notIsFailed = false}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = false}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = false}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = false}}}};

  auto effectiveWriteConcern =
      computeEffectiveWriteConcern(config, participants, health);
  ASSERT_EQ(effectiveWriteConcern, 2U);
}

TEST_F(
    LogSupervisionTest,
    test_compute_effective_write_concern_with_no_intersection_between_participants_and_health) {
  auto const config = LogTargetConfig(2, 5, false);
  auto const participants = ParticipantsFlagsMap{{"A", {}}};
  auto const health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{44}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  auto effectiveWriteConcern =
      computeEffectiveWriteConcern(config, participants, health);
  ASSERT_EQ(effectiveWriteConcern, 2U);
}

TEST_F(LogSupervisionTest, test_convergence_no_leader_established) {
  AgencyLogBuilder log;
  log.setTargetConfig(defaultConfig)
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags)
      .setTargetVersion(5);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags);
  log.setPlanLeader("A").setPlanConfig(defaultPlanConfig);
  log.acknowledgeTerm("A").acknowledgeTerm("B").acknowledgeTerm("C");
  log.allSnapshotsTrue().allStatesReady();

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});

  {
    SupervisionContext ctx;
    checkReplicatedLog(ctx, log.get(), health);
    EXPECT_FALSE(ctx.hasAction());
  }
  log.establishLeadership();
  {
    SupervisionContext ctx;
    checkReplicatedLog(ctx, log.get(), health);
    EXPECT_TRUE(ctx.hasAction());
    auto const& r = ctx.getAction();
    EXPECT_TRUE(std::holds_alternative<ConvergedToTargetAction>(r));
  }
}

TEST_F(LogSupervisionTest, test_leader_election_sets_write_concern) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig{2, 3, true})
      .setId(logId)
      .setTargetParticipant("A", defaultFlags)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags)
      .setTargetParticipant("D", defaultFlags)
      .setTargetVersion(1);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags)
      .setPlanParticipant("D", defaultFlags);
  log.setPlanLeader("A").setPlanConfig(defaultPlanConfig);

  log.establishLeadership();
  log.setEmptyTerm();

  log.acknowledgeTerm("A")
      .acknowledgeTerm("B")
      .acknowledgeTerm("C")
      .acknowledgeTerm("D")
      .allSnapshotsTrue()
      .allStatesReady();

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = false});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "D", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});

  {
    SupervisionContext ctx;
    checkReplicatedLog(ctx, log.get(), health);
    EXPECT_TRUE(ctx.hasAction());

    auto const& r = ctx.getAction();
    EXPECT_TRUE(std::holds_alternative<LeaderElectionAction>(r))
        << fmt::format("{}", r);

    auto const& elec = std::get<LeaderElectionAction>(r);
    EXPECT_EQ(elec._assumedWriteConcern, 2U);
    EXPECT_EQ(elec._effectiveWriteConcern, 3U);
  }
}

TEST_F(LogSupervisionTest, test_wait_for_config_committed_action) {
  AgencyLogBuilder log;
  log.setTargetConfig(LogTargetConfig{2, 2, true})
      .setId(logId)
      .setTargetParticipant("B", defaultFlags)
      .setTargetParticipant("C", defaultFlags)
      .setTargetParticipant("D", defaultFlags)
      .setTargetVersion(1);

  log.setPlanParticipant("A", defaultFlags)
      .setPlanParticipant("B", defaultFlags)
      .setPlanParticipant("C", defaultFlags)
      .setPlanParticipant("D", defaultFlags);
  log.setPlanLeader("A").setPlanConfig(defaultPlanConfig);
  log.establishLeadership();
  log.commitCurrentParticipantsConfig();
  log.setPlanConfigGeneration(2);
  log.setPlanParticipant("B", {true, true, true});

  log.acknowledgeTerm("A")
      .acknowledgeTerm("B")
      .acknowledgeTerm("C")
      .acknowledgeTerm("D")
      .allSnapshotsTrue();

  replicated_log::ParticipantsHealth health;
  health._health.emplace(
      "A", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "B", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "C", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});
  health._health.emplace(
      "D", replicated_log::ParticipantHealth{.rebootId = RebootId(0),
                                             .notIsFailed = true});

  {
    SupervisionContext ctx;
    checkReplicatedLog(ctx, log.get(), health);
    EXPECT_TRUE(ctx.hasAction());

    auto const& r = ctx.getAction();
    ASSERT_TRUE(std::holds_alternative<NoActionPossibleAction>(r))
        << fmt::format("{}", r);
  }
}
