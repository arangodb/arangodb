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
#include <fakeit.hpp>

#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateAgencyHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;
using namespace arangodb::replication2::test;

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

struct MockDocumentStateHandlersFactory
    : IDocumentStateHandlersFactory,
      std::enable_shared_from_this<MockDocumentStateHandlersFactory> {
  MockDocumentStateHandlersFactory() { reset(); }

  auto createAgencyHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateAgencyHandler> override {
    return std::shared_ptr<IDocumentStateAgencyHandler>(
        &agencyHandlerMock.get());
  }

  auto createShardHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> override {
    fakeit::When(Method(shardHandlerMock, createLocalShard))
        .AlwaysReturn(ResultT<std::string>::success(
            DocumentStateShardHandler::stateIdToShardId(gid.id)));
    return std::shared_ptr<IDocumentStateShardHandler>(&shardHandlerMock.get());
  }

  auto createTransactionHandler(GlobalLogIdentifier gid)
      -> std::unique_ptr<IDocumentStateTransactionHandler> override {
    fakeit::Fake(Dtor(dbGuardMock));
    auto transactionHandler = std::make_unique<DocumentStateTransactionHandler>(
        std::unique_ptr<IDatabaseGuard>(&dbGuardMock.get()),
        shared_from_this());
    transactionHandlerPtrs.push_back(transactionHandler.get());
    return transactionHandler;
  }

  auto createTransaction(DocumentLogEntry const& doc,
                         IDatabaseGuard const& dbGuard)
      -> std::shared_ptr<IDocumentStateTransaction> override {
    return std::shared_ptr<IDocumentStateTransaction>(&transactionMock.get());
  }

  void reset() {
    agencyHandlerMock.ClearInvocationHistory();
    shardHandlerMock.ClearInvocationHistory();
    transactionMock.ClearInvocationHistory();
    transactionHandlerPtrs.clear();
    fakeit::When(Method(agencyHandlerMock, getCollectionPlan))
        .AlwaysReturn(std::make_shared<VPackBuilder>());
    fakeit::When(Method(agencyHandlerMock, reportShardInCurrent))
        .AlwaysReturn(Result{});
  }

  fakeit::Mock<IDocumentStateAgencyHandler> agencyHandlerMock;
  fakeit::Mock<IDocumentStateShardHandler> shardHandlerMock;
  fakeit::Mock<IDocumentStateTransaction> transactionMock;
  fakeit::Mock<IDatabaseGuard> dbGuardMock;
  std::vector<DocumentStateTransactionHandler*> transactionHandlerPtrs;
};

struct DocumentStateMachineTest : test::ReplicatedLogTest {
  DocumentStateMachineTest() {
    feature->registerStateType<DocumentState>(std::string{DocumentState::NAME},
                                              factory);
  }

  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();
  std::shared_ptr<MockDocumentStateHandlersFactory> factory =
      std::make_shared<MockDocumentStateHandlersFactory>();
};

TEST_F(DocumentStateMachineTest, leader_follower_integration) {
  const std::string collectionId = "testCollectionID";
  const std::string dbName = "testDB";
  const LogId logId = LogId{1};
  auto const shardId = DocumentStateShardHandler::stateIdToShardId(logId);

  auto followerLog = makeReplicatedLog(logId);
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(logId);
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);
  leader->triggerAsyncReplication();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, leaderLog));
  ASSERT_NE(leaderReplicatedState, nullptr);

  auto coreParams = document::DocumentCoreParameters{collectionId, dbName};
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());

  // Very methods called during core construction
  fakeit::Verify(
      Method(factory->agencyHandlerMock, getCollectionPlan).Using(collectionId))
      .Exactly(1);
  fakeit::Verify(Method(factory->agencyHandlerMock, reportShardInCurrent)
                     .Using(collectionId, shardId, fakeit::_))
      .Exactly(1);
  fakeit::Verify(Method(factory->shardHandlerMock, createLocalShard)
                     .Using(coreParams.collectionId, fakeit::_))
      .Exactly(1);

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
          feature->createReplicatedState(DocumentState::NAME, followerLog));
  ASSERT_NE(followerReplicatedState, nullptr);
  followerReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}),
      coreParams.toSharedSlice());

  // Very methods called during core construction
  fakeit::Verify(
      Method(factory->agencyHandlerMock, getCollectionPlan).Using(collectionId))
      .Exactly(2);
  fakeit::Verify(Method(factory->agencyHandlerMock, reportShardInCurrent)
                     .Using(collectionId, shardId, fakeit::_))
      .Exactly(2);
  fakeit::Verify(Method(factory->shardHandlerMock, createLocalShard)
                     .Using(coreParams.collectionId, fakeit::_))
      .Exactly(2);

  auto followerState = followerReplicatedState->getFollower();
  ASSERT_NE(followerState, nullptr);

  fakeit::Mock<IDocumentStateTransactionHandler> transactionHandlerFollowerSpy(
      *factory->transactionHandlerPtrs[1]);
  fakeit::Spy(Method(transactionHandlerFollowerSpy, applyEntry));

  // Insert a document
  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add("document1_key", "document1_value");
    ob->close();

    auto operation = OperationType::kInsert;
    auto tid = TransactionId{1};
    auto res = leaderState->replicateOperation(builder.sharedSlice(), operation,
                                               tid, ReplicationOptions{});

    ASSERT_TRUE(res.isReady());
    auto logIndex = res.result().get();

    inMemoryLog = leader->copyInMemoryLog();
    entry = inMemoryLog.getEntryByIndex(logIndex);
    doc = velocypack::deserialize<DocumentLogEntry>(
        entry->entry().logPayload()->slice()[1]);
    ASSERT_EQ(doc.shardId, shardId);
    ASSERT_EQ(doc.operation, operation);
    ASSERT_EQ(doc.tid, tid);
    ASSERT_EQ(doc.data.get("document1_key").stringView(), "document1_value");

    fakeit::When(Method(factory->transactionMock, apply))
        .Do([](DocumentLogEntry const& entry) {
          return DocumentStateTransactionResult(
              entry.tid, OperationResult{Result{}, OperationOptions{}});
        });
    follower->runAllAsyncAppendEntries();
    fakeit::Verify(Method(factory->transactionMock, apply)).Exactly(1);
    fakeit::Verify(Method(transactionHandlerFollowerSpy, applyEntry)
                       .Using(fakeit::_, ApplyEntryErrorHandling::kFail))
        .Exactly(1);
  }

  // Commit
  transactionHandlerFollowerSpy.ClearInvocationHistory();
  {
    auto operation = OperationType::kCommit;
    auto tid = TransactionId{1};
    auto res = leaderState->replicateOperation(
        velocypack::SharedSlice{}, operation, tid,
        ReplicationOptions{.waitForCommit = true});

    ASSERT_FALSE(res.isReady());
    fakeit::When(Method(factory->transactionMock, commit)).Return(Result{});

    follower->runAllAsyncAppendEntries();
    ASSERT_TRUE(res.isReady());
    auto logIndex = res.result().get();
    fakeit::Verify(Method(factory->transactionMock, commit)).Exactly(1);
    fakeit::Verify(Method(transactionHandlerFollowerSpy, applyEntry)
                       .Using(fakeit::_, ApplyEntryErrorHandling::kFail))
        .Exactly(1);

    inMemoryLog = follower->copyInMemoryLog();
    entry = inMemoryLog.getEntryByIndex(logIndex);
    doc = velocypack::deserialize<DocumentLogEntry>(
        entry->entry().logPayload()->slice()[1]);
    ASSERT_EQ(doc.shardId, shardId);
    ASSERT_EQ(doc.operation, operation);
    ASSERT_EQ(doc.tid, tid);
    ASSERT_TRUE(doc.data.isNone());
  }
}

TEST(DocumentStateTransactionHandlerTest, test_ensureTransaction) {
  fakeit::Mock<IDatabaseGuard> dbGuardMock;
  fakeit::Mock<IDocumentStateHandlersFactory> handlersFactoryMock;
  fakeit::Mock<IDocumentStateTransaction> transactionMock;

  fakeit::Fake(Dtor(dbGuardMock));
  auto transactionHandler = DocumentStateTransactionHandler(
      std::unique_ptr<IDatabaseGuard>(&dbGuardMock.get()),
      std::shared_ptr<IDocumentStateHandlersFactory>(
          &handlersFactoryMock.get()));

  auto tid = TransactionId{1};
  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), tid};

  fakeit::When(Method(handlersFactoryMock, createTransaction))
      .Return(
          std::shared_ptr<IDocumentStateTransaction>(&transactionMock.get()));

  // Use a new entry and expect the transaction to be created
  auto trx = transactionHandler.ensureTransaction(doc);
  fakeit::Verify(Method(handlersFactoryMock, createTransaction)).Once();

  // Use an existing entry, and expect the transaction to be reused
  ASSERT_EQ(trx, transactionHandler.ensureTransaction(doc));
  fakeit::VerifyNoOtherInvocations(handlersFactoryMock);
}

TEST(DocumentStateTransactionHandlerTest, test_applyEntry_basic) {
  fakeit::Mock<IDatabaseGuard> dbGuardMock;
  fakeit::Mock<IDocumentStateHandlersFactory> handlersFactoryMock;
  fakeit::Mock<IDocumentStateTransaction> transactionMock;

  fakeit::Fake(Dtor(dbGuardMock));
  auto transactionHandler = DocumentStateTransactionHandler(
      std::unique_ptr<IDatabaseGuard>(&dbGuardMock.get()),
      std::shared_ptr<IDocumentStateHandlersFactory>(
          &handlersFactoryMock.get()));
  fakeit::Mock<IDocumentStateTransactionHandler> transactionHandlerSpy(
      transactionHandler);

  fakeit::When(Method(handlersFactoryMock, createTransaction))
      .AlwaysDo(
          [&transactionMock](DocumentLogEntry const&, IDatabaseGuard const&) {
            return std::shared_ptr<IDocumentStateTransaction>(
                &transactionMock.get());
          });
  fakeit::When(Method(transactionMock, apply))
      .AlwaysDo([](DocumentLogEntry const& entry) {
        return DocumentStateTransactionResult(
            entry.tid, OperationResult{Result{}, OperationOptions{}});
      });

  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), TransactionId{1}};

  // Expect the transaction to be started an applied successfully
  auto result =
      transactionHandler.applyEntry(doc, ApplyEntryErrorHandling::kFail);
  ASSERT_TRUE(result.ok());
  fakeit::Verify(Method(transactionMock, apply)).Once();

  // After commit, expect the transaction to be removed
  fakeit::Spy(Method(transactionHandlerSpy, removeTransaction));
  fakeit::When(Method(transactionMock, commit)).Return(Result{});
  doc.operation = OperationType::kCommit;
  result = transactionHandler.applyEntry(doc, ApplyEntryErrorHandling::kFail);
  ASSERT_TRUE(result.ok());
  fakeit::Verify(Method(transactionMock, commit)).Once();
  fakeit::Verify(Method(transactionHandlerSpy, removeTransaction)).Once();

  // Start a new transaction and then abort it.
  transactionMock.ClearInvocationHistory();
  transactionHandlerSpy.ClearInvocationHistory();
  doc = DocumentLogEntry{"s1234", OperationType::kRemove,
                         velocypack::SharedSlice(), TransactionId{2}};
  result = transactionHandler.applyEntry(doc, ApplyEntryErrorHandling::kFail);
  ASSERT_TRUE(result.ok());
  fakeit::Verify(Method(transactionMock, apply)).Once();

  // Expect the transaction to be removed after abort
  fakeit::When(Method(transactionMock, abort)).Return(Result{});
  doc.operation = OperationType::kAbort;
  result = transactionHandler.applyEntry(doc, ApplyEntryErrorHandling::kFail);
  ASSERT_TRUE(result.ok());
  fakeit::Verify(Method(transactionMock, abort)).Once();
  fakeit::Verify(Method(transactionHandlerSpy, removeTransaction)).Once();

  // No transaction should be created during AbortAllOngoingTrx
  transactionHandlerSpy.ClearInvocationHistory();
  doc.operation = OperationType::kAbortAllOngoingTrx;
  result = transactionHandler.applyEntry(doc, ApplyEntryErrorHandling::kFail);
  ASSERT_TRUE(result.ok());
  fakeit::Verify(Method(transactionHandlerSpy, ensureTransaction)).Never();
}

TEST(DocumentStateTransactionHandlerTest,
     test_applyEntry_error_without_recovery) {
  fakeit::Mock<IDatabaseGuard> dbGuardMock;
  fakeit::Mock<IDocumentStateHandlersFactory> handlersFactoryMock;
  fakeit::Mock<IDocumentStateTransaction> transactionMock;

  fakeit::Fake(Dtor(dbGuardMock));
  auto transactionHandler = DocumentStateTransactionHandler(
      std::unique_ptr<IDatabaseGuard>(&dbGuardMock.get()),
      std::shared_ptr<IDocumentStateHandlersFactory>(
          &handlersFactoryMock.get()));

  fakeit::When(Method(handlersFactoryMock, createTransaction))
      .AlwaysDo(
          [&transactionMock](DocumentLogEntry const&, IDatabaseGuard const&) {
            return std::shared_ptr<IDocumentStateTransaction>(
                &transactionMock.get());
          });

  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), TransactionId{1}};

  // OperationResult failed, transaction should fail
  fakeit::When(Method(transactionMock, apply))
      .Do([](DocumentLogEntry const& entry) {
        return DocumentStateTransactionResult(
            entry.tid,
            OperationResult{Result{TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION},
                            OperationOptions{}});
      });
  auto result =
      transactionHandler.applyEntry(doc, ApplyEntryErrorHandling::kFail);
  ASSERT_TRUE(result.fail());
  fakeit::Verify(Method(transactionMock, apply)).Exactly(1);

  // Unique constraint violation, should fail because we are not doing recovery
  fakeit::When(Method(transactionMock, apply))
      .Do([](DocumentLogEntry const& entry) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED] = 1;
        return DocumentStateTransactionResult(entry.tid, std::move(opRes));
      });
  result = transactionHandler.applyEntry(doc, ApplyEntryErrorHandling::kFail);
  ASSERT_TRUE(result.fail());
  fakeit::Verify(Method(transactionMock, apply)).Exactly(2);

  // Other type of error inside countErrorCodes
  fakeit::When(Method(transactionMock, apply))
      .Do([](DocumentLogEntry const& entry) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION] = 1;
        return DocumentStateTransactionResult(entry.tid, std::move(opRes));
      });
  result = transactionHandler.applyEntry(doc, ApplyEntryErrorHandling::kFail);
  ASSERT_TRUE(result.fail());
  fakeit::Verify(Method(transactionMock, apply)).Exactly(3);
}

TEST(DocumentStateTransactionHandlerTest,
     test_applyEntry_error_during_recovery) {
  fakeit::Mock<IDatabaseGuard> dbGuardMock;
  fakeit::Mock<IDocumentStateHandlersFactory> handlersFactoryMock;
  fakeit::Mock<IDocumentStateTransaction> transactionMock;

  fakeit::Fake(Dtor(dbGuardMock));
  auto transactionHandler = DocumentStateTransactionHandler(
      std::unique_ptr<IDatabaseGuard>(&dbGuardMock.get()),
      std::shared_ptr<IDocumentStateHandlersFactory>(
          &handlersFactoryMock.get()));

  fakeit::When(Method(handlersFactoryMock, createTransaction))
      .AlwaysDo(
          [&transactionMock](DocumentLogEntry const&, IDatabaseGuard const&) {
            return std::shared_ptr<IDocumentStateTransaction>(
                &transactionMock.get());
          });

  auto doc = DocumentLogEntry{"s1234", OperationType::kInsert,
                              velocypack::SharedSlice(), TransactionId{1}};

  // OperationResult failed, transaction should fail
  fakeit::When(Method(transactionMock, apply))
      .Do([](DocumentLogEntry const& entry) {
        return DocumentStateTransactionResult(
            entry.tid,
            OperationResult{Result{TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION},
                            OperationOptions{}});
      });
  auto result = transactionHandler.applyEntry(
      doc, ApplyEntryErrorHandling::kIgnoreRecoveryErrors);
  ASSERT_TRUE(result.fail());
  fakeit::Verify(Method(transactionMock, apply)).Exactly(1);

  // Unique constraint violation, should not fail because we are doing recovery
  fakeit::When(Method(transactionMock, apply))
      .Do([](DocumentLogEntry const& entry) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED] = 1;
        return DocumentStateTransactionResult(entry.tid, std::move(opRes));
      });
  result = transactionHandler.applyEntry(
      doc, ApplyEntryErrorHandling::kIgnoreRecoveryErrors);
  ASSERT_FALSE(result.fail());
  fakeit::Verify(Method(transactionMock, apply)).Exactly(2);

  // Other type of error inside countErrorCodes, transaction should fail
  fakeit::When(Method(transactionMock, apply))
      .Do([](DocumentLogEntry const& entry) {
        auto opRes = OperationResult{Result{}, OperationOptions{}};
        opRes.countErrorCodes[TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION] = 1;
        return DocumentStateTransactionResult(entry.tid, std::move(opRes));
      });
  result = transactionHandler.applyEntry(
      doc, ApplyEntryErrorHandling::kIgnoreRecoveryErrors);
  ASSERT_TRUE(result.fail());
  fakeit::Verify(Method(transactionMock, apply)).Exactly(3);
}
