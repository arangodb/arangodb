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
#include "Replication2/ReplicatedState/ReplicatedStateImpl.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "Inspection/VPack.h"
#include "Mocks/Servers.h"
#include "Replication2/Mocks/DocumentStateMocks.h"
#include "Replication2/Mocks/MockVocbase.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "velocypack/Value.h"
#include "gmock/gmock.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::replication2;
using namespace arangodb::replication2::tests;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;

struct ShardHandlerTest : testing::Test {
  arangodb::tests::mocks::MockServer mockServer =
      arangodb::tests::mocks::MockServer();
  std::shared_ptr<MockVocbase> vocbaseMock = nullptr;

  void SetUp() override {
    vocbaseMock = std::make_shared<MockVocbase>(mockServer.server(),
                                                "shardHandlerTestDb", 1);
  }

  void TearDown() override { vocbaseMock.reset(); }
};

TEST_F(ShardHandlerTest, ensureShard_all_cases) {
  using namespace testing;

  auto gid = GlobalLogIdentifier{"db", LogId{1}};
  auto maintenance =
      std::make_shared<NiceMock<MockMaintenanceActionExecutor>>();
  auto shardHandler =
      std::make_shared<replicated_state::document::DocumentStateShardHandler>(
          *vocbaseMock, gid, maintenance);

  auto shardId = ShardID{"s1000"};
  auto collectionType = TRI_COL_TYPE_DOCUMENT;
  auto properties = velocypack::SharedSlice();

  {
    // Successful shard creation
    EXPECT_CALL(*maintenance,
                executeCreateCollection(shardId, collectionType, _))
        .Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(1);
    auto res = shardHandler->ensureShard(shardId, collectionType, properties);
    ASSERT_TRUE(res.ok()) << res;
    Mock::VerifyAndClearExpectations(maintenance.get());
  }

  {
    // Shard should not be created again a second time.
    // However, the maintenance executor will be called.
    EXPECT_CALL(*maintenance, executeCreateCollection(_, _, _)).Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(1);
    auto res = shardHandler->ensureShard(shardId, collectionType, properties);
    ASSERT_TRUE(res.ok()) << res;
    Mock::VerifyAndClearExpectations(maintenance.get());
  }

  {
    // Failure to create shard is propagated.
    // The maintenance executor will be called anyways and the database marked
    // as dirty.
    shardId = ShardID{"s1001"};
    EXPECT_CALL(*maintenance,
                executeCreateCollection(shardId, collectionType, _))
        .Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(1);
    ON_CALL(*maintenance, executeCreateCollection(_, _, _))
        .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));
    auto res = shardHandler->ensureShard(shardId, collectionType, properties);
    ASSERT_TRUE(res.is(TRI_ERROR_WAS_ERLAUBE)) << res;
    Mock::VerifyAndClearExpectations(maintenance.get());
  }
}

TEST_F(ShardHandlerTest, dropShard_all_cases) {
  using namespace testing;

  auto gid = GlobalLogIdentifier{"db", LogId{1}};
  auto maintenance =
      std::make_shared<NiceMock<MockMaintenanceActionExecutor>>();
  auto shardHandler =
      std::make_shared<replicated_state::document::DocumentStateShardHandler>(
          *vocbaseMock, gid, maintenance);

  auto shardId = ShardID{"s1000"};
  auto collectionType = TRI_COL_TYPE_DOCUMENT;
  auto properties = velocypack::SharedSlice();

  {
    // Create shard first
    auto res = shardHandler->ensureShard(shardId, collectionType, properties);
    ASSERT_TRUE(res.ok()) << res;
    vocbaseMock->registerLogicalCollection(shardId, LogId{1});
  }

  {
    // Successful shard deletion
    EXPECT_CALL(*maintenance, executeDropCollection(_)).Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(1);
    auto res = shardHandler->dropShard(shardId);
    ASSERT_TRUE(res.ok()) << res;
    Mock::VerifyAndClearExpectations(maintenance.get());
  }

  {
    // Should not be able to delete a non-existent shard.
    EXPECT_CALL(*maintenance, executeDropCollection(_)).Times(0);
    EXPECT_CALL(*maintenance, addDirty()).Times(0);
    auto res = shardHandler->dropShard(ShardID{shardId.id() + 1337});
    ASSERT_TRUE(res.fail()) << res;
    Mock::VerifyAndClearExpectations(maintenance.get());
  }

  {
    // Failure to delete shard is propagated
    // Database is marked as dirty anyway
    EXPECT_CALL(*maintenance, executeDropCollection(_)).Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(1);
    ON_CALL(*maintenance, executeDropCollection(_))
        .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));
    auto res = shardHandler->dropShard(shardId);
    ASSERT_TRUE(res.fail()) << res;
    Mock::VerifyAndClearExpectations(maintenance.get());
  }
}

TEST_F(ShardHandlerTest, dropAllShards_test) {
  using namespace testing;

  auto gid = GlobalLogIdentifier{"db", LogId{1}};
  auto maintenance =
      std::make_shared<NiceMock<MockMaintenanceActionExecutor>>();
  auto shardHandler =
      std::make_shared<replicated_state::document::DocumentStateShardHandler>(
          *vocbaseMock, gid, maintenance);
  vocbaseMock->registerLogicalCollection(ShardID{"s1000"}, LogId{1});

  // Failure to drop all shards is propagated
  ON_CALL(*maintenance, executeDropCollection(_))
      .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));
  auto res = shardHandler->dropAllShards();
  ASSERT_TRUE(res.fail());
}

TEST_F(ShardHandlerTest, modifyShard_all_cases) {
  using namespace testing;

  auto gid = GlobalLogIdentifier{"db", LogId{1}};
  auto maintenance =
      std::make_shared<NiceMock<MockMaintenanceActionExecutor>>();
  auto shardHandler =
      std::make_shared<replicated_state::document::DocumentStateShardHandler>(
          *vocbaseMock, gid, maintenance);

  auto shardId = ShardID{"s1000"};
  auto collectionType = TRI_COL_TYPE_DOCUMENT;
  auto collectionId = CollectionID{"c1000"};
  {
    // Create the shard
    auto res = shardHandler->ensureShard(shardId, collectionType,
                                         velocypack::SharedSlice());
    ASSERT_TRUE(res.ok());
    vocbaseMock->registerLogicalCollection(shardId, LogId{1});
  }

  // Modify shard properties
  VPackBuilder b;
  std::unordered_map<std::string, std::string> newProperties{{"hund", "katze"}};
  velocypack::serialize(b, newProperties);
  auto properties = b.sharedSlice();
  {
    EXPECT_CALL(*maintenance, executeModifyCollection(_, collectionId, _))
        .Times(1);
    EXPECT_CALL(*maintenance, addDirty()).Times(1);

    auto res = shardHandler->modifyShard(shardId, collectionId, properties);
    ASSERT_TRUE(res.ok());
    Mock::VerifyAndClearExpectations(maintenance.get());
  }

  {
    // Failure to modify shard is propagated
    ON_CALL(*maintenance, executeModifyCollection(_, _, _))
        .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));
    auto res = shardHandler->modifyShard(shardId, collectionId, properties);
    ASSERT_TRUE(res.fail());
    Mock::VerifyAndClearExpectations(maintenance.get());
  }

  {
    // Failure to modify non-existing shard
    shardId = ShardID{"s1001"};
    EXPECT_CALL(*maintenance, executeModifyCollection(_, collectionId, _))
        .Times(0);
    EXPECT_CALL(*maintenance, addDirty()).Times(0);

    auto res = shardHandler->modifyShard(shardId, collectionId, properties);
    ASSERT_TRUE(res.fail());
    Mock::VerifyAndClearExpectations(maintenance.get());
  }
}
