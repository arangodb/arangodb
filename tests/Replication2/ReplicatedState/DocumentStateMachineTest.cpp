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
#include "Replication2/Mocks/MockVocbase.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "Transaction/Manager.h"
#include "velocypack/Value.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "gmock/gmock.h"

#include <thread>
#include <vector>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::replication2;
using namespace arangodb::replication2::tests;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;

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
  std::shared_ptr<testing::NiceMock<MockMaintenanceActionExecutor>>
      maintenanceActionExecutorMock =
          std::make_shared<testing::NiceMock<MockMaintenanceActionExecutor>>();

  std::shared_ptr<testing::NiceMock<MockDocumentStateHandlersFactory>>
      handlersFactoryMock =
          std::make_shared<testing::NiceMock<MockDocumentStateHandlersFactory>>(
              databaseSnapshotFactoryMock);
  MockTransactionManager transactionManagerMock;
  mocks::MockServer mockServer = mocks::MockServer();
  MockVocbase vocbaseMock =
      MockVocbase(mockServer.server(), "documentStateMachineTestDb", 2);

  auto createDocumentEntry(TRI_voc_document_operation_e op, TransactionId tid) {
    return DocumentLogEntry{ReplicatedOperation::buildDocumentOperation(
        TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{tid}, shardId,
        velocypack::SharedSlice())};
  }

  auto createRealTransactionHandler()
      -> std::shared_ptr<MockDocumentStateTransactionHandler> {
    auto transactionHandlerMock =
        handlersFactoryMock->makeRealTransactionHandler(&vocbaseMock, globalId,
                                                        shardHandlerMock);

    ON_CALL(*handlersFactoryMock,
            createTransactionHandler(testing::_, testing::_, testing::_))
        .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const&,
                           std::shared_ptr<IDocumentStateShardHandler> const&) {
          return std::make_unique<
              testing::NiceMock<MockDocumentStateTransactionHandler>>(
              transactionHandlerMock);
        });

    return transactionHandlerMock;
  }

  auto createLeader() -> std::shared_ptr<DocumentLeaderStateWrapper> {
    auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
    return std::make_shared<DocumentLeaderStateWrapper>(
        factory.constructCore(vocbaseMock, globalId, coreParams),
        handlersFactoryMock, transactionManagerMock);
  }

  auto createFollower() -> std::shared_ptr<DocumentFollowerStateWrapper> {
    auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
    return std::make_shared<DocumentFollowerStateWrapper>(
        factory.constructCore(vocbaseMock, globalId, coreParams),
        handlersFactoryMock);
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

    ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&]() {
      return futures::Future<ResultT<SnapshotConfig>>{
          std::in_place, SnapshotConfig{SnapshotId{1}, shardMap}};
    });
    ON_CALL(*leaderInterfaceMock, nextSnapshotBatch)
        .WillByDefault([&](SnapshotId) {
          // An array is needed so that we can call the "length" method on the
          // the slice later on.
          auto payload = std::vector<int>{1, 2, 3};
          return futures::Future<ResultT<SnapshotBatch>>{
              std::in_place,
              SnapshotBatch{.snapshotId = SnapshotId{1},
                            .shardId = shardId,
                            .hasMore = false,
                            .payload = velocypack::serialize(payload)}};
        });
    ON_CALL(*leaderInterfaceMock, finishSnapshot)
        .WillByDefault(
            [&](SnapshotId) { return futures::Future<Result>{std::in_place}; });

    ON_CALL(*networkHandlerMock, getLeaderInterface)
        .WillByDefault(Return(leaderInterfaceMock));

    ON_CALL(*maintenanceActionExecutorMock, executeCreateCollectionAction)
        .WillByDefault(Return(Result{}));
    ON_CALL(*maintenanceActionExecutorMock, executeDropCollectionAction)
        .WillByDefault(Return(Result{}));

    ON_CALL(*handlersFactoryMock, createShardHandler)
        .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
          ON_CALL(*shardHandlerMock, ensureShard).WillByDefault(Return(true));
          ON_CALL(*shardHandlerMock, dropShard).WillByDefault(Return(true));
          ON_CALL(*shardHandlerMock, dropAllShards)
              .WillByDefault(Return(Result{}));
          ON_CALL(*shardHandlerMock, isShardAvailable)
              .WillByDefault(Return(true));
          ON_CALL(*shardHandlerMock, getShardMap)
              .WillByDefault(
                  Return(replication2::replicated_state::document::ShardMap{}));
          return shardHandlerMock;
        });

    ON_CALL(*handlersFactoryMock, createTransactionHandler)
        .WillByDefault(
            [&](TRI_vocbase_t&, GlobalLogIdentifier gid,
                std::shared_ptr<IDocumentStateShardHandler> shardHandler) {
              return std::make_unique<DocumentStateTransactionHandler>(
                  std::move(gid), nullptr, handlersFactoryMock,
                  std::move(shardHandler));
            });

    ON_CALL(*handlersFactoryMock, createSnapshotHandler)
        .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
          return std::make_unique<DocumentStateSnapshotHandler>(
              handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
              MockDocumentStateSnapshotHandler::rebootTracker);
        });

    ON_CALL(*handlersFactoryMock, createTransaction)
        .WillByDefault(Return(transactionMock));

    ON_CALL(*handlersFactoryMock, createNetworkHandler)
        .WillByDefault(Return(networkHandlerMock));

    ON_CALL(*handlersFactoryMock, createMaintenanceActionExecutor)
        .WillByDefault(Return(maintenanceActionExecutorMock));
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
       ShardProperties{.collection = collectionId,
                       .properties = std::make_shared<VPackBuilder>()}}};
};

TEST_F(DocumentStateMachineTest, constructing_the_core_does_not_create_shard) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);

  EXPECT_CALL(*shardHandlerMock, ensureShard(shardId, collectionId, _))
      .Times(0);
  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);

  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest, dropping_the_core_with_error_messages) {
  using namespace testing;
  auto transactionHandlerMock = createRealTransactionHandler();

  ON_CALL(*transactionHandlerMock, applyEntry(Matcher<ReplicatedOperation>(_)))
      .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  EXPECT_CALL(*shardHandlerMock, dropAllShards()).Times(1);
  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  ON_CALL(*shardHandlerMock, dropAllShards)
      .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));
  core->drop();
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest,
       shard_is_dropped_and_transactions_aborted_during_cleanup) {
  using namespace testing;

  // For simplicity, no shards for this snapshot.
  ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&]() {
    return futures::Future<ResultT<SnapshotConfig>>{
        std::in_place, SnapshotConfig{SnapshotId{1}, {}}};
  });

  auto transactionHandlerMock = handlersFactoryMock->makeRealTransactionHandler(
      &vocbaseMock, globalId, shardHandlerMock);
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_, _, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const&,
                         std::shared_ptr<IDocumentStateShardHandler> const&) {
        return std::make_unique<NiceMock<MockDocumentStateTransactionHandler>>(
            transactionHandlerMock);
      });

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(vocbaseMock, globalId, coreParams),
      handlersFactoryMock);

  // transaction should be aborted before the snapshot is acquired
  EXPECT_CALL(
      *transactionHandlerMock,
      applyEntry(ReplicatedOperation::buildAbortAllOngoingTrxOperation()))
      .Times(1);
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());

  EXPECT_CALL(
      *transactionHandlerMock,
      applyEntry(ReplicatedOperation::buildAbortAllOngoingTrxOperation()))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, dropAllShards()).Times(1);
  auto cleanupHandler = factory.constructCleanupHandler();
  auto core = std::move(*follower).resign();
  cleanupHandler->drop(std::move(core));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
}

TEST_F(DocumentStateMachineTest, follower_associated_shard_map) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());

  ON_CALL(*shardHandlerMock, getShardMap()).WillByDefault(Return(shardMap));

  auto shardIds = follower->getAssociatedShardList();
  EXPECT_EQ(shardIds.size(), 1);
  EXPECT_EQ(shardIds[0], shardId);
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

  auto transactionHandlerMock = createRealTransactionHandler();

  // Acquire a snapshot containing a single shard
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
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
  follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());

  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest, snapshot_fetch_multiple_shards) {
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

TEST_F(DocumentStateMachineTest, snapshot_handler_cannot_find_snapshot) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueDatabaseSnapshotFactory(),
      MockDocumentStateSnapshotHandler::rebootTracker);
  auto res = snapshotHandler.find(SnapshotId::create());
  ASSERT_TRUE(res.fail());
}

TEST_F(DocumentStateMachineTest,
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

TEST_F(DocumentStateMachineTest, snapshot_handler_abort_snapshot) {
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

TEST_F(DocumentStateMachineTest,
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

TEST_F(
    DocumentStateMachineTest,
    test_transactionHandler_ensureTransaction_creates_new_transaction_only_once) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, &vocbaseMock,
      handlersFactoryMock, shardHandlerMock);

  auto tid = TransactionId{6};
  auto op = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_UPDATE, tid, shardId,
      velocypack::SharedSlice());

  EXPECT_CALL(*handlersFactoryMock,
              createTransaction(_, tid, shardId, AccessMode::Type::WRITE))
      .Times(1);
  auto res = transactionHandler.applyEntry(op);
  EXPECT_TRUE(res.ok()) << res;
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());
  EXPECT_EQ(transactionHandler.getUnfinishedTransactions().size(), 1);

  // Use an existing entry and expect the transaction to be reused
  EXPECT_CALL(*handlersFactoryMock, createTransaction(_, _, _, _)).Times(0);
  res = transactionHandler.applyEntry(op);
  EXPECT_TRUE(res.ok()) << res;
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());
  EXPECT_EQ(transactionHandler.getUnfinishedTransactions().size(), 1);
}

TEST_F(DocumentStateMachineTest, test_transactionHandler_removeTransaction) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, &vocbaseMock,
      handlersFactoryMock, shardHandlerMock);

  auto tid = TransactionId{6};
  auto op = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_REPLACE, tid, shardId,
      velocypack::SharedSlice());
  auto res = transactionHandler.applyEntry(op);
  EXPECT_TRUE(res.ok());
  EXPECT_EQ(transactionHandler.getUnfinishedTransactions().size(), 1);
  transactionHandler.removeTransaction(tid);
  EXPECT_TRUE(transactionHandler.getUnfinishedTransactions().empty());
}

TEST_F(DocumentStateMachineTest,
       test_transactionHandler_applyEntry_abortAll_clears_everything) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, &vocbaseMock,
      handlersFactoryMock, shardHandlerMock);

  auto tid = TransactionId{6};
  auto op = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_REMOVE, tid, shardId,
      velocypack::SharedSlice());
  auto res = transactionHandler.applyEntry(op);
  EXPECT_TRUE(res.ok()) << res;
  ASSERT_EQ(transactionHandler.getUnfinishedTransactions().size(), 1);

  op = ReplicatedOperation::buildAbortAllOngoingTrxOperation();
  res = transactionHandler.applyEntry(op);
  EXPECT_TRUE(res.ok()) << res;
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().empty());
}

TEST_F(DocumentStateMachineTest, test_applyEntry_apply_transaction_and_commit) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, &vocbaseMock,
      handlersFactoryMock, shardHandlerMock);

  auto tid = TransactionId{6};
  auto op = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_INSERT, tid, shardId,
      velocypack::SharedSlice());

  // Expect the transaction to be created and applied successfully
  EXPECT_CALL(*handlersFactoryMock, createTransaction(_, tid, shardId, _))
      .Times(1);
  EXPECT_CALL(*transactionMock, apply).Times(1);
  auto result = transactionHandler.applyEntry(op);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // An intermediate commit should not affect the transaction
  op = ReplicatedOperation::buildIntermediateCommitOperation(tid);
  result = transactionHandler.applyEntry(op);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(transactionMock.get());
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().contains(
      TransactionId{6}));

  // After commit, expect the transaction to be removed
  op = ReplicatedOperation::buildCommitOperation(tid);
  result = transactionHandler.applyEntry(op);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(transactionMock.get());
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().empty());
}

TEST_F(DocumentStateMachineTest, test_applyEntry_apply_transaction_and_abort) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, &vocbaseMock,
      handlersFactoryMock, shardHandlerMock);

  // Start a new transaction and then abort it.
  auto tid = TransactionId{6};
  auto op = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_INSERT, tid, shardId,
      velocypack::SharedSlice());

  EXPECT_CALL(*handlersFactoryMock, createTransaction).Times(1);
  EXPECT_CALL(*transactionMock, apply).Times(1);
  auto res = transactionHandler.applyEntry(op);
  EXPECT_TRUE(res.ok()) << res;
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().contains(tid));
  Mock::VerifyAndClearExpectations(transactionMock.get());
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());

  // Expect the transaction to be removed after abort
  op = ReplicatedOperation::buildAbortOperation(tid);
  res = transactionHandler.applyEntry(op);
  EXPECT_TRUE(res.ok()) << res;
  Mock::VerifyAndClearExpectations(transactionMock.get());
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().empty());
}

TEST_F(DocumentStateMachineTest, test_applyEntry_handle_errors) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, &vocbaseMock,
      handlersFactoryMock, shardHandlerMock);

  auto tid = TransactionId{6};
  auto op = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_INSERT, tid, shardId,
      velocypack::SharedSlice());

  // OperationResult failed, transaction should fail
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce(Return(
          OperationResult{Result{TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION},
                          OperationOptions{}}));
  auto result = transactionHandler.applyEntry(op);
  EXPECT_TRUE(result.fail());
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // Unique constraint violation, should not fail
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce([](ReplicatedOperation::OperationType const&) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED] = 1;
        return opRes;
      });
  result = transactionHandler.applyEntry(op);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // DOCUMENT_NOT_FOUND error, should not fail
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce([](ReplicatedOperation::OperationType const&) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND] = 1;
        return opRes;
      });
  result = transactionHandler.applyEntry(op);
  EXPECT_TRUE(result.ok()) << result;
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // An error inside countErrorCodes, transaction should fail
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce([](ReplicatedOperation::OperationType const&) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION] = 1;
        return opRes;
      });
  result = transactionHandler.applyEntry(op);
  ASSERT_TRUE(result.fail());
  Mock::VerifyAndClearExpectations(transactionMock.get());
}

TEST_F(DocumentStateMachineTest,
       follower_acquire_snapshot_calls_leader_interface) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();

  // The first call to applyEntry should be AbortAllOngoingTrx
  // 3 transactions are expected to be applied
  // 1 CreateShard due to the snapshot transfer
  // 1 Insert and 1 Commit due to the first batch
  EXPECT_CALL(*transactionHandlerMock,
              applyEntry(Matcher<ReplicatedOperation>(_)))
      .Times(4);
  EXPECT_CALL(*leaderInterfaceMock, startSnapshot()).Times(1);
  EXPECT_CALL(*leaderInterfaceMock, nextSnapshotBatch(SnapshotId{1})).Times(1);
  EXPECT_CALL(*leaderInterfaceMock, finishSnapshot(SnapshotId{1})).Times(1);
  EXPECT_CALL(*networkHandlerMock, getLeaderInterface("participantId"))
      .Times(1);

  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());

  Mock::VerifyAndClearExpectations(networkHandlerMock.get());
  Mock::VerifyAndClearExpectations(leaderInterfaceMock.get());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
}

TEST_F(DocumentStateMachineTest,
       follower_resigning_while_acquiring_snapshot_concurrently) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();

  MockDocumentStateSnapshotHandler::rebootTracker.updateServerState(
      {{"participantId",
        ServerHealthState{RebootId{1}, arangodb::ServerHealth::kUnclear}}});

  std::atomic<bool> acquireSnapshotCalled = false;

  // The snapshot will not stop until the follower resigns
  ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&]() {
    acquireSnapshotCalled.store(true);
    acquireSnapshotCalled.notify_one();
    return futures::Future<ResultT<SnapshotConfig>>{
        std::in_place,
        SnapshotConfig{.snapshotId = SnapshotId{1}, .shards = shardMap}};
  });
  auto emptyPayload = velocypack::SharedSlice(std::shared_ptr<uint8_t const>(
      velocypack::Slice::emptyArraySliceData,
      [](auto) { /* don't delete the pointer */ }));
  ON_CALL(*leaderInterfaceMock, nextSnapshotBatch)
      .WillByDefault([&](SnapshotId id) {
        return futures::Future<ResultT<SnapshotBatch>>{
            std::in_place, SnapshotBatch{.snapshotId = id,
                                         .shardId = shardId,
                                         .hasMore = true,
                                         .payload = emptyPayload}};
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

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  std::vector<DocumentLogEntry> entries;
  for (std::uint64_t tid : {6, 10, 14}) {
    entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                             TransactionId{tid}));
  }
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildAbortAllOngoingTrxOperation()});

  // AbortAllOngoingTrx should count towards the release index
  auto expectedReleaseIndex = LogIndex{4};
  for (std::uint64_t tid : {18, 22}) {
    entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                             TransactionId{tid}));
  }

  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  EXPECT_CALL(*stream, release).WillOnce([&](LogIndex index) {
    EXPECT_EQ(index, expectedReleaseIndex);
  });
  follower->applyEntries(std::move(entryIterator));
}

TEST_F(DocumentStateMachineTest,
       follower_applyEntries_applies_transactions_but_does_not_release) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  std::vector<DocumentLogEntry> entries;
  for (std::uint64_t tid : {6, 10, 14}) {
    entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                             TransactionId{tid}));
  }

  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  // We only call release on commit or abort
  EXPECT_CALL(*stream, release).Times(0);
  EXPECT_CALL(*transactionHandlerMock,
              applyEntry(Matcher<ReplicatedOperation::OperationType const&>(_)))
      .Times(3);
  follower->applyEntries(std::move(entryIterator));
}

TEST_F(DocumentStateMachineTest,
       follower_intermediate_commit_does_not_release) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  std::vector<DocumentLogEntry> entries;
  auto tid = TransactionId{6};
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{tid}));
  entries.emplace_back(
      DocumentLogEntry{ReplicatedOperation::buildIntermediateCommitOperation(
          TransactionId{tid})});
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildIntermediateCommitOperation(TransactionId{8})});

  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  EXPECT_CALL(*stream, release).Times(0);
  follower->applyEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(stream.get());
}

TEST_F(DocumentStateMachineTest,
       follower_applyEntries_dies_if_transaction_fails) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  ON_CALL(*transactionHandlerMock,
          applyEntry(Matcher<ReplicatedOperation::OperationType const&>(_)))
      .WillByDefault(Return(Result(TRI_ERROR_WAS_ERLAUBE)));
  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      DocumentLogEntry{ReplicatedOperation::buildDocumentOperation(
          TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{6}, shardId,
          velocypack::SharedSlice())});
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  ASSERT_DEATH_CORE_FREE(follower->applyEntries(std::move(entryIterator)), "");
}

TEST_F(DocumentStateMachineTest,
       follower_applyEntries_commit_and_abort_calls_release) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  // First commit then abort
  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{6}));
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{10}));
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildCommitOperation(TransactionId{6})});
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{14}));
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{18}));
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildAbortOperation(TransactionId{10})});
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{22}));
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*stream, release).WillOnce([&](LogIndex index) {
    EXPECT_EQ(index.value, 3);
  });
  EXPECT_CALL(*transactionHandlerMock,
              applyEntry(Matcher<ReplicatedOperation::OperationType const&>(_)))
      .Times(7);
  follower->applyEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(stream.get());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());

  // First abort then commit
  follower = createFollower();
  res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);
  entries.clear();

  entries.emplace_back(
      createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{6}));
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{10}));
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildAbortOperation(TransactionId{6})});
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{14}));
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{18}));
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildCommitOperation(TransactionId{10})});
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{22}));
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*stream, release).WillOnce([&](LogIndex index) {
    EXPECT_EQ(index.value, 3);
  });
  EXPECT_CALL(*transactionHandlerMock,
              applyEntry(Matcher<ReplicatedOperation::OperationType const&>(_)))
      .Times(7);
  follower->applyEntries(std::move(entryIterator));
}

TEST_F(DocumentStateMachineTest,
       follower_applyEntries_creates_and_drops_shard) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());

  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  ShardID const myShard = "s12";
  CollectionID const myCollection = "myCollection";

  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      DocumentLogEntry{ReplicatedOperation::buildCreateShardOperation(
          myShard, myCollection, std::make_shared<VPackBuilder>())});
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*shardHandlerMock, ensureShard(myShard, myCollection, _))
      .Times(1);
  EXPECT_CALL(*stream, release).Times(1);
  follower->applyEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(stream.get());

  entries.clear();
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildDropShardOperation(myShard, myCollection)});
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*shardHandlerMock, dropShard(myShard)).Times(1);
  EXPECT_CALL(*stream, release).Times(1);
  follower->applyEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(stream.get());

  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest,
       follower_dies_if_shard_creation_or_deletion_fails) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      DocumentLogEntry{ReplicatedOperation::buildCreateShardOperation(
          shardId, collectionId, std::make_shared<VPackBuilder>())});
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  ON_CALL(*shardHandlerMock, ensureShard(shardId, collectionId, _))
      .WillByDefault(Return(Result(TRI_ERROR_WAS_ERLAUBE)));
  ASSERT_DEATH_CORE_FREE(follower->applyEntries(std::move(entryIterator)), "");

  entries.clear();
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildDropShardOperation(shardId, collectionId)});
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  ON_CALL(*shardHandlerMock, dropShard(shardId))
      .WillByDefault(Return(Result(TRI_ERROR_WAS_ERLAUBE)));
  ASSERT_DEATH_CORE_FREE(follower->applyEntries(std::move(entryIterator)), "");
}

TEST_F(DocumentStateMachineTest, follower_ignores_invalid_transactions) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  // Try to apply a regular entry, but pretend the shard is not available
  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{6}));
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  ON_CALL(*shardHandlerMock, isShardAvailable(shardId))
      .WillByDefault(Return(false));

  EXPECT_CALL(*shardHandlerMock, isShardAvailable(shardId)).Times(1);
  EXPECT_CALL(*transactionHandlerMock, applyEntry(entries[0].operation))
      .Times(0);
  follower->applyEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
  ON_CALL(*shardHandlerMock, isShardAvailable(shardId))
      .WillByDefault(Return(true));

  // Try to commit the previous entry
  entries.clear();
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildCommitOperation(TransactionId{6})});
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*shardHandlerMock, isShardAvailable(shardId)).Times(0);
  EXPECT_CALL(*transactionHandlerMock, applyEntry(entries[0].operation))
      .Times(0);
  follower->applyEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());

  // Try to apply another entry, this time making the shard available
  entries.clear();
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{10}));
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*shardHandlerMock, isShardAvailable(shardId)).Times(1);
  EXPECT_CALL(*transactionHandlerMock,
              applyEntry(entries[0].getInnerOperation()))
      .Times(1);
  follower->applyEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
}

TEST_F(DocumentStateMachineTest,
       follower_aborts_transactions_of_dropped_shard) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  auto follower = createFollower();
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  auto stream = std::make_shared<MockProducerStream>();
  follower->setStream(stream);

  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      DocumentLogEntry{ReplicatedOperation::buildDocumentOperation(
          TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{6}, "shard1",
          velocypack::SharedSlice())});
  entries.emplace_back(
      DocumentLogEntry{ReplicatedOperation::buildDocumentOperation(
          TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{10}, "shard2",
          velocypack::SharedSlice())});
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  follower->applyEntries(std::move(entryIterator));

  entries.clear();
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildDropShardOperation("shard1", collectionId)});
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  ON_CALL(*transactionHandlerMock, getTransactionsForShard("shard1"))
      .WillByDefault(Return(std::vector<TransactionId>{TransactionId{6}}));
  ON_CALL(*transactionHandlerMock, getTransactionsForShard("shard2"))
      .WillByDefault(Return(std::vector<TransactionId>{TransactionId{10}}));
  EXPECT_CALL(*transactionHandlerMock, getTransactionsForShard("shard1"))
      .Times(1);
  EXPECT_CALL(
      *transactionHandlerMock,
      applyEntry(ReplicatedOperation::buildAbortOperation(TransactionId{6})))
      .Times(1);
  EXPECT_CALL(*transactionHandlerMock, getTransactionsForShard("shard2"))
      .Times(0);
  EXPECT_CALL(
      *transactionHandlerMock,
      applyEntry(ReplicatedOperation::buildAbortOperation(TransactionId{10})))
      .Times(0);
  EXPECT_CALL(*transactionHandlerMock,
              applyEntry(entries[0].getInnerOperation()))
      .Times(1);
  EXPECT_CALL(*stream, release(LogIndex{1})).Times(1);

  follower->applyEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
  Mock::VerifyAndClearExpectations(stream.get());
}

TEST_F(DocumentStateMachineTest, leader_manipulates_snapshot_successfully) {
  using namespace testing;

  cluster::RebootTracker fakeRebootTracker{nullptr};
  fakeRebootTracker.updateServerState(

      {{"documentStateMachineServer",
        ServerHealthState{RebootId{1}, arangodb::ServerHealth::kUnclear}}});

  auto snapshotHandler =
      handlersFactoryMock->makeRealSnapshotHandler(&fakeRebootTracker);
  ON_CALL(*handlersFactoryMock, createSnapshotHandler(_, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
        return snapshotHandler;
      });

  auto leader = createLeader();
  EXPECT_CALL(*snapshotHandler, create(_, _)).Times(1);
  auto snapshotStartRes = leader->snapshotStart(SnapshotParams::Start{
      .serverId = "documentStateMachineServer", .rebootId = RebootId{1}});
  EXPECT_TRUE(snapshotStartRes.ok()) << snapshotStartRes.result();
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  auto snapshotId = snapshotStartRes.get().snapshotId;

  EXPECT_CALL(*snapshotHandler, find(snapshotId)).Times(1);
  auto snapshotNextRes = leader->snapshotNext(SnapshotParams::Next{snapshotId});
  EXPECT_TRUE(snapshotNextRes.ok()) << snapshotNextRes.result();
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  EXPECT_CALL(*snapshotHandler, finish(snapshotId)).Times(1);
  auto snapshotFinishRes =
      leader->snapshotFinish(SnapshotParams::Finish{snapshotId});
  EXPECT_TRUE(snapshotFinishRes.ok()) << snapshotFinishRes;
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  // The snapshot should be cleared after finish was called
  EXPECT_CALL(*snapshotHandler, find(snapshotId)).Times(1);
  auto snapshotStatusRes = leader->snapshotStatus(snapshotId);
  EXPECT_TRUE(snapshotStatusRes.fail());
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  EXPECT_TRUE(leader->allSnapshotsStatus().ok());
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest, leader_manipulates_snapshots_with_errors) {
  using namespace testing;

  auto snapshotHandler = handlersFactoryMock->makeRealSnapshotHandler();
  ON_CALL(*handlersFactoryMock, createSnapshotHandler(_, _))
      .WillByDefault([&](TRI_vocbase_t&, GlobalLogIdentifier const& gid) {
        return snapshotHandler;
      });
  ON_CALL(*snapshotHandler, create(_, _))
      .WillByDefault(Return(
          ResultT<std::weak_ptr<Snapshot>>::error(TRI_ERROR_WAS_ERLAUBE)));
  ON_CALL(*snapshotHandler, find(SnapshotId{1}))
      .WillByDefault(Return(
          ResultT<std::weak_ptr<Snapshot>>::error(TRI_ERROR_WAS_ERLAUBE)));

  auto leader = createLeader();
  EXPECT_TRUE(leader->snapshotStart(SnapshotParams::Start{}).fail());
  EXPECT_TRUE(leader->snapshotNext(SnapshotParams::Next{SnapshotId{1}}).fail());
  EXPECT_TRUE(
      leader->snapshotFinish(SnapshotParams::Finish{SnapshotId{1}}).fail());
  EXPECT_TRUE(leader->snapshotStatus(SnapshotId{1}).fail());
}

TEST_F(DocumentStateMachineTest,
       leader_resign_should_abort_active_transactions) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
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

    for (std::uint64_t tid : {5, 9, 13}) {
      auto res =
          leaderState
              ->replicateOperation(
                  ReplicatedOperation::buildDocumentOperation(
                      TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{tid},
                      shardId, velocypack::SharedSlice()),
                  ReplicationOptions{})
              .get();
      EXPECT_TRUE(res.ok()) << res.result();
    }
  }
  EXPECT_EQ(3U, leaderState->getActiveTransactionsCount());

  {
    VPackBuilder builder;
    auto res =
        leaderState
            ->replicateOperation(
                ReplicatedOperation::buildAbortOperation(TransactionId{5}),
                ReplicationOptions{})
            .get();
    EXPECT_TRUE(res.ok()) << res.result();
    leaderState->release(TransactionId{5}, res.get());

    res = leaderState
              ->replicateOperation(
                  ReplicatedOperation::buildCommitOperation(TransactionId{9}),
                  ReplicationOptions{})
              .get();
    EXPECT_TRUE(res.ok()) << res.result();
    leaderState->release(TransactionId{9}, res.get());
  }
  EXPECT_EQ(1U, leaderState->getActiveTransactionsCount());

  // resigning should abort the remaining transaction with ID 13
  EXPECT_CALL(transactionManagerMock,
              abortManagedTrx(TransactionId{13}, globalId.database))
      .Times(1);

  // resigning should abort all ongoing transactions
  EXPECT_CALL(
      *transactionHandlerMock,
      applyEntry(ReplicatedOperation::buildAbortAllOngoingTrxOperation()))
      .Times(1);

  std::ignore = std::move(*leaderState).resign();
  Mock::VerifyAndClearExpectations(&transactionManagerMock);
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
}

TEST_F(DocumentStateMachineTest,
       recoverEntries_should_abort_remaining_active_transactions) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      DocumentLogEntry{ReplicatedOperation::buildCreateShardOperation(
          shardId, collectionId, std::make_shared<VPackBuilder>())});
  // Transaction IDs are of follower type, as if they were replicated.
  entries.emplace_back(
      createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{6}));
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{10}));
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{14}));
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildAbortOperation(TransactionId{6})});
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildCommitOperation(TransactionId{10})});

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));
  auto stream = std::make_shared<MockProducerStream>();

  leaderState->setStream(stream);
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  EXPECT_CALL(*stream, insert).WillOnce([&](auto const& entry) {
    EXPECT_EQ(entry.operation,
              ReplicatedOperation::buildAbortAllOngoingTrxOperation());
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
       recoverEntries_should_abort_transactions_before_dropping_shard) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  ON_CALL(*transactionHandlerMock, getTransactionsForShard(shardId))
      .WillByDefault(Return(std::vector<TransactionId>{
          TransactionId{6}, TransactionId{10}, TransactionId{14}}));

  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{6}));
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{10}));
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{14}));
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildDropShardOperation(shardId, collectionId)});

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));
  auto stream = std::make_shared<MockProducerStream>();

  leaderState->setStream(stream);
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  EXPECT_CALL(*stream, insert).Times(1);
  EXPECT_CALL(*transactionMock, abort).Times(3);
  leaderState->recoverEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(transactionMock.get());
}

TEST_F(DocumentStateMachineTest,
       leader_recoverEntries_dies_if_transaction_is_invalid) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  ON_CALL(*transactionHandlerMock,
          validate(Matcher<ReplicatedOperation::OperationType const&>(_)))
      .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));

  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{10}));

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));
  auto stream = std::make_shared<MockProducerStream>();

  leaderState->setStream(stream);
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);

  ASSERT_DEATH_CORE_FREE(leaderState->recoverEntries(std::move(entryIterator)),
                         "");
}

TEST_F(DocumentStateMachineTest,
       leader_should_not_replicate_unknown_transactions) {
  using namespace testing;

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));

  auto operation = ReplicatedOperation::buildCommitOperation(TransactionId{5});
  EXPECT_FALSE(leaderState->needsReplication(operation));

  operation = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{5}, shardId,
      velocypack::SharedSlice());
  EXPECT_TRUE(leaderState->needsReplication(operation));

  operation = ReplicatedOperation::buildCommitOperation(TransactionId{5});
  EXPECT_FALSE(leaderState->needsReplication(operation));
}

TEST_F(DocumentStateMachineTest,
       leader_ignores_invalid_transactions_during_recovery) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));
  auto stream = std::make_shared<MockProducerStream>();

  leaderState->setStream(stream);

  // Try to apply a regular entry, but pretend the shard is not available
  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT, TransactionId{6}));
  auto entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  ON_CALL(*shardHandlerMock, isShardAvailable(shardId))
      .WillByDefault(Return(false));

  EXPECT_CALL(*stream, insert).Times(1);  // AbortAllOngoingTrx
  EXPECT_CALL(*stream, release).Times(1);
  EXPECT_CALL(*shardHandlerMock, isShardAvailable(shardId)).Times(1);
  EXPECT_CALL(*transactionMock, apply(entries[0].getInnerOperation())).Times(0);
  leaderState->recoverEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
  Mock::VerifyAndClearExpectations(transactionMock.get());
  Mock::VerifyAndClearExpectations(stream.get());
  ON_CALL(*shardHandlerMock, isShardAvailable(shardId))
      .WillByDefault(Return(true));

  // Try to commit the previous entry
  entries.clear();
  entries.emplace_back(DocumentLogEntry{
      ReplicatedOperation::buildCommitOperation(TransactionId{6})});
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*stream, insert).Times(1);  // AbortAllOngoingTrx
  EXPECT_CALL(*stream, release).Times(1);
  EXPECT_CALL(*shardHandlerMock, isShardAvailable(shardId)).Times(0);
  EXPECT_CALL(*transactionMock, commit()).Times(0);
  leaderState->recoverEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
  Mock::VerifyAndClearExpectations(transactionMock.get());
  Mock::VerifyAndClearExpectations(stream.get());

  // Try to apply another entry, this time making the shard available
  entries.clear();
  entries.emplace_back(createDocumentEntry(TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                           TransactionId{10}));
  entryIterator = std::make_unique<DocumentLogEntryIterator>(entries);
  EXPECT_CALL(*stream, insert).Times(1);  // AbortAllOngoingTrx
  EXPECT_CALL(*stream, release).Times(1);
  EXPECT_CALL(transactionManagerMock, abortManagedTrx(_, _)).Times(1);
  EXPECT_CALL(*shardHandlerMock, isShardAvailable(shardId)).Times(1);
  EXPECT_CALL(*transactionMock, apply(entries[0].getInnerOperation())).Times(1);
  leaderState->recoverEntries(std::move(entryIterator));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
  Mock::VerifyAndClearExpectations(transactionMock.get());
  Mock::VerifyAndClearExpectations(stream.get());
}

TEST_F(DocumentStateMachineTest, leader_create_and_drop_shard) {
  using namespace testing;

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

  auto core = factory.constructCore(vocbaseMock, globalId, coreParams);
  auto leaderState = factory.constructLeader(std::move(core));
  auto stream = std::make_shared<testing::NiceMock<MockProducerStream>>();
  leaderState->setStream(stream);

  auto builder = std::make_shared<VPackBuilder>();

  EXPECT_CALL(*stream, insert)
      .Times(1)
      .WillOnce([&](DocumentLogEntry const& entry) {
        EXPECT_EQ(entry.operation,
                  ReplicatedOperation::buildCreateShardOperation(
                      shardId, collectionId, builder));
        return LogIndex{12};
      });

  EXPECT_CALL(*stream, waitFor(LogIndex{12})).Times(1).WillOnce([](auto) {
    return futures::Future<MockProducerStream::WaitForResult>{
        MockProducerStream::WaitForResult{}};
  });

  EXPECT_CALL(*stream, release(LogIndex{12})).Times(1);

  EXPECT_CALL(*shardHandlerMock, ensureShard(shardId, collectionId, _))
      .Times(1);

  auto res = leaderState->createShard(shardId, collectionId, builder).get();
  EXPECT_TRUE(res.ok()) << res;

  Mock::VerifyAndClearExpectations(stream.get());
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  EXPECT_CALL(*stream, insert)
      .Times(1)
      .WillOnce([&](DocumentLogEntry const& entry) {
        EXPECT_EQ(entry.operation, ReplicatedOperation::buildDropShardOperation(
                                       shardId, collectionId));
        return LogIndex{12};
      });

  EXPECT_CALL(*stream, waitFor(LogIndex{12})).Times(1).WillOnce([](auto) {
    return futures::Future<MockProducerStream::WaitForResult>{
        MockProducerStream::WaitForResult{}};
  });

  EXPECT_CALL(*stream, release(LogIndex{12})).Times(1);

  EXPECT_CALL(*shardHandlerMock, dropShard(shardId)).Times(1);

  leaderState->dropShard(shardId, collectionId);
}

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

  ASSERT_EQ(activeTrx.getReleaseIndex(), std::nullopt);
  activeTrx.markAsActive(TransactionId{100}, LogIndex{100});
  ASSERT_EQ(activeTrx.getTransactions().size(), 1);
  activeTrx.markAsInactive(TransactionId{100});
  ASSERT_EQ(activeTrx.getTransactions().size(), 0);
  ASSERT_EQ(activeTrx.getReleaseIndex(), std::nullopt);

  activeTrx.markAsActive(TransactionId{200}, LogIndex{200});
  activeTrx.markAsActive(TransactionId{300}, LogIndex{300});
  activeTrx.markAsActive(TransactionId{400}, LogIndex{400});
  ASSERT_EQ(activeTrx.getTransactions().size(), 3);

  activeTrx.markAsInactive(TransactionId{200});
  ASSERT_EQ(activeTrx.getReleaseIndex(), LogIndex{299});
  activeTrx.markAsInactive(TransactionId{400});
  ASSERT_EQ(activeTrx.getReleaseIndex(), LogIndex{299});
  activeTrx.markAsInactive(TransactionId{300});
  ASSERT_EQ(activeTrx.getReleaseIndex(), std::nullopt);

  activeTrx.markAsActive(TransactionId{500}, LogIndex{500});
  ASSERT_EQ(activeTrx.getTransactions().size(), 1);
  activeTrx.clear();
  ASSERT_EQ(activeTrx.getTransactions().size(), 0);

  activeTrx.markAsActive(LogIndex{600});
  ASSERT_EQ(activeTrx.getReleaseIndex(), LogIndex{599});
  activeTrx.markAsActive(TransactionId{700}, LogIndex{700});
  ASSERT_EQ(activeTrx.getReleaseIndex(), LogIndex{599});
  activeTrx.markAsActive(LogIndex{800});
  ASSERT_EQ(activeTrx.getReleaseIndex(), LogIndex{599});
  activeTrx.markAsInactive(LogIndex{800});
  ASSERT_EQ(activeTrx.getReleaseIndex(), LogIndex{599});
  activeTrx.markAsInactive(LogIndex{600});
  ASSERT_EQ(activeTrx.getReleaseIndex(), LogIndex{699});
  activeTrx.markAsInactive(TransactionId{700});
  ASSERT_EQ(activeTrx.getReleaseIndex(), std::nullopt);
}

TEST(ActiveTransactionsQueueTest, test_activeTransactions_death) {
  auto activeTrx = ActiveTransactionsQueue{};
  activeTrx.markAsActive(TransactionId{100}, LogIndex{100});
  ASSERT_DEATH_CORE_FREE(activeTrx.markAsActive(LogIndex{99}), "");
}
