////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

  auto shards = logicalCollections;
  shards.emplace_back(makeLogicalCollection(shardId));

  // A new snapshot is created. Nothing should happen, only initialization.
  // No collection should be read from yet.
  EXPECT_CALL(*collectionReaderMock, getDocCount()).Times(0);
  auto snapshot = Snapshot(
      SnapshotId{12345}, globalId, shards,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock),
      loggerContext);
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());

  auto status = snapshot.status();
  ASSERT_EQ(status.state,
            replication2::replicated_state::document::kStringOngoing);
  EXPECT_EQ(status.statistics.shards.size(), 1);
  EXPECT_TRUE(status.statistics.shards.contains(shardId));
  EXPECT_EQ(status.statistics.shards[shardId].totalDocs, std::nullopt);
  EXPECT_EQ(status.statistics.shards[shardId].docsSent, 0);
  EXPECT_EQ(status.statistics.batchesSent, 0);
  EXPECT_EQ(status.statistics.bytesSent, 0);
}

TEST_F(DocumentStateSnapshotTest, snapshot_fetch_from_ongoing_state) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
  auto shards = logicalCollections;
  shards.emplace_back(makeLogicalCollection(shardId));
  auto snapshot = Snapshot(
      snapshotId, globalId, shards,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock),
      loggerContext);

  std::size_t bytesSent{0};
  for (std::size_t idx{0}; idx < collectionData.size(); ++idx) {
    EXPECT_CALL(*collectionReaderMock, read(_, _)).Times(1);
    EXPECT_CALL(*collectionReaderMock, hasMore()).Times(1);
    auto batchRes = snapshot.fetch();
    Mock::VerifyAndClearExpectations(collectionReaderMock.get());

    ASSERT_TRUE(batchRes.ok()) << batchRes.result();
    auto batch = batchRes.get();
    EXPECT_EQ(batch.snapshotId, snapshotId);
    EXPECT_EQ(batch.hasMore, idx < collectionData.size() - 1);
    if (idx == 0) {
      // First batch contains the CreateShard operation, besides Insert and
      // Commit
      EXPECT_EQ(batch.operations.size(), 3);
    } else {
      // Additional batches contain only Insert and Commit (unless a new shard
      // is "opened" for transfer)
      EXPECT_EQ(batch.operations.size(), 2);
    }

    auto status = snapshot.status();
    ASSERT_EQ(status.state,
              replication2::replicated_state::document::kStringOngoing);
    EXPECT_EQ(status.statistics.shards[shardId].docsSent, idx + 1);
    EXPECT_EQ(status.statistics.batchesSent, idx + 1);

    EXPECT_GT(status.statistics.bytesSent, bytesSent);
    bytesSent = status.statistics.bytesSent;
  }
}

TEST_F(DocumentStateSnapshotTest,
       snapshot_remove_previous_shards_and_create_new_ones) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();

  // Default initialize a follower
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId");
  EXPECT_TRUE(res.isReady() && res.waitAndGet().ok());

  // Acquire a new snapshot with a different set of shards
  const ShardID shardId1{123};
  const ShardID shardId2{345};
  ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&]() {
    return futures::Future<ResultT<SnapshotBatch>>{
        std::in_place,
        SnapshotBatch{
            .snapshotId = SnapshotId{1},
            .hasMore = true,
            .operations = {ReplicatedOperation::buildCreateShardOperation(
                shardId1, TRI_COL_TYPE_DOCUMENT, velocypack::SharedSlice())}}};
  });
  ON_CALL(*leaderInterfaceMock, nextSnapshotBatch).WillByDefault([&]() {
    return futures::Future<ResultT<SnapshotBatch>>{
        std::in_place,
        SnapshotBatch{
            .snapshotId = SnapshotId{1},
            .hasMore = false,
            .operations = {ReplicatedOperation::buildCreateShardOperation(
                shardId2, TRI_COL_TYPE_EDGE, velocypack::SharedSlice())}}};
  });

  // There should be exactly two batches sent
  EXPECT_CALL(*leaderInterfaceMock, startSnapshot()).Times(1);
  EXPECT_CALL(*leaderInterfaceMock, nextSnapshotBatch(SnapshotId{1})).Times(1);
  EXPECT_CALL(*leaderInterfaceMock, finishSnapshot(SnapshotId{1})).Times(1);

  // The previous shard should be dropped
  EXPECT_CALL(*shardHandlerMock, dropAllShards()).Times(1);

  // New shards should be created
  EXPECT_CALL(*shardHandlerMock,
              ensureShard(shardId1, TRI_COL_TYPE_DOCUMENT, _))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, ensureShard(shardId2, TRI_COL_TYPE_EDGE, _))
      .Times(1);

  std::ignore = follower->acquireSnapshot("participantId");
  EXPECT_TRUE(res.isReady() && res.waitAndGet().ok());

  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
  Mock::VerifyAndClearExpectations(leaderInterfaceMock.get());
}

TEST_F(DocumentStateSnapshotTest, snapshot_fetch_multiple_shards) {
  using namespace testing;

  auto snapshotId = SnapshotId{1};
  const ShardID shardId1{1};
  const ShardID shardId2{2};
  const ShardID shardId3{3};

  // The snapshot should fetch the shards in the reverse order of their
  // insertion. This way, we ensure the order looks natural (1, 2, 3).
  auto shards = logicalCollections;
  shards.emplace_back(makeLogicalCollection(shardId3));
  shards.emplace_back(makeLogicalCollection(shardId2));
  shards.emplace_back(makeLogicalCollection(shardId1));

  // s1 has 2 documents
  std::vector<std::string> collectionData1 = {"1", "2"};
  // s2 has 1 document
  std::vector<std::string> collectionData2 = {"1"};
  // s3 is empty
  std::vector<std::string> collectionData3 = {};

  // Setup collection reader creation.
  // This step will check that the collection reader is created for each shard.
  std::shared_ptr<testing::NiceMock<MockCollectionReader>>
      collectionReaderMock1 =
          std::make_shared<testing::NiceMock<MockCollectionReader>>(
              collectionData1);
  std::shared_ptr<testing::NiceMock<MockCollectionReader>>
      collectionReaderMock2 =
          std::make_shared<testing::NiceMock<MockCollectionReader>>(
              collectionData2);
  std::shared_ptr<testing::NiceMock<MockCollectionReader>>
      collectionReaderMock3 =
          std::make_shared<testing::NiceMock<MockCollectionReader>>(
              collectionData3);
  EXPECT_CALL(*databaseSnapshotMock, createCollectionReader(_))
      .WillRepeatedly([&](std::shared_ptr<LogicalCollection> const& shard) {
        if (shard->name() == "s1") {
          return std::make_unique<MockCollectionReaderDelegator>(
              collectionReaderMock1);
        } else if (shard->name() == "s2") {
          return std::make_unique<MockCollectionReaderDelegator>(
              collectionReaderMock2);
        } else if (shard->name() == "s3") {
          return std::make_unique<MockCollectionReaderDelegator>(
              collectionReaderMock3);
        }
        TRI_ASSERT(false) << "Unexpected shard name: " << shard->name();
        // The following is only to keep the compiler happy in non-maintainer
        // builds:
        return std::unique_ptr<MockCollectionReaderDelegator>(nullptr);
      });

  auto snapshot = Snapshot(
      snapshotId, globalId, shards,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock),
      loggerContext);

  // First batch should contain the creation of s1 along with the first document
  EXPECT_CALL(*collectionReaderMock1, read(_, _)).Times(1);
  EXPECT_CALL(*collectionReaderMock1, hasMore()).Times(1);
  auto batchRes = snapshot.fetch();
  ASSERT_TRUE(batchRes.ok()) << batchRes.result();
  Mock::VerifyAndClearExpectations(collectionReaderMock1.get());
  auto batch = batchRes.get();
  EXPECT_EQ(snapshotId, batch.snapshotId) << batch;
  EXPECT_TRUE(batch.hasMore) << batch;
  EXPECT_EQ(batch.operations.size(), 3) << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::CreateShard>(
      batch.operations[0].operation))
      << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::Insert>(
      batch.operations[1].operation))
      << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::Commit>(
      batch.operations[2].operation))
      << batch;

  // Second batch should contain the last document of s1
  EXPECT_CALL(*collectionReaderMock1, read(_, _)).Times(1);
  EXPECT_CALL(*collectionReaderMock1, hasMore()).Times(1);
  batchRes = snapshot.fetch();
  ASSERT_TRUE(batchRes.ok()) << batchRes.result();
  Mock::VerifyAndClearExpectations(collectionReaderMock1.get());
  batch = batchRes.get();
  EXPECT_EQ(snapshotId, batch.snapshotId) << batch;
  EXPECT_TRUE(batch.hasMore) << batch;
  EXPECT_EQ(batch.operations.size(), 2) << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::Insert>(
      batch.operations[0].operation))
      << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::Commit>(
      batch.operations[1].operation))
      << batch;

  // Third batch should contain the creation of s2 along with its document
  EXPECT_CALL(*collectionReaderMock2, read(_, _)).Times(1);
  EXPECT_CALL(*collectionReaderMock2, hasMore()).Times(1);
  EXPECT_CALL(*collectionReaderMock1, hasMore()).Times(0);
  batchRes = snapshot.fetch();
  ASSERT_TRUE(batchRes.ok()) << batchRes.result();
  Mock::VerifyAndClearExpectations(collectionReaderMock1.get());
  Mock::VerifyAndClearExpectations(collectionReaderMock2.get());
  batch = batchRes.get();
  EXPECT_EQ(snapshotId, batch.snapshotId) << batch;
  EXPECT_TRUE(batch.hasMore) << batch;
  EXPECT_EQ(batch.operations.size(), 3) << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::CreateShard>(
      batch.operations[0].operation))
      << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::Insert>(
      batch.operations[1].operation))
      << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::Commit>(
      batch.operations[2].operation))
      << batch;

  // Fourth batch should contain the creation of s3 long with an empty Insert
  // and Commit
  EXPECT_CALL(*collectionReaderMock3, read(_, _)).Times(1);
  EXPECT_CALL(*collectionReaderMock3, hasMore()).Times(1);
  EXPECT_CALL(*collectionReaderMock2, hasMore()).Times(0);
  EXPECT_CALL(*collectionReaderMock1, hasMore()).Times(0);
  batchRes = snapshot.fetch();
  ASSERT_TRUE(batchRes.ok()) << batchRes.result();
  Mock::VerifyAndClearExpectations(collectionReaderMock1.get());
  Mock::VerifyAndClearExpectations(collectionReaderMock2.get());
  Mock::VerifyAndClearExpectations(collectionReaderMock3.get());
  batch = batchRes.get();
  EXPECT_EQ(snapshotId, batch.snapshotId) << batch;
  EXPECT_FALSE(batch.hasMore) << batch;
  EXPECT_EQ(batch.operations.size(), 3) << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::CreateShard>(
      batch.operations[0].operation))
      << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::Insert>(
      batch.operations[1].operation))
      << batch;
  EXPECT_TRUE(
      std::get<ReplicatedOperation::Insert>(batch.operations[1].operation)
          .payload.slice()
          .isEmptyArray())
      << batch;
  EXPECT_TRUE(std::holds_alternative<ReplicatedOperation::Commit>(
      batch.operations[2].operation))
      << batch;

  // Any further batches should come as empty
  EXPECT_CALL(*collectionReaderMock3, hasMore()).Times(0);
  EXPECT_CALL(*collectionReaderMock2, hasMore()).Times(0);
  EXPECT_CALL(*collectionReaderMock1, hasMore()).Times(0);
  batchRes = snapshot.fetch();
  ASSERT_TRUE(batchRes.ok()) << batchRes.result();
  Mock::VerifyAndClearExpectations(collectionReaderMock1.get());
  Mock::VerifyAndClearExpectations(collectionReaderMock2.get());
  Mock::VerifyAndClearExpectations(collectionReaderMock3.get());
  batch = batchRes.get();
  EXPECT_EQ(snapshotId, batch.snapshotId) << batch;
  EXPECT_FALSE(batch.hasMore) << batch;
  EXPECT_TRUE(batch.operations.empty()) << batch;

  // Check statistics
  auto status = snapshot.status();
  ASSERT_EQ(replication2::replicated_state::document::kStringOngoing,
            status.state);
  EXPECT_EQ(status.statistics.shards[shardId1].docsSent,
            collectionData1.size());
  EXPECT_EQ(status.statistics.shards[shardId2].docsSent,
            collectionData2.size());
  EXPECT_EQ(status.statistics.shards[shardId3].docsSent,
            collectionData3.size());
  EXPECT_EQ(status.statistics.batchesSent, 4);
  EXPECT_GT(status.statistics.bytesSent, 0);
}

TEST_F(DocumentStateSnapshotTest, snapshot_fetch_empty) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};

  auto databaseSnapshotMock =
      std::make_shared<testing::StrictMock<MockDatabaseSnapshot>>(nullptr);

  auto snapshot = Snapshot(
      snapshotId, globalId, logicalCollections,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock),
      loggerContext);

  auto batchRes = snapshot.fetch();

  ASSERT_TRUE(batchRes.ok()) << batchRes.result();
  auto batch = batchRes.get();
  EXPECT_EQ(snapshotId, batch.snapshotId) << batch;
  EXPECT_FALSE(batch.hasMore) << batch;
  EXPECT_TRUE(batch.operations.empty()) << batch;

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
      snapshotId, globalId, logicalCollections,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock),
      loggerContext);

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
      snapshotId, globalId, logicalCollections,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock),
      loggerContext);

  snapshot.abort();

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
      snapshotId, globalId, logicalCollections,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock),
      loggerContext);

  snapshot.abort();

  EXPECT_CALL(*collectionReaderMock, read(_, _)).Times(0);
  EXPECT_CALL(*collectionReaderMock, hasMore()).Times(0);
  auto res = snapshot.finish();
  ASSERT_TRUE(res.fail());
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());

  // Calling abort again should have no effect
  snapshot.abort();
}

TEST_F(DocumentStateSnapshotTest, snapshot_try_abort_after_finish) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
  auto snapshot = Snapshot(
      snapshotId, globalId, logicalCollections,
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock),
      loggerContext);

  auto res = snapshot.finish();
  ASSERT_TRUE(res.ok()) << res;

  EXPECT_CALL(*collectionReaderMock, read(_, _)).Times(0);
  EXPECT_CALL(*collectionReaderMock, hasMore()).Times(0);
  snapshot.abort();
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());

  // Calling finish again should have no effect
  res = snapshot.finish();
  ASSERT_TRUE(res.ok()) << res;
}

TEST_F(DocumentStateSnapshotTest, snapshot_handler_creation_error) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
      MockDocumentStateSnapshotHandler::rebootTracker, globalId, loggerContext);
  EXPECT_CALL(*databaseSnapshotFactoryMock, createSnapshot)
      .WillOnce([]() -> std::unique_ptr<
                         replicated_state::document::IDatabaseSnapshot> {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_WAS_ERLAUBE);
      });
  auto res = snapshotHandler.create(logicalCollections, {});
  ASSERT_TRUE(res.fail());
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());
}

TEST_F(DocumentStateSnapshotTest, snapshot_handler_cannot_find_snapshot) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
      MockDocumentStateSnapshotHandler::rebootTracker, globalId, loggerContext);
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
      fakeRebootTracker, globalId, loggerContext);

  auto res = snapshotHandler.create(
      logicalCollections,
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
      fakeRebootTracker, globalId, loggerContext);

  auto res = snapshotHandler.create(
      logicalCollections,
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
      fakeRebootTracker, globalId, loggerContext);

  auto shards = logicalCollections;
  shards.emplace_back(makeLogicalCollection(shardId));
  auto res = snapshotHandler.create(
      shards,
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
  document::SnapshotBatch batch{
      .snapshotId = SnapshotId{1234}, .hasMore = false, .operations = {}};
  auto s = velocypack::serialize(batch);
  auto d = velocypack::deserialize<document::SnapshotBatch>(s.slice());
  ASSERT_EQ(d.snapshotId, batch.snapshotId);
  ASSERT_EQ(d.hasMore, batch.hasMore);
  ASSERT_EQ(d.operations, batch.operations);
}
