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
#include <gmock/gmock.h>

#include "Replication2/Supervision/CollectionGroupSupervision.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::document::supervision;
namespace ag = arangodb::replication2::agency;

struct FakeUniqueIdProvider : UniqueIdProvider {
  auto next() noexcept -> std::uint64_t override { return ++_next; }
  std::uint64_t _next{0};
};

struct CollectionGroupsSupervisionTest : ::testing::Test {
  FakeUniqueIdProvider uniqid;
};

TEST_F(CollectionGroupsSupervisionTest, check_create_collection_group_plan) {
  constexpr auto numberOfShards = 3;

  CollectionGroup group;
  group.target.id = ag::CollectionGroupId{12};
  group.target.version = 1;
  group.target.collections["A"] = {};
  group.target.collections["B"] = {};
  group.target.attributes.mutableAttributes.replicationFactor = 3;
  group.target.attributes.mutableAttributes.writeConcern = 3;
  group.target.attributes.immutableAttributes.numberOfShards = numberOfShards;

  group.targetCollections["A"].groupId = group.target.id;
  group.targetCollections["B"].groupId = group.target.id;

  replicated_log::ParticipantsHealth health;
  health.update("DB1", RebootId{12}, true);
  health.update("DB2", RebootId{11}, true);
  health.update("DB3", RebootId{110}, true);

  auto result = checkCollectionGroup(group, uniqid, health);
  ASSERT_TRUE(std::holds_alternative<AddCollectionGroupToPlan>(result))
      << result.index();

  auto const& action = std::get<AddCollectionGroupToPlan>(result);

  EXPECT_EQ(action.sheaves.size(), numberOfShards);
  EXPECT_EQ(action.spec.shardSheaves.size(), numberOfShards);
}

TEST_F(CollectionGroupsSupervisionTest, check_add_server) {
  constexpr auto numberOfShards = 3;

  CollectionGroup group;
  group.target.id = ag::CollectionGroupId{12};
  group.target.version = 1;
  group.target.collections["A"] = {};
  group.target.collections["B"] = {};
  group.target.attributes.mutableAttributes.replicationFactor = 4;
  group.target.attributes.mutableAttributes.writeConcern = 3;
  group.target.attributes.mutableAttributes.waitForSync = true;
  group.target.attributes.immutableAttributes.numberOfShards = numberOfShards;

  group.targetCollections["A"].groupId = group.target.id;
  group.targetCollections["B"].groupId = group.target.id;

  group.plan.emplace();
  group.plan->attributes = group.target.attributes;
  group.plan->id = group.target.id;
  group.plan->collections["A"] = {};
  group.plan->collections["B"] = {};
  group.plan->shardSheaves.resize(3);
  group.plan->shardSheaves[0].replicatedLog = LogId{1};
  group.plan->shardSheaves[1].replicatedLog = LogId{2};
  group.plan->shardSheaves[2].replicatedLog = LogId{3};

  group.planCollections["A"].groupId = group.target.id;
  group.planCollections["A"].shardList.assign({"s1", "s2", "s3"});
  group.planCollections["B"].groupId = group.target.id;
  group.planCollections["B"].shardList.assign({"s1", "s2", "s3"});

  auto const currentConfig = ag::LogTargetConfig(3, 3, true);
  auto const expectedConfig = ag::LogTargetConfig(3, 4, true);
  group.logs[LogId{1}].target.id = LogId{1};
  group.logs[LogId{1}].target.config = currentConfig;
  group.logs[LogId{1}].target.participants["DB1"];
  group.logs[LogId{1}].target.participants["DB2"];
  group.logs[LogId{1}].target.participants["DB3"];
  group.logs[LogId{1}].target.leader = "DB1";
  group.logs[LogId{2}].target.id = LogId{2};
  group.logs[LogId{2}].target.config = currentConfig;
  group.logs[LogId{2}].target.participants["DB1"];
  group.logs[LogId{2}].target.participants["DB2"];
  group.logs[LogId{2}].target.participants["DB3"];
  group.logs[LogId{2}].target.leader = "DB2";
  group.logs[LogId{3}].target.id = LogId{3};
  group.logs[LogId{3}].target.config = currentConfig;
  group.logs[LogId{3}].target.participants["DB1"];
  group.logs[LogId{3}].target.participants["DB2"];
  group.logs[LogId{3}].target.participants["DB3"];
  group.logs[LogId{3}].target.leader = "DB3";

  replicated_log::ParticipantsHealth health;
  health.update("DB1", RebootId{12}, true);
  health.update("DB2", RebootId{11}, true);
  health.update("DB3", RebootId{110}, true);
  health.update("DB4", RebootId{110}, true);

  // do this three times for each replicated log
  for (int i = 0; i < 3; i++) {
    // first we expect a config update
    auto result = checkCollectionGroup(group, uniqid, health);
    ASSERT_TRUE(std::holds_alternative<UpdateReplicatedLogConfig>(result))
        << i << " " << result.index();
    auto const& action = std::get<UpdateReplicatedLogConfig>(result);
    EXPECT_EQ(action.config, expectedConfig);
    EXPECT_TRUE(group.logs.contains(action.logId)) << action.logId;
    EXPECT_EQ(group.logs[action.logId].target.config, currentConfig);
    group.logs[action.logId].target.config = action.config;

    // next action should be an add server action
    auto result2 = checkCollectionGroup(group, uniqid, health);
    ASSERT_TRUE(std::holds_alternative<AddParticipantToLog>(result2))
        << i << " " << result2.index();
    auto const& action2 = std::get<AddParticipantToLog>(result2);
    EXPECT_EQ(action2.logId, action.logId);
    EXPECT_EQ(action2.participant, "DB4");  // only available server
    group.logs[action.logId].target.participants[action2.participant];
  }

  // now we expect updates of the deprecated shard maps
  // TODO remove this part of the test later
  for (int i = 0; i < 2; i++) {  // two collections
    auto result = checkCollectionGroup(group, uniqid, health);
    ASSERT_TRUE(std::holds_alternative<UpdateCollectionShardMap>(result))
        << result.index() << " " << i;
    auto const& action = std::get<UpdateCollectionShardMap>(result);

    ASSERT_TRUE(group.planCollections.contains(action.cid));
    auto& collection = group.planCollections[action.cid];
    // check all shards are part of the collection
    for (auto const& shard : collection.shardList) {
      EXPECT_TRUE(action.mapping.shards.contains(shard));
    }

    collection.deprecatedShardMap = action.mapping;
  }
}
