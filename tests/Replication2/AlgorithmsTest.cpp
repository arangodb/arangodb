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
    return {leader, rebootId};
  }

  auto makeTermSpecification(LogTerm term, agency::LogPlanConfig const& config,
                             ParticipantInfo const& info)
      -> agency::LogPlanTermSpecification {
    auto termSpec = agency::LogPlanTermSpecification{};
    termSpec.term = term;
    termSpec.config = config;
    std::transform(info.begin(), info.end(),
                   std::inserter(termSpec.participants, termSpec.participants.end()),
                   [](auto const& info) {
                     return std::make_pair(info.first,
                                           agency::LogPlanTermSpecification::Participant{});
                   });
    return termSpec;
  }

  auto makeLogCurrent() -> agency::LogCurrent {
    auto current = agency::LogCurrent{};
    return current;
  }

  auto makeLogCurrentReportAll(ParticipantInfo const& info, LogTerm term, LogIndex spearhead, LogTerm spearheadTerm) {
    auto current = agency::LogCurrent{};
    std::transform(info.begin(), info.end(), std::inserter(current.localState, current.localState.end()), [&](auto const& info) {
      auto state = agency::LogCurrentLocalState{};
      state.term = term;
      state.spearhead = replicated_log::TermIndexPair{spearheadTerm, spearhead};
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
  spec.currentTerm =
      makeTermSpecification(LogTerm{1}, {}, participants);
  spec.currentTerm->leader = makeLeader("A", RebootId{1});
  auto current = makeLogCurrent();

  auto result = checkReplicatedLog("db", spec, current, participants);
  ASSERT_EQ(result, std::nullopt);
}

TEST_F(CheckLogsAlgorithmTest, check_do_nothing_if_follower_fails) {

  auto const participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, true}},
      {"B", ParticipantRecord{RebootId{2}, false}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm =
      makeTermSpecification(LogTerm{1}, {}, participants);
  spec.currentTerm->leader = makeLeader("A", RebootId{1});
  auto current = makeLogCurrent();

  auto result = checkReplicatedLog("db", spec, current, participants);
  ASSERT_EQ(result, std::nullopt);
}

TEST_F(CheckLogsAlgorithmTest, check_do_increase_term_if_leader_reboots) {

  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{2}, false}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm =
      makeTermSpecification(LogTerm{1}, {}, participants);
  spec.currentTerm->leader = makeLeader("A", RebootId{1});
  auto current = makeLogCurrent();

  auto result = checkReplicatedLog("db", spec, current, participants);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->leader, std::nullopt);
  EXPECT_EQ(result->term, LogTerm{2});
  EXPECT_EQ(result->config, spec.currentTerm->config);
}

TEST_F(CheckLogsAlgorithmTest, check_elect_leader_if_all_available) {

  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, true}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{1}, {}, participants);
  auto current = makeLogCurrentReportAll(participants, LogTerm{1}, LogIndex{4}, LogTerm{1});

  auto result = checkReplicatedLog("db", spec, current, participants);
  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->leader, std::nullopt);
  ASSERT_TRUE(participants.find(result->leader->serverId) != participants.end());
  EXPECT_EQ(participants.at(result->leader->serverId).rebootId, result->leader->rebootId);
  EXPECT_EQ(result->term, LogTerm{2});
  EXPECT_EQ(result->config, spec.currentTerm->config);
}

TEST_F(CheckLogsAlgorithmTest, do_nothing_if_non_healthy) {

  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, false}},
      {"B", ParticipantRecord{RebootId{1}, false}},
      {"C", ParticipantRecord{RebootId{1}, false}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{1}, {}, participants);
  auto current = makeLogCurrentReportAll(participants, LogTerm{1}, LogIndex{4}, LogTerm{1});

  auto result = checkReplicatedLog("db", spec, current, participants);
  ASSERT_FALSE(result.has_value());
}

TEST_F(CheckLogsAlgorithmTest, check_elect_leader_non_reported) {

  auto participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, true}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{2}, {}, participants);
  auto current = makeLogCurrentReportAll(participants, LogTerm{1}, LogIndex{4}, LogTerm{1});

  auto result = checkReplicatedLog("db", spec, current, participants);
  ASSERT_FALSE(result.has_value());
}
