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
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"

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
  const LogPlanSpecification fromVPack(from_velocypack, slice);
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
  spec = LogPlanSpecification{from_velocypack, jsonSlice};
  EXPECT_EQ(spec.currentTerm->leader, std::nullopt);

  jsonBuffer = R"({
    "id": 1234,
    "participantsConfig": {
      "generation": 15,
      "participants": {}
    }
  })"_vpack;

  jsonSlice = velocypack::Slice(jsonBuffer->data());
  spec = LogPlanSpecification{from_velocypack, jsonSlice};
  EXPECT_EQ(spec.currentTerm, std::nullopt);
}
