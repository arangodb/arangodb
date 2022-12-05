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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Inspection/VPack.h"
#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateAgencyHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

#include "Transaction/Manager.h"
#include "velocypack/Value.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;
using namespace arangodb::replication2::test;

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/Mocks/MockStatePersistorInterface.h"

struct MockDatabaseGuard : IDatabaseGuard {
  MOCK_METHOD(TRI_vocbase_t&, database, (), (const, noexcept, override));
};

struct MockTransactionManager : transaction::IManager {
  MOCK_METHOD(Result, abortManagedTrx,
              (TransactionId, std::string const& database), (override));
};

struct MockDocumentStateHandlersFactory : IDocumentStateHandlersFactory {
  MOCK_METHOD(std::shared_ptr<IDocumentStateAgencyHandler>, createAgencyHandler,
              (GlobalLogIdentifier), (override));
  MOCK_METHOD(std::shared_ptr<IDocumentStateShardHandler>, createShardHandler,
              (GlobalLogIdentifier), (override));
  MOCK_METHOD(std::unique_ptr<IDocumentStateTransactionHandler>,
              createTransactionHandler, (GlobalLogIdentifier), (override));
  MOCK_METHOD(std::shared_ptr<IDocumentStateTransaction>, createTransaction,
              (DocumentLogEntry const&, IDatabaseGuard const&), (override));
  MOCK_METHOD(std::shared_ptr<IDocumentStateNetworkHandler>,
              createNetworkHandler, (GlobalLogIdentifier), (override));
};

struct MockDocumentStateTransaction : IDocumentStateTransaction {
  MOCK_METHOD(OperationResult, apply, (DocumentLogEntry const&), (override));
  MOCK_METHOD(Result, intermediateCommit, (), (override));
  MOCK_METHOD(Result, commit, (), (override));
  MOCK_METHOD(Result, abort, (), (override));
};

struct MockDocumentStateTransactionHandler : IDocumentStateTransactionHandler {
  explicit MockDocumentStateTransactionHandler(
      std::shared_ptr<IDocumentStateTransactionHandler> real)
      : _real(std::move(real)) {
    ON_CALL(*this, applyEntry(testing::_))
        .WillByDefault([this](DocumentLogEntry doc) {
          return _real->applyEntry(std::move(doc));
        });
    ON_CALL(*this, ensureTransaction(testing::_))
        .WillByDefault([this](DocumentLogEntry const& doc)
                           -> std::shared_ptr<IDocumentStateTransaction> {
          return _real->ensureTransaction(doc);
        });
    ON_CALL(*this, removeTransaction(testing::_))
        .WillByDefault([this](TransactionId tid) {
          return _real->removeTransaction(tid);
        });
    ON_CALL(*this, getUnfinishedTransactions())
        .WillByDefault([this]() -> TransactionMap const& {
          return _real->getUnfinishedTransactions();
        });
  }

  MOCK_METHOD(Result, applyEntry, (DocumentLogEntry doc), (override));
  MOCK_METHOD(std::shared_ptr<IDocumentStateTransaction>, ensureTransaction,
              (DocumentLogEntry const& doc), (override));
  MOCK_METHOD(void, removeTransaction, (TransactionId tid), (override));
  MOCK_METHOD(TransactionMap const&, getUnfinishedTransactions, (),
              (const, override));

 private:
  std::shared_ptr<IDocumentStateTransactionHandler> _real;
};

struct MockDocumentStateAgencyHandler : IDocumentStateAgencyHandler {
  MOCK_METHOD(ResultT<std::shared_ptr<velocypack::Builder>>, getCollectionPlan,
              (std::string const&), (override));
  MOCK_METHOD(Result, reportShardInCurrent,
              (std::string const&, std::string const&,
               std::shared_ptr<velocypack::Builder> const&),
              (override));
};

struct MockDocumentStateShardHandler : IDocumentStateShardHandler {
  MOCK_METHOD(ResultT<std::string>, createLocalShard,
              (std::string const&, std::shared_ptr<velocypack::Builder> const&),
              (override));
  MOCK_METHOD(Result, dropLocalShard, (std::string const&), (override));
};

struct MockDocumentStateLeaderInterface : IDocumentStateLeaderInterface {
  MOCK_METHOD(futures::Future<ResultT<Snapshot>>, getSnapshot, (LogIndex),
              (override));
};

struct MockDocumentStateNetworkHandler : IDocumentStateNetworkHandler {
  MOCK_METHOD(std::shared_ptr<IDocumentStateLeaderInterface>,
              getLeaderInterface, (ParticipantId), (override, noexcept));
};

struct DocumentStateMachineTest : test::ReplicatedLogTest {
  DocumentStateMachineTest() {
    feature->registerStateType<DocumentState>(std::string{DocumentState::NAME},
                                              handlersFactoryMock,
                                              transactionManagerMock);
  }

  std::shared_ptr<MockStatePersistorInterface> statePersistor =
      std::make_shared<MockStatePersistorInterface>();
  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();

  std::shared_ptr<testing::NiceMock<MockDocumentStateHandlersFactory>>
      handlersFactoryMock = std::make_shared<
          testing::NiceMock<MockDocumentStateHandlersFactory>>();
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

  void SetUp() override {
    using namespace testing;

    ON_CALL(*transactionMock, commit).WillByDefault(Return(Result{}));
    ON_CALL(*transactionMock, abort).WillByDefault(Return(Result{}));
    ON_CALL(*transactionMock, apply(_)).WillByDefault([]() {
      return OperationResult{Result{}, OperationOptions{}};
    });

    ON_CALL(*leaderInterfaceMock, getSnapshot).WillByDefault([&](LogIndex) {
      return futures::Future<ResultT<Snapshot>>{std::in_place};
    });

    ON_CALL(*networkHandlerMock, getLeaderInterface)
        .WillByDefault(Return(leaderInterfaceMock));

    ON_CALL(*handlersFactoryMock, createAgencyHandler)
        .WillByDefault([&](GlobalLogIdentifier gid) {
          ON_CALL(*agencyHandlerMock, getCollectionPlan)
              .WillByDefault(Return(std::make_shared<VPackBuilder>()));
          ON_CALL(*agencyHandlerMock, reportShardInCurrent)
              .WillByDefault(Return(Result{}));
          return agencyHandlerMock;
        });

    ON_CALL(*handlersFactoryMock, createShardHandler)
        .WillByDefault([&](GlobalLogIdentifier gid) {
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

    ON_CALL(*handlersFactoryMock, createTransaction)
        .WillByDefault(Return(transactionMock));

    ON_CALL(*handlersFactoryMock, createNetworkHandler)
        .WillByDefault(Return(networkHandlerMock));
  }

  void TearDown() override {
    using namespace testing;

    Mock::VerifyAndClearExpectations(handlersFactoryMock.get());
    Mock::VerifyAndClearExpectations(agencyHandlerMock.get());
    Mock::VerifyAndClearExpectations(shardHandlerMock.get());
    Mock::VerifyAndClearExpectations(transactionMock.get());
  }

  const std::string collectionId = "testCollectionID";
  const LogId logId = LogId{1};
  const std::string dbName = "testDB";
  const GlobalLogIdentifier globalId{dbName, logId};
  const std::string shardId =
      DocumentStateShardHandler::stateIdToShardId(logId);
  const document::DocumentCoreParameters coreParams{collectionId, dbName};
};

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
  EXPECT_EQ(3U, leaderState->getActiveTransactionsCount());

  {
    VPackBuilder builder;
    std::ignore = leaderState->replicateOperation(
        builder.sharedSlice(), OperationType::kAbort, TransactionId{5},
        ReplicationOptions{});
    std::ignore = leaderState->replicateOperation(
        builder.sharedSlice(), OperationType::kCommit, TransactionId{9},
        ReplicationOptions{});
  }
  EXPECT_EQ(1U, leaderState->getActiveTransactionsCount());

  EXPECT_CALL(transactionManagerMock,
              abortManagedTrx(TransactionId{13}, globalId.database))
      .Times(1);

  // resigning as leader should abort the remaining transaction with id 3
  std::ignore = leaderLog->becomeFollower("leader", LogTerm{2}, "dummy");
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

  EXPECT_CALL(*agencyHandlerMock, getCollectionPlan(collectionId)).Times(1);
  EXPECT_CALL(*agencyHandlerMock,
              reportShardInCurrent(collectionId, shardId, _))
      .Times(1);
  EXPECT_CALL(*shardHandlerMock, createLocalShard(collectionId, _)).Times(1);

  EXPECT_CALL(*transactionMock, apply).Times(3);
  EXPECT_CALL(*transactionMock, commit).Times(1);
  EXPECT_CALL(*transactionMock, abort).Times(1);

  // The leader adds a tombstone for its own transaction.
  EXPECT_CALL(transactionManagerMock,
              abortManagedTrx(TransactionId{14}.asLeaderTransactionId(),
                              globalId.database))
      .Times(1);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());
}

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

  std::shared_ptr<DocumentStateTransactionHandler> real;
  std::shared_ptr<MockDocumentStateTransactionHandler> transactionHandlerMock;
  ON_CALL(*handlersFactoryMock, createTransactionHandler(_))
      .WillByDefault([&](GlobalLogIdentifier gid) {
        real = std::make_shared<DocumentStateTransactionHandler>(
            std::move(gid), std::make_unique<MockDatabaseGuard>(),
            handlersFactoryMock);
        transactionHandlerMock =
            std::make_shared<NiceMock<MockDocumentStateTransactionHandler>>(
                real);
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

TEST_F(DocumentStateMachineTest, test_SnapshotTransfer) {
  using namespace testing;

  const std::string_view key = "document1_key";
  const std::string_view value = "document1_value";
  ON_CALL(*leaderInterfaceMock, getSnapshot)
      .WillByDefault([&](LogIndex) -> futures::Future<ResultT<Snapshot>> {
        VPackBuilder builder;
        {
          VPackObjectBuilder ob(&builder);
          builder.add(key, value);
        }
        return ResultT<Snapshot>::success(Snapshot{builder.sharedSlice()});
      });
  EXPECT_CALL(*leaderInterfaceMock, getSnapshot(_)).Times(1);

  auto allEntries = std::vector<DocumentLogEntry>{};
  ON_CALL(*transactionMock, apply(_))
      .WillByDefault([&allEntries](DocumentLogEntry const& entry) {
        allEntries.emplace_back(entry);
        return OperationResult{Result{}, OperationOptions{}};
      });
  EXPECT_CALL(*transactionMock, apply(_)).Times(2);
  EXPECT_CALL(*transactionMock, commit()).Times(2);

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
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());

  follower->runAllAsyncAppendEntries();
  auto followerReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, followerLog,
                                         statePersistor));
  followerReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());

  Mock::VerifyAndClearExpectations(transactionMock.get());
  Mock::VerifyAndClearExpectations(leaderInterfaceMock.get());
  ASSERT_EQ(allEntries.size(), 2);
  ASSERT_EQ(allEntries[0].operation, OperationType::kTruncate);
  ASSERT_EQ(allEntries[1].operation, OperationType::kInsert);
}

TEST(DocumentStateTransactionHandlerTest, test_ensureTransaction) {
  using namespace testing;

  auto dbGuardMock = std::make_unique<MockDatabaseGuard>();
  auto handlersFactoryMock =
      std::make_shared<MockDocumentStateHandlersFactory>();
  auto transactionMock = std::make_shared<MockDocumentStateTransaction>();

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, std::move(dbGuardMock),
      handlersFactoryMock);

  auto tid = TransactionId{6};
  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), tid};

  EXPECT_CALL(*handlersFactoryMock, createTransaction)
      .WillOnce(Return(transactionMock));

  // Use a new entry and expect the transaction to be created
  auto trx = transactionHandler.ensureTransaction(doc);
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());

  // Use an existing entry, and expect the transaction to be reused
  ASSERT_EQ(trx, transactionHandler.ensureTransaction(doc));
}

TEST(DocumentStateTransactionHandlerTest, test_applyEntry_basic) {
  using namespace testing;

  auto dbGuardMock = std::make_unique<MockDatabaseGuard>();
  auto handlersFactoryMock =
      std::make_shared<MockDocumentStateHandlersFactory>();
  auto transactionMock = std::make_shared<MockDocumentStateTransaction>();

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, std::move(dbGuardMock),
      handlersFactoryMock);

  ON_CALL(*handlersFactoryMock, createTransaction)
      .WillByDefault(Return(transactionMock));

  ON_CALL(*transactionMock, apply(_)).WillByDefault([]() {
    return OperationResult{Result{}, OperationOptions{}};
  });

  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), TransactionId{6}};

  // Expect the transaction to be started an applied successfully
  EXPECT_CALL(*handlersFactoryMock, createTransaction).Times(1);
  EXPECT_CALL(*transactionMock, apply).Times(1);
  auto result = transactionHandler.applyEntry(doc);
  ASSERT_TRUE(result.ok());
  Mock::VerifyAndClearExpectations(transactionMock.get());
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());

  // An intermediate commit should not affect the transaction
  EXPECT_CALL(*transactionMock, intermediateCommit).WillOnce(Return(Result{}));
  doc.operation = OperationType::kIntermediateCommit;
  result = transactionHandler.applyEntry(doc);
  ASSERT_TRUE(result.ok());
  Mock::VerifyAndClearExpectations(transactionMock.get());
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().contains(
      TransactionId{6}));

  // After commit, expect the transaction to be removed
  EXPECT_CALL(*transactionMock, commit).WillOnce(Return(Result{}));
  doc.operation = OperationType::kCommit;
  result = transactionHandler.applyEntry(doc);
  ASSERT_TRUE(result.ok());
  Mock::VerifyAndClearExpectations(transactionMock.get());
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().empty());

  // Start a new transaction and then abort it.
  doc = DocumentLogEntry{"s1234", OperationType::kRemove,
                         velocypack::SharedSlice(), TransactionId{10}};
  EXPECT_CALL(*handlersFactoryMock, createTransaction).Times(1);
  EXPECT_CALL(*transactionMock, apply).Times(1);
  result = transactionHandler.applyEntry(doc);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(transactionHandler.getUnfinishedTransactions().contains(
      TransactionId{10}));
  Mock::VerifyAndClearExpectations(transactionMock.get());
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());

  // Expect the transaction to be removed after abort
  EXPECT_CALL(*transactionMock, abort).WillOnce(Return(Result{}));
  doc.operation = OperationType::kAbort;
  result = transactionHandler.applyEntry(doc);
  ASSERT_TRUE(result.ok());
  Mock::VerifyAndClearExpectations(transactionMock.get());
  ASSERT_TRUE(!transactionHandler.getUnfinishedTransactions().contains(
      TransactionId{10}));

  // No transaction should be created during AbortAllOngoingTrx
  doc.operation = OperationType::kAbortAllOngoingTrx;
  result = transactionHandler.applyEntry(doc);
  ASSERT_TRUE(result.ok());
}

TEST(DocumentStateTransactionHandlerTest, test_applyEntry_errors) {
  using namespace testing;

  auto dbGuardMock = std::make_unique<MockDatabaseGuard>();
  auto handlersFactoryMock =
      std::make_shared<MockDocumentStateHandlersFactory>();
  auto transactionMock = std::make_shared<MockDocumentStateTransaction>();

  auto transactionHandler = DocumentStateTransactionHandler(
      GlobalLogIdentifier{"testDb", LogId{1}}, std::move(dbGuardMock),
      handlersFactoryMock);

  EXPECT_CALL(*handlersFactoryMock, createTransaction)
      .WillOnce(Return(transactionMock));

  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), TransactionId{6}};

  // OperationResult failed, transaction should fail
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce(Return(
          OperationResult{Result{TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION},
                          OperationOptions{}}));
  auto result = transactionHandler.applyEntry(doc);
  ASSERT_TRUE(result.fail());
  Mock::VerifyAndClearExpectations(handlersFactoryMock.get());
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // Unique constraint violation, should not fail because we are doing recovery
  EXPECT_CALL(*transactionMock, apply(_))
      .WillOnce([](DocumentLogEntry const& entry) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED] = 1;
        return opRes;
      });
  result = transactionHandler.applyEntry(doc);
  ASSERT_FALSE(result.fail());
  Mock::VerifyAndClearExpectations(transactionMock.get());

  // Other type of error inside countErrorCodes, transaction should fail
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

TEST(ActiveTransactionsQueueTEst, test_activeTransactions) {
  auto activeTrx = ActiveTransactionsQueue{};
  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{99}), LogIndex{99});
  activeTrx.emplace(TransactionId{100}, LogIndex{100});
  ASSERT_TRUE(activeTrx.erase(TransactionId{100}));
  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{103}), LogIndex{103});
  ASSERT_FALSE(activeTrx.erase(TransactionId{100}));
  activeTrx.emplace(TransactionId{200}, LogIndex{200});
  activeTrx.emplace(TransactionId{300}, LogIndex{300});
  activeTrx.emplace(TransactionId{400}, LogIndex{400});
  ASSERT_TRUE(activeTrx.erase(TransactionId{200}));
  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{1000}), LogIndex{299});
  ASSERT_TRUE(activeTrx.erase(TransactionId{400}));
  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{1000}), LogIndex{299});
  ASSERT_TRUE(activeTrx.erase(TransactionId{300}));
  ASSERT_EQ(activeTrx.getReleaseIndex(LogIndex{1000}), LogIndex{1000});
}
