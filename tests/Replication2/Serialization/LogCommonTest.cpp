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
#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::basics;
using namespace arangodb::tests;

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
  EXPECT_ANY_THROW({CommitFailReason::fromVelocyPack(jsonSlice);});
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
}
