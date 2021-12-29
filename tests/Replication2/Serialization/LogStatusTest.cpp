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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Basics/VelocyPackHelper.h"
#include "velocypack/Builder.h"

#include <gtest/gtest.h>

#include "Aql/VelocyPackHelper.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/types.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::basics;
using namespace arangodb::tests;
using namespace std::literals::chrono_literals;

TEST(LogStatusTest, log_statistics) {
  LogStatistics statistics;
  statistics.spearHead = TermIndexPair{LogTerm{2}, LogIndex{1}};
  statistics.commitIndex = LogIndex{1};
  statistics.firstIndex = LogIndex{1};
  VPackBuilder builder;
  statistics.toVelocyPack(builder);
  auto slice = builder.slice();
  auto fromVPack = LogStatistics::fromVelocyPack(slice);
  EXPECT_EQ(statistics, fromVPack);

  auto jsonBuffer = R"({
    "commitIndex": 1,
    "firstIndex": 1,
    "spearhead": {
      "term": 2,
      "index": 1
    }
  })"_vpack;
  auto jsonSlice = velocypack::Slice(jsonBuffer->data());
  EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
      << "expected " << jsonSlice.toJson() << " found " << slice.toJson();
}

TEST(LogStatusTest, commit_fail_reason) {
  VPackBuilder builder;
  auto reason = CommitFailReason::withNothingToCommit();
  reason.toVelocyPack(builder);
  auto slice = builder.slice();
  auto fromVPack = CommitFailReason::fromVelocyPack(slice);
  EXPECT_EQ(reason, fromVPack);

  auto jsonBuffer = R"({
    "reason": "NothingToCommit"
  })"_vpack;
  auto jsonSlice = velocypack::Slice(jsonBuffer->data());
  EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
      << "expected " << jsonSlice.toJson() << " found " << slice.toJson();

  builder.clear();
  reason = CommitFailReason::withQuorumSizeNotReached("PRMR-1234");
  reason.toVelocyPack(builder);
  slice = builder.slice();
  fromVPack = CommitFailReason::fromVelocyPack(slice);
  EXPECT_EQ(reason, fromVPack);

  builder.clear();
  reason = CommitFailReason::withForcedParticipantNotInQuorum("PRMR-1234");
  reason.toVelocyPack(builder);
  slice = builder.slice();
  fromVPack = CommitFailReason::fromVelocyPack(slice);
  EXPECT_EQ(reason, fromVPack);

  jsonBuffer = R"({"xyz": "NothingToCommit", "reason": "xyz"})"_vpack;
  jsonSlice = velocypack::Slice(jsonBuffer->data());
  EXPECT_ANY_THROW({ CommitFailReason::fromVelocyPack(jsonSlice); });
}

TEST(LogStatusTest, append_entries_error_reason) {
  // Default, no error
  {
    AppendEntriesErrorReason reason;
    VPackBuilder builder;
    reason.toVelocyPack(builder);
    auto slice = builder.slice();
    auto fromVPack = AppendEntriesErrorReason::fromVelocyPack(slice);
    EXPECT_EQ(reason, fromVPack);
  }

  // Error and details
  {
    auto jsonBuffer = R"({
      "error": "MessageOutdated",
      "errorMessage": "Message is outdated",
      "details": "foo bar"
    })"_vpack;
    auto jsonSlice = velocypack::Slice(jsonBuffer->data());
    auto reason = AppendEntriesErrorReason{
        AppendEntriesErrorReason::ErrorType::kMessageOutdated, "foo bar"};
    VPackBuilder builder;
    reason.toVelocyPack(builder);
    auto slice = builder.slice();
    EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
        << "expected " << jsonSlice.toJson() << " found " << slice.toJson();
    EXPECT_EQ(reason, AppendEntriesErrorReason::fromVelocyPack(jsonSlice));
  }
}

TEST(LogStatusTest, follower_statistics_exceptions) {
  // Missing commitIndex
  EXPECT_ANY_THROW({
    FollowerStatistics::fromVelocyPack(velocypack::Slice(R"({
      "missing_commitIndex": 4,
      "spearhead": {
        "term": 2,
        "index": 4
      },
      "lastErrorReason": {"error": "None"},
      "lastRequestLatencyMS": 0.012983,
      "state": {
        "state": "up-to-date"
      }
      })"_vpack->data()));
  });

  // Wrong type for commitIndex
  EXPECT_ANY_THROW({
    FollowerStatistics::fromVelocyPack(velocypack::Slice(R"({
      "commitIndex": "4",
      "spearhead": {
        "term": 2,
        "index": 4
      },
      "lastErrorReason": {"error": "None"},
      "lastRequestLatencyMS": 0.012983,
      "state": {
        "state": "up-to-date"
      }
      })"_vpack->data()));
  });
}

TEST(LogStatusTest, leader_status) {
  LeaderStatus leaderStatus;
  LogStatistics statistics;
  statistics.spearHead = TermIndexPair{LogTerm{2}, LogIndex{1}};
  statistics.commitIndex = LogIndex{1};
  statistics.firstIndex = LogIndex{1};
  leaderStatus.local = statistics;
  leaderStatus.term = LogTerm{2};
  leaderStatus.largestCommonIndex = LogIndex{1};
  leaderStatus.activeParticipantConfig.generation = 14;
  leaderStatus.committedParticipantConfig.generation = 18;
  std::unordered_map<ParticipantId, FollowerStatistics> follower(
      {{"PRMR-45c56239-6a83-4ab0-961e-9adea5078286",
        FollowerStatistics::fromVelocyPack(velocypack::Slice(R"({
        "commitIndex": 4,
        "spearhead": {"term": 2, "index": 4},
        "lastErrorReason": {"error": "None"},
        "lastRequestLatencyMS": 0.012983,
        "state": {
          "state": "up-to-date"
        }
        })"_vpack->data()))},
       {"PRMR-13608015-4a2c-46aa-985f-73b6b8a73568",
        FollowerStatistics::fromVelocyPack(velocypack::Slice(R"({
          "commitIndex": 3,
          "spearhead": {"term": 2, "index": 3},
          "lastErrorReason": {"error": "CommunicationError", "details": "foo"},
          "lastRequestLatencyMS": 11159.799272,
          "state": {
            "state": "request-in-flight",
            "durationMS": 4143.651874
          }
        })"_vpack->data()))}});
  leaderStatus.follower = std::move(follower);
  leaderStatus.lastCommitStatus = CommitFailReason::withNothingToCommit();
  leaderStatus.commitLagMS = 0.014453ms;
  VPackBuilder builder;
  leaderStatus.toVelocyPack(builder);
  auto slice = builder.slice();
  auto fromVPack = LeaderStatus::fromVelocyPack(slice);
  EXPECT_EQ(leaderStatus, fromVPack);
}

TEST(LogStatusTest, follower_status) {
  auto jsonBuffer = R"({
    "role": "follower",
    "leader": "PRMR-d2a1b29e-ff75-412e-8b97-f3bfbf464fab",
    "term": 2,
    "largestCommonIndex": 3,
    "local": {
      "commitIndex": 4,
      "firstIndex": 1,
      "spearhead": {
        "term": 2,
        "index": 4
      }
    }
  })"_vpack;
  auto followerSlice = velocypack::Slice(jsonBuffer->data());
  auto followerStatus = FollowerStatus::fromVelocyPack(followerSlice);
  EXPECT_TRUE(followerStatus.leader.has_value());

  VPackBuilder builder;
  followerStatus.toVelocyPack(builder);
  auto builderSlice = builder.slice();
  EXPECT_TRUE(VelocyPackHelper::equal(followerSlice, builderSlice, true))
      << "expected " << followerSlice.toJson() << " found "
      << builderSlice.toJson();

  builder.clear();
  followerStatus.leader = std::nullopt;
  followerStatus.toVelocyPack(builder);
  builderSlice = builder.slice();
  jsonBuffer = R"({
    "role": "follower",
    "term": 2,
    "largestCommonIndex": 3,
    "local": {
      "commitIndex": 4,
      "firstIndex": 1,
      "spearhead": {
        "term": 2,
        "index": 4
      }
    }
  })"_vpack;
  followerSlice = velocypack::Slice(jsonBuffer->data());
  auto followerStatusNoLeader = FollowerStatus::fromVelocyPack(followerSlice);
  EXPECT_FALSE(followerStatusNoLeader.leader.has_value());
  EXPECT_TRUE(VelocyPackHelper::equal(followerSlice, builderSlice, true))
      << "expected " << followerSlice.toJson() << " found "
      << builderSlice.toJson();
}

TEST(LogStatusTest, global_status) {
  auto election = agency::LogCurrentSupervisionElection{};
  election.term = LogTerm{1};
  election.participantsRequired = 2;
  election.participantsAvailable = 0;

  auto supervision = agency::LogCurrentSupervision{};
  supervision.election = std::move(election);

  auto status = GlobalStatus{supervision, LogStatus{UnconfiguredStatus{}}};

  VPackBuilder builder;
  status.toVelocyPack(builder);
  auto slice = builder.slice();

  auto jsonBuffer = R"({
    "supervision": {
      "election": {
        "term": 1,
        "participantsRequired": 2,
        "participantsAvailable": 0,
        "details": {}
      }
    },
    "logStatus": {
      "role": "unconfigured"
    }
  })"_vpack;
  auto statusSlice = velocypack::Slice(jsonBuffer->data());
  EXPECT_TRUE(VelocyPackHelper::equal(slice, statusSlice, true))
      << "expected " << slice.toJson() << " found " << statusSlice.toJson();

  builder.clear();
  status.logStatus = std::nullopt;
  status.toVelocyPack(builder);
  status = GlobalStatus::fromVelocyPack(builder.slice());
  EXPECT_EQ(status.logStatus, std::nullopt);
}
