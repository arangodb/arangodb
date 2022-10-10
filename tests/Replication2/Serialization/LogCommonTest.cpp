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
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;
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

TEST(LogCommonTest, participants_config) {
  auto participantsConfig = ParticipantsConfig{
      .generation = 15, .participants = {{"A", ParticipantFlags{}}}};

  VPackBuilder builder;
  velocypack::serialize(builder, participantsConfig);
  auto slice = builder.slice();
  auto fromVPack = velocypack::deserialize<ParticipantsConfig>(slice);
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
      "config": {
        "effectiveWriteConcern":1,
        "waitForSync":false
      },
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
