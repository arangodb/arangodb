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
#include <variant>

#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/Supervision.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;
using namespace arangodb::replication2::replicated_log;

struct LeaderElectionCampaignTest : ::testing::Test {};
TEST_F(LeaderElectionCampaignTest, test_computeReason) {
  {
    auto r = computeReason(LogCurrentLocalState(LogTerm{1}, TermIndexPair{}),
                           true, false, LogTerm{1});
    EXPECT_EQ(r, LogCurrentSupervisionElection::ErrorCode::OK);
  }

  {
    auto r = computeReason(LogCurrentLocalState(LogTerm{1}, TermIndexPair{}),
                           false, false, LogTerm{1});
    EXPECT_EQ(r, LogCurrentSupervisionElection::ErrorCode::SERVER_NOT_GOOD);
  }

  {
    auto r = computeReason(LogCurrentLocalState(LogTerm{1}, TermIndexPair{}),
                           true, false, LogTerm{3});
    EXPECT_EQ(r, LogCurrentSupervisionElection::ErrorCode::TERM_NOT_CONFIRMED);
  }

  {
    auto r = computeReason(LogCurrentLocalState(LogTerm{1}, TermIndexPair{}),
                           true, true, LogTerm{3});
    EXPECT_EQ(r, LogCurrentSupervisionElection::ErrorCode::SERVER_EXCLUDED);
  }
}

TEST_F(LeaderElectionCampaignTest, test_runElectionCampaign_allElectible) {
  auto localStates = LogCurrentLocalStates{
      {"A", {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}}},
      {"B", {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}}},
      {"C", {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}}}};

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

  auto campaign = runElectionCampaign(localStates, config, health, LogTerm{1});

  EXPECT_EQ(campaign.participantsAvailable, 3);  // TODO: Fixme
                                                 // << campaign;
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{1}, LogIndex{1}}));
  // TODO: FIXME<< campaign;

  auto expectedElectible = std::set<ParticipantId>{"A", "B", "C"};
  auto electible = std::set<ParticipantId>{};
  std::copy(std::begin(campaign.electibleLeaderSet),
            std::end(campaign.electibleLeaderSet),
            std::inserter(electible, std::begin(electible)));
  EXPECT_EQ(electible, expectedElectible);
}

TEST_F(LeaderElectionCampaignTest, test_runElectionCampaign_oneElectible) {
  auto localStates = LogCurrentLocalStates{
      {"A", {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}}},
      {"B", {LogTerm{2}, TermIndexPair{LogTerm{1}, LogIndex{1}}}},
      {"C", {LogTerm{2}, TermIndexPair{LogTerm{2}, LogIndex{1}}}}};

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

  auto campaign = runElectionCampaign(localStates, config, health, LogTerm{2});

  EXPECT_EQ(campaign.participantsAvailable, 1);
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{2}, LogIndex{1}}));

  auto expectedElectible = std::set<ParticipantId>{"C"};
  auto electible = std::set<ParticipantId>{};
  std::copy(std::begin(campaign.electibleLeaderSet),
            std::end(campaign.electibleLeaderSet),
            std::inserter(electible, std::begin(electible)));
  EXPECT_EQ(electible, expectedElectible);
}

// TODO: election campaigns that fail

struct LeaderStateMachineTest : ::testing::Test {};

TEST_F(LeaderStateMachineTest, test_election_success) {
  // We have no leader, so we have to first run a leadership campaign and then
  // select a leader.
  auto const& config = LogConfig(3, 3, 3, true);

  auto current = LogCurrent();
  current.localState = std::unordered_map<ParticipantId, LogCurrentLocalState>(
      {{"A", LogCurrentLocalState(LogTerm{1},
                                  TermIndexPair{LogTerm{1}, LogIndex{1}})},
       {"B", LogCurrentLocalState(LogTerm{1},
                                  TermIndexPair{LogTerm{1}, LogIndex{1}})},
       {"C", LogCurrentLocalState(LogTerm{1},
                                  TermIndexPair{LogTerm{1}, LogIndex{1}})}});
  current.supervision = LogCurrentSupervision{};

  auto plan = LogPlanSpecification(
      LogId{1}, LogPlanTermSpecification(LogTerm{1}, config, std::nullopt),
      ParticipantsConfig{
          .generation = 1,
          .participants = {
              {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
              {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},

              {"C",
               ParticipantFlags{.forced = false, .allowedAsLeader = true}}}});

  auto health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = true}},
                  {"B", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = true}},
                  {"C", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = true}}}};

  auto r = doLeadershipElection(plan, current, health);
  EXPECT_TRUE(std::holds_alternative<LeaderElectionAction>(r));

  auto& action = std::get<LeaderElectionAction>(r);

  auto possibleLeaders = std::set<ParticipantId>{"A", "B", "C"};
  EXPECT_TRUE(possibleLeaders.contains(action._electedLeader.serverId));
  EXPECT_EQ(action._electedLeader.rebootId, RebootId{1});
}

TEST_F(LeaderStateMachineTest, test_election_fails) {
  // Here the RebootId of the leader "A" in the Plan is 42, but
  // the health record says its RebootId is 43; this
  // means that the leader is not acceptable anymore and we
  // expect a new term that has the leader removed.
  auto const& config = LogConfig(3, 3, 3, true);

  auto current = LogCurrent();
  current.localState = std::unordered_map<ParticipantId, LogCurrentLocalState>(
      {{"A", LogCurrentLocalState(LogTerm{1},
                                  TermIndexPair{LogTerm{1}, LogIndex{1}})},
       {"B", LogCurrentLocalState(LogTerm{1},
                                  TermIndexPair{LogTerm{1}, LogIndex{1}})},
       {"C", LogCurrentLocalState(LogTerm{1},
                                  TermIndexPair{LogTerm{1}, LogIndex{1}})}});
  current.supervision = LogCurrentSupervision{};

  auto const& plan = LogPlanSpecification(
      LogId{1},
      LogPlanTermSpecification(
          LogTerm{1}, config,
          LogPlanTermSpecification::Leader{"A", RebootId{42}}),
      ParticipantsConfig{
          .generation = 1,
          .participants = {
              {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
              {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},

              {"C",
               ParticipantFlags{.forced = false, .allowedAsLeader = true}}}});

  auto const& health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{43},
                                          .notIsFailed = true}},
                  {"B", ParticipantHealth{.rebootId = RebootId{14},
                                          .notIsFailed = true}},
                  {"C", ParticipantHealth{.rebootId = RebootId{14},
                                          .notIsFailed = true}}}};

  // TODO: This doesn't test what it claims to
  auto r = isLeaderFailed(*plan.currentTerm->leader, health);

  EXPECT_TRUE(r);
}

TEST_F(LeaderStateMachineTest, test_election_leader_with_higher_term) {
  // here we have a participant "C" with a *better* TermIndexPair than the
  // others because it has a higher LogTerm, but a lower LogIndex
  // so we expect "C" to be elected leader
  auto const& config = LogConfig(3, 3, 3, true);

  auto current = LogCurrent();
  current.localState = std::unordered_map<ParticipantId, LogCurrentLocalState>(
      {{"A", LogCurrentLocalState(LogTerm{1},
                                  TermIndexPair{LogTerm{1}, LogIndex{15}})},
       {"B", LogCurrentLocalState(LogTerm{1},
                                  TermIndexPair{LogTerm{1}, LogIndex{27}})},
       {"C", LogCurrentLocalState(LogTerm{1},
                                  TermIndexPair{LogTerm{4}, LogIndex{42}})}});
  current.supervision = LogCurrentSupervision{};

  auto const& plan = LogPlanSpecification(
      LogId{1}, LogPlanTermSpecification(LogTerm{1}, config, std::nullopt),
      ParticipantsConfig{
          .generation = 1,
          .participants = {
              {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
              {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
              {"C",
               ParticipantFlags{.forced = false, .allowedAsLeader = true}}}});

  auto const& health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{43},
                                          .notIsFailed = true}},
                  {"B", ParticipantHealth{.rebootId = RebootId{14},
                                          .notIsFailed = true}},
                  {"C", ParticipantHealth{.rebootId = RebootId{14},
                                          .notIsFailed = true}}}};

  auto r = doLeadershipElection(plan, current, health);

  EXPECT_TRUE(std::holds_alternative<LeaderElectionAction>(r));

  auto& action = std::get<LeaderElectionAction>(r);
  EXPECT_EQ(action._electedLeader.serverId, "C");
  EXPECT_EQ(action._electedLeader.rebootId, RebootId{14});
}

TEST_F(LeaderStateMachineTest, test_leader_intact) {
  auto const& config = LogConfig(3, 3, 3, true);
  auto const& plan = LogPlanSpecification(
      LogId{1},
      LogPlanTermSpecification(
          LogTerm{1}, config,
          LogPlanTermSpecification::Leader{"A", RebootId{1}}),
      {});

  auto const& health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = true}},
                  {"B", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = true}},
                  {"C", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = true}}}};

  auto r = isLeaderFailed(*plan.currentTerm->leader, health);
  EXPECT_FALSE(r);
}

struct SupervisionLogTest : ::testing::Test {};

TEST_F(SupervisionLogTest, test_log_created) {
  auto const config = LogConfig(3, 2, 3, true);
  auto const participants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto r = checkReplicatedLog(LogTarget(LogId{44}, participants, config),
                              std::nullopt, std::nullopt, ParticipantsHealth{});

  EXPECT_TRUE(std::holds_alternative<AddLogToPlanAction>(r));

  auto& action = std::get<AddLogToPlanAction>(r);
  EXPECT_EQ(action._participants, participants);
}

TEST_F(SupervisionLogTest, test_log_not_created) {
  auto const config = LogConfig(3, 2, 3, true);
  auto const participants = ParticipantsFlagsMap{
      {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto r = checkReplicatedLog(LogTarget(LogId{44}, participants, config),
                              std::nullopt, std::nullopt, ParticipantsHealth{});

  EXPECT_TRUE(std::holds_alternative<ErrorAction>(r));

  auto& action = std::get<ErrorAction>(r);
  EXPECT_EQ(action._error,
            LogCurrentSupervisionError::TARGET_NOT_ENOUGH_PARTICIPANTS);
}

TEST_F(SupervisionLogTest, test_log_present) {
  auto const config = LogConfig(3, 2, 3, true);
  auto const participants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"C", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto r = checkReplicatedLog(LogTarget(LogId(44), participants, config),
                              LogPlanSpecification(), std::nullopt,
                              ParticipantsHealth());

  EXPECT_TRUE(std::holds_alternative<CreateInitialTermAction>(r))
      << to_string(r);
}

struct LogSupervisionTest : ::testing::Test {};

TEST_F(LogSupervisionTest, test_leader_not_failed) {
  // Leader is not failed and the reboot id is as expected
  auto const leader = LogPlanTermSpecification::Leader{"A", RebootId{1}};
  auto const health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = true}}}};

  auto r = isLeaderFailed(leader, health);
  EXPECT_FALSE(r);
}

TEST_F(LogSupervisionTest, test_leader_failed) {
  auto const leader = LogPlanTermSpecification::Leader{"A", RebootId{1}};
  auto const health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{1},
                                          .notIsFailed = false}}}};

  auto r = isLeaderFailed(leader, health);
  EXPECT_TRUE(r);
}

TEST_F(LogSupervisionTest, test_leader_wrong_reboot_id) {
  auto const leader = LogPlanTermSpecification::Leader{"A", RebootId{1}};
  auto const health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = RebootId{15},
                                          .notIsFailed = false}}}};

  auto r = isLeaderFailed(leader, health);
  EXPECT_TRUE(r);
}

TEST_F(LogSupervisionTest, test_leader_not_known_in_health) {
  auto const leader = LogPlanTermSpecification::Leader{"A", RebootId{1}};
  auto const health = ParticipantsHealth{
      ._health = {{"B", ParticipantHealth{.rebootId = RebootId{15},
                                          .notIsFailed = false}}}};

  auto r = isLeaderFailed(leader, health);
  EXPECT_TRUE(r);
}

TEST_F(LogSupervisionTest, test_participant_added) {
  auto const targetParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto const planParticipants = ParticipantsFlagsMap{};

  auto r = getAddedParticipant(targetParticipants, planParticipants);
  EXPECT_TRUE(r);

  EXPECT_EQ(r->first, "A");
  EXPECT_EQ(r->second,
            (ParticipantFlags{.forced = false, .allowedAsLeader = true}));
}

TEST_F(LogSupervisionTest, test_no_participant_added) {
  auto const targetParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto const planParticipants = targetParticipants;

  auto r = getAddedParticipant(targetParticipants, planParticipants);
  EXPECT_FALSE(r);
}

TEST_F(LogSupervisionTest, test_participant_removed) {
  auto const targetParticipants = ParticipantsFlagsMap{};

  auto const planParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto r = getRemovedParticipant(targetParticipants, planParticipants);
  EXPECT_TRUE(r);

  EXPECT_EQ(r->first, "A");
}

TEST_F(LogSupervisionTest, test_no_participant_removed) {
  auto const targetParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto const planParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto r = getRemovedParticipant(targetParticipants, planParticipants);
  EXPECT_FALSE(r);
}

TEST_F(LogSupervisionTest, test_no_flags_changed) {
  auto const targetParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto const planParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto r = getParticipantWithUpdatedFlags(targetParticipants, planParticipants,
                                          std::nullopt, "A");
  EXPECT_FALSE(r);
}

TEST_F(LogSupervisionTest, test_flags_changed) {
  auto const targetParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = false}}};

  auto const planParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto r = getParticipantWithUpdatedFlags(targetParticipants, planParticipants,
                                          std::nullopt, "A");
  EXPECT_TRUE(r);
  EXPECT_EQ(r->first, "A");
  EXPECT_EQ(r->second,
            (ParticipantFlags{.forced = false, .allowedAsLeader = false}));
}

TEST_F(LogSupervisionTest, test_leader_changed) {
  auto const targetParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto const planParticipants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto r = getParticipantWithUpdatedFlags(targetParticipants, planParticipants,
                                          "B", "A");
  EXPECT_TRUE(r);

  // IF the leader is changed via target, expect it to be forced first
  EXPECT_EQ(r->first, "B");
  EXPECT_EQ(r->second,
            (ParticipantFlags{.forced = true, .allowedAsLeader = true}));
}

TEST_F(LogSupervisionTest, test_acceptable_leader_set) {
  auto const participants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"C", ParticipantFlags{.forced = false, .allowedAsLeader = false}},
      {"D", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto r = getParticipantsAcceptableAsLeaders("A", participants);

  auto expectedAcceptable = std::set<ParticipantId>{"B", "D"};
  auto acceptable = std::set<ParticipantId>{};
  std::copy(std::begin(r), std::end(r),
            std::inserter(acceptable, std::begin(acceptable)));
  EXPECT_EQ(acceptable, expectedAcceptable);

  // IF the leader is changed via target, expect it to be forced first
  EXPECT_EQ(expectedAcceptable, acceptable);
}

TEST_F(LogSupervisionTest, test_dictate_leader_no_current) {
  auto const& logId = LogId{44};
  auto const& config = LogConfig(3, 3, 3, true);
  auto const& participants = ParticipantsFlagsMap{};
  auto const& target = LogTarget(logId, participants, config);

  auto const& plan = LogPlanSpecification(
      logId, LogPlanTermSpecification(LogTerm{1}, config, std::nullopt),
      ParticipantsConfig{.generation = 1, .participants = participants});

  auto const& current = LogCurrent{};

  auto const& health = ParticipantsHealth{._health = {}};

  auto r = dictateLeader(target, plan, current, health);

  ASSERT_TRUE(std::holds_alternative<DictateLeaderFailedAction>(r))
      << to_string(r);
}

TEST_F(LogSupervisionTest, test_dictate_leader_force_first) {
  auto const& logId = LogId{44};
  auto const& config = LogConfig(3, 3, 3, true);

  auto const& participants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"C", ParticipantFlags{.forced = false, .allowedAsLeader = false}},
      {"D", ParticipantFlags{.forced = false, .allowedAsLeader = true}}};

  auto const& target = LogTarget(logId, participants, config);

  auto const& participantsConfig =
      ParticipantsConfig{.generation = 1, .participants = participants};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(
          LogTerm{1}, config,
          LogPlanTermSpecification::Leader{"A", RebootId{42}}),
      participantsConfig);

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfig,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{43}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  auto r = dictateLeader(target, plan, current, health);

  // Should get an UpdateParticipantsFlagAction for one of the
  // acceptable participants that are acceptable as leaders to
  // become forced
  ASSERT_TRUE(std::holds_alternative<UpdateParticipantFlagsAction>(r))
      << to_string(r);

  auto action = std::get<UpdateParticipantFlagsAction>(r);
  auto acceptableParticipants =
      getParticipantsAcceptableAsLeaders("A", participants);

  ASSERT_NE(std::find(std::begin(acceptableParticipants),
                      std::end(acceptableParticipants), action._participant),
            std::end(acceptableParticipants));

  ASSERT_TRUE(action._flags.forced);
}

TEST_F(LogSupervisionTest, test_dictate_leader_success) {
  auto const& logId = LogId{44};
  auto const& config = LogConfig(3, 3, 3, true);

  auto const& participants = ParticipantsFlagsMap{
      {"A", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"B", ParticipantFlags{.forced = false, .allowedAsLeader = true}},
      {"C", ParticipantFlags{.forced = false, .allowedAsLeader = false}},
      {"D", ParticipantFlags{.forced = true, .allowedAsLeader = true}}};

  auto const& target = LogTarget(logId, participants, config);

  auto const& participantsConfig =
      ParticipantsConfig{.generation = 1, .participants = participants};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(
          LogTerm{1}, config,
          LogPlanTermSpecification::Leader{"A", RebootId{42}}),
      participantsConfig);

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfig,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{43}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}}}};

  auto r = dictateLeader(target, plan, current, health);

  // Should get an UpdateParticipantsFlagAction for one of the
  // acceptable participants that are acceptable as leaders to
  // become forced
  ASSERT_TRUE(std::holds_alternative<DictateLeaderAction>(r)) << to_string(r);

  auto action = std::get<DictateLeaderAction>(r);

  ASSERT_EQ(action._leader.serverId, "D");
}

TEST_F(LogSupervisionTest, test_remove_participant_action) {
  auto const& logId = LogId{44};
  auto const& config = LogConfig(3, 3, 3, true);

  // Server D is missing in target
  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}}},
                config);

  ParticipantsFlagsMap participantsFlags{{"A", ParticipantFlags{}},
                                         {"B", ParticipantFlags{}},
                                         {"C", ParticipantFlags{}},
                                         {"D", ParticipantFlags{}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 1, .participants = participantsFlags};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(
          LogTerm{1}, config,
          LogPlanTermSpecification::Leader{"A", RebootId{42}}),
      participantsConfig);

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfig,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};

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

  auto r = checkReplicatedLog(target, plan, current, health);
  // We expect a UpdateParticipantsFlagsAction to unset the allowedInQuorum flag
  // for d
  ASSERT_TRUE(std::holds_alternative<UpdateParticipantFlagsAction>(r))
      << to_string(r);

  auto action = std::get<UpdateParticipantFlagsAction>(r);
  ASSERT_EQ(action._participant, "D");
  ASSERT_EQ(action._flags, (ParticipantFlags{.forced = false,
                                             .allowedInQuorum = false,
                                             .allowedAsLeader = true}));
}

TEST_F(LogSupervisionTest, test_remove_participant_action_wait_for_committed) {
  auto const& logId = LogId{44};
  auto const& config = LogConfig(3, 3, 3, true);

  // Server D is missing in target and has set the allowedInQuorum flag to false
  // but the config is not yet committed
  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}}},
                config);

  ParticipantsFlagsMap participantsFlags{
      {"A", ParticipantFlags{}},
      {"B", ParticipantFlags{}},
      {"C", ParticipantFlags{}},
      {"D", ParticipantFlags{.allowedInQuorum = false}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 2, .participants = participantsFlags};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(
          LogTerm{1}, config,
          LogPlanTermSpecification::Leader{"A", RebootId{42}}),
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

  auto r = checkReplicatedLog(target, plan, current, health);
  // We expect an EmptyAction
  ASSERT_TRUE(std::holds_alternative<EmptyAction>(r)) << to_string(r);
}

TEST_F(LogSupervisionTest, test_remove_participant_action_committed) {
  auto const& logId = LogId{44};
  auto const& config = LogConfig(3, 3, 3, true);

  // Server D is missing in target and has set the allowedInQuorum flag to false
  // but the config is not yet committed
  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}}},
                config);

  ParticipantsFlagsMap participantsFlags{
      {"A", ParticipantFlags{}},
      {"B", ParticipantFlags{}},
      {"C", ParticipantFlags{}},
      {"D", ParticipantFlags{.allowedInQuorum = false}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 2, .participants = participantsFlags};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(
          LogTerm{1}, config,
          LogPlanTermSpecification::Leader{"A", RebootId{42}}),
      participantsConfig);

  auto current = LogCurrent();
  current.leader =
      LogCurrent::Leader{.serverId = "A",
                         .term = LogTerm{1},
                         .committedParticipantsConfig = participantsConfig,
                         .leadershipEstablished = true,
                         .commitStatus = std::nullopt};

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

  auto r = checkReplicatedLog(target, plan, current, health);
  // We expect an RemoveParticipantFromPlanAction to finally remove D
  ASSERT_TRUE(std::holds_alternative<RemoveParticipantFromPlanAction>(r))
      << to_string(r);

  auto action = std::get<RemoveParticipantFromPlanAction>(r);
  ASSERT_EQ(action._participant, "D");
}

TEST_F(LogSupervisionTest, test_write_empty_term) {
  auto const& logId = LogId{44};
  auto const& config = LogConfig(3, 3, 3, true);

  auto const& target =
      LogTarget(logId,
                ParticipantsFlagsMap{{"A", ParticipantFlags{}},
                                     {"B", ParticipantFlags{}},
                                     {"C", ParticipantFlags{}},
                                     {"D", ParticipantFlags{}}},
                config);

  ParticipantsFlagsMap participantsFlags{
      {"A", ParticipantFlags{}},
      {"B", ParticipantFlags{}},
      {"C", ParticipantFlags{}},
      {"D", ParticipantFlags{.allowedInQuorum = false}}};
  auto const& participantsConfig =
      ParticipantsConfig{.generation = 2, .participants = participantsFlags};

  auto const& plan = LogPlanSpecification(
      logId,
      LogPlanTermSpecification(
          LogTerm{2}, config,
          LogPlanTermSpecification::Leader{"A", RebootId{42}}),
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
      {"A", LogCurrentLocalState(LogTerm(2),
                                 TermIndexPair(LogTerm(1), LogIndex(44)))},
      {"B", LogCurrentLocalState(LogTerm(2),
                                 TermIndexPair(LogTerm(1), LogIndex(44)))},
      {"C", LogCurrentLocalState(LogTerm(2),
                                 TermIndexPair(LogTerm(3), LogIndex(44)))},
      {"D", LogCurrentLocalState(LogTerm(2),
                                 TermIndexPair(LogTerm(1), LogIndex(44)))}};

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

  auto r = checkReplicatedLog(target, plan, current, health);

  // Since the leader is `A` and the rebootId in health is higher than the one
  // in plan, we need to write an empty term
  ASSERT_TRUE(std::holds_alternative<WriteEmptyTermAction>(r)) << to_string(r);

  auto writeEmptyTermAction = std::get<WriteEmptyTermAction>(r);

  ASSERT_EQ(writeEmptyTermAction.minTerm, LogTerm{3});
}
