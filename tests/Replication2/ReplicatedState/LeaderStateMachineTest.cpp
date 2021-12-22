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

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedState/LeaderStateMachine.h"

using namespace arangodb::replication2::replicated_state;

struct LeaderElectionCampaignTest : ::testing::Test {};
TEST_F(LeaderElectionCampaignTest, test_computeReason) {
  {
    auto r =
        computeReason(Log::Current::LocalState{.term = LogTerm{1},
                                               .spearhead = TermIndexPair{}},
                      true, LogTerm{1});
    EXPECT_EQ(r, LeaderElectionCampaign::Reason::OK);
  }

  {
    auto r =
        computeReason(Log::Current::LocalState{.term = LogTerm{1},
                                               .spearhead = TermIndexPair{}},
                      false, LogTerm{1});
    EXPECT_EQ(r, LeaderElectionCampaign::Reason::ServerIll);
  }

  {
    auto r =
        computeReason(Log::Current::LocalState{.term = LogTerm{1},
                                               .spearhead = TermIndexPair{}},
                      true, LogTerm{3});
    EXPECT_EQ(r, LeaderElectionCampaign::Reason::TermNotConfirmed);
  }
}

TEST_F(LeaderElectionCampaignTest, test_runElectionCampaign_allElectible) {
  auto localStates = Log::Current::LocalStates{
      {"A", {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}}},
      {"B", {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}}},
      {"C", {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}}}};

  auto health = ParticipantsHealth{
      ._health{{"A", ParticipantHealth{.rebootId = 0, .isHealthy = true}},
               {"B", ParticipantHealth{.rebootId = 0, .isHealthy = true}},
               {"C", ParticipantHealth{.rebootId = 0, .isHealthy = true}}}};

  auto campaign = runElectionCampaign(localStates, health, LogTerm{1});

  EXPECT_EQ(campaign.numberOKParticipants, 3);
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{1}, LogIndex{1}}));

  auto expectedElectible = std::set<ParticipantId>{"A", "B", "C"};
  auto electible = std::set<ParticipantId>{};
  std::copy(std::begin(campaign.electibleLeaderSet), std::end(campaign.electibleLeaderSet), std::inserter(electible, std::begin(electible)));
  EXPECT_EQ(electible, expectedElectible);
}

TEST_F(LeaderElectionCampaignTest, test_runElectionCampaign_oneElectible) {
  auto localStates = Log::Current::LocalStates{
      {"A", {LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}}},
      {"B", {LogTerm{2}, TermIndexPair{LogTerm{1}, LogIndex{1}}}},
      {"C", {LogTerm{2}, TermIndexPair{LogTerm{2}, LogIndex{1}}}}};

  auto health = ParticipantsHealth{
      ._health{{"A", ParticipantHealth{.rebootId = 0, .isHealthy = false}},
               {"B", ParticipantHealth{.rebootId = 0, .isHealthy = false}},
               {"C", ParticipantHealth{.rebootId = 0, .isHealthy = true}}}};

  auto campaign = runElectionCampaign(localStates, health, LogTerm{2});

  EXPECT_EQ(campaign.numberOKParticipants, 1);
  EXPECT_EQ(campaign.bestTermIndex, (TermIndexPair{LogTerm{2}, LogIndex{1}}));

  auto expectedElectible = std::set<ParticipantId>{"C"};
  auto electible = std::set<ParticipantId>{};
  std::copy(std::begin(campaign.electibleLeaderSet), std::end(campaign.electibleLeaderSet), std::inserter(electible, std::begin(electible)));
  EXPECT_EQ(electible, expectedElectible);
}


struct LeaderStateMachineTest : ::testing::Test {};

TEST_F(LeaderStateMachineTest, test_log_no_leader) {
  // We have no leader, so we have to first run a leadership campaign and then
  // select a leader.

  auto log =
      Log{.target = Log::Target{},
          .plan =
              Log::Plan{
                  .termSpec =
                      Log::Plan::TermSpecification{
                          .term = LogTerm{1},
                          .leader = std::nullopt,
                          .config =
                              Log::Plan::TermSpecification::Config{
                                  .waitForSync = true,
                                  .writeConcern = 3,
                                  .softWriteConcern = 3}},
                  .participants =
                      Log::Plan::Participants{
                          .generation = 1,
                          .set = {{"A", {.forced = false, .excluded = false}},
                                  {"B", {.forced = false, .excluded = false}},
                                  {"C", {.forced = false, .excluded = false}}}},
              },
          .current = Log::Current{
              .localStates =
                  {{"A",
                    {.term = LogTerm{1},
                     .spearhead = TermIndexPair{LogTerm{1}, LogIndex{1}}}},
                   {"B",
                    {.term = LogTerm{1},
                     .spearhead = TermIndexPair{LogTerm{1}, LogIndex{1}}}},
                   {"C",
                    {.term = LogTerm{1},
                     .spearhead = TermIndexPair{LogTerm{1}, LogIndex{1}}}}},
              .leader = Log::Current::Leader{},
              .supervision = Log::Current::Supervision{}}};

  auto health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = 1, .isHealthy = true}},
                  {"B", ParticipantHealth{.rebootId = 1, .isHealthy = true}},
                  {"C", ParticipantHealth{.rebootId = 1, .isHealthy = true}}}};

  auto r = replicatedLogAction(log, health);

  EXPECT_NE(r, nullptr);

  auto& action = dynamic_cast<SuccessfulLeaderElectionAction&>(*r);
}

TEST_F(LeaderStateMachineTest, test_log_with_dead_leader) {
  auto log = Log{
      .target = Log::Target{},
      .plan = Log::Plan{.termSpec =
                            Log::Plan::TermSpecification{
                                .term = LogTerm{1},
                                .leader =
                                    Log::Plan::TermSpecification::Leader{
                                        .serverId = "A", .rebootId = 42},
                                .config =
                                    Log::Plan::TermSpecification::Config{
                                        .waitForSync = true,
                                        .writeConcern = 3,
                                        .softWriteConcern = 3}},
                        .participants =
                            Log::Plan::Participants{.generation = 1, .set = {}}

      },
      .current = Log::Current{}};

  auto health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = 43, .isHealthy = true}},
                  {"B", ParticipantHealth{.rebootId = 14, .isHealthy = true}},
                  {"C", ParticipantHealth{.rebootId = 14, .isHealthy = true}}}};

  auto r = replicatedLogAction(log, health);

  ASSERT_NE(r, nullptr);

  auto& action = dynamic_cast<UpdateTermAction&>(*r);

  // TODO: Friend op == for newTerm
  EXPECT_EQ(action._newTerm.term, LogTerm{log.plan.termSpec.term.value + 1});
  EXPECT_EQ(action._newTerm.leader, std::nullopt);
}

TEST_F(LeaderStateMachineTest, test_log_establish_leader) {
  auto log = Log{
      .target = Log::Target{},
      .plan = Log::Plan{.termSpec =
                            Log::Plan::TermSpecification{
                                .term = LogTerm{1},
                                .leader = std::nullopt,
                                .config =
                                    Log::Plan::TermSpecification::Config{
                                        .waitForSync = true,
                                        .writeConcern = 3,
                                        .softWriteConcern = 3}},
                        .participants = Log::Plan::Participants{.generation = 1,
                                                                .set = {}}},
      .current = Log::Current{}};

  auto health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = 43, .isHealthy = true}},
                  {"B", ParticipantHealth{.rebootId = 14, .isHealthy = true}},
                  {"C", ParticipantHealth{.rebootId = 14, .isHealthy = true}}}};

  auto r = replicatedLogAction(log, health);

  ASSERT_NE(r, nullptr);

  auto& action = dynamic_cast<SuccessfulLeaderElectionAction&>(*r);

  // TODO: Test
  EXPECT_EQ(action._newLeader, "A");
}
