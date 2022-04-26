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
#include "Cluster/ClusterTypes.h"
#include "Inspection/VPack.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;
using namespace arangodb::basics;
using namespace arangodb::tests;

TEST(AgencyLogSpecificationTest, log_plan_term_specification) {
  auto id = LogId{1234};
  auto spec = LogPlanSpecification{
      id,
      LogPlanTermSpecification{
          LogTerm{1}, LogConfig{1, 1, 1, false},
          LogPlanTermSpecification::Leader{"leaderId", RebootId{100}}},
      ParticipantsConfig{15, {{"p1", {true, false}}, {"p2", {}}}}};

  VPackBuilder builder;
  spec.toVelocyPack(builder);
  auto slice = builder.slice();
  auto const fromVPack = LogPlanSpecification::fromVelocyPack(slice);
  EXPECT_EQ(spec, fromVPack);

  auto jsonBuffer = R"({
    "id": 1234,
    "currentTerm": {
      "term": 1,
      "config": {
        "writeConcern": 1,
        "softWriteConcern": 1,
        "replicationFactor": 1,
        "waitForSync": false
      },
      "leader": {
        "serverId": "leaderId",
        "rebootId": 100
      }
    },
    "participantsConfig": {
      "generation": 15,
      "participants": {
        "p1": {
          "forced": true,
          "allowedInQuorum": false,
          "allowedAsLeader": true
        },
        "p2": {
          "forced": false,
          "allowedInQuorum": true,
          "allowedAsLeader": true
        }
      }
    }
  })"_vpack;

  auto jsonSlice = velocypack::Slice(jsonBuffer->data());
  EXPECT_TRUE(VelocyPackHelper::equal(jsonSlice, slice, true))
      << "expected " << jsonSlice.toJson() << " found " << slice.toJson();

  jsonBuffer = R"({
    "id": 1234,
    "currentTerm": {
      "term": 1,
      "config": {
        "writeConcern": 1,
        "softWriteConcern": 1,
        "replicationFactor": 1,
        "waitForSync": false
      }
    },
    "participantsConfig": {
      "generation": 15,
      "participants": {}
    }
  })"_vpack;

  jsonSlice = velocypack::Slice(jsonBuffer->data());
  spec = LogPlanSpecification::fromVelocyPack(jsonSlice);
  EXPECT_EQ(spec.currentTerm->leader, std::nullopt);

  jsonBuffer = R"({
    "id": 1234,
    "participantsConfig": {
      "generation": 15,
      "participants": {}
    }
  })"_vpack;

  jsonSlice = velocypack::Slice(jsonBuffer->data());
  spec = LogPlanSpecification::fromVelocyPack(jsonSlice);
  EXPECT_EQ(spec.currentTerm, std::nullopt);
}

TEST(AgencyLogSpecificationTest, log_target_supervision_test) {
  {
    auto supervision = LogTarget::Supervision{.maxActionsTraceLength = 15};

    VPackBuilder builder;
    serialize(builder, supervision);
    auto slice = builder.slice();

    auto fromVPack = deserialize<LogTarget::Supervision>(slice);
    EXPECT_EQ(supervision, fromVPack);
  }

  {
    auto jsonBuffer = R"({
    "maxActionsTraceLength": 1234
  })"_vpack;

    auto jsonSlice = velocypack::Slice(jsonBuffer->data());
    auto supervision = deserialize<LogTarget::Supervision>(jsonSlice);
    EXPECT_EQ(supervision.maxActionsTraceLength, 1234);
  }
}

TEST(AgencyLogSpecificationTest, log_target_test) {
  {
    auto config = LogConfig();
    config.replicationFactor = 3;
    config.writeConcern = 2;
    config.softWriteConcern = 2;
    config.waitForSync = false;

    auto target = LogTarget(
        LogId{5}, ParticipantsFlagsMap{{"A", ParticipantFlags{}}}, config);

    VPackBuilder builder;
    serialize(builder, target);
    auto slice = builder.slice();

    auto fromVPack = deserialize<LogTarget>(slice);
    EXPECT_EQ(target, fromVPack);
  }

  {
    auto config = LogConfig();
    config.replicationFactor = 3;
    config.writeConcern = 2;
    config.softWriteConcern = 2;
    config.waitForSync = true;

    auto expectedTarget = LogTarget(
        LogId{12},
        ParticipantsFlagsMap{{"A", ParticipantFlags{.allowedInQuorum = false}}},
        config);

    expectedTarget.leader = "A";

    auto jsonBuffer = R"({
      "id": 12,
      "participants": { "A": { "allowedInQuorum": false } },
      "config": {
        "writeConcern": 2,
        "replicationFactor": 3,
        "waitForSync": true },
      "leader": "A"
    })"_vpack;

    auto jsonSlice = velocypack::Slice(jsonBuffer->data());
    auto target = deserialize<LogTarget>(jsonSlice);

    EXPECT_EQ(target, expectedTarget);
  }
}
