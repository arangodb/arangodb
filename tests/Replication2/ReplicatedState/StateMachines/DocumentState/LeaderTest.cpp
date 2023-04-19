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

#include "Mocks/Death_Test.h"
#include "Replication2/ReplicatedState/StateMachines/DocumentState/DocumentStateMachineTest.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::replication2;
using namespace arangodb::replication2::tests;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;

struct DocumentStateLeaderTest : DocumentStateMachineTest {};

TEST_F(DocumentStateLeaderTest, leader_manipulates_snapshot_successfully) {
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

TEST_F(DocumentStateLeaderTest, leader_manipulates_snapshots_with_errors) {
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

TEST_F(DocumentStateLeaderTest,
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

TEST_F(DocumentStateLeaderTest,
       recoverEntries_should_abort_remaining_active_transactions) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(
      DocumentLogEntry{ReplicatedOperation::buildCreateShardOperation(
          shardId, collectionId, std::make_shared<VPackBuilder>())});
  // Transaction IDs are of follower type, as if they were replicated.
  entries.emplace_back(createDocumentEntry(TransactionId{6}));
  entries.emplace_back(createDocumentEntry(TransactionId{10}));
  entries.emplace_back(createDocumentEntry(TransactionId{14}));
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

TEST_F(DocumentStateLeaderTest,
       recoverEntries_should_abort_transactions_before_dropping_shard) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  ON_CALL(*transactionHandlerMock, getTransactionsForShard(shardId))
      .WillByDefault(Return(std::vector<TransactionId>{
          TransactionId{6}, TransactionId{10}, TransactionId{14}}));

  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(createDocumentEntry(TransactionId{6}));
  entries.emplace_back(createDocumentEntry(TransactionId{10}));
  entries.emplace_back(createDocumentEntry(TransactionId{14}));
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

TEST_F(DocumentStateLeaderTest,
       leader_recoverEntries_dies_if_transaction_is_invalid) {
  using namespace testing;

  auto transactionHandlerMock = createRealTransactionHandler();
  ON_CALL(*transactionHandlerMock,
          validate(Matcher<ReplicatedOperation::OperationType const&>(_)))
      .WillByDefault(Return(Result{TRI_ERROR_WAS_ERLAUBE}));

  std::vector<DocumentLogEntry> entries;
  entries.emplace_back(createDocumentEntry(TransactionId{10}));

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

TEST_F(DocumentStateLeaderTest,
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

TEST_F(DocumentStateLeaderTest,
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
  entries.emplace_back(createDocumentEntry(TransactionId{6}));
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
  entries.emplace_back(createDocumentEntry(TransactionId{10}));
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

TEST_F(DocumentStateLeaderTest, leader_create_and_drop_shard) {
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
