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

#include "Replication2/ReplicatedState/LeaderStateMachine.h"

using namespace arangodb::replication2::replicated_state;

struct LeaderStateMachineTest : ::testing::Test {};

TEST_F(LeaderStateMachineTest, test_log_no_leader) {
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
                      Log::Plan::Participants{.generation = 1, .set = {}},
              },
          .current = Log::Current{}};

  auto health = ParticipantsHealth{
      ._health = {{"A", ParticipantHealth{.rebootId = 0, .isHealthy = true}},
                  {"B", ParticipantHealth{.rebootId = 0, .isHealthy = true}},
                  {"C", ParticipantHealth{.rebootId = 0, .isHealthy = true}}}};

  auto r = replicatedLogAction(log, health);
  EXPECT_EQ(r, nullptr);
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
