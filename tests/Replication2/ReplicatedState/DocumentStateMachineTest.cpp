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
#include "Mocks/Servers.h"
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
  MOCK_METHOD(std::shared_ptr<velocypack::Builder>, getCollectionPlan,
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

struct DocumentStateMachineTest : testing::Test {
  DocumentStateMachineTest() {
    feature->registerStateType<DocumentState>(std::string{DocumentState::NAME},
                                              handlersFactoryMock,
                                              transactionManagerMock);
  }

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

  DocumentFactory factory =
      DocumentFactory(handlersFactoryMock, transactionManagerMock);

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

  tests::mocks::MockRestServer mockApplicationServer =
      tests::mocks::MockRestServer();
  std::unique_ptr<SupervisedScheduler> scheduler =
      std::make_unique<SupervisedScheduler>(
          DocumentStateMachineTest::mockApplicationServer.server(), 2, 64, 128,
          1024 * 1024, 4096, 4096, 128, 0.0);
  const std::string collectionId = "testCollectionID";
  static constexpr LogId logId = LogId{1};
  const std::string dbName = "testDB";
  const GlobalLogIdentifier globalId{dbName, logId};
  const std::string shardId =
      DocumentStateShardHandler::stateIdToShardId(logId);
  const document::DocumentCoreParameters coreParams{collectionId, dbName};
  const velocypack::SharedSlice coreParamsSlice = coreParams.toSharedSlice();
  const std::string leaderId = "leader";
};

// struct MockReplicatedStateHandle : replicated_log::IReplicatedStateHandle {
//   MOCK_METHOD(std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>,
//               resignCurrentState, (), (noexcept));
//   MOCK_METHOD(void, leadershipEstablished,
//               (std::unique_ptr<IReplicatedLogLeaderMethods>), ());
//   MOCK_METHOD(void, becomeFollower,
//               (std::unique_ptr<IReplicatedLogFollowerMethods>), ());
//   MOCK_METHOD(void, acquireSnapshot, (ServerID leader, LogIndex), ());
//   MOCK_METHOD(void, updateCommitIndex, (LogIndex), ());
//   MOCK_METHOD(std::optional<replicated_state::StateStatus>, getStatus, (),
//               (const));
//   MOCK_METHOD(std::shared_ptr<replicated_state::IReplicatedFollowerStateBase>,
//               getFollower, (), (const));
//   MOCK_METHOD(std::shared_ptr<replicated_state::IReplicatedLeaderStateBase>,
//               getLeader, (), (const));
//   MOCK_METHOD(void, dropEntries, (), ());
// };

struct MockReplicatedLogLeaderMethods : IReplicatedLogLeaderMethods {
  // base methods
  MOCK_METHOD(void, releaseIndex, (LogIndex), ());
  MOCK_METHOD(InMemoryLog, getLogSnapshot, (), ());
  MOCK_METHOD(ILogParticipant::WaitForFuture, waitFor, (LogIndex), ());
  MOCK_METHOD(ILogParticipant::WaitForIteratorFuture, waitForIterator,
              (LogIndex), ());

  // leader methods
  MOCK_METHOD(LogIndex, insert, (LogPayload), ());
  MOCK_METHOD((std::pair<LogIndex, DeferredAction>), insertDeferred,
              (LogPayload), ());
};

struct MockReplicatedLogFollowerMethods : IReplicatedLogFollowerMethods {
  // base methods
  MOCK_METHOD(void, releaseIndex, (LogIndex), ());
  MOCK_METHOD(InMemoryLog, getLogSnapshot, (), ());
  MOCK_METHOD(ILogParticipant::WaitForFuture, waitFor, (LogIndex), ());
  MOCK_METHOD(ILogParticipant::WaitForIteratorFuture, waitForIterator,
              (LogIndex), ());

  // follower methods
  MOCK_METHOD(Result, snapshotCompleted, (), ());
};

struct MockProducerStream : streams::ProducerStream<DocumentLogEntry> {
  // Stream<T>
  MOCK_METHOD(futures::Future<WaitForResult>, waitFor, (LogIndex), ());
  MOCK_METHOD(futures::Future<std::unique_ptr<Iterator>>, waitForIterator,
              (LogIndex), ());
  MOCK_METHOD(void, release, (LogIndex), ());
  // ProducerStream<T>
  MOCK_METHOD(LogIndex, insert, (DocumentLogEntry const&), ());
  MOCK_METHOD((std::pair<LogIndex, DeferredAction>), insertDeferred,
              (DocumentLogEntry const&), ());

  MockProducerStream() {
    ON_CALL(*this, insert).WillByDefault([this](DocumentLogEntry const& doc) {
      auto idx = current;
      current += 1;
      entries[idx] = doc;
      return idx;
    });
  }

  LogIndex current{1};
  std::map<LogIndex, DocumentLogEntry> entries;
};

TEST_F(DocumentStateMachineTest,
       leader_resign_should_abort_active_transactions) {
  using namespace testing;

  auto core = factory.constructCore(globalId, coreParams);

  auto leaderState = factory.constructLeader(std::move(core));

  auto stream = std::make_shared<MockProducerStream>();
  // Just here to silence "Uninteresting mock function call" warnings.
  // Feel free to change this when the number of release calls changes.
  EXPECT_CALL(*stream, release).Times(2);

  leaderState->setStream(stream);

  ASSERT_NE(leaderState, nullptr);
  ASSERT_EQ(leaderState->shardId, shardId);

  EXPECT_CALL(*stream, insert).Times(3);
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

  EXPECT_CALL(*stream, insert).Times(2);
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

  // resigning should abort the remaining transaction with id 13
  EXPECT_CALL(transactionManagerMock,
              abortManagedTrx(TransactionId{13}, globalId.database))
      .Times(1);

  std::ignore = std::move(*leaderState).resign();
}

TEST_F(DocumentStateMachineTest,
       recoverEntries_should_abort_remaining_active_transactions) {
  using namespace testing;

  std::vector<DocumentLogEntry> entries;

  auto addEntry = [&entries, this](OperationType op, TransactionId trxId) {
    VPackBuilder builder;
    builder.openObject();
    builder.close();
    auto entry = DocumentLogEntry{shardId, op, builder.sharedSlice(), trxId};
    entries.emplace_back(std::move(entry));
  };

  // Transaction IDs are of follower type, as if they were replicated.
  addEntry(OperationType::kInsert, TransactionId{6});
  addEntry(OperationType::kInsert, TransactionId{10});
  addEntry(OperationType::kInsert, TransactionId{14});
  addEntry(OperationType::kAbort, TransactionId{6});
  addEntry(OperationType::kCommit, TransactionId{10});

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

  auto core = factory.constructCore(globalId, coreParams);

  auto leaderState = factory.constructLeader(std::move(core));

  auto stream = std::make_shared<MockProducerStream>();

  leaderState->setStream(stream);

  struct Iterator
      : TypedLogRangeIterator<streams::StreamEntryView<DocumentLogEntry>> {
    Iterator(std::vector<DocumentLogEntry> entries)
        : entries(std::move(entries)), iter(this->entries.begin()) {}

    auto next()
        -> std::optional<streams::StreamEntryView<DocumentLogEntry>> override {
      if (iter != entries.end()) {
        auto idx = LogIndex(std::distance(std::begin(entries), iter) + 1);
        auto res = std::make_pair(idx, std::ref(*iter));
        ++iter;
        return res;
      } else {
        return std::nullopt;
      }
    }
    auto range() const noexcept -> LogRange override {
      return LogRange{LogIndex{1}, LogIndex{entries.size() + 1}};
    }

    std::vector<DocumentLogEntry> entries;
    decltype(entries)::iterator iter;
  };

  auto entryIterator = std::make_unique<Iterator>(entries);

  auto entry = DocumentLogEntry{
      .shardId = "s1", .operation = OperationType::kAbortAllOngoingTrx};
  EXPECT_CALL(*stream, insert).WillOnce([&](auto const& entry) {
    EXPECT_EQ(entry.shardId, "s1");
    EXPECT_EQ(entry.operation, OperationType::kAbortAllOngoingTrx);
    return LogIndex{entries.size() + 1};
  });
  leaderState->recoverEntries(std::move(entryIterator));
}

// TODO What does this test test? Rewrite it.
#if 0
TEST_F(DocumentStateMachineTest, leader_follower_integration) {
  using namespace testing;

  auto followerLog = makeReplicatedLog(logId);
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(logId);
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);
  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, dbName, leaderLog,
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
          feature->createReplicatedState(DocumentState::NAME, dbName,
                                         followerLog, statePersistor));
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
#endif

TEST_F(DocumentStateMachineTest, test_SnapshotTransfer) {
  using namespace testing;

  const std::string_view key = "document1_key";
  const std::string_view value = "document1_value";
  const auto snapshotLogIndex = LogIndex{14};
  ON_CALL(*leaderInterfaceMock, getSnapshot)
      .WillByDefault([&](LogIndex index) -> futures::Future<ResultT<Snapshot>> {
        EXPECT_EQ(snapshotLogIndex, index);
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

  auto core = factory.constructCore(globalId, coreParams);

  auto followerState = factory.constructFollower(std::move(core));

  auto stream = std::make_shared<MockProducerStream>();

  followerState->setStream(stream);

  followerState->acquireSnapshot(leaderId, snapshotLogIndex);

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

TEST(ActiveTransactionsQueueTest, test_activeTransactions) {
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
