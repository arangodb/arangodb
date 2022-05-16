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
#include "Inspection/VPack.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::basics;
using namespace arangodb::tests;

TEST(LogCommonTest, log_id) {
  auto id = LogId{42};

  VPackBuilder builder;
  serialize(builder, id);

  auto fromVPack = deserialize<LogId>(builder.slice());

  EXPECT_EQ(id, fromVPack);
}

TEST(LogCommonTest, log_index) {
  auto index = LogIndex{1};

  VPackBuilder builder;
  serialize(builder, index);

  auto fromVPack = deserialize<LogIndex>(builder.slice());

  EXPECT_EQ(index, fromVPack);
}

TEST(LogCommonTest, term_index_pair) {
  auto spearHead = TermIndexPair{LogTerm{2}, LogIndex{1}};
  VPackBuilder builder;
  spearHead.toVelocyPack(builder);
  auto slice = builder.slice();
  auto fromVPack = TermIndexPair::fromVelocyPack(slice);
  EXPECT_EQ(spearHead, fromVPack);

  auto jsonBuffer = R"({
    "term": 2,
    "index": 1
  })"_vpack;
  auto jsonSlice = velocypack::Slice(jsonBuffer->data());
  EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
      << "expected " << jsonSlice.toJson() << " found " << slice.toJson();
}

TEST(LogCommonTest, commit_fail_reason) {
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
  reason = CommitFailReason::withQuorumSizeNotReached(
      {{"PRMR-1234",
        {.isFailed = false,
         .isAllowedInQuorum = false,
         .lastAcknowledged = TermIndexPair(LogTerm(1), LogIndex(2))}}},
      TermIndexPair(LogTerm(3), LogIndex(4)));
  reason.toVelocyPack(builder);
  slice = builder.slice();
  fromVPack = CommitFailReason::fromVelocyPack(slice);
  EXPECT_EQ(reason, fromVPack)
      << "original: " << to_string(reason) << "\n"
      << "intermediate velocypack: " << slice.toJson() << "\n"
      << "result: " << to_string(fromVPack);

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

TEST(LogCommonTest, log_config) {
  auto logConfig = LogConfig{1, 1, 1, false};
  VPackBuilder builder;
  logConfig.toVelocyPack(builder);
  auto slice = builder.slice();
  const LogConfig fromVPack(slice);
  EXPECT_EQ(logConfig, fromVPack);

  auto jsonBuffer = R"({
    "writeConcern": 1,
    "softWriteConcern": 1,
    "replicationFactor": 1,
    "waitForSync": false
  })"_vpack;
  auto jsonSlice = velocypack::Slice(jsonBuffer->data());
  EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
      << "expected " << jsonSlice.toJson() << " found " << slice.toJson();

  // Test defaulting of softWriteConcern to writeConcern
  jsonBuffer = R"({
    "writeConcern": 2,
    "replicationFactor": 3,
    "waitForSync": false
  })"_vpack;
  jsonSlice = velocypack::Slice(jsonBuffer->data());
  logConfig = LogConfig{jsonSlice};
  EXPECT_EQ(logConfig.softWriteConcern, logConfig.writeConcern);
}

TEST(LogCommonTest, log_config_inspector) {
  auto logConfig = LogConfig{1, 1, 1, false};
  VPackBuilder builder;

  serialize(builder, logConfig);
  auto slice = builder.slice();
  auto const fromVPack = deserialize<LogConfig>(slice);
  EXPECT_EQ(logConfig, fromVPack);

  auto jsonBuffer = R"({
    "writeConcern": 1,
    "softWriteConcern": 1,
    "replicationFactor": 1,
    "waitForSync": false
  })"_vpack;
  auto jsonSlice = velocypack::Slice(jsonBuffer->data());
  EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
      << "expected " << jsonSlice.toJson() << " found " << slice.toJson();

  // Test defaulting of softWriteConcern to writeConcern
  jsonBuffer = R"({
    "writeConcern": 2,
    "replicationFactor": 3,
    "waitForSync": false
  })"_vpack;
  jsonSlice = velocypack::Slice(jsonBuffer->data());
  logConfig = deserialize<LogConfig>(jsonSlice);
  EXPECT_EQ(logConfig.softWriteConcern, logConfig.writeConcern);
}

TEST(LogCommonTest, participant_flags) {
  {
    auto participantFlags = ParticipantFlags{
        .forced = true, .allowedInQuorum = false, .allowedAsLeader = true};

    VPackBuilder builder;
    participantFlags.toVelocyPack(builder);
    auto slice = builder.slice();
    auto fromVPack = ParticipantFlags::fromVelocyPack(slice);
    EXPECT_EQ(participantFlags, fromVPack);
  }

  {
    auto participantFlags = ParticipantFlags{
        .forced = true, .allowedInQuorum = false, .allowedAsLeader = true};

    VPackBuilder builder;
    serialize(builder, participantFlags);
    auto slice = builder.slice();

    auto fromVPack = deserialize<ParticipantFlags>(slice);
    EXPECT_EQ(participantFlags, fromVPack);

    auto jsonBuffer = R"({
      "allowedInQuorum": false,
      "forced": true,
      "allowedAsLeader": true
    })"_vpack;
    auto jsonSlice = velocypack::Slice(jsonBuffer->data());
    EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
        << "expected " << jsonSlice.toJson() << " found " << slice.toJson();
  }

  {
    auto const expectedFlags = ParticipantFlags{
        .forced = true, .allowedInQuorum = true, .allowedAsLeader = true};

    // if allowedInQuorum or allowedAsLeader are not given,
    // then they are as a default set to true
    auto jsonBuffer = R"({
      "forced": true
    })"_vpack;
    auto jsonSlice = velocypack::Slice(jsonBuffer->data());

    auto flags = deserialize<ParticipantFlags>(jsonSlice);

    EXPECT_EQ(expectedFlags, flags);
  }
}

TEST(LogCommonTest, participants_config) {
  auto participantsConfig = ParticipantsConfig{
      .generation = 15, .participants = {{"A", ParticipantFlags{}}}};

  VPackBuilder builder;
  participantsConfig.toVelocyPack(builder);
  auto slice = builder.slice();
  auto fromVPack = ParticipantsConfig::fromVelocyPack(slice);
  EXPECT_EQ(participantsConfig, fromVPack);
}

TEST(LogCommonTest, participants_config_inspector) {
  auto participantsConfig = ParticipantsConfig{
      .generation = 15, .participants = {{"A", ParticipantFlags{}}}};

  VPackBuilder builder;
  serialize(builder, participantsConfig);
  auto slice = builder.slice();

  auto fromVPack = deserialize<ParticipantsConfig>(slice);
  EXPECT_EQ(participantsConfig, fromVPack);

  auto jsonBuffer = R"({
      "generation": 15,
      "participants": {
        "A": {
          "forced": false,
          "allowedInQuorum": true,
          "allowedAsLeader": true
        }
      }
      })"_vpack;
  auto jsonSlice = velocypack::Slice(jsonBuffer->data());
  EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
      << "expected " << jsonSlice.toJson() << " found " << slice.toJson();
}
