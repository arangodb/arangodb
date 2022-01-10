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
#include "Replication2/ReplicatedLog/SupervisionTypes.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;
using namespace arangodb::replication2::replicated_log;

struct LeaderElectionCampaignTest : ::testing::Test {};
TEST_F(LeaderElectionCampaignTest, test_computeReason) {
  {
    auto r = computeReason(LogCurrentLocalState(LogTerm{1}, TermIndexPair{}),
                           true, LogTerm{1});
    EXPECT_EQ(r, LeaderElectionCampaign::Reason::OK);
  }

  {
    auto r = computeReason(LogCurrentLocalState(LogTerm{1}, TermIndexPair{}),
                           false, LogTerm{1});
    EXPECT_EQ(r, LeaderElectionCampaign::Reason::ServerIll);
  }

  {
    auto r = computeReason(LogCurrentLocalState(LogTerm{1}, TermIndexPair{}),
                           true, LogTerm{3});
    EXPECT_EQ(r, LeaderElectionCampaign::Reason::TermNotConfirmed);
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

  auto campaign = runElectionCampaign(localStates, health, LogTerm{1});

  EXPECT_EQ(campaign.numberOKParticipants, 3) << to_string(campaign);
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{1}, LogIndex{1}}))
      << to_string(campaign);

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

  auto campaign = runElectionCampaign(localStates, health, LogTerm{2});

  EXPECT_EQ(campaign.numberOKParticipants, 1);
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
      LogId{1},
      LogPlanTermSpecification(LogTerm{1}, config, std::nullopt,
                               {{"A", {}}, {"B", {}}, {"C", {}}}),
      config,
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

  EXPECT_EQ(r->type(), Action::ActionType::SuccessfulLeaderElectionAction)
      << *r;

  auto& action = dynamic_cast<SuccessfulLeaderElectionAction&>(*r);

  auto possibleLeaders = std::set<ParticipantId>{"A", "B", "C"};
  EXPECT_TRUE(possibleLeaders.contains(action._newLeader));
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
          LogPlanTermSpecification::Leader{"A", RebootId{42}},
          {{"A", {}}, {"B", {}}, {"C", {}}}),
      config,
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
  EXPECT_EQ(r->type(), Action::ActionType::UpdateTermAction);

  auto& action = dynamic_cast<UpdateTermAction&>(*r);

  // TODO: Friend op == for newTerm
  EXPECT_EQ(action._newTerm.term, LogTerm{plan.currentTerm->term.value + 1});
  EXPECT_EQ(action._newTerm.leader, std::nullopt);
}

#if 0
TEST_F(LeaderStateMachineTest, test_leader_intact) {
  auto const& config = LogConfig(3, 3, 3, true);
  auto log =
      Log{.current = LogCurrent{},
          .plan = LogPlanSpecification(
              LogId{1},
              LogPlanTermSpecification(
                  LogTerm{1}, config,
                  LogPlanTermSpecification::Leader{"A", RebootId{1}}, {}),
              config, {})};

  auto health = ParticipantsHealth{
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}}}};

  auto r = replicatedLogAction(log, health);

  EXPECT_EQ(r, nullptr);
}

TEST_F(LeaderStateMachineTest, test_log_no_leader) {
  // We have no leader, so we have to first run a leadership campaign and then
  // select a leader.
  auto const& config = LogConfig(3, 3, 3, true);

  auto logCurrent = LogCurrent();
  logCurrent.localState =
      std::unordered_map<ParticipantId, LogCurrentLocalState>(
          {{"A", LogCurrentLocalState(LogTerm{1},
                                      TermIndexPair{LogTerm{1}, LogIndex{1}})},
           {"B", LogCurrentLocalState(LogTerm{1},
                                      TermIndexPair{LogTerm{1}, LogIndex{1}})},
           {"C", LogCurrentLocalState(
                     LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}})}});
  logCurrent.supervision = LogCurrentSupervision{};

  auto log = Log{
      .current = logCurrent,
      .plan = LogPlanSpecification(
          LogId{1},
          LogPlanTermSpecification(LogTerm{1}, config, std::nullopt,
                                   {{"A", {}}, {"B", {}}, {"C", {}}}),
          config,
          ParticipantsConfig{
              .generation = 1,
              .participants = {
                  {"A", ParticipantFlags{.forced = false, .excluded = false}},
                  {"B", ParticipantFlags{.forced = false, .excluded = false}},

                  {"C",
                   ParticipantFlags{.forced = false, .excluded = false}}}})};

  auto health = ParticipantsHealth{
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{1}, .isHealthy = true}}}};

  auto r = replicatedLogAction(log, health);
  EXPECT_NE(r, nullptr);

  EXPECT_EQ(r->type(), Action::ActionType::SuccessfulLeaderElectionAction)
      << *r;

  auto& action = dynamic_cast<SuccessfulLeaderElectionAction&>(*r);

  auto possibleLeaders = std::set<ParticipantId>{"A", "B", "C"};
  EXPECT_TRUE(possibleLeaders.contains(action._newLeader));
}

TEST_F(LeaderStateMachineTest, test_log_with_dead_leader) {
  // Here the RebootId of the leader "A" in the Plan is 42, but
  // the health record says its RebootId is 43; this
  // means that the leader is not acceptable anymore and we
  // expect a new term that has the leader removed.
  auto const& config = LogConfig(3, 3, 3, true);

  auto logCurrent = LogCurrent();
  logCurrent.localState =
      std::unordered_map<ParticipantId, LogCurrentLocalState>(
          {{"A", LogCurrentLocalState(LogTerm{1},
                                      TermIndexPair{LogTerm{1}, LogIndex{1}})},
           {"B", LogCurrentLocalState(LogTerm{1},
                                      TermIndexPair{LogTerm{1}, LogIndex{1}})},
           {"C", LogCurrentLocalState(
                     LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}})}});
  logCurrent.supervision = LogCurrentSupervision{};

  auto log = Log{
      .current = logCurrent,
      .plan = LogPlanSpecification(
          LogId{1},
          LogPlanTermSpecification(
              LogTerm{1}, config,
              LogPlanTermSpecification::Leader{"A", RebootId{42}},
              {{"A", {}}, {"B", {}}, {"C", {}}}),
          config,
          ParticipantsConfig{
              .generation = 1,
              .participants = {
                  {"A", ParticipantFlags{.forced = false, .excluded = false}},
                  {"B", ParticipantFlags{.forced = false, .excluded = false}},

                  {"C",
                   ParticipantFlags{.forced = false, .excluded = false}}}})};

  auto health = ParticipantsHealth{
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{43}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}}}};

  auto r = replicatedLogAction(log, health);

  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->type(), Action::ActionType::UpdateTermAction);

  auto& action = dynamic_cast<UpdateTermAction&>(*r);

  // TODO: Friend op == for newTerm
  EXPECT_EQ(action._newTerm.term,
            LogTerm{log.plan.currentTerm->term.value + 1});
  EXPECT_EQ(action._newTerm.leader, std::nullopt);
}

TEST_F(LeaderStateMachineTest, test_log_establish_leader) {
  // Here we have no leader, so we expect to get a leadership election;
  // given the HealthRecord all participants are electible as leader,
  // but we will get one of them at random.
  auto const& config = LogConfig(3, 3, 3, true);

  auto logCurrent = LogCurrent();
  logCurrent.localState =
      std::unordered_map<ParticipantId, LogCurrentLocalState>(
          {{"A", LogCurrentLocalState(LogTerm{1},
                                      TermIndexPair{LogTerm{1}, LogIndex{1}})},
           {"B", LogCurrentLocalState(LogTerm{1},
                                      TermIndexPair{LogTerm{1}, LogIndex{1}})},
           {"C", LogCurrentLocalState(
                     LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{42}})}});
  logCurrent.supervision = LogCurrentSupervision{};

  auto log = Log{
      .current = logCurrent,
      .plan = LogPlanSpecification(
          LogId{1},
          LogPlanTermSpecification(LogTerm{1}, config, std::nullopt,
                                   {{"A", {}}, {"B", {}}, {"C", {}}}),
          config,
          ParticipantsConfig{
              .generation = 1,
              .participants = {
                  {"A", ParticipantFlags{.forced = false, .excluded = false}},
                  {"B", ParticipantFlags{.forced = false, .excluded = false}},

                  {"C",
                   ParticipantFlags{.forced = false, .excluded = false}}}})};

  auto health = ParticipantsHealth{
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{43}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}}}};

  auto r = replicatedLogAction(log, health);

  ASSERT_NE(r, nullptr);

  EXPECT_EQ(r->type(), Action::ActionType::SuccessfulLeaderElectionAction)
      << *r;

  auto& action = dynamic_cast<SuccessfulLeaderElectionAction&>(*r);

  EXPECT_EQ(action._newLeader, "C") << *r;
}

TEST_F(LeaderStateMachineTest, test_log_establish_leader_with_higher_term) {
  // here we have a participant "C" with a *better* TermIndexPair than the
  // others because it has a higher LogTerm, but a lower LogIndex
  // so we expect "C" to be elected leader
  auto const& config = LogConfig(3, 3, 3, true);

  auto logCurrent = LogCurrent();
  logCurrent.localState =
      std::unordered_map<ParticipantId, LogCurrentLocalState>(
          {{"A", LogCurrentLocalState(LogTerm{1},
                                      TermIndexPair{LogTerm{1}, LogIndex{15}})},
           {"B", LogCurrentLocalState(LogTerm{1},
                                      TermIndexPair{LogTerm{1}, LogIndex{27}})},
           {"C", LogCurrentLocalState(
                     LogTerm{1}, TermIndexPair{LogTerm{4}, LogIndex{42}})}});
  logCurrent.supervision = LogCurrentSupervision{};

  auto log = Log{
      .current = logCurrent,
      .plan = LogPlanSpecification(
          LogId{1},
          LogPlanTermSpecification(LogTerm{1}, config, std::nullopt,
                                   {{"A", {}}, {"B", {}}, {"C", {}}}),
          config,
          ParticipantsConfig{
              .generation = 1,
              .participants = {
                  {"A", ParticipantFlags{.forced = false, .excluded = false}},
                  {"B", ParticipantFlags{.forced = false, .excluded = false}},

                  {"C",
                   ParticipantFlags{.forced = false, .excluded = false}}}})};

  auto health = ParticipantsHealth{
      ._health = {
          {"A", ParticipantHealth{.rebootId = RebootId{43}, .isHealthy = true}},
          {"B", ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .isHealthy = true}}}};

  auto r = replicatedLogAction(log, health);

  ASSERT_NE(r, nullptr);

  EXPECT_EQ(r->type(), Action::ActionType::SuccessfulLeaderElectionAction)
      << *r;

  auto& action = dynamic_cast<SuccessfulLeaderElectionAction&>(*r);

  EXPECT_EQ(action._newLeader, "C") << *r;
}
#endif
