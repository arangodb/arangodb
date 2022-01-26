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

#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/Supervision.h"

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
      {"A", ParticipantHealth{.rebootId = RebootId{0}, .isHealthy = true}},
      {"B", ParticipantHealth{.rebootId = RebootId{0}, .isHealthy = true}},
      {"C", ParticipantHealth{.rebootId = RebootId{0}, .isHealthy = true}}}};

  auto config = ParticipantsConfig{
      .generation = 0,
      .participants = {
          {"A", ParticipantFlags{.forced = false, .excluded = false}},
          {"B", ParticipantFlags{.forced = false, .excluded = false}},
          {"C", ParticipantFlags{.forced = false, .excluded = false}}}};

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
      {"A", ParticipantHealth{.rebootId = RebootId{0}, .isHealthy = false}},
      {"B", ParticipantHealth{.rebootId = RebootId{0}, .isHealthy = false}},
      {"C", ParticipantHealth{.rebootId = RebootId{0}, .isHealthy = true}}}};

  auto config = ParticipantsConfig{
      .generation = 0,
      .participants = {
          {"A", ParticipantFlags{.forced = false, .excluded = false}},
          {"B", ParticipantFlags{.forced = false, .excluded = false}},
          {"C", ParticipantFlags{.forced = false, .excluded = false}}}};

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
              {"A", ParticipantFlags{.forced = false, .excluded = false}},
              {"B", ParticipantFlags{.forced = false, .excluded = false}},

              {"C", ParticipantFlags{.forced = false, .excluded = false}}}});

  auto health = ParticipantsHealth{
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}}}};

  auto r = tryLeadershipElection(plan, current, health);
  EXPECT_NE(r, nullptr);

  EXPECT_EQ(r->type(), Action::ActionType::LeaderElectionAction) << *r;

  auto& action = dynamic_cast<LeaderElectionAction&>(*r);
  EXPECT_EQ(action._election.outcome,
            LogCurrentSupervisionElection::Outcome::SUCCESS);

  auto possibleLeaders = std::set<ParticipantId>{"A", "B", "C"};
  EXPECT_TRUE(bool(action._newTerm));
  EXPECT_TRUE(bool(action._newTerm->leader));
  EXPECT_TRUE(possibleLeaders.contains(action._newTerm->leader->serverId));
  EXPECT_EQ(action._newTerm->leader->rebootId, RebootId{1});
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
              {"A", ParticipantFlags{.forced = false, .excluded = false}},
              {"B", ParticipantFlags{.forced = false, .excluded = false}},

              {"C", ParticipantFlags{.forced = false, .excluded = false}}}});

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{43}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}}}};

  auto r = checkLeaderHealth(plan, health);

  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->type(), Action::ActionType::UpdateTermAction);

  auto& action = dynamic_cast<UpdateTermAction&>(*r);

  // TODO: Friend op == for newTerm
  EXPECT_EQ(action._newTerm.term, LogTerm{plan.currentTerm->term.value + 1});
  EXPECT_EQ(action._newTerm.leader, std::nullopt);
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
              {"A", ParticipantFlags{.forced = false, .excluded = false}},
              {"B", ParticipantFlags{.forced = false, .excluded = false}},
              {"C", ParticipantFlags{.forced = false, .excluded = false}}}});

  auto const& health = ParticipantsHealth{
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{43}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}}}};

  auto r = tryLeadershipElection(plan, current, health);

  ASSERT_NE(r, nullptr);

  EXPECT_EQ(r->type(), Action::ActionType::LeaderElectionAction) << *r;

  auto& action = dynamic_cast<LeaderElectionAction&>(*r);
  EXPECT_TRUE(bool(action._newTerm));
  EXPECT_TRUE(bool(action._newTerm->leader));
  EXPECT_EQ(action._newTerm->leader->serverId, "C");
  EXPECT_EQ(action._newTerm->leader->rebootId, RebootId{14});
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
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}}}};

  auto r = checkLeaderHealth(plan, health);

  EXPECT_NE(r, nullptr);
  EXPECT_EQ(r->type(), Action::ActionType::EmptyAction);
}

struct SupervisionLogTest : ::testing::Test {};

TEST_F(SupervisionLogTest, test_log_created) {
  auto const config = LogConfig(3, 2, 3, true);
  auto const participants = LogTarget::Participants{
      {"A", ParticipantFlags{.forced = false, .excluded = false}},
      {"B", ParticipantFlags{.forced = false, .excluded = false}},

      {"C", ParticipantFlags{.forced = false, .excluded = false}}};

  auto r =
      checkLogAdded(Log{.target = LogTarget(LogId{44}, participants, config)},
                    ParticipantsHealth{});

  EXPECT_NE(r, nullptr);
  EXPECT_EQ(r->type(), Action::ActionType::AddLogToPlanAction) << *r;

  auto& action = dynamic_cast<AddLogToPlanAction&>(*r);
  EXPECT_EQ(
      action._spec.participantsConfig,
      (ParticipantsConfig{.generation = 1, .participants = participants}));

  // TODO check that the plan spec contains the required info
}

TEST_F(SupervisionLogTest, test_log_present) {
  auto const config = LogConfig(3, 2, 3, true);
  auto const participants = LogTarget::Participants{
      {"A", ParticipantFlags{.forced = false, .excluded = false}},
      {"B", ParticipantFlags{.forced = false, .excluded = false}},

      {"C", ParticipantFlags{.forced = false, .excluded = false}}};

  auto r =
      checkLogAdded(Log{.target = LogTarget(LogId(44), participants, config),
                        .plan = LogPlanSpecification()},
                    ParticipantsHealth());

  EXPECT_NE(r, nullptr);
  EXPECT_EQ(r->type(), Action::ActionType::EmptyAction) << *r;
}

TEST_F(SupervisionLogTest, test_checkleader_present) {
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
  current.leader = LogCurrent::Leader{};  // it doesn't matter that the leader
                                          // is empty since we only check for
                                          // the presence of a value

  auto plan = LogPlanSpecification(
      LogId{1},
      LogPlanTermSpecification(
          LogTerm{1}, config,
          LogPlanTermSpecification::Leader{"A", RebootId{1}}),
      ParticipantsConfig{
          .generation = 1,
          .participants = {
              {"A", ParticipantFlags{.forced = false, .excluded = false}},
              {"B", ParticipantFlags{.forced = false, .excluded = false}},

              {"C", ParticipantFlags{.forced = false, .excluded = false}}}});

  auto health = ParticipantsHealth{
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}}}};

  auto r = checkLeaderPresent(plan, current, health);
  EXPECT_NE(r, nullptr);
  EXPECT_EQ(r->type(), Action::ActionType::EmptyAction) << *r;
}
