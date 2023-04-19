////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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

// Must be included early to avoid
//   "explicit specialization of 'std::char_traits<unsigned char>' after
//   instantiation"
// errors.
#include "utils/string.hpp"

#include "Basics/Exceptions.h"
#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "Inspection/VPack.h"
#include "Mocks/Servers.h"
#include "Replication2/Mocks/DocumentStateMocks.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "velocypack/Value.h"
#include "gmock/gmock.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::replication2;
using namespace arangodb::replication2::tests;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;

TEST(ShardHandlerTest, ensureShard_all_cases) {
  using namespace testing;

  auto gid = GlobalLogIdentifier{"db", LogId{1}};
  auto maintenance =
      std::make_shared<NiceMock<MockMaintenanceActionExecutor>>();
  auto shardHandler =
      std::make_shared<replicated_state::document::DocumentStateShardHandler>(
          gid, maintenance);

  auto shardId = ShardID{"s1000"};
  auto collectionId = CollectionID{"c1000"};
  auto properties = std::make_shared<VPackBuilder>();

  {
    // Successful shard creation
    EXPECT_CALL(*maintenance,
                executeCreateCollectionAction(shardId, collectionId, _))
        .Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(1);
    auto res = shardHandler->ensureShard(shardId, collectionId, properties);
    ASSERT_TRUE(res.ok());
    ASSERT_TRUE(res.get());
    Mock::VerifyAndClearExpectations(maintenance.get());
    auto shardMap = shardHandler->getShardMap();
    ASSERT_EQ(shardMap.size(), 1);
    ASSERT_TRUE(shardMap.find(shardId) != shardMap.end());
    EXPECT_EQ(shardMap.find(shardId)->second.collection, collectionId);
  }

  {
    // Shard should not be created again a second time
    EXPECT_CALL(*maintenance, executeCreateCollectionAction(_, _, _)).Times(0);
    EXPECT_CALL(*maintenance, addDirty()).Times(0);
    auto res = shardHandler->ensureShard(shardId, collectionId, properties);
    ASSERT_TRUE(res.ok());
    ASSERT_FALSE(res.get());
    Mock::VerifyAndClearExpectations(maintenance.get());
    auto shardMap = shardHandler->getShardMap();
    ASSERT_EQ(shardMap.size(), 1);
  }

  {
    // Failure to create shard is propagated
    shardId = ShardID{"s1001"};
    EXPECT_CALL(*maintenance,
                executeCreateCollectionAction(shardId, collectionId, _))
        .Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(0);
    ON_CALL(*maintenance, executeCreateCollectionAction(_, _, _))
        .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));
    auto res = shardHandler->ensureShard(shardId, collectionId, properties);
    ASSERT_TRUE(res.fail());
    Mock::VerifyAndClearExpectations(maintenance.get());
    auto shardMap = shardHandler->getShardMap();
    ASSERT_EQ(shardMap.size(), 1);
    ASSERT_TRUE(shardMap.find(shardId) == shardMap.end());
  }
}

TEST(ShardHandlerTest, dropShard_all_cases) {
  using namespace testing;

  auto gid = GlobalLogIdentifier{"db", LogId{1}};
  auto maintenance =
      std::make_shared<NiceMock<MockMaintenanceActionExecutor>>();
  auto shardHandler =
      std::make_shared<replicated_state::document::DocumentStateShardHandler>(
          gid, maintenance);

  auto shardId = ShardID{"s1000"};
  auto collectionId = CollectionID{"c1000"};
  auto properties = std::make_shared<VPackBuilder>();

  {
    // Create shard first
    auto res = shardHandler->ensureShard(shardId, collectionId, properties);
    ASSERT_TRUE(res.ok());
    ASSERT_TRUE(res.get());
    auto shardMap = shardHandler->getShardMap();
    ASSERT_EQ(shardMap.size(), 1);
    ASSERT_TRUE(shardHandler->isShardAvailable(shardId));
  }

  {
    // Successful shard deletion
    EXPECT_CALL(*maintenance,
                executeDropCollectionAction(shardId, collectionId))
        .Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(1);
    auto res = shardHandler->dropShard(shardId);
    ASSERT_TRUE(res.ok());
    ASSERT_TRUE(res.get());
    Mock::VerifyAndClearExpectations(maintenance.get());
    auto shardMap = shardHandler->getShardMap();
    ASSERT_EQ(shardMap.size(), 0);
    ASSERT_FALSE(shardHandler->isShardAvailable(shardId));
  }

  {
    // Shard should not be deleted again a second time
    EXPECT_CALL(*maintenance, executeDropCollectionAction(_, _)).Times(0);
    EXPECT_CALL(*maintenance, addDirty()).Times(0);
    auto res = shardHandler->dropShard(shardId);
    ASSERT_TRUE(res.ok());
    ASSERT_FALSE(res.get());
    Mock::VerifyAndClearExpectations(maintenance.get());
    auto shardMap = shardHandler->getShardMap();
    ASSERT_EQ(shardMap.size(), 0);
    ASSERT_FALSE(shardHandler->isShardAvailable(shardId));
  }

  {
    // Create shard again
    auto res = shardHandler->ensureShard(shardId, collectionId, properties);
    ASSERT_TRUE(res.ok());
    ASSERT_TRUE(res.get());
    auto shardMap = shardHandler->getShardMap();
    ASSERT_EQ(shardMap.size(), 1);
    ASSERT_TRUE(shardHandler->isShardAvailable(shardId));
  }

  {
    // Failure to delete shard is propagated
    EXPECT_CALL(*maintenance,
                executeDropCollectionAction(shardId, collectionId))
        .Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(0);
    ON_CALL(*maintenance, executeDropCollectionAction(_, _))
        .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));
    auto res = shardHandler->dropShard(shardId);
    ASSERT_TRUE(res.fail());
    Mock::VerifyAndClearExpectations(maintenance.get());
    auto shardMap = shardHandler->getShardMap();
    ASSERT_EQ(shardMap.size(), 1);
    ASSERT_TRUE(shardHandler->isShardAvailable(shardId));
  }
}

TEST(ShardHandlerTest, dropAllShards_test) {
  using namespace testing;

  auto gid = GlobalLogIdentifier{"db", LogId{1}};
  auto maintenance =
      std::make_shared<NiceMock<MockMaintenanceActionExecutor>>();
  auto shardHandler =
      std::make_shared<replicated_state::document::DocumentStateShardHandler>(
          gid, maintenance);

  auto collectionId = CollectionID{"c1000"};
  auto properties = std::make_shared<VPackBuilder>();
  auto const limit = 10;

  // Create some shards
  for (std::size_t idx = 0; idx < limit; ++idx) {
    auto shardId = ShardID{std::to_string(idx)};
    auto res =
        shardHandler->ensureShard(std::move(shardId), collectionId, properties);
    ASSERT_TRUE(res.ok());
    ASSERT_TRUE(res.get());
  }

  auto shardMap = shardHandler->getShardMap();
  ASSERT_EQ(shardMap.size(), limit);

  // Failure to drop all shards is propagated
  ON_CALL(*maintenance, executeDropCollectionAction(_, _))
      .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));
  auto res = shardHandler->dropAllShards();
  ASSERT_TRUE(res.fail());

  // Successful deletion of all shards should clear the shard map
  ON_CALL(*maintenance, executeDropCollectionAction(_, _))
      .WillByDefault(Return(Result{}));
  EXPECT_CALL(*maintenance, addDirty()).Times(1);
  EXPECT_CALL(*maintenance, executeDropCollectionAction(_, _)).Times(limit);
  res = shardHandler->dropAllShards();
  ASSERT_TRUE(res.ok());
  Mock::VerifyAndClearExpectations(maintenance.get());
  shardMap = shardHandler->getShardMap();
  ASSERT_EQ(shardMap.size(), 0);
}
