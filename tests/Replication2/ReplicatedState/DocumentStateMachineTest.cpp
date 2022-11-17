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

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "Inspection/VPack.h"
#include "Replication2/ReplicatedLog/TestHelper.h"
#include "Replication2/Mocks/DocumentStateMocks.h"
#include "Replication2/Mocks/MockStatePersistorInterface.h"

#include "velocypack/Value.h"

#include <thread>
#include <vector>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;
using namespace arangodb::replication2::test;

struct DocumentStateMachineTest : test::ReplicatedLogTest {
  DocumentStateMachineTest() {
    feature->registerStateType<DocumentState>(std::string{DocumentState::NAME},
                                              handlersFactoryMock,
                                              transactionManagerMock);
  }

  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();

  std::shared_ptr<MockStatePersistorInterface> statePersistor =
      std::make_shared<MockStatePersistorInterface>();

  std::vector<std::string> collectionData;
  std::shared_ptr<testing::NiceMock<MockCollectionReader>>
      collectionReaderMock =
          std::make_shared<testing::NiceMock<MockCollectionReader>>(
              collectionData);
  std::shared_ptr<testing::NiceMock<MockCollectionReaderFactory>>
      collectionReaderFactoryMock =
          std::make_shared<testing::NiceMock<MockCollectionReaderFactory>>(
              collectionReaderMock);

  std::shared_ptr<testing::NiceMock<MockDocumentStateTransaction>>
      transactionMock =
          std::make_shared<testing::NiceMock<MockDocumentStateTransaction>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateAgencyHandler>>
      agencyHandlerMock =
          std::make_shared<testing::NiceMock<MockDocumentStateAgencyHandler>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateShardHandler>>
      shardHandlerMock =
          std::make_shared<testing::NiceMock<MockDocumentStateShardHandler>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateNetworkHandler>>
      networkHandlerMock = std::make_shared<
          testing::NiceMock<MockDocumentStateNetworkHandler>>();
  std::shared_ptr<testing::NiceMock<MockDocumentStateLeaderInterface>>
      leaderInterfaceMock = std::make_shared<
          testing::NiceMock<MockDocumentStateLeaderInterface>>();

  MockTransactionManager transactionManagerMock;

  const std::string collectionId = "testCollectionID";
  const LogId logId = LogId{1};
  const std::string dbName = "testDB";
  const GlobalLogIdentifier globalId{dbName, logId};
  const ShardID shardId = DocumentStateShardHandler::stateIdToShardId(logId);
  const document::DocumentCoreParameters coreParams{collectionId, dbName};

  std::shared_ptr<testing::NiceMock<MockDocumentStateHandlersFactory>>
      handlersFactoryMock =
          std::make_shared<testing::NiceMock<MockDocumentStateHandlersFactory>>(
              collectionReaderFactoryMock);

  void SetUp() override {
    using namespace testing;

    collectionData.emplace_back("foo");
    collectionData.emplace_back("bar");
    collectionData.emplace_back("baz");

    ON_CALL(*collectionReaderFactoryMock, createCollectionReader)
        .WillByDefault([&]() {
          return ResultT<std::unique_ptr<ICollectionReader>>::success(
              std::make_unique<MockCollectionReaderDelegator>(
                  collectionReaderMock));
        });

    ON_CALL(*transactionMock, commit).WillByDefault(Return(Result{}));
    ON_CALL(*transactionMock, abort).WillByDefault(Return(Result{}));
    ON_CALL(*transactionMock, apply(_)).WillByDefault([]() {
      return OperationResult{Result{}, OperationOptions{}};
    });
    ON_CALL(*transactionMock, intermediateCommit)
        .WillByDefault(Return(Result{}));

    ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&](LogIndex) {
      return futures::Future<ResultT<SnapshotBatch>>{
          std::in_place, SnapshotBatch{SnapshotId{1}, shardId}};
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

    ON_CALL(*agencyHandlerMock, getCollectionPlan)
        .WillByDefault(Return(std::make_shared<VPackBuilder>()));
    ON_CALL(*agencyHandlerMock, reportShardInCurrent)
        .WillByDefault(Return(Result{}));

    ON_CALL(*handlersFactoryMock, createAgencyHandler)
        .WillByDefault(Return(agencyHandlerMock));

    ON_CALL(*handlersFactoryMock, createShardHandler)
        .WillByDefault([&](GlobalLogIdentifier const& gid) {
          ON_CALL(*shardHandlerMock, createLocalShard)
              .WillByDefault(Return(ResultT<std::string>::success(
                  DocumentStateShardHandler::stateIdToShardId(gid.id))));
          return shardHandlerMock;
        });

    ON_CALL(*handlersFactoryMock, createTransactionHandler)
        .WillByDefault([&](GlobalLogIdentifier gid) {
          return std::make_unique<DocumentStateTransactionHandler>(
              std::move(gid), std::make_unique<MockDatabaseGuard>(),
              handlersFactoryMock);
        });

    ON_CALL(*handlersFactoryMock, createSnapshotHandler)
        .WillByDefault([&](GlobalLogIdentifier const& gid) {
          return std::make_unique<DocumentStateSnapshotHandler>(
              handlersFactoryMock->makeUniqueCollectionReaderFactory());
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
    Mock::VerifyAndClearExpectations(agencyHandlerMock.get());
    Mock::VerifyAndClearExpectations(shardHandlerMock.get());
    Mock::VerifyAndClearExpectations(transactionMock.get());
    Mock::VerifyAndClearExpectations(networkHandlerMock.get());
    Mock::VerifyAndClearExpectations(leaderInterfaceMock.get());
    Mock::VerifyAndClearExpectations(collectionReaderMock.get());
    Mock::VerifyAndClearExpectations(collectionReaderFactoryMock.get());
  }
};

TEST_F(DocumentStateMachineTest,
       constructing_the_core_creates_shard_successfully) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);

  EXPECT_CALL(*agencyHandlerMock, getCollectionPlan(collectionId)).Times(1);
  EXPECT_CALL(*agencyHandlerMock,
              reportShardInCurrent(collectionId, shardId, _))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, createLocalShard(collectionId, _)).Times(1);
  auto core = factory.constructCore(globalId, coreParams);

  Mock::VerifyAndClearExpectations(agencyHandlerMock.get());
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  EXPECT_EQ(core->getShardId(), shardId);
  EXPECT_EQ(core->getGid().database, dbName);
  EXPECT_EQ(core->getGid().id, logId);
}

TEST_F(DocumentStateMachineTest, shard_is_dropped_during_cleanup) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto core = factory.constructCore(globalId, coreParams);
  EXPECT_CALL(*shardHandlerMock, dropLocalShard(collectionId)).Times(1);
  auto cleanupHandler = factory.constructCleanupHandler();
  cleanupHandler->drop(std::move(core));
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());
}

TEST_F(DocumentStateMachineTest, snapshot_has_valid_ongoing_state) {
  using namespace testing;

  EXPECT_CALL(*collectionReaderMock, getDocCount()).Times(1);
  auto snapshot = Snapshot(
      SnapshotId{12345}, shardId,
      std::make_unique<MockCollectionReaderDelegator>(collectionReaderMock));
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());

  auto status = snapshot.status();
  ASSERT_TRUE(std::holds_alternative<state::Ongoing>(status.state));
  EXPECT_EQ(status.shardId, shardId);
  EXPECT_EQ(status.totalDocs, collectionReaderMock->getDocCount());
  EXPECT_EQ(status.docsSent, 0);
  EXPECT_EQ(status.batchesSent, 0);
  EXPECT_EQ(status.bytesSent, 0);
}

TEST_F(DocumentStateMachineTest, snapshot_fetch_from_ongoing_state) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
  auto snapshot = Snapshot(
      snapshotId, shardId,
      std::make_unique<MockCollectionReaderDelegator>(collectionReaderMock));
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
    ASSERT_TRUE(std::holds_alternative<state::Ongoing>(status.state));
    EXPECT_EQ(status.docsSent, idx + 1);
    EXPECT_EQ(status.batchesSent, idx + 1);

    bytesSent += batch.payload.byteSize();
    EXPECT_EQ(status.bytesSent, bytesSent);
  }
}

TEST_F(DocumentStateMachineTest, snapshot_try_fetch_after_finish) {
  using namespace testing;

  auto snapshotId = SnapshotId{12345};
  auto snapshot = Snapshot(
      snapshotId, shardId,
      std::make_unique<MockCollectionReaderDelegator>(collectionReaderMock));

  auto res = snapshot.finish();
  ASSERT_TRUE(res.ok()) << res;

  auto status = snapshot.status();
  ASSERT_TRUE(std::holds_alternative<state::Finished>(status.state));

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
      snapshotId, shardId,
      std::make_unique<MockCollectionReaderDelegator>(collectionReaderMock));

  auto res = snapshot.abort();
  ASSERT_TRUE(res.ok()) << res;

  auto status = snapshot.status();
  ASSERT_TRUE(std::holds_alternative<state::Aborted>(status.state));

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
      snapshotId, shardId,
      std::make_unique<MockCollectionReaderDelegator>(collectionReaderMock));

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
      snapshotId, shardId,
      std::make_unique<MockCollectionReaderDelegator>(collectionReaderMock));

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
      handlersFactoryMock->makeUniqueCollectionReaderFactory());
  EXPECT_CALL(*collectionReaderFactoryMock, createCollectionReader)
      .WillOnce([]() {
        return ResultT<std::unique_ptr<ICollectionReader>>::error(
            TRI_ERROR_WAS_ERLAUBE);
      });
  auto res = snapshotHandler.create(shardId);
  ASSERT_TRUE(res.fail());
  Mock::VerifyAndClearExpectations(collectionReaderMock.get());
}

TEST_F(DocumentStateMachineTest, snapshot_handler_cannot_find_snapshot) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueCollectionReaderFactory());
  auto res = snapshotHandler.find(SnapshotId::create());
  ASSERT_TRUE(res.fail());
}

TEST_F(DocumentStateMachineTest,
       snapshot_handler_create_and_find_successfully_then_clear) {
  using namespace testing;

  auto snapshotHandler = DocumentStateSnapshotHandler(
      handlersFactoryMock->makeUniqueCollectionReaderFactory());

  auto res = snapshotHandler.create(shardId);
  ASSERT_TRUE(res.ok()) << res.result();

  auto snapshot = res.get().lock();
  auto status = snapshot->status();
  EXPECT_TRUE(std::holds_alternative<state::Ongoing>(status.state));

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
    test__transactionHandler_ensureTransaction_creates_new_transaction_only_once) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}},
      std::make_unique<MockDatabaseGuard>(), handlersFactoryMock);

  auto tid = TransactionId{6};
  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), tid};

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
      GlobalLogIdentifier{"testDb", LogId{1}},
      std::make_unique<MockDatabaseGuard>(), handlersFactoryMock);

  auto tid = TransactionId{6};
  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), tid};
  auto trx = transactionHandler.ensureTransaction(doc);
  EXPECT_EQ(transactionHandler.getUnfinishedTransactions().size(), 1);
  transactionHandler.removeTransaction(tid);
  EXPECT_TRUE(transactionHandler.getUnfinishedTransactions().empty());
}

TEST_F(DocumentStateMachineTest,
       test_transactionHandler_applyEntry_abortAll_clears_everything) {
  using namespace testing;

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}},
      std::make_unique<MockDatabaseGuard>(), handlersFactoryMock);

  auto tid = TransactionId{6};
  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), tid};
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
      GlobalLogIdentifier{"testDb", LogId{1}},
      std::make_unique<MockDatabaseGuard>(), handlersFactoryMock);

  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), TransactionId{6}};

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
      GlobalLogIdentifier{"testDb", LogId{1}},
      std::make_unique<MockDatabaseGuard>(), handlersFactoryMock);

  // Start a new transaction and then abort it.
  auto doc = DocumentLogEntry{"s1234", OperationType::kRemove,
                              velocypack::SharedSlice(), TransactionId{10}};
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
      GlobalLogIdentifier{"testDb", LogId{1}},
      std::make_unique<MockDatabaseGuard>(), handlersFactoryMock);
  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), TransactionId{6}};

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
       shard_is_created_along_with_the_replicated_state) {
  using namespace testing;

  auto leaderLog = makeReplicatedLog(globalId);
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {}, 1);
  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, leaderLog,
                                         statePersistor));
  ASSERT_NE(leaderReplicatedState, nullptr);

  EXPECT_CALL(*agencyHandlerMock, getCollectionPlan(collectionId)).Times(1);
  EXPECT_CALL(*agencyHandlerMock,
              reportShardInCurrent(collectionId, shardId, _))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, createLocalShard(collectionId, _)).Times(1);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());

  // Very methods called during core construction
  Mock::VerifyAndClearExpectations(agencyHandlerMock.get());
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  auto leaderState = leaderReplicatedState->getLeader();
  ASSERT_NE(leaderState, nullptr);
  ASSERT_EQ(leaderState->shardId, shardId);
}

TEST_F(
    DocumentStateMachineTest,
    follower_acquire_snapshot_truncates_collection_and_calls_leader_interface) {
  using namespace testing;

  auto transactionHandlerMock =
      handlersFactoryMock->makeRealTransactionHandler(globalId);
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_))
      .WillByDefault([&](GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateTransactionHandler>>(
            transactionHandlerMock);
      });

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(globalId, coreParams), handlersFactoryMock);

  // 1 truncate, 2 inserts and 3 commits
  EXPECT_CALL(*transactionHandlerMock, applyEntry(_)).Times(6);

  EXPECT_CALL(*networkHandlerMock, getLeaderInterface("participantId"))
      .Times(1);

  ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&](LogIndex) {
    return futures::Future<ResultT<SnapshotBatch>>{
        std::in_place,
        SnapshotBatch{
            .snapshotId = SnapshotId{1}, .shardId = shardId, .hasMore = true}};
  });

  EXPECT_CALL(*leaderInterfaceMock, startSnapshot(LogIndex{1})).Times(1);
  EXPECT_CALL(*leaderInterfaceMock, nextSnapshotBatch(SnapshotId{1})).Times(1);
  EXPECT_CALL(*leaderInterfaceMock, finishSnapshot(SnapshotId{1})).Times(1);

  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().ok());
  Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
  Mock::VerifyAndClearExpectations(networkHandlerMock.get());
  Mock::VerifyAndClearExpectations(leaderInterfaceMock.get());
}

TEST_F(DocumentStateMachineTest, follower_acquire_snapshot_truncation_fails) {
  using namespace testing;

  auto transactionHandlerMock =
      handlersFactoryMock->makeRealTransactionHandler(globalId);
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_))
      .WillByDefault([&](GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateTransactionHandler>>(
            transactionHandlerMock);
      });

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(globalId, coreParams), handlersFactoryMock);

  ON_CALL(*transactionHandlerMock, applyEntry(_))
      .WillByDefault(Return(TRI_ERROR_WAS_ERLAUBE));
  auto res = follower->acquireSnapshot("participantId", LogIndex{1});
  EXPECT_TRUE(res.isReady() && res.get().fail() &&
              res.get().errorNumber() == TRI_ERROR_WAS_ERLAUBE);
}

TEST_F(DocumentStateMachineTest,
       follower_resigning_while_acquiring_snapshot_concurrently) {
  using namespace testing;

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto follower = std::make_shared<DocumentFollowerStateWrapper>(
      factory.constructCore(globalId, coreParams), handlersFactoryMock);

  std::atomic<bool> acquireSnapshotCalled = false;

  // The snapshot will not stop until the follower resigns
  ON_CALL(*leaderInterfaceMock, startSnapshot).WillByDefault([&](LogIndex) {
    acquireSnapshotCalled.store(true);
    acquireSnapshotCalled.notify_one();
    return futures::Future<ResultT<SnapshotBatch>>{
        std::in_place,
        SnapshotBatch{
            .snapshotId = SnapshotId{1}, .shardId = shardId, .hasMore = true}};
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
    EXPECT_TRUE(res.isReady() && res.get().fail() &&
                res.get().errorNumber() ==
                    TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED);
  });

  acquireSnapshotCalled.wait(false);
  std::move(*follower).resign();
  t.join();
}

TEST_F(DocumentStateMachineTest, leader_manipulates_snapshot_successfully) {
  using namespace testing;

  auto snapshotHandler = handlersFactoryMock->makeRealSnapshotHandler();
  ON_CALL(*handlersFactoryMock, createSnapshotHandler(_))
      .WillByDefault([&](GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateSnapshotHandler>>(
            snapshotHandler);
      });

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto leader = std::make_shared<DocumentLeaderStateWrapper>(
      factory.constructCore(globalId, coreParams), handlersFactoryMock,
      transactionManagerMock);

  EXPECT_CALL(*snapshotHandler, create(shardId)).Times(1);
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
  ON_CALL(*handlersFactoryMock, createSnapshotHandler(_))
      .WillByDefault([&](GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateSnapshotHandler>>(
            snapshotHandler);
      });
  ON_CALL(*snapshotHandler, create(shardId))
      .WillByDefault(Return(
          ResultT<std::weak_ptr<Snapshot>>::error(TRI_ERROR_WAS_ERLAUBE)));
  ON_CALL(*snapshotHandler, find(SnapshotId{1}))
      .WillByDefault(Return(
          ResultT<std::weak_ptr<Snapshot>>::error(TRI_ERROR_WAS_ERLAUBE)));

  auto factory = DocumentFactory(handlersFactoryMock, transactionManagerMock);
  auto leader = std::make_shared<DocumentLeaderStateWrapper>(
      factory.constructCore(globalId, coreParams), handlersFactoryMock,
      transactionManagerMock);

  EXPECT_TRUE(leader->snapshotStart(SnapshotParams::Start{LogIndex{1}}).fail());
  EXPECT_TRUE(leader->snapshotNext(SnapshotParams::Next{SnapshotId{1}}).fail());
  EXPECT_TRUE(
      leader->snapshotFinish(SnapshotParams::Finish{SnapshotId{1}}).fail());
  EXPECT_TRUE(leader->snapshotStatus(SnapshotId{1}).fail());
}

TEST_F(DocumentStateMachineTest,
       leader_resign_should_abort_active_transactions) {
  using namespace testing;

  auto leaderLog = makeReplicatedLog(globalId);
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {}, 1);
  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, leaderLog,
                                         statePersistor));
  ASSERT_NE(leaderReplicatedState, nullptr);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());

  auto leaderState = leaderReplicatedState->getLeader();
  ASSERT_NE(leaderState, nullptr);

  {
    VPackBuilder builder;
    builder.openObject();
    builder.close();

    auto operation = OperationType::kInsert;
    std::ignore =
        leaderState->replicateOperation(builder.sharedSlice(), operation,
                                        TransactionId{5}, ReplicationOptions{});
    std::ignore =
        leaderState->replicateOperation(builder.sharedSlice(), operation,
                                        TransactionId{9}, ReplicationOptions{});
    std::ignore = leaderState->replicateOperation(builder.sharedSlice(),
                                                  operation, TransactionId{13},
                                                  ReplicationOptions{});
  }
  EXPECT_EQ(3, leaderState->getActiveTransactionsCount());

  {
    VPackBuilder builder;
    std::ignore = leaderState->replicateOperation(
        builder.sharedSlice(), OperationType::kAbort, TransactionId{5},
        ReplicationOptions{});
    std::ignore = leaderState->replicateOperation(
        builder.sharedSlice(), OperationType::kCommit, TransactionId{9},
        ReplicationOptions{});
  }
  EXPECT_EQ(1, leaderState->getActiveTransactionsCount());

  EXPECT_CALL(transactionManagerMock,
              abortManagedTrx(TransactionId{13}, globalId.database))
      .Times(1);

  // resigning as leader should abort the remaining transaction with id 13
  std::ignore = leaderLog->becomeFollower("leader", LogTerm{2}, "dummy");
  Mock::VerifyAndClearExpectations(&transactionManagerMock);
}

TEST_F(DocumentStateMachineTest,
       recoverEntries_should_abort_remaining_active_transactions) {
  using namespace testing;

  std::vector<PersistingLogEntry> entries;

  auto addEntry = [&entries, this](OperationType op, TransactionId trxId) {
    VPackBuilder builder;
    builder.openObject();
    builder.close();
    auto entry = DocumentLogEntry{shardId, op, builder.sharedSlice(), trxId};

    builder.clear();
    builder.openArray();
    builder.add(VPackValue(1));
    velocypack::serialize(builder, entry);
    builder.close();

    entries.emplace_back(LogTerm{1}, LogIndex{entries.size() + 1},
                         LogPayload::createFromSlice(builder.slice()));
  };

  // Transaction IDs are of follower type, as if they were replicated.
  addEntry(OperationType::kInsert, TransactionId{6});
  addEntry(OperationType::kInsert, TransactionId{10});
  addEntry(OperationType::kInsert, TransactionId{14});
  addEntry(OperationType::kAbort, TransactionId{6});
  addEntry(OperationType::kCommit, TransactionId{10});

  auto core = makeLogCore<MockLog>(globalId);
  auto it = test::make_iterator(entries);
  core->insert(*it, true);

  auto leaderLog = std::make_shared<TestReplicatedLog>(
      std::move(core), _logMetricsMock, _optionsMock,
      LoggerContext(Logger::REPLICATION2));

  auto leader = leaderLog->becomeLeader("leader", LogTerm{2}, {}, 1);
  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, leaderLog,
                                         statePersistor));
  ASSERT_NE(leaderReplicatedState, nullptr);

  EXPECT_CALL(*transactionMock, apply).Times(3);
  EXPECT_CALL(*transactionMock, commit).Times(1);
  EXPECT_CALL(*transactionMock, abort).Times(1);

  // The leader adds a tombstone for the remaining transaction.
  EXPECT_CALL(transactionManagerMock,
              abortManagedTrx(TransactionId{14}.asLeaderTransactionId(),
                              globalId.database))
      .Times(1);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());
  Mock::VerifyAndClearExpectations(&transactionManagerMock);
  Mock::VerifyAndClearExpectations(transactionMock.get());
}

/*
 * This is a larger test designed to check the overall integration between
 * leader and follower. It also servers as an example of how to use the document
 * state in other unit tests.
 */
TEST_F(DocumentStateMachineTest, leader_follower_integration) {
  using namespace testing;

  auto followerLog = makeReplicatedLog(logId);
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(logId);
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);
  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, leaderLog,
                                         statePersistor));
  ASSERT_NE(leaderReplicatedState, nullptr);

  EXPECT_CALL(*agencyHandlerMock, getCollectionPlan(collectionId)).Times(1);
  EXPECT_CALL(*agencyHandlerMock,
              reportShardInCurrent(collectionId, shardId, _))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, createLocalShard(collectionId, _)).Times(1);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());

  // Very methods called during core construction
  Mock::VerifyAndClearExpectations(agencyHandlerMock.get());
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  follower->runAllAsyncAppendEntries();
  auto leaderState = leaderReplicatedState->getLeader();
  ASSERT_NE(leaderState, nullptr);
  ASSERT_EQ(leaderState->shardId, shardId);

  // During leader recovery, all ongoing transactions must be aborted
  auto inMemoryLog = leader->copyInMemoryLog();
  auto lastIndex = inMemoryLog.getLastIndex();
  auto entry = inMemoryLog.getEntryByIndex(lastIndex);
  auto doc = velocypack::deserialize<DocumentLogEntry>(
      entry->entry().logPayload()->slice()[1]);
  ASSERT_EQ(doc.operation, OperationType::kAbortAllOngoingTrx);

  auto followerReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, followerLog,
                                         statePersistor));
  ASSERT_NE(followerReplicatedState, nullptr);

  auto transactionHandlerMock =
      handlersFactoryMock->makeRealTransactionHandler(globalId);
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_))
      .WillByDefault([&](GlobalLogIdentifier const& gid) {
        return std::make_unique<NiceMock<MockDocumentStateTransactionHandler>>(
            transactionHandlerMock);
      });

  EXPECT_CALL(*agencyHandlerMock, getCollectionPlan(collectionId)).Times(1);
  EXPECT_CALL(*agencyHandlerMock,
              reportShardInCurrent(collectionId, shardId, _))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, createLocalShard(collectionId, _)).Times(1);
  followerReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());

  // Very methods called during core construction
  Mock::VerifyAndClearExpectations(agencyHandlerMock.get());
  Mock::VerifyAndClearExpectations(shardHandlerMock.get());

  auto followerState = followerReplicatedState->getFollower();
  ASSERT_NE(followerState, nullptr);

  // Insert a document
  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add("document1_key", "document1_value");
    ob->close();

    auto operation = OperationType::kInsert;
    auto tid = TransactionId{5};
    auto res = leaderState->replicateOperation(builder.sharedSlice(), operation,
                                               tid, ReplicationOptions{});

    ASSERT_TRUE(res.isReady());
    auto logIndex = res.result().get();

    inMemoryLog = leader->copyInMemoryLog();
    entry = inMemoryLog.getEntryByIndex(logIndex);
    doc = velocypack::deserialize<DocumentLogEntry>(
        entry->entry().logPayload()->slice()[1]);
    EXPECT_EQ(doc.shardId, shardId);
    EXPECT_EQ(doc.operation, operation);
    EXPECT_EQ(doc.tid, tid.asFollowerTransactionId());
    EXPECT_EQ(doc.data.get("document1_key").stringView(), "document1_value");

    EXPECT_CALL(*transactionHandlerMock, applyEntry(_)).Times(1);
    EXPECT_CALL(*transactionMock, apply).Times(1);
    follower->runAllAsyncAppendEntries();
    Mock::VerifyAndClearExpectations(transactionMock.get());
    Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
  }

  // Insert another document, but fail with UNIQUE_CONSTRAINT_VIOLATED. The
  // follower should continue.
  builder.clear();
  {
    VPackObjectBuilder ob(&builder);
    builder.add("document2_key", "document2_value");
    ob->close();

    auto operation = OperationType::kInsert;
    auto tid = TransactionId{5};
    auto res = leaderState->replicateOperation(builder.sharedSlice(), operation,
                                               tid, ReplicationOptions{});

    ASSERT_TRUE(res.isReady());
    auto logIndex = res.result().get();

    inMemoryLog = leader->copyInMemoryLog();
    entry = inMemoryLog.getEntryByIndex(logIndex);
    doc = velocypack::deserialize<DocumentLogEntry>(
        entry->entry().logPayload()->slice()[1]);
    EXPECT_EQ(doc.shardId, shardId);
    EXPECT_EQ(doc.operation, operation);
    EXPECT_EQ(doc.tid, tid.asFollowerTransactionId());
    EXPECT_EQ(doc.data.get("document2_key").stringView(), "document2_value");

    EXPECT_CALL(*transactionHandlerMock, applyEntry(_)).Times(1);
    EXPECT_CALL(*transactionMock, apply(_))
        .WillOnce([](DocumentLogEntry const& entry) {
          auto opRes = OperationResult{Result{}, OperationOptions{}};
          opRes.countErrorCodes[TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED] =
              1;
          return opRes;
        });
    follower->runAllAsyncAppendEntries();
    Mock::VerifyAndClearExpectations(transactionMock.get());
    Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
  }

  // Commit
  {
    auto operation = OperationType::kCommit;
    auto tid = TransactionId{5};
    auto res = leaderState->replicateOperation(
        velocypack::SharedSlice{}, operation, tid,
        ReplicationOptions{.waitForCommit = true});

    ASSERT_FALSE(res.isReady());

    EXPECT_CALL(*transactionHandlerMock, applyEntry(_)).Times(1);
    EXPECT_CALL(*transactionMock, commit).Times(1);
    follower->runAllAsyncAppendEntries();
    Mock::VerifyAndClearExpectations(transactionMock.get());
    Mock::VerifyAndClearExpectations(transactionHandlerMock.get());
    ASSERT_TRUE(res.isReady());
    auto logIndex = res.result().get();

    inMemoryLog = follower->copyInMemoryLog();
    entry = inMemoryLog.getEntryByIndex(logIndex);
    doc = velocypack::deserialize<DocumentLogEntry>(
        entry->entry().logPayload()->slice()[1]);
    EXPECT_EQ(doc.shardId, shardId);
    EXPECT_EQ(doc.operation, operation);
    EXPECT_EQ(doc.tid, tid.asFollowerTransactionId());
    EXPECT_TRUE(doc.data.isNone());
  }
}

TEST(SnapshotIdTest, parse_snapshot_id_successfully) {
  auto id = SnapshotId::fromString("12345");
  ASSERT_TRUE(id.has_value());
  ASSERT_EQ(id->id(), 12345);
  ASSERT_EQ(to_string(*id), "12345");
}

TEST(SnapshotIdTest, parse_snapshot_id_error) {
  auto id = SnapshotId::fromString("#!@#abcd");
  ASSERT_FALSE(id.has_value());
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
