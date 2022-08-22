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

struct MockDocumentStateAgencyHandler : IDocumentStateAgencyHandler {
  auto getCollectionPlan(std::string const& collectionId)
      -> std::shared_ptr<velocypack::Builder> override {
    return std::make_shared<VPackBuilder>();
  }

  auto reportShardInCurrent(
      std::string const& collectionId, std::string const& shardId,
      std::shared_ptr<velocypack::Builder> const& properties)
      -> Result override {
    shards.emplace_back(shardId, collectionId);
    return {};
  }

  // A mapping between shardId and collectionId
  std::vector<std::pair<std::string, std::string>> shards;
};

struct MockDocumentStateShardHandler : IDocumentStateShardHandler {
  MockDocumentStateShardHandler() : shardId{0} {};

  auto createLocalShard(std::string const& collectionId,
                        std::shared_ptr<velocypack::Builder> const& properties)
      -> ResultT<std::string> override {
    ++shardId;
    return ResultT<std::string>::success(std::to_string(shardId));
  }

  int shardId;
};

struct MockDocumentStateTransaction : IDocumentStateTransaction {
  explicit MockDocumentStateTransaction(TransactionId tid) : tid(tid) {}

  auto apply(DocumentLogEntry const& entry)
      -> DocumentStateTransactionResult override {
    TRI_ASSERT(!applied);
    applied = true;
    return DocumentStateTransactionResult{TransactionId{1},
                                          OperationResult{Result{}, {}}};
  }

  auto commit() -> Result override {
    TRI_ASSERT(!committed);
    committed = true;
    return Result{};
  }

  auto abort() -> Result override {
    TRI_ASSERT(!aborted);
    aborted = true;
    return Result{};
  }

  TransactionId tid;
  bool ensured = false;
  bool applied = false;
  bool committed = false;
  bool removed = false;
  bool aborted = false;
};

struct MockDocumentStateTransactionHandler : IDocumentStateTransactionHandler {
  auto applyEntry(DocumentLogEntry doc) -> Result override {
    auto trx = ensureTransaction(doc);
    TRI_ASSERT(trx != nullptr);
    switch (doc.operation) {
      case OperationType::kInsert:
      case OperationType::kUpdate:
      case OperationType::kReplace:
      case OperationType::kRemove:
      case OperationType::kTruncate: {
        auto res = trx->apply(doc);
        return res.result();
      }
      case OperationType::kCommit: {
        auto res = trx->commit();
        removeTransaction(doc.tid);
        return res;
      }
      case OperationType::kAbort: {
        auto res = trx->abort();
        removeTransaction(doc.tid);
        return res;
      }
      case OperationType::kAbortAllOngoingTrx:
        // do nothing
        break;
      default:
        TRI_ASSERT(false);
    }
    return {};
  }

  auto ensureTransaction(DocumentLogEntry doc)
      -> std::shared_ptr<IDocumentStateTransaction> override {
    if (auto trx = getTransaction(doc.tid)) {
      return trx;
    }
    auto trx = std::make_shared<MockDocumentStateTransaction>(doc.tid);
    trx->ensured = true;
    transactions.emplace(doc.tid, trx);
    return trx;
  }

  void removeTransaction(TransactionId tid) override {
    auto trx = getTransaction(tid);
    trx->removed = true;
    transactions.erase(tid);
  }

  auto getTransaction(TransactionId tid)
      -> std::shared_ptr<MockDocumentStateTransaction> {
    if (auto trx = transactions.find(tid); trx != transactions.end()) {
      return trx->second;
    }
    return nullptr;
  }

  std::string database;
  std::unordered_map<TransactionId,
                     std::shared_ptr<MockDocumentStateTransaction>>
      transactions;
};

struct MockDocumentStateHandlersFactory : IDocumentStateHandlersFactory {
  MockDocumentStateHandlersFactory(
      std::shared_ptr<IDocumentStateAgencyHandler> ah,
      std::shared_ptr<IDocumentStateShardHandler> sh,
      MockDocumentStateTransactionHandler*& transactionHandler)
      : agencyHandler(std::move(ah)),
        shardHandler(std::move(sh)),
        transactionHandler(transactionHandler) {}

  auto createAgencyHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateAgencyHandler> override {
    return agencyHandler;
  }

  auto createShardHandler(GlobalLogIdentifier gid)
      -> std::shared_ptr<IDocumentStateShardHandler> override {
    return shardHandler;
  }

  auto createTransactionHandler(GlobalLogIdentifier gid)
      -> std::unique_ptr<IDocumentStateTransactionHandler> override {
    auto th = std::make_unique<MockDocumentStateTransactionHandler>();
    transactionHandler = th.get();
    return std::move(th);
  }

  std::shared_ptr<IDocumentStateAgencyHandler> agencyHandler;
  std::shared_ptr<IDocumentStateShardHandler> shardHandler;
  MockDocumentStateTransactionHandler*& transactionHandler;
};

struct DocumentStateMachineTest : test::ReplicatedLogTest {
  DocumentStateMachineTest() {
    feature->registerStateType<DocumentState>(std::string{DocumentState::NAME},
                                              factory);
  }

  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();
  std::shared_ptr<MockDocumentStateAgencyHandler> agencyHandler =
      std::make_shared<MockDocumentStateAgencyHandler>();
  std::shared_ptr<MockDocumentStateShardHandler> shardHandler =
      std::make_shared<MockDocumentStateShardHandler>();
  MockDocumentStateTransactionHandler* transactionHandler{nullptr};
  std::shared_ptr<IDocumentStateHandlersFactory> factory =
      std::make_shared<MockDocumentStateHandlersFactory>(
          agencyHandler, shardHandler, transactionHandler);
};

TEST_F(DocumentStateMachineTest, simple_operations) {
  const std::string collectionId = "testCollectionID";

  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();

  auto parameters =
      document::DocumentCoreParameters{collectionId, "testDb"}.toSharedSlice();

  auto leaderReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, leaderLog));
  ASSERT_NE(leaderReplicatedState, nullptr);
  leaderReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), parameters);
  follower->runAllAsyncAppendEntries();
  ASSERT_EQ(shardHandler->shardId, 1);
  ASSERT_EQ(agencyHandler->shards.size(), 1);
  ASSERT_EQ(agencyHandler->shards[0].first, "1");
  ASSERT_EQ(agencyHandler->shards[0].second, collectionId);

  auto leaderState = leaderReplicatedState->getLeader();
  ASSERT_NE(leaderState, nullptr);
  ASSERT_EQ(leaderState->shardId, "1");

  auto followerReplicatedState =
      std::dynamic_pointer_cast<ReplicatedState<DocumentState>>(
          feature->createReplicatedState(DocumentState::NAME, followerLog));
  ASSERT_NE(followerReplicatedState, nullptr);
  followerReplicatedState->start(
      std::make_unique<ReplicatedStateToken>(StateGeneration{1}), parameters);
  ASSERT_EQ(shardHandler->shardId, 2);
  ASSERT_EQ(agencyHandler->shards.size(), 2);
  ASSERT_EQ(agencyHandler->shards[1].first, "2");
  ASSERT_EQ(agencyHandler->shards[1].second, collectionId);

  auto followerState = followerReplicatedState->getFollower();
  ASSERT_NE(followerState, nullptr);

  follower->runAllAsyncAppendEntries();

  // insert operation
  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add("testfoo", "testbar");
    ob->close();

    // starting from index 3 because the 2nd is an AbortAllOngoingTrx
    auto logIndex = LogIndex{3};
    auto operation = OperationType::kInsert;
    auto tid = TransactionId{1};
    auto res = leaderState->replicateOperation(builder.sharedSlice(), operation,
                                               tid, ReplicationOptions{});

    ASSERT_TRUE(res.isReady());
    ASSERT_EQ(res.result().get(), logIndex);

    follower->runAllAsyncAppendEntries();
    auto inMemoryLog = leader->copyInMemoryLog();
    auto entry = inMemoryLog.getEntryByIndex(logIndex);
    auto docEntry = velocypack::deserialize<DocumentLogEntry>(
        entry->entry().logPayload()->slice()[1]);
    ASSERT_EQ(docEntry.shardId, "1");
    ASSERT_EQ(docEntry.operation, operation);
    ASSERT_EQ(docEntry.tid, tid);
    ASSERT_EQ(docEntry.data.get("testfoo").stringView(), "testbar");

    auto trx = transactionHandler->getTransaction(tid);
    ASSERT_NE(trx, nullptr);
    ASSERT_TRUE(trx->ensured);
    ASSERT_TRUE(trx->applied);
    ASSERT_FALSE(trx->committed);
    ASSERT_FALSE(trx->aborted);
    ASSERT_FALSE(trx->removed);
  }

  // commit operation
  {
    auto logIndex = LogIndex{4};
    auto operation = OperationType::kCommit;
    auto tid = TransactionId{1};
    auto trx = transactionHandler->getTransaction(tid);
    auto res = leaderState->replicateOperation(
        velocypack::SharedSlice{}, operation, tid,
        ReplicationOptions{.waitForCommit = true});

    ASSERT_FALSE(res.isReady());
    follower->runAllAsyncAppendEntries();
    ASSERT_TRUE(res.isReady());
    ASSERT_EQ(res.result().get(), logIndex);

    follower->runAllAsyncAppendEntries();
    auto inMemoryLog = leader->copyInMemoryLog();
    auto entry = inMemoryLog.getEntryByIndex(logIndex);
    auto docEntry = velocypack::deserialize<DocumentLogEntry>(
        entry->entry().logPayload()->slice()[1]);
    ASSERT_EQ(docEntry.shardId, "1");
    ASSERT_EQ(docEntry.operation, operation);
    ASSERT_EQ(docEntry.tid, tid);
    ASSERT_TRUE(docEntry.data.isNone());

    ASSERT_NE(trx, nullptr);
    ASSERT_TRUE(trx->committed);
    ASSERT_TRUE(trx->removed);
    ASSERT_FALSE(trx->aborted);
  }
}
