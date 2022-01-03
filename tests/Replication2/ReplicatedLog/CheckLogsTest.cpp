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

#include "Replication2/ReplicatedLog/Algorithms.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::algorithms;

struct CheckLogsAlgorithmTest : ::testing::Test {
  auto makePlanSpecification(LogId id) -> agency::LogPlanSpecification {
    auto spec = agency::LogPlanSpecification{};
    spec.id = id;
    spec.targetConfig.writeConcern = 1;
    spec.targetConfig.waitForSync = false;
    return spec;
  }

  auto makeLeader(ParticipantId leader, RebootId rebootId)
      -> agency::LogPlanTermSpecification::Leader {
    return {std::move(leader), rebootId};
  }

  auto makeTermSpecification(LogTerm term, LogConfig const& config,
                             ParticipantInfo const& info)
      -> agency::LogPlanTermSpecification {
    auto termSpec = agency::LogPlanTermSpecification{};
    termSpec.term = term;
    termSpec.config = config;
    std::transform(
        info.begin(), info.end(),
        std::inserter(termSpec.participants, termSpec.participants.end()),
        [](auto const& info) {
          return std::make_pair(
              info.first, agency::LogPlanTermSpecification::Participant{});
        });
    return termSpec;
  }

  auto makeLogCurrent() -> agency::LogCurrent {
    auto current = agency::LogCurrent{};
    return current;
  }

  auto makeLogCurrentReportAll(ParticipantInfo const& info, LogTerm term,
                               LogIndex spearhead, LogTerm spearheadTerm) {
    auto current = agency::LogCurrent{};
    std::transform(info.begin(), info.end(),
                   std::inserter(current.localState, current.localState.end()),
                   [&](auto const& info) {
                     auto state = agency::LogCurrentLocalState{
                         term, TermIndexPair{spearheadTerm, spearhead}};
                     return std::make_pair(info.first, state);
                   });
    return current;
  }
};

TEST_F(CheckLogsAlgorithmTest, check_do_nothing_if_all_good) {
  auto const participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, true}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{1}, {}, participants);
  spec.currentTerm->leader = makeLeader("A", RebootId{1});
  auto current = makeLogCurrent();

  auto v = checkReplicatedLog("db", spec, current, participants);
  ASSERT_TRUE(std::holds_alternative<std::monostate>(v));
}

TEST_F(CheckLogsAlgorithmTest, check_do_nothing_if_follower_fails) {
  auto const participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, true}},
      {"B", ParticipantRecord{RebootId{2}, false}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{1}, {}, participants);
  spec.currentTerm->leader = makeLeader("A", RebootId{1});
  auto current = makeLogCurrent();

  auto v = checkReplicatedLog("db", spec, current, participants);
  ASSERT_TRUE(std::holds_alternative<std::monostate>(v));
}

TEST_F(CheckLogsAlgorithmTest, check_do_increase_term_if_leader_reboots) {
  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{2}, false}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{1}, {}, participants);
  spec.currentTerm->leader = makeLeader("A", RebootId{1});
  auto current = makeLogCurrent();

  auto v = checkReplicatedLog("db", spec, current, participants);
  ASSERT_TRUE(std::holds_alternative<agency::LogPlanTermSpecification>(v));
  auto result = std::get<agency::LogPlanTermSpecification>(v);
  EXPECT_EQ(result.leader, std::nullopt);
  EXPECT_EQ(result.term, LogTerm{2});
  EXPECT_EQ(result.config, spec.currentTerm->config);
}

TEST_F(CheckLogsAlgorithmTest, check_elect_leader_if_all_available) {
  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, true}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{1}, {}, participants);
  auto current = makeLogCurrentReportAll(participants, LogTerm{1}, LogIndex{4},
                                         LogTerm{1});

  auto v = checkReplicatedLog("db", spec, current, participants);
  ASSERT_TRUE(std::holds_alternative<agency::LogPlanTermSpecification>(v));
  auto result = std::get<agency::LogPlanTermSpecification>(v);
  EXPECT_NE(result.leader, std::nullopt);
  ASSERT_TRUE(participants.find(result.leader->serverId) != participants.end());
  EXPECT_EQ(participants.at(result.leader->serverId).rebootId,
            result.leader->rebootId);
  EXPECT_TRUE(participants.at(result.leader->serverId).isHealthy);
  EXPECT_EQ(result.term, LogTerm{2});
  EXPECT_EQ(result.config, spec.currentTerm->config);
}

TEST_F(CheckLogsAlgorithmTest, do_nothing_if_non_healthy) {
  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, false}},
      {"B", ParticipantRecord{RebootId{1}, false}},
      {"C", ParticipantRecord{RebootId{1}, false}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{1}, {}, participants);
  auto current = makeLogCurrentReportAll(participants, LogTerm{1}, LogIndex{4},
                                         LogTerm{1});

  auto v = checkReplicatedLog("db", spec, current, participants);
  ASSERT_TRUE(std::holds_alternative<agency::LogCurrentSupervisionElection>(v));
}

TEST_F(CheckLogsAlgorithmTest, check_elect_leader_non_reported) {
  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, true}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{2}, {}, participants);
  auto current = makeLogCurrentReportAll(participants, LogTerm{1}, LogIndex{4},
                                         LogTerm{1});

  auto v = checkReplicatedLog("db", spec, current, participants);
  ASSERT_TRUE(std::holds_alternative<agency::LogCurrentSupervisionElection>(v));
  auto& e = std::get<agency::LogCurrentSupervisionElection>(v);
  EXPECT_EQ(e.term, spec.currentTerm->term);
  EXPECT_EQ(e.participantsRequired, 3);
  EXPECT_EQ(e.participantsAvailable, 0);
  EXPECT_EQ(
      e.detail.at("A"),
      agency::LogCurrentSupervisionElection::ErrorCode::TERM_NOT_CONFIRMED);
  EXPECT_EQ(
      e.detail.at("B"),
      agency::LogCurrentSupervisionElection::ErrorCode::TERM_NOT_CONFIRMED);
  EXPECT_EQ(
      e.detail.at("C"),
      agency::LogCurrentSupervisionElection::ErrorCode::TERM_NOT_CONFIRMED);
}

TEST_F(CheckLogsAlgorithmTest, check_elect_leader_two_reported_wc_2) {
  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, false}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.targetConfig.writeConcern = 2;
  spec.currentTerm = makeTermSpecification(LogTerm{2}, {}, participants);
  spec.currentTerm->config.writeConcern = 2;
  auto current = makeLogCurrentReportAll(participants, LogTerm{2}, LogIndex{4},
                                         LogTerm{1});

  auto v = checkReplicatedLog("db", spec, current, participants);
  ASSERT_TRUE(std::holds_alternative<agency::LogPlanTermSpecification>(v));
  auto result = std::get<agency::LogPlanTermSpecification>(v);
  ASSERT_TRUE(participants.find(result.leader->serverId) != participants.end());
  EXPECT_EQ(participants.at(result.leader->serverId).rebootId,
            result.leader->rebootId);
  EXPECT_TRUE(participants.at(result.leader->serverId).isHealthy);
  EXPECT_EQ(result.term, LogTerm{3});
  EXPECT_EQ(result.config, spec.currentTerm->config);
}

TEST_F(CheckLogsAlgorithmTest, check_dont_elect_leader_two_reported_wc_2) {
  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, false}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.targetConfig.writeConcern = 2;
  spec.currentTerm = makeTermSpecification(LogTerm{2}, {}, participants);
  spec.currentTerm->config.writeConcern = 2;
  auto current = makeLogCurrent();
  current.localState["A"] = agency::LogCurrentLocalState{
      LogTerm{2}, TermIndexPair{LogTerm{1}, LogIndex{1}}};
  current.localState["B"] = agency::LogCurrentLocalState{
      LogTerm{1}, TermIndexPair{LogTerm{1}, LogIndex{1}}};
  current.localState["C"] = agency::LogCurrentLocalState{
      LogTerm{2}, TermIndexPair{LogTerm{1}, LogIndex{1}}};
  // only C is available, because it is healthy and it has confirmed term 2

  auto v = checkReplicatedLog("db", spec, current, participants);
  auto& e = std::get<agency::LogCurrentSupervisionElection>(v);
  EXPECT_EQ(e.term, spec.currentTerm->term);
  EXPECT_EQ(e.participantsRequired, 2);
  EXPECT_EQ(e.participantsAvailable, 1);
  EXPECT_EQ(e.detail.at("A"),
            agency::LogCurrentSupervisionElection::ErrorCode::SERVER_NOT_GOOD);
  EXPECT_EQ(
      e.detail.at("B"),
      agency::LogCurrentSupervisionElection::ErrorCode::TERM_NOT_CONFIRMED);
  EXPECT_EQ(e.detail.at("C"),
            agency::LogCurrentSupervisionElection::ErrorCode::OK);
}

TEST_F(CheckLogsAlgorithmTest, check_constitute_first_term) {
  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, false}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.targetConfig.writeConcern = 2;
  spec.targetConfig.replicationFactor = 2;
  auto current = makeLogCurrent();

  auto v = checkReplicatedLog("db", spec, current, participants);
  auto& e = std::get<agency::LogPlanTermSpecification>(v);
  EXPECT_EQ(e.term, LogTerm{1});
  EXPECT_EQ(e.config, spec.targetConfig);
  EXPECT_EQ(e.participants.size(), 2);
  EXPECT_TRUE(e.participants.find("B") != e.participants.end());
  EXPECT_TRUE(e.participants.find("C") != e.participants.end());
}

TEST_F(CheckLogsAlgorithmTest, check_constitute_first_term_r3_wc2) {
  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, true}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.targetConfig.writeConcern = 2;
  spec.targetConfig.replicationFactor = 3;
  auto current = makeLogCurrent();

  auto v = checkReplicatedLog("db", spec, current, participants);
  auto& e = std::get<agency::LogPlanTermSpecification>(v);
  EXPECT_EQ(e.term, LogTerm{1});
  EXPECT_EQ(e.config, spec.targetConfig);
  EXPECT_EQ(e.participants.size(), 3);
  EXPECT_TRUE(e.participants.find("B") != e.participants.end());
  EXPECT_TRUE(e.participants.find("C") != e.participants.end());
  EXPECT_TRUE(e.participants.find("A") != e.participants.end());
}

TEST_F(CheckLogsAlgorithmTest,
       check_constitute_first_term_not_enough_participants) {
  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, false}},
      {"B", ParticipantRecord{RebootId{1}, false}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.targetConfig.writeConcern = 2;
  spec.targetConfig.replicationFactor = 2;
  auto current = makeLogCurrent();

  auto v = checkReplicatedLog("db", spec, current, participants);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(v));
}
