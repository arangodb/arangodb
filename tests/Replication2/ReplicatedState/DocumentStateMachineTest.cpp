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

// Must be included early to avoid
//   "explicit specialization of 'std::char_traits<unsigned char>' after
//   instantiation"
// errors.
#include "../3rdParty/iresearch/core/utils/string.hpp"

#include "Basics/Exceptions.h"
#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "Inspection/VPack.h"
#include "Mocks/Death_Test.h"
#include "Mocks/Servers.h"
#include "Replication2/Mocks/DocumentStateMocks.h"
#include "Replication2/ReplicatedLog/TestHelper.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "gmock/gmock.h"

#include <thread>
#include <vector>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;
using namespace arangodb::replication2::test;

struct DocumentStateMachineTest : testing::Test {
  std::vector<std::string> collectionData;
  std::shared_ptr<testing::NiceMock<MockCollectionReader>>
      collectionReaderMock =
          std::make_shared<testing::NiceMock<MockCollectionReader>>(
              collectionData);
  std::shared_ptr<testing::NiceMock<MockDatabaseSnapshot>>
      databaseSnapshotMock =
          std::make_shared<testing::NiceMock<MockDatabaseSnapshot>>(
              collectionReaderMock);
  std::shared_ptr<testing::NiceMock<MockDatabaseSnapshotFactory>>
      databaseSnapshotFactoryMock =
          std::make_shared<testing::NiceMock<MockDatabaseSnapshotFactory>>();

  std::shared_ptr<testing::NiceMock<MockDocumentStateTransaction>>
      transactionMock =
          std::make_shared<testing::NiceMock<MockDocumentStateTransaction>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateShardHandler>>
      shardHandlerMock =
          std::make_shared<testing::NiceMock<MockDocumentStateShardHandler>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateNetworkHandler>>
      networkHandlerMock = std::make_shared<
          testing::NiceMock<MockDocumentStateNetworkHandler>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateLeaderInterface>>
      leaderInterfaceMock = std::make_shared<
          testing::NiceMock<MockDocumentStateLeaderInterface>>();

  std::shared_ptr<testing::NiceMock<MockDocumentStateHandlersFactory>>
      handlersFactoryMock =
          std::make_shared<testing::NiceMock<MockDocumentStateHandlersFactory>>(
              databaseSnapshotFactoryMock);
  MockTransactionManager transactionManagerMock;
  tests::mocks::MockServer mockServer = tests::mocks::MockServer();
  MockVocbase vocbaseMock =
      MockVocbase(mockServer.server(), "documentStateMachineTestDb", 2);

  void addEntry(std::vector<DocumentLogEntry>& entries, OperationType op,
                TransactionId trxId) {
    VPackBuilder builder;
    builder.openObject();
    builder.close();
    auto entry =
        DocumentLogEntry{shardId, op, builder.sharedSlice(), trxId, {}};
    entries.emplace_back(std::move(entry));
  }

  void addShardEntry(std::vector<DocumentLogEntry>& entries, OperationType op,
                     ShardID shard, CollectionID collection) {
    VPackBuilder builder;
    builder.openObject();
    builder.close();
    auto entry = DocumentLogEntry{std::move(shard), op, builder.sharedSlice(),
                                  TransactionId{}, std::move(collection)};
    entries.emplace_back(std::move(entry));
  }

  void SetUp() override {
    using namespace testing;

    collectionData.emplace_back("foo");
    collectionData.emplace_back("bar");
    collectionData.emplace_back("baz");

    ON_CALL(*databaseSnapshotFactoryMock, createSnapshot).WillByDefault([&]() {
      return std::make_unique<MockDatabaseSnapshotDelegator>(
          databaseSnapshotMock);
    });

    ON_CALL(*transactionMock, commit).WillByDefault(Return(Result{}));
    ON_CALL(*transactionMock, abort).WillByDefault(Return(Result{}));
    ON_CALL(*transactionMock, apply(_)).WillByDefault([]() {
      return OperationResult{Result{}, OperationOptions{}};
    });
    ON_CALL(*transactionMock, intermediateCommit)
        .WillByDefault(Return(Result{}));

    ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&](LogIndex) {
      return futures::Future<ResultT<SnapshotConfig>>{
          std::in_place, SnapshotConfig{SnapshotId{1}, shardMap}};
    });
    ON_CALL(*leaderInterfaceMock, nextSnapshotBatch)
        .WillByDefault([&](SnapshotId) {
          return futures::Future<ResultT<SnapshotBatch>>{
              std::in_place, SnapshotBatch{SnapshotId{1}, shardId}};
        });
    ON_CALL(*leaderInterfaceMock, finishSnapshot)
        .WillByDefault(
            [&](SnapshotId) { return futures::Future<Result>{std::in_place}; });

    ON_CALL(*networkHandlerMock, getLeaderInterface)
        .WillByDefault(Return(leaderInterfaceMock));

    ON_CALL(*handlersFactoryMock, createShardHandler)
        .WillByDefault([&](GlobalLogIdentifier const& gid) {
          ON_CALL(*shardHandlerMock, createLocalShard)
              .WillByDefault(Return(Result{}));
          ON_CALL(*shardHandlerMock, dropLocalShard)
              .WillByDefault(Return(Result{}));
          return shardHandlerMock;
        });

    ON_CALL(*handlersFactoryMock, createTransactionHandler)
        .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier gid) {
          return std::make_unique<DocumentStateTransactionHandler>(
              std::move(gid), nullptr, handlersFactoryMock);
        });

    ON_CALL(*handlersFactoryMock, createSnapshotHandler)
        .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
          return std::make_unique<DocumentStateSnapshotHandler>(
              handlersFactoryMock->makeUniqueDatabaseSnapshotFactory());
        });

    ON_CALL(*handlersFactoryMock, createTransaction)
        .WillByDefault(Return(transactionMock));

    ON_CALL(*handlersFactoryMock, createNetworkHandler)
        .WillByDefault(Return(networkHandlerMock));
  }

  void TearDown() override {
    using namespace testing;

    collectionReaderMock->reset();
    Mock::VerifyAndClearExpectations(handlersFactoryMock.get());
    Mock::VerifyAndClearExpectations(shardHandlerMock.get());
    Mock::VerifyAndClearExpectations(transactionMock.get());
    Mock::VerifyAndClearExpectations(networkHandlerMock.get());
    Mock::VerifyAndClearExpectations(leaderInterfaceMock.get());
    Mock::VerifyAndClearExpectations(collectionReaderMock.get());
    Mock::VerifyAndClearExpectations(databaseSnapshotFactoryMock.get());
  }

  const std::string collectionId = "testCollectionID";
  static constexpr LogId logId = LogId{1};
  const std::string dbName = "testDB";
  const GlobalLogIdentifier globalId{dbName, logId};
  const ShardID shardId = "s1";
  const document::DocumentCoreParameters coreParams{dbName, 0, 0};
  const velocypack::SharedSlice coreParamsSlice = coreParams.toSharedSlice();
  const std::string leaderId = "leader";
  const document::ShardMap shardMap{
      {shardId,
       ShardProperties{.collectionId = collectionId,
                       .properties = std::make_shared<VPackBuilder>()}}};
};

TEST_F(DocumentStateMachineTest, constructing_the_core_does_not_create_shard) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);

  EXPECT_CALL(*shardHandlerMock, createLocalShard(shardId, collectionId, _))
      .Times(0);
  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);

  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest, shard_is_dropped_during_cleanup) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());

  EXPECT_CALL(*shardHandlerMock, dropLocalShard(shardId, collectionId))
      .Times(1);
  auto cleanupHandler = factory.constructCleanupHandler();
  auto core = std::move(*follower).resign();
  cleanupHandler->drop(std::move(core));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest, snapshot_has_valid_ongoing_state) {
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

TEST_F(DocumentStateMachineTest, snapshot_fetch_from_ongoing_state) {
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

TEST_F(DocumentStateMachineTest,
       snapshot_remove_previous_shards_and_create_new_ones) {
  using namespace testing;

  // Acquire a snapshot containing a single shard
  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());

  // We now acquire a second snapshot with a different set of shards
  const ShardID shardId1 = "s123";
  const ShardID shardId2 = "s345";
  auto newShardMap = document::ShardMap{
      {shardId1,
       ShardProperties{.collectionId = collectionId,
                       .properties = std::make_shared<VPackBuilder>()}},
      {shardId2,
       ShardProperties{.collectionId = collectionId,
                       .properties = std::make_shared<VPackBuilder>()}}};

  ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&](LogIndex) {
    return futures::Future<ResultT<SnapshotConfig>>{
        std::in_place, SnapshotConfig{SnapshotId{12345}, newShardMap}};
  });

  // The previous shard should be dropped
  EXPECT_CALL(*shardHandlerMock, dropLocalShard(shardId, collectionId))
      .Times(1);
  // The new shards should be created
  EXPECT_CALL(*shardHandlerMock, createLocalShard(shardId1, collectionId, _))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, createLocalShard(shardId2, collectionId, _))
      .Times(1);
  follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());

  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest, snapshot_fetch_multiple_shards) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
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
        ShardProperties{.collectionId = collectionId,
                        .properties = std::make_shared<VPackBuilder>()}},
       {shardId2,
        ShardProperties{.collectionId = collectionId,
                        .properties = std::make_shared<VPackBuilder>()}}},
      std::make_unique<MockDatabaseSnapshotDelegator>(databaseSnapshotMock));
  std::size_t bytesSent{0};

  // fetch data from shard1
  for (std::size_t idx{0}; idx < collectionData.size(); ++idx) {
    EXPECT_CALL(*collectionReaderMock1, read(_, _)).Times(1);
    EXPECT_CALL(*collectionReaderMock1, hasMore()).Times(1);
    auto batchRes = snapshot.fetch();
    Mock::VerifyAndClearExpectations(collectionReaderMock1.get());

    ASSERT_TRUE(batchRes.ok()) << batchRes.result();
    auto batch = batchRes.get();
    EXPECT_EQ(snapshotId, batch.snapshotId);
    EXPECT_EQ(batch.shardId, shardId1);
    EXPECT_TRUE(batch.hasMore);
    EXPECT_TRUE(batch.payload.isArray());

    auto status = snapshot.status();
    ASSERT_EQ(replication2::replicated_state::document::kStringOngoing,
              status.state);
    EXPECT_EQ(2, status.statistics.shards.size());
    EXPECT_EQ(idx + 1, status.statistics.shards[shardId1].docsSent);
    EXPECT_EQ(idx + 1, status.statistics.batchesSent);

    bytesSent += batch.payload.byteSize();
    EXPECT_EQ(bytesSent, status.statistics.bytesSent);
  }

  // fetch data from shard2
  for (std::size_t idx{0}; idx < collectionData.size(); ++idx) {
    EXPECT_CALL(*collectionReaderMock2, read(_, _)).Times(1);
    EXPECT_CALL(*collectionReaderMock2, hasMore()).Times(1);
    auto batchRes = snapshot.fetch();
    Mock::VerifyAndClearExpectations(collectionReaderMock2.get());

    ASSERT_TRUE(batchRes.ok()) << batchRes.result();
    auto batch = batchRes.get();
    EXPECT_EQ(snapshotId, batch.snapshotId);
    EXPECT_EQ(batch.shardId, shardId2);
    EXPECT_EQ(batch.hasMore, idx < collectionData.size() - 1);
    EXPECT_TRUE(batch.payload.isArray());

    auto status = snapshot.status();
    ASSERT_EQ(replication2::replicated_state::document::kStringOngoing,
              status.state);
    EXPECT_EQ(idx + 1, status.statistics.shards[shardId2].docsSent);
    EXPECT_EQ(collectionData.size() + idx + 1, status.statistics.batchesSent);

    bytesSent += batch.payload.byteSize();
    EXPECT_EQ(bytesSent, status.statistics.bytesSent);
  }
}

TEST_F(DocumentStateMachineTest, snapshot_fetch_empty) {
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

TEST_F(DocumentStateMachineTest, snapshot_try_fetch_after_finish) {
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

TEST_F(DocumentStateMachineTest, snapshot_try_fetch_after_abort) {
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

TEST_F(DocumentStateMachineTest, snapshot_try_finish_after_abort) {
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

TEST_F(DocumentStateMachineTest, snapshot_try_abort_after_finish) {
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

TEST_F(DocumentStateMachineTest, snapshot_handler_creation_error) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory());
  EXPECT_CALL(*databaseSnapshotFactoryMock, createSnapshot)
      .WillOnce([]() -> std::unique_ptr<
                         replicated_state::document::IDatabaseSnapshot> {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_WAS_ERLAUBE);
      });
  auto res = snapshotHandler.create(shardMap);
  ASSERT_TRUE(res.fail());
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());
}

TEST_F(DocumentStateMachineTest, snapshot_handler_cannot_find_snapshot) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory());
  auto res = snapshotHandler.find(SnapshotId::create());
  ASSERT_TRUE(res.fail());
}

TEST_F(DocumentStateMachineTest,
       snapshot_handler_create_and_find_successfully_then_clear) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory());

  auto res = snapshotHandler.create(shardMap);
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

TEST_F(
    DocumentStateMachineTest,
    test_transactionHandler_ensureTransaction_creates_new_transaction_only_once) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, nullptr, handlersFactoryMock);

  auto tid = TransactionId{6};
  auto doc = DocumentLogEntry{
      "s1234", OperationType::kInsert, velocypack::SharedSlice(), tid, {}};

  EXPECT_CALL(*handlersFactoryMock, createTransaction(_, _)).Times(1);
  auto trx = transactionHandler.ensureTransaction(doc);
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());

  // Use an existing entry, and expect the transaction to be reused
  EXPECT_CALL(*handlersFactoryMock, createTransaction(_, _)).Times(0);
  ASSERT_EQ(trx, transactionHandler.ensureTransaction(doc));
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());
}

TEST_F(DocumentStateMachineTest, test_transactionHandler_removeTransaction) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, nullptr, handlersFactoryMock);

  auto tid = TransactionId{6};
  auto doc = DocumentLogEntry{
      "s1234", OperationType::kInsert, velocypack::SharedSlice(), tid, {}};
  auto trx = transactionHandler.ensureTransaction(doc);
  EXPECT_EQ(transactionHandler.getUnfinishedTransactions().size(), 1);
  transactionHandler.removeTransaction(tid);
  EXPECT_TRUE(transactionHandler.getUnfinishedTransactions().empty());
}

TEST_F(DocumentStateMachineTest,
       test_transactionHandler_applyEntry_abortAll_clears_everything) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, nullptr, handlersFactoryMock);

  auto tid = TransactionId{6};
  auto doc = DocumentLogEntry{
      "s1234", OperationType::kInsert, velocypack::SharedSlice(), tid, {}};
  auto trx = transactionHandler.ensureTransaction(doc);
  ASSERT_EQ(transactionHandler.getUnfinishedTransactions().size(), 1);

  doc.operation = OperationType::kAbortAllOngoingTrx;
  auto res = transactionHandler.applyEntry(doc);
  EXPECT_TRUE(res.ok()) << res;
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().empty());
}

TEST_F(DocumentStateMachineTest, test_applyEntry_apply_transaction_and_commit) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, nullptr, handlersFactoryMock);

  auto doc = DocumentLogEntry{"s1234",
                              OperationType::kInsert,
                              velocypack::SharedSlice(),
                              TransactionId{6},
                              {}};

  // Expect the transaction to be started an applied successfully
  EXPECT_CALL(*handlersFactoryMock, createTransaction).Times(1);
  EXPECT_CALL(*transactionMock, apply).Times(1);
  auto result = transactionHandler.applyEntry(doc);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // An intermediate commit should not affect the transaction
  doc.operation = OperationType::kIntermediateCommit;
  result = transactionHandler.applyEntry(doc);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(transactionMock.get());
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().contains(
      TransactionId{6}));

  // After commit, expect the transaction to be removed
  doc.operation = OperationType::kCommit;
  result = transactionHandler.applyEntry(doc);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(transactionMock.get());
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().empty());
}

TEST_F(DocumentStateMachineTest, test_applyEntry_apply_transaction_and_abort) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, nullptr, handlersFactoryMock);

  // Start a new transaction and then abort it.
  auto doc = DocumentLogEntry{"s1234",
                              OperationType::kRemove,
                              velocypack::SharedSlice(),
                              TransactionId{10},
                              {}};
  EXPECT_CALL(*handlersFactoryMock, createTransaction).Times(1);
  EXPECT_CALL(*transactionMock, apply).Times(1);
  auto res = transactionHandler.applyEntry(doc);
  EXPECT_TRUE(res.ok()) << res;
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().contains(
      TransactionId{10}));
  Mock::VerifyAndClearExpectations(transactionMock.get());
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());

  // Expect the transaction to be removed after abort
  doc.operation = OperationType::kAbort;
  res = transactionHandler.applyEntry(doc);
  EXPECT_TRUE(res.ok()) << res;
  Mock::VerifyAndClearExpectations(transactionMock.get());
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().empty());
}

TEST_F(DocumentStateMachineTest, test_applyEntry_handle_errors) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, nullptr, handlersFactoryMock);
  auto doc = DocumentLogEntry{"s1234",
                              OperationType::kInsert,
                              velocypack::SharedSlice(),
                              TransactionId{6},
                              {}};

  // OperationResult failed, transaction should fail
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce(Return(
          OperationResult{Result{TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION},
                          OperationOptions{}}));
  auto result = transactionHandler.applyEntry(doc);
  EXPECT_TRUE(result.fail());
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // Unique constraint violation, should not fail
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce([](DocumentLogEntry const& entry) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED] = 1;
        return opRes;
      });
  result = transactionHandler.applyEntry(doc);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // DOCUMENT_NOT_FOUND error, should not fail
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce([](DocumentLogEntry const& entry) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND] = 1;
        return opRes;
      });
  result = transactionHandler.applyEntry(doc);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // An error inside countErrorCodes, transaction should fail
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce([](DocumentLogEntry const& entry) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION] = 1;
        return opRes;
      });
  result = transactionHandler.applyEntry(doc);
  ASSERT_TRUE(result.fail());
  Mock::VerifyAndClearExpectations(transactionMock.get());
}

TEST_F(DocumentStateMachineTest,
       follower_acquire_snapshot_calls_leader_interface) {
  using namespace testing;

  auto transactionHandlerMock =
      handlersFactoryMock->makeRealTransactionHandler(globalId);
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateTransactionHandler>>(
            transactionHandlerMock);
      });

  // 1 insert + commit due to the first batch
  EXPECT_CALL(*transactionHandlerMock, applyEntry(_)).Times(2);
  EXPECT_CALL(*leaderInterfaceMock, startSnapshot(LogIndex{1})).Times(1);
  EXPECT_CALL(*leaderInterfaceMock, nextSnapshotBatch(SnapshotId{1})).Times(1);
  EXPECT_CALL(*leaderInterfaceMock, finishSnapshot(SnapshotId{1})).Times(1);
  EXPECT_CALL(*networkHandlerMock, getLeaderInterface("participantId"))
      .Times(1);

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());

  Mock::VerifyAndClearExpectations(networkHandlerMock.get());
  Mock::VerifyAndClearExpectations(leaderInterfaceMock.get());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
}

TEST_F(DocumentStateMachineTest,
       follower_resigning_while_acquiring_snapshot_concurrently) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);

  std::atomic<bool> acquireSnapshotCalled = false;

  // The snapshot will not stop until the follower resigns
  ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&](LogIndex) {
    acquireSnapshotCalled.store(true);
    acquireSnapshotCalled.notify_one();
    return futures::Future<ResultT<SnapshotConfig>>{
        std::in_place,
        SnapshotConfig{.snapshotId = SnapshotId{1}, .shards = shardMap}};
  });
  ON_CALL(*leaderInterfaceMock, nextSnapshotBatch)
      .WillByDefault([&](SnapshotId id) {
        return futures::Future<ResultT<SnapshotBatch>>{
            std::in_place,
            SnapshotBatch{
                .snapshotId = id, .shardId = shardId, .hasMore = true}};
      });

  std::thread t([follower]() {
    auto res = follower->acquireSnapshot("participantId", LogIndex{1});
    EXPECT_TRUE(res.isReady());
    EXPECT_TRUE(res.get().fail());
    EXPECT_TRUE(res.get().errorNumber() ==
                TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED);
  });

  acquireSnapshotCalled.wait(false);
  std::move(*follower).resign();
  t.join();
}

TEST_F(DocumentStateMachineTest,
       follower_applyEntries_encounters_AbortAllOngoingTrx_and_aborts_all_trx) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  std::vector<DocumentLogEntry> entries;
  addEntry(entries, OperationType::kInsert, TransactionId{6});
  addEntry(entries, OperationType::kInsert, TransactionId{10});
  addEntry(entries, OperationType::kInsert, TransactionId{14});
  addEntry(entries, OperationType::kAbortAllOngoingTrx, TransactionId{0});

  // AbortAllOngoingTrx should count towards the release index
  auto expectedReleaseIndex = LogIndex{4};
  addEntry(entries, OperationType::kInsert, TransactionId{18});
  addEntry(entries, OperationType::kInsert, TransactionId{22});

  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  EXPECT_CALL(*stream, release).WillOnce([&](LogIndex index) {
    EXPECT_EQ(index, expectedReleaseIndex);
  });
  follower->applyEntries(std::move(entryIterator));
}

TEST_F(DocumentStateMachineTest,
       follower_applyEntries_applies_transactions_but_does_not_release) {
  using namespace testing;

  auto transactionHandlerMock =
      handlersFactoryMock->makeRealTransactionHandler(globalId);
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateTransactionHandler>>(
            transactionHandlerMock);
      });

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  std::vector<DocumentLogEntry> entries;
  addEntry(entries, OperationType::kInsert, TransactionId{6});
  addEntry(entries, OperationType::kInsert, TransactionId{10});
  addEntry(entries, OperationType::kInsert, TransactionId{14});

  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  // We only call release on commit or abort
  EXPECT_CALL(*stream, release).Times(0);
  EXPECT_CALL(*transactionHandlerMock, applyEntry(_)).Times(3);
  follower->applyEntries(std::move(entryIterator));
}

TEST_F(DocumentStateMachineTest,
       follower_applyEntries_dies_if_transaction_fails) {
  using namespace testing;

  auto transactionHandlerMock =
      handlersFactoryMock->makeRealTransactionHandler(globalId);
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateTransactionHandler>>(
            transactionHandlerMock);
      });
  ON_CALL(*transactionHandlerMock, applyEntry(_))
      .WillByDefault(Return(Result(TRI_ERROR_WAS_ERLAUBE)));

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  std::vector<DocumentLogEntry> entries;
  addEntry(entries, OperationType::kInsert, TransactionId{6});
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  ASSERT_DEATH_CORE_FREE(follower->applyEntries(std::move(entryIterator)), "");
}

TEST_F(DocumentStateMachineTest,
       follower_applyEntries_commit_and_abort_calls_release) {
  using namespace testing;

  auto transactionHandlerMock =
      handlersFactoryMock->makeRealTransactionHandler(globalId);
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateTransactionHandler>>(
            transactionHandlerMock);
      });

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  // First commit then abort
  std::vector<DocumentLogEntry> entries;
  addEntry(entries, OperationType::kInsert, TransactionId{6});
  addEntry(entries, OperationType::kInsert, TransactionId{10});
  addEntry(entries, OperationType::kCommit, TransactionId{6});
  addEntry(entries, OperationType::kInsert, TransactionId{14});
  addEntry(entries, OperationType::kInsert, TransactionId{18});
  addEntry(entries, OperationType::kAbort, TransactionId{10});
  addEntry(entries, OperationType::kInsert, TransactionId{22});
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*stream, release).WillOnce([&](LogIndex index) {
    EXPECT_EQ(index.value, 3);
  });
  EXPECT_CALL(*transactionHandlerMock, applyEntry(_)).Times(7);
  follower->applyEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(stream.get());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());

  // First abort then commit
  follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);
  entries.clear();
  addEntry(entries, OperationType::kInsert, TransactionId{6});
  addEntry(entries, OperationType::kInsert, TransactionId{10});
  addEntry(entries, OperationType::kAbort, TransactionId{6});
  addEntry(entries, OperationType::kInsert, TransactionId{14});
  addEntry(entries, OperationType::kInsert, TransactionId{18});
  addEntry(entries, OperationType::kCommit, TransactionId{10});
  addEntry(entries, OperationType::kInsert, TransactionId{22});
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*stream, release).WillOnce([&](LogIndex index) {
    EXPECT_EQ(index.value, 3);
  });
  EXPECT_CALL(*transactionHandlerMock, applyEntry(_)).Times(7);
  follower->applyEntries(std::move(entryIterator));
}

TEST_F(DocumentStateMachineTest,
       follower_applyEntries_creates_and_drops_shard) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());

  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);
  EXPECT_CALL(*stream, release).Times(0);

  ShardID const myShard = "s12";
  CollectionID const myCollection = "myCollection";

  std::vector<DocumentLogEntry> entries;
  addShardEntry(entries, OperationType::kCreateShard, myShard, myCollection);
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*shardHandlerMock, createLocalShard(myShard, myCollection, _))
      .Times(1);
  follower->applyEntries(std::move(entryIterator));

  entries.clear();
  addShardEntry(entries, OperationType::kDropShard, myShard, myCollection);
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*shardHandlerMock, dropLocalShard(myShard, myCollection))
      .Times(1);
  follower->applyEntries(std::move(entryIterator));

  Mock::VerifyAndClearExpectations(stream.get());
}

TEST_F(DocumentStateMachineTest,
       follower_dies_if_shard_creation_or_deletion_fails) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  std::vector<DocumentLogEntry> entries;
  addShardEntry(entries, OperationType::kCreateShard, "randomShardId",
                collectionId);
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  ON_CALL(*shardHandlerMock, createLocalShard("randomShardId", collectionId, _))
      .WillByDefault(Return(Result(TRI_ERROR_WAS_ERLAUBE)));
  ASSERT_DEATH_CORE_FREE(follower->applyEntries(std::move(entryIterator)), "");

  entries.clear();
  addShardEntry(entries, OperationType::kDropShard, shardId, collectionId);
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  ON_CALL(*shardHandlerMock, dropLocalShard(shardId, collectionId))
      .WillByDefault(Return(Result(TRI_ERROR_WAS_ERLAUBE)));
  ASSERT_DEATH_CORE_FREE(follower->applyEntries(std::move(entryIterator)), "");
}

TEST_F(DocumentStateMachineTest, leader_manipulates_snapshot_successfully) {
  using namespace testing;

  auto snapshotHandler = handlersFactoryMock->makeRealSnapshotHandler();
  ON_CALL(*handlersFactoryMock, createSnapshotHandler(_, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateSnapshotHandler>>(
            snapshotHandler);
      });

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto leader = std::make_shared<DocumentLeaderStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock, transactionManagerMock);

  EXPECT_CALL(*snapshotHandler, create(_)).Times(1);
  auto snapshotStartRes =
      leader->snapshotStart(SnapshotParams::Start{LogIndex{1}});
  EXPECT_TRUE(snapshotStartRes.ok()) << snapshotStartRes.result();
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  auto snapshotId = snapshotStartRes.get().snapshotId;

  EXPECT_CALL(*snapshotHandler, find(snapshotId)).Times(1);
  auto snapshotNextRes = leader->snapshotNext(SnapshotParams::Next{snapshotId});
  EXPECT_TRUE(snapshotNextRes.ok()) << snapshotNextRes.result();
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  EXPECT_CALL(*snapshotHandler, find(snapshotId)).Times(1);
  auto snapshotFinishRes =
      leader->snapshotFinish(SnapshotParams::Finish{snapshotId});
  EXPECT_TRUE(snapshotFinishRes.ok()) << snapshotFinishRes;
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  EXPECT_CALL(*snapshotHandler, find(snapshotId)).Times(1);
  auto snapshotStatusRes = leader->snapshotStatus(snapshotId);
  EXPECT_TRUE(snapshotStatusRes.ok()) << snapshotStatusRes.result();
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  EXPECT_CALL(*snapshotHandler, status()).Times(1);
  EXPECT_TRUE(leader->allSnapshotsStatus().ok());
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest, leader_manipulates_snapshots_with_errors) {
  using namespace testing;

  auto snapshotHandler = handlersFactoryMock->makeRealSnapshotHandler();
  ON_CALL(*handlersFactoryMock, createSnapshotHandler(_, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateSnapshotHandler>>(
            snapshotHandler);
      });
  ON_CALL(*snapshotHandler, create(_))
      .WillByDefault(Return(
          ResultT<std::weak_ptr<Snapshot>>::error(TRI_ERROR_WAS_ERLAUBE)));
  ON_CALL(*snapshotHandler, find(SnapshotId{1}))
      .WillByDefault(Return(
          ResultT<std::weak_ptr<Snapshot>>::error(TRI_ERROR_WAS_ERLAUBE)));

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto leader = std::make_shared<DocumentLeaderStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock, transactionManagerMock);

  EXPECT_TRUE(leader->snapshotStart(SnapshotParams::Start{LogIndex{1}}).fail());
  EXPECT_TRUE(leader->snapshotNext(SnapshotParams::Next{SnapshotId{1}}).fail());
  EXPECT_TRUE(
      leader->snapshotFinish(SnapshotParams::Finish{SnapshotId{1}}).fail());
  EXPECT_TRUE(leader->snapshotStatus(SnapshotId{1}).fail());
}

TEST_F(DocumentStateMachineTest,
       leader_resign_should_abort_active_transactions) {
  using namespace testing;

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));
  auto stream = std::make_shared<testing::NiceMock<MockProducerStream>>();
  leaderState->setStream(stream);

  {
    VPackBuilder builder;
    builder.openObject();
    builder.close();

    auto operation = OperationType::kInsert;
    std::ignore = leaderState->replicateOperation(
        builder.sharedSlice(), operation, TransactionId{5}, shardId,
        ReplicationOptions{});
    std::ignore = leaderState->replicateOperation(
        builder.sharedSlice(), operation, TransactionId{9}, shardId,
        ReplicationOptions{});
    std::ignore = leaderState->replicateOperation(
        builder.sharedSlice(), operation, TransactionId{13}, shardId,
        ReplicationOptions{});
  }
  EXPECT_EQ(3U, leaderState->getActiveTransactionsCount());

  {
    VPackBuilder builder;
    std::ignore = leaderState->replicateOperation(
        builder.sharedSlice(), OperationType::kAbort, TransactionId{5}, shardId,
        ReplicationOptions{});
    std::ignore = leaderState->replicateOperation(
        builder.sharedSlice(), OperationType::kCommit, TransactionId{9},
        shardId, ReplicationOptions{});
  }
  EXPECT_EQ(1U, leaderState->getActiveTransactionsCount());

  // resigning should abort the remaining transaction with ID 13
  EXPECT_CALL(transactionManagerMock,
              abortManagedTrx(TransactionId{13}, globalId.database))
      .Times(1);
  std::ignore = std::move(*leaderState).resign();
  Mock::VerifyAndClearExpectations(&transactionManagerMock);
}

TEST_F(DocumentStateMachineTest,
       recoverEntries_should_abort_remaining_active_transactions) {
  using namespace testing;

  std::vector<DocumentLogEntry> entries;
  addShardEntry(entries, OperationType::kCreateShard, shardId, collectionId);
  // Transaction IDs are of follower type, as if they were replicated.
  addEntry(entries, OperationType::kInsert, TransactionId{6});
  addEntry(entries, OperationType::kInsert, TransactionId{10});
  addEntry(entries, OperationType::kInsert, TransactionId{14});
  addEntry(entries, OperationType::kAbort, TransactionId{6});
  addEntry(entries, OperationType::kCommit, TransactionId{10});

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));
  auto stream = std::make_shared<MockProducerStream>();

  leaderState->setStream(stream);
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  EXPECT_CALL(*stream, insert).WillOnce([&](auto const& entry) {
    EXPECT_EQ(entry.shardId, "");  // covers all shards
    EXPECT_EQ(entry.operation, OperationType::kAbortAllOngoingTrx);
    return LogIndex{entries.size() + 1};
  });
  EXPECT_CALL(transactionManagerMock,
              abortManagedTrx(TransactionId{14}.asLeaderTransactionId(),
                              globalId.database))
      .Times(1);
  EXPECT_CALL(*transactionMock, apply).Times(3);
  EXPECT_CALL(*transactionMock, commit).Times(1);
  EXPECT_CALL(*transactionMock, abort).Times(1);

  leaderState->recoverEntries(std::move(entryIterator));

  Mock::VerifyAndClearExpectations(&transactionManagerMock);
  Mock::VerifyAndClearExpectations(transactionMock.get());
}

TEST_F(DocumentStateMachineTest,
       leader_should_not_replicate_unknown_transactions) {
  using namespace testing;

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));
  auto stream = std::make_shared<testing::NiceMock<MockProducerStream>>();
  leaderState->setStream(stream);

  VPackBuilder builder;
  builder.openObject();
  builder.close();
  auto operation = OperationType::kCommit;
  auto logIndex =
      leaderState
          ->replicateOperation(builder.sharedSlice(), operation,
                               TransactionId{5}, shardId, ReplicationOptions{})
          .get();
  EXPECT_CALL(*stream, insert).Times(0);
  EXPECT_EQ(logIndex, LogIndex{});
}

TEST_F(DocumentStateMachineTest, leader_create_and_drop_shard) {
  using namespace testing;

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));
  auto stream = std::make_shared<testing::NiceMock<MockProducerStream>>();
  leaderState->setStream(stream);

  VPackBuilder builder;
  builder.openObject();
  builder.close();

  EXPECT_CALL(*stream, insert).Times(1).WillOnce([&](DocumentLogEntry entry) {
    EXPECT_EQ(entry.operation, OperationType::kCreateShard);
    EXPECT_EQ(entry.shardId, shardId);
    EXPECT_EQ(entry.collectionId, collectionId);
    return LogIndex{12};
  });

  EXPECT_CALL(*stream, waitFor(LogIndex{12})).Times(1).WillOnce([](auto) {
    return futures::Future<MockProducerStream::WaitForResult>{
        MockProducerStream::WaitForResult{}};
  });

  EXPECT_CALL(*shardHandlerMock, createLocalShard(shardId, collectionId, _))
      .Times(1);

  leaderState->createShard(shardId, collectionId, velocypack::SharedSlice());

  Mock::VerifyAndClearExpectations(stream.get());
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  EXPECT_CALL(*stream, insert).Times(1).WillOnce([&](DocumentLogEntry entry) {
    EXPECT_EQ(entry.operation, OperationType::kDropShard);
    EXPECT_EQ(entry.shardId, shardId);
    EXPECT_EQ(entry.collectionId, collectionId);
    return LogIndex{12};
  });

  EXPECT_CALL(*stream, waitFor(LogIndex{12})).Times(1).WillOnce([](auto) {
    return futures::Future<MockProducerStream::WaitForResult>{
        MockProducerStream::WaitForResult{}};
  });

  EXPECT_CALL(*shardHandlerMock, dropLocalShard(shardId, collectionId))
      .Times(1);

  leaderState->dropShard(shardId, collectionId);
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

TEST(ActiveTransactionsQueueTest,
     test_activeTransactions_releaseIndex_calculation) {
  auto activeTrx = ActiveTransactionsQueue{};

  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{99}), LogIndex{99});
  activeTrx.emplace(TransactionId{100}, LogIndex{100});
  ASSERT_EQ(activeTrx.size(), 1);
  ASSERT_TRUE(activeTrx.erase(TransactionId{100}));
  ASSERT_EQ(activeTrx.size(), 0);

  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{103}), LogIndex{103});
  ASSERT_FALSE(activeTrx.erase(TransactionId{100}));

  activeTrx.emplace(TransactionId{200}, LogIndex{200});
  activeTrx.emplace(TransactionId{300}, LogIndex{300});
  activeTrx.emplace(TransactionId{400}, LogIndex{400});
  auto transactions = activeTrx.getTransactions();
  ASSERT_EQ(transactions.size(), activeTrx.size());

  ASSERT_TRUE(activeTrx.erase(TransactionId{200}));
  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{1000}), LogIndex{299});
  ASSERT_TRUE(activeTrx.erase(TransactionId{400}));
  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{1000}), LogIndex{299});
  ASSERT_TRUE(activeTrx.erase(TransactionId{300}));
  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{1000}), LogIndex{1000});

  activeTrx.emplace(TransactionId{500}, LogIndex{500});
  ASSERT_EQ(activeTrx.size(), 1);
  activeTrx.clear();
  ASSERT_EQ(activeTrx.size(), 0);
}
