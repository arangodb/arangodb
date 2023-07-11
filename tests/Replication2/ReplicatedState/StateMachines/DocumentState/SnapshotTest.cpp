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

#include "Replication2/ReplicatedState/StateMachines/DocumentState/DocumentStateMachineTest.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::replication2;
using namespace arangodb::replication2::tests;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;

struct DocumentStateSnapshotTest : DocumentStateMachineTest {};

TEST_F(DocumentStateSnapshotTest, snapshot_has_valid_ongoing_state) {
  using namespace testing;

  EXPECT_CALL(*collectionReaderMock, getDocCount()).Times(1);
  auto snapshot = Snapshot(
      SnapshotId{12345}, shardMap,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock));
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());

  auto status = snapshot.status();
  ASSERT_EQ(status.state,
            replication2::replicated_state::document::kStringOngoing);
  EXPECT_EQ(status.statistics.shards.size(), 1);
  EXPECT_TRUE(status.statistics.shards.contains(shardId));
  EXPECT_EQ(status.statistics.shards[shardId].totalDocs,
            collectionReaderMock->getDocCount());
  EXPECT_EQ(status.statistics.shards[shardId].docsSent, 0);
  EXPECT_EQ(status.statistics.batchesSent, 0);
  EXPECT_EQ(status.statistics.bytesSent, 0);
}

TEST_F(DocumentStateSnapshotTest, snapshot_fetch_from_ongoing_state) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
  auto snapshot = Snapshot(
      snapshotId, shardMap,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock));
  std::size_t bytesSent{0};

  for (std::size_t idx{0}; idx < collectionData.size(); ++idx) {
    EXPECT_CALL(*collectionReaderMock, read(_, _)).Times(1);
    EXPECT_CALL(*collectionReaderMock, hasMore()).Times(1);
    auto batchRes = snapshot.fetch();
    Mock::VerifyAndClearExpectations(collectionReaderMock.get());

    ASSERT_TRUE(batchRes.ok()) << batchRes.result();
    auto batch = batchRes.get();
    EXPECT_EQ(batch.snapshotId, snapshotId);
    EXPECT_EQ(batch.shardId, shardId);
    EXPECT_EQ(batch.hasMore, idx < collectionData.size() - 1);
    EXPECT_TRUE(batch.payload.isArray());

    auto status = snapshot.status();
    ASSERT_EQ(status.state,
              replication2::replicated_state::document::kStringOngoing);
    EXPECT_EQ(status.statistics.shards[shardId].docsSent, idx + 1);
    EXPECT_EQ(status.statistics.batchesSent, idx + 1);

    bytesSent += batch.payload.byteSize();
    EXPECT_EQ(status.statistics.bytesSent, bytesSent);
  }
}

TEST_F(DocumentStateSnapshotTest,
       snapshot_remove_previous_shards_and_create_new_ones) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();

  // Acquire a snapshot containing a single shard
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId");
  EXPECT_TRUE(res.isReady() && res.get().ok());

  // We now acquire a second snapshot with a different set of shards
  const ShardID shardId1 = "s123";
  const ShardID shardId2 = "s345";
  auto newShardMap = document::ShardMap{
      {shardId1,
       ShardProperties{.collection = collectionId,
                       .properties = std::make_shared<VPackBuilder>()}},
      {shardId2,
       ShardProperties{.collection = collectionId,
                       .properties = std::make_shared<VPackBuilder>()}}};

  ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&]() {
    return futures::Future<ResultT<SnapshotConfig>>{
        std::in_place, SnapshotConfig{SnapshotId{1}, newShardMap}};
  });

  // The previous shard should be dropped
  EXPECT_CALL(*shardHandlerMock, dropAllShards()).Times(1);
  // The new shards should be created
  EXPECT_CALL(*shardHandlerMock, ensureShard(shardId1, collectionId, _))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, ensureShard(shardId2, collectionId, _))
      .Times(1);
  follower->acquireSnapshot("participantId");
  EXPECT_TRUE(res.isReady() && res.get().ok());

  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateSnapshotTest, snapshot_fetch_multiple_shards) {
  using namespace testing;

  auto snapshotId = SnapshotId{1};
  const ShardID shardId1 = "s1";
  const ShardID shardId2 = "s2";

  std::shared_ptr<testing::NiceMock<MockCollectionReader>>
      collectionReaderMock1 =
          std::make_shared<testing::NiceMock<MockCollectionReader>>(
              collectionData);
  std::shared_ptr<testing::NiceMock<MockCollectionReader>>
      collectionReaderMock2 =
          std::make_shared<testing::NiceMock<MockCollectionReader>>(
              collectionData);

  EXPECT_CALL(*databaseSnapshotMock, createCollectionReader(shardId1))
      .WillOnce([&collectionReaderMock1](std::string_view collectionName) {
        return std::make_unique<MockCollectionReaderDelegator>(
            collectionReaderMock1);
      });
  EXPECT_CALL(*databaseSnapshotMock, createCollectionReader(shardId2))
      .WillOnce([&collectionReaderMock2](std::string_view collectionName) {
        return std::make_unique<MockCollectionReaderDelegator>(
            collectionReaderMock2);
      });

  auto snapshot = Snapshot(
      snapshotId,
      {{shardId1,
        ShardProperties{.collection = collectionId,
                        .properties = std::make_shared<VPackBuilder>()}},
       {shardId2,
        ShardProperties{.collection = collectionId,
                        .properties = std::make_shared<VPackBuilder>()}}},
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock));
  std::size_t bytesSent{0};

  EXPECT_CALL(*collectionReaderMock1, read(_, _)).Times(1);
  EXPECT_CALL(*collectionReaderMock1, hasMore()).Times(1);
  EXPECT_CALL(*collectionReaderMock2, read(_, _)).Times(1);
  EXPECT_CALL(*collectionReaderMock2, hasMore()).Times(1);
  std::optional<ShardID> shardID;

  // fetch data from first shard
  for (std::size_t idx{0}; idx < collectionData.size(); ++idx) {
    auto batchRes = snapshot.fetch();
    ASSERT_TRUE(batchRes.ok()) << batchRes.result();
    auto batch = batchRes.get();
    EXPECT_EQ(snapshotId, batch.snapshotId);
    shardID = batch.shardId;

    if (shardID == shardId1) {
      Mock::VerifyAndClearExpectations(collectionReaderMock1.get());
    } else if (shardID == shardId2) {
      Mock::VerifyAndClearExpectations(collectionReaderMock2.get());
    } else {
      ADD_FAILURE();
    }

    EXPECT_TRUE(batch.hasMore);
    EXPECT_TRUE(batch.payload.isArray());

    auto status = snapshot.status();
    ASSERT_EQ(replication2::replicated_state::document::kStringOngoing,
              status.state);
    EXPECT_EQ(2, status.statistics.shards.size());
    EXPECT_EQ(idx + 1, status.statistics.shards[*shardID].docsSent);
    EXPECT_EQ(idx + 1, status.statistics.batchesSent);

    bytesSent += batch.payload.byteSize();
    EXPECT_EQ(bytesSent, status.statistics.bytesSent);
  }

  // fetch data from second shard
  for (std::size_t idx{0}; idx < collectionData.size(); ++idx) {
    auto batchRes = snapshot.fetch();
    ASSERT_TRUE(batchRes.ok()) << batchRes.result();
    auto batch = batchRes.get();
    EXPECT_EQ(snapshotId, batch.snapshotId);
    shardID = batch.shardId;

    if (shardID == shardId1) {
      Mock::VerifyAndClearExpectations(collectionReaderMock1.get());
    } else if (shardID == shardId2) {
      Mock::VerifyAndClearExpectations(collectionReaderMock2.get());
    } else {
      ADD_FAILURE();
    }

    EXPECT_EQ(snapshotId, batch.snapshotId);
    EXPECT_EQ(batch.hasMore, idx < collectionData.size() - 1);
    EXPECT_TRUE(batch.payload.isArray());

    auto status = snapshot.status();
    ASSERT_EQ(replication2::replicated_state::document::kStringOngoing,
              status.state);
    EXPECT_EQ(idx + 1, status.statistics.shards[*shardID].docsSent);
    EXPECT_EQ(collectionData.size() + idx + 1, status.statistics.batchesSent);

    bytesSent += batch.payload.byteSize();
    EXPECT_EQ(bytesSent, status.statistics.bytesSent);
  }
}

TEST_F(DocumentStateSnapshotTest, snapshot_fetch_empty) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};

  auto databaseSnapshotMock =
      std::make_shared<testing::StrictMock<MockDatabaseSnapshot>>(nullptr);

  auto snapshot = Snapshot(
      snapshotId, {},
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock));

  auto batchRes = snapshot.fetch();

  ASSERT_TRUE(batchRes.ok()) << batchRes.result();
  auto batch = batchRes.get();
  EXPECT_EQ(snapshotId, batch.snapshotId);
  EXPECT_FALSE(batch.shardId.has_value());
  EXPECT_FALSE(batch.hasMore);
  EXPECT_TRUE(batch.payload.isNone());

  auto status = snapshot.status();
  ASSERT_EQ(replication2::replicated_state::document::kStringOngoing,
            status.state);
  EXPECT_EQ(0, status.statistics.shards.size());
  EXPECT_EQ(0, status.statistics.batchesSent);
}

TEST_F(DocumentStateSnapshotTest, snapshot_try_fetch_after_finish) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
  auto snapshot = Snapshot(
      snapshotId, shardMap,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock));

  auto res = snapshot.finish();
  ASSERT_TRUE(res.ok()) << res;

  auto status = snapshot.status();
  ASSERT_EQ(status.state,
            replication2::replicated_state::document::kStringFinished);

  EXPECT_CALL(*collectionReaderMock, read(_, _)).Times(0);
  EXPECT_CALL(*collectionReaderMock, hasMore()).Times(0);
  auto batchRes = snapshot.fetch();
  ASSERT_TRUE(batchRes.fail());
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());
}

TEST_F(DocumentStateSnapshotTest, snapshot_try_fetch_after_abort) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
  auto snapshot = Snapshot(
      snapshotId, shardMap,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock));

  auto res = snapshot.abort();
  ASSERT_TRUE(res.ok()) << res;

  auto status = snapshot.status();
  ASSERT_EQ(status.state,
            replication2::replicated_state::document::kStringAborted);

  EXPECT_CALL(*collectionReaderMock, read(_, _)).Times(0);
  EXPECT_CALL(*collectionReaderMock, hasMore()).Times(0);
  auto batchRes = snapshot.fetch();
  ASSERT_TRUE(batchRes.fail());
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());
}

TEST_F(DocumentStateSnapshotTest, snapshot_try_finish_after_abort) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
  auto snapshot = Snapshot(
      snapshotId, shardMap,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock));

  auto res = snapshot.abort();
  ASSERT_TRUE(res.ok()) << res;

  EXPECT_CALL(*collectionReaderMock, read(_, _)).Times(0);
  EXPECT_CALL(*collectionReaderMock, hasMore()).Times(0);
  res = snapshot.finish();
  ASSERT_TRUE(res.fail());
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());

  // Calling abort again should have no effect
  res = snapshot.abort();
  ASSERT_TRUE(res.ok()) << res;
}

TEST_F(DocumentStateSnapshotTest, snapshot_try_abort_after_finish) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
  auto snapshot = Snapshot(
      snapshotId, shardMap,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock));

  auto res = snapshot.finish();
  ASSERT_TRUE(res.ok()) << res;

  EXPECT_CALL(*collectionReaderMock, read(_, _)).Times(0);
  EXPECT_CALL(*collectionReaderMock, hasMore()).Times(0);
  res = snapshot.abort();
  ASSERT_TRUE(res.fail());
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());

  // Calling finish again should have no effect
  res = snapshot.finish();
  ASSERT_TRUE(res.ok()) << res;
}

TEST_F(DocumentStateSnapshotTest, snapshot_handler_creation_error) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
      MockDocumentStateSnapshotHandler::rebootTracker);
  EXPECT_CALL(*databaseSnapshotFactoryMock, createSnapshot)
      .WillOnce([]() -> std::unique_ptr<
                         replicated_state::document::IDatabaseSnapshot> {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_WAS_ERLAUBE);
      });
  auto res = snapshotHandler.create(shardMap, {});
  ASSERT_TRUE(res.fail());
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());
}

TEST_F(DocumentStateSnapshotTest, snapshot_handler_cannot_find_snapshot) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
      MockDocumentStateSnapshotHandler::rebootTracker);
  auto res = snapshotHandler.find(SnapshotId::create());
  ASSERT_TRUE(res.fail());
}

TEST_F(DocumentStateSnapshotTest,
       snapshot_handler_create_and_find_successfully_then_clear) {
  using namespace testing;

  cluster::RebootTracker fakeRebootTracker{nullptr};
  fakeRebootTracker.updateServerState(

      {{"documentStateMachineServer",
        ServerHealthState{RebootId{1}, arangodb::ServerHealth::kUnclear}}});

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
      fakeRebootTracker);

  auto res = snapshotHandler.create(
      shardMap,
      {.serverId = "documentStateMachineServer", .rebootId = RebootId(1)});
  ASSERT_TRUE(res.ok()) << res.result();

  auto snapshot = res.get().lock();
  auto status = snapshot->status();
  ASSERT_EQ(status.state,
            replication2::replicated_state::document::kStringOngoing);

  auto allStatuses = snapshotHandler.status();
  ASSERT_EQ(allStatuses.snapshots.size(), 1);

  auto batchRes = snapshot->fetch();
  ASSERT_TRUE(batchRes.ok()) << batchRes.result();
  auto snapshotId = batchRes->snapshotId;
  EXPECT_TRUE(allStatuses.snapshots.contains(snapshotId));

  auto findRes = snapshotHandler.find(snapshotId);
  EXPECT_TRUE(findRes.ok()) << findRes.result();

  snapshotHandler.clear();
  allStatuses = snapshotHandler.status();
  ASSERT_EQ(allStatuses.snapshots.size(), 0);
}

TEST_F(DocumentStateSnapshotTest, snapshot_handler_abort_snapshot) {
  using namespace testing;

  cluster::RebootTracker fakeRebootTracker{nullptr};
  fakeRebootTracker.updateServerState(
      {{"documentStateMachineServer",
        ServerHealthState{RebootId{1}, arangodb::ServerHealth::kUnclear}}});

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
      fakeRebootTracker);

  auto res = snapshotHandler.create(
      shardMap,
      {.serverId = "documentStateMachineServer", .rebootId = RebootId(1)});
  ASSERT_TRUE(res.ok()) << res.result();

  auto snapshot = res.get().lock();
  auto id = snapshot->getId();
  EXPECT_TRUE(snapshotHandler.abort(id).ok());
  EXPECT_TRUE(snapshotHandler.abort(SnapshotId{123}).fail());
}

TEST_F(DocumentStateSnapshotTest,
       snapshot_handler_gives_up_shard_and_resets_transaction) {
  using namespace testing;

  cluster::RebootTracker fakeRebootTracker{nullptr};
  fakeRebootTracker.updateServerState(

      {{"documentStateMachineServer",
        ServerHealthState{RebootId{1}, arangodb::ServerHealth::kUnclear}}});

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
      fakeRebootTracker);

  auto res = snapshotHandler.create(
      shardMap,
      {.serverId = "documentStateMachineServer", .rebootId = RebootId(1)});
  ASSERT_TRUE(res.ok()) << res.result();

  EXPECT_CALL(*databaseSnapshotMock, resetTransaction).Times(1);
  snapshotHandler.giveUpOnShard(shardId);
  Mock::VerifyAndClearExpectations(databaseSnapshotMock.get());

  snapshotHandler.clear();
  EXPECT_CALL(*databaseSnapshotMock, resetTransaction).Times(0);
  snapshotHandler.giveUpOnShard(shardId);
  Mock::VerifyAndClearExpectations(databaseSnapshotMock.get());
}

TEST(SnapshotIdTest, parse_snapshot_id_successfully) {
  auto id = SnapshotId::fromString("12345");
  ASSERT_TRUE(id.ok()) << id.result();
  ASSERT_EQ(id->id(), 12345);
  ASSERT_EQ(to_string(id.get()), "12345");
}

TEST(SnapshotIdTest, parse_snapshot_id_error_bad_characters) {
  auto id = SnapshotId::fromString("#!@#abcd");
  ASSERT_TRUE(id.fail());
}

TEST(SnapshotIdTest,
     parse_snapshot_id_error_number_follower_by_bad_characters) {
  auto id = SnapshotId::fromString("123$");
  ASSERT_TRUE(id.fail());
}

TEST(SnapshotIdTest, parse_snapshot_id_error_overflow) {
  auto id = SnapshotId::fromString("123456789012345678901234567890");
  ASSERT_TRUE(id.fail());
}

TEST(SnapshotStatusTest, serialize_snapshot_statistics) {
  auto state = state::Ongoing{};
  document::SnapshotStatus status(state, document::SnapshotStatistics{});
  ASSERT_EQ(velocypack::serialize(status).get("state").stringView(), "ongoing");
}

TEST(SnapshotStatusTest, serialize_snapshot_batch) {
  document::SnapshotBatch batch{SnapshotId{1234}, "s123", false,
                                velocypack::SharedSlice()};
  auto s = velocypack::serialize(batch);
  auto d = velocypack::deserialize<document::SnapshotBatch>(s.slice());
  ASSERT_EQ(d.snapshotId, batch.snapshotId);
  ASSERT_EQ(d.shardId, batch.shardId);
  ASSERT_EQ(d.hasMore, batch.hasMore);
}
