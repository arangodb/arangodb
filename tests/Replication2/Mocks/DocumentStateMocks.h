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

#include <gmock/gmock.h>

#include "Replication2/StateMachines/Document/CollectionReader.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshotHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/ShardProperties.h"
#include "Replication2/StateMachines/Document/MaintenanceActionExecutor.h"

#include "Cluster/ClusterTypes.h"
#include "Transaction/Manager.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"

namespace arangodb::replication2::tests {
struct MockDocumentStateTransactionHandler;
struct MockDocumentStateSnapshotHandler;

struct MockTransactionManager : transaction::IManager {
  MOCK_METHOD(Result, abortManagedTrx,
              (TransactionId, std::string const& database), (override));
};

struct MockCollectionReader : replicated_state::document::ICollectionReader {
  explicit MockCollectionReader(std::vector<std::string> const& data);

  MOCK_METHOD(bool, hasMore, (), (override));
  MOCK_METHOD(std::optional<uint64_t>, getDocCount, (), (override));
  MOCK_METHOD(void, read, (VPackBuilder & builder, std::size_t softLimit),
              (override));

  void reset();

  std::size_t controlledSoftLimit;

 private:
  std::vector<std::string> const& _data;
  std::size_t _idx;
};

/*
 * Only servers as a way to return a unique_ptr, allowing us to use the
 * shared mock.
 */
struct MockCollectionReaderDelegator
    : replicated_state::document::ICollectionReader {
  explicit MockCollectionReaderDelegator(
      std::shared_ptr<ICollectionReader> reader)
      : _reader(std::move(reader)) {}

  bool hasMore() override { return _reader->hasMore(); }
  std::optional<uint64_t> getDocCount() override {
    return _reader->getDocCount();
  }
  void read(VPackBuilder& builder, std::size_t softLimit) override {
    return _reader->read(builder, softLimit);
  }

 private:
  std::shared_ptr<ICollectionReader> _reader;
};

struct MockDatabaseSnapshot : replicated_state::document::IDatabaseSnapshot {
  explicit MockDatabaseSnapshot(
      std::shared_ptr<replicated_state::document::ICollectionReader> reader);

  MOCK_METHOD(std::unique_ptr<replicated_state::document::ICollectionReader>,
              createCollectionReader, (std::string_view collectionName),
              (override));

  MOCK_METHOD(Result, resetTransaction, (), (override));

 private:
  std::shared_ptr<replicated_state::document::ICollectionReader> _reader;
};

struct MockDatabaseSnapshotDelegator
    : replicated_state::document::IDatabaseSnapshot {
  explicit MockDatabaseSnapshotDelegator(
      std::shared_ptr<replicated_state::document::IDatabaseSnapshot> snapshot)
      : _snapshot(std::move(snapshot)) {}

  std::unique_ptr<replicated_state::document::ICollectionReader>
  createCollectionReader(std::string_view collectionName) override {
    return _snapshot->createCollectionReader(collectionName);
  }

  auto resetTransaction() -> Result override {
    return _snapshot->resetTransaction();
  }

 private:
  std::shared_ptr<replicated_state::document::IDatabaseSnapshot> _snapshot;
};

struct MockDatabaseSnapshotFactory
    : replicated_state::document::IDatabaseSnapshotFactory {
  MOCK_METHOD(std::unique_ptr<replicated_state::document::IDatabaseSnapshot>,
              createSnapshot, (), (override));
};

struct MockDatabaseSnapshotFactoryDelegator
    : replicated_state::document::IDatabaseSnapshotFactory {
  explicit MockDatabaseSnapshotFactoryDelegator(
      std::shared_ptr<replicated_state::document::IDatabaseSnapshotFactory>
          factory)
      : _factory(std::move(factory)) {}

  std::unique_ptr<replicated_state::document::IDatabaseSnapshot>
  createSnapshot() override {
    return _factory->createSnapshot();
  }

 private:
  std::shared_ptr<IDatabaseSnapshotFactory> _factory;
};

struct MockDocumentStateHandlersFactory
    : replicated_state::document::IDocumentStateHandlersFactory,
      std::enable_shared_from_this<MockDocumentStateHandlersFactory> {
  explicit MockDocumentStateHandlersFactory(
      std::shared_ptr<MockDatabaseSnapshotFactory> snapshotFactory);

  MOCK_METHOD(
      std::shared_ptr<replicated_state::document::IDocumentStateShardHandler>,
      createShardHandler, (TRI_vocbase_t&, GlobalLogIdentifier), (override));
  MOCK_METHOD(std::shared_ptr<
                  replicated_state::document::IDocumentStateSnapshotHandler>,
              createSnapshotHandler,
              (TRI_vocbase_t&, GlobalLogIdentifier const&), (override));
  MOCK_METHOD(
      std::unique_ptr<
          replicated_state::document::IDocumentStateTransactionHandler>,
      createTransactionHandler,
      (TRI_vocbase_t & vocbase, GlobalLogIdentifier,
       std::shared_ptr<replicated_state::document::IDocumentStateShardHandler>),
      (override));
  MOCK_METHOD(
      std::shared_ptr<replicated_state::document::IDocumentStateTransaction>,
      createTransaction,
      (TRI_vocbase_t&, TransactionId, ShardID const&, AccessMode::Type),
      (override));
  MOCK_METHOD(
      std::shared_ptr<replicated_state::document::IDocumentStateNetworkHandler>,
      createNetworkHandler, (GlobalLogIdentifier), (override));
  MOCK_METHOD(
      std::shared_ptr<replicated_state::document::IMaintenanceActionExecutor>,
      createMaintenanceActionExecutor, (GlobalLogIdentifier, ServerID),
      (override));

  auto makeUniqueDatabaseSnapshotFactory()
      -> std::unique_ptr<replicated_state::document::IDatabaseSnapshotFactory>;
  auto makeRealSnapshotHandler(cluster::RebootTracker* rebootTracker = nullptr)
      -> std::shared_ptr<MockDocumentStateSnapshotHandler>;
  auto makeRealTransactionHandler(
      TRI_vocbase_t* vocbase, GlobalLogIdentifier const&,
      std::shared_ptr<replicated_state::document::IDocumentStateShardHandler>)
      -> std::shared_ptr<MockDocumentStateTransactionHandler>;

  std::shared_ptr<MockDatabaseSnapshotFactory> databaseSnapshotFactory;
};

struct MockDocumentStateTransaction
    : replicated_state::document::IDocumentStateTransaction {
  MOCK_METHOD(
      OperationResult, apply,
      (replicated_state::document::ReplicatedOperation::OperationType const&),
      (override));
  MOCK_METHOD(Result, intermediateCommit, (), (override));
  MOCK_METHOD(Result, commit, (), (override));
  MOCK_METHOD(Result, abort, (), (override));
  MOCK_METHOD(bool, containsShard, (ShardID const&), (override));
};

struct MockDocumentStateTransactionHandler
    : replicated_state::document::IDocumentStateTransactionHandler {
  /*
   * Can be used to delegate function calls to a real instance of
   * DocumentStateTransactionHandler.
   */
  explicit MockDocumentStateTransactionHandler(
      std::shared_ptr<
          replicated_state::document::IDocumentStateTransactionHandler>
          real);

  MOCK_METHOD(Result, applyEntry,
              (replicated_state::document::ReplicatedOperation), (override));
  MOCK_METHOD(
      Result, applyEntry,
      (replicated_state::document::ReplicatedOperation::OperationType const&),
      (override));
  MOCK_METHOD(void, removeTransaction, (TransactionId tid), (override));
  MOCK_METHOD(std::vector<TransactionId>, getTransactionsForShard,
              (ShardID const&), (override));
  MOCK_METHOD(TransactionMap const&, getUnfinishedTransactions, (),
              (const, override));
  MOCK_METHOD(
      Result, validate,
      (replicated_state::document::ReplicatedOperation::OperationType const&),
      (const, override));

 private:
  std::shared_ptr<replicated_state::document::IDocumentStateTransactionHandler>
      _real;
};

struct MockMaintenanceActionExecutor
    : replicated_state::document::IMaintenanceActionExecutor {
  MOCK_METHOD(Result, executeCreateCollectionAction,
              (ShardID, CollectionID, std::shared_ptr<VPackBuilder>),
              (override));
  MOCK_METHOD(Result, executeDropCollectionAction, (ShardID, CollectionID),
              (override));
  MOCK_METHOD(void, addDirty, (), (override));
};

struct MockDocumentStateShardHandler
    : replicated_state::document::IDocumentStateShardHandler {
  MOCK_METHOD(ResultT<bool>, ensureShard,
              (ShardID, CollectionID, std::shared_ptr<VPackBuilder>),
              (override));
  MOCK_METHOD(ResultT<bool>, dropShard, (ShardID const&), (override));
  MOCK_METHOD(Result, dropAllShards, (), (override));
  MOCK_METHOD(bool, isShardAvailable, (ShardID const&), (override));
  MOCK_METHOD(replicated_state::document::ShardMap, getShardMap, (),
              (override));
};

struct MockDocumentStateSnapshotHandler
    : replicated_state::document::IDocumentStateSnapshotHandler {
  /*
   * Can be used to delegate function calls to a real instance of
   * DocumentStateSnapshotHandler.
   */
  explicit MockDocumentStateSnapshotHandler(
      std::shared_ptr<replicated_state::document::IDocumentStateSnapshotHandler>
          real);

  MOCK_METHOD(ResultT<std::weak_ptr<replicated_state::document::Snapshot>>,
              create,
              (replicated_state::document::ShardMap,
               replicated_state::document::SnapshotParams::Start const& params),
              (override));
  MOCK_METHOD(ResultT<std::weak_ptr<replicated_state::document::Snapshot>>,
              find, (replicated_state::document::SnapshotId const&),
              (override, noexcept));
  MOCK_METHOD(replicated_state::document::AllSnapshotsStatus, status, (),
              (const, override));
  MOCK_METHOD(Result, abort, (replicated_state::document::SnapshotId const& id),
              (override));
  MOCK_METHOD(Result, finish,
              (replicated_state::document::SnapshotId const& id), (override));
  MOCK_METHOD(void, clear, (), (override));
  MOCK_METHOD(void, giveUpOnShard, (ShardID const& shardId), (override));

  static cluster::RebootTracker rebootTracker;

 private:
  std::shared_ptr<replicated_state::document::IDocumentStateSnapshotHandler>
      _real;
};

struct MockDocumentStateLeaderInterface
    : replicated_state::document::IDocumentStateLeaderInterface {
  MOCK_METHOD(
      futures::Future<ResultT<replicated_state::document::SnapshotConfig>>,
      startSnapshot, (), (override));
  MOCK_METHOD(
      futures::Future<ResultT<replicated_state::document::SnapshotBatch>>,
      nextSnapshotBatch, (replicated_state::document::SnapshotId), (override));
  MOCK_METHOD(futures::Future<Result>, finishSnapshot,
              (replicated_state::document::SnapshotId), (override));
};

struct MockDocumentStateNetworkHandler
    : replicated_state::document::IDocumentStateNetworkHandler {
  MOCK_METHOD(std::shared_ptr<
                  replicated_state::document::IDocumentStateLeaderInterface>,
              getLeaderInterface, (ParticipantId), (override, noexcept));
};

/*
 * A wrapper to make some protected methods public.
 */
struct DocumentFollowerStateWrapper
    : replicated_state::document::DocumentFollowerState {
  DocumentFollowerStateWrapper(
      std::unique_ptr<replicated_state::document::DocumentCore> core,
      std::shared_ptr<
          replicated_state::document::IDocumentStateHandlersFactory> const&
          handlersFactory)
      : DocumentFollowerState(std::move(core), handlersFactory) {}

  auto resign() && noexcept
      -> std::unique_ptr<replicated_state::document::DocumentCore> override {
    return std::move(*this).DocumentFollowerState::resign();
  }

  auto acquireSnapshot(ParticipantId const& destination,
                       LogIndex waitForIndex) noexcept
      -> futures::Future<Result> override {
    return DocumentFollowerState::acquireSnapshot(destination, waitForIndex);
  }

  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override {
    return DocumentFollowerState::applyEntries(std::move(ptr));
  }
};

/*
 * A wrapper to make some protected methods public.
 */
struct DocumentLeaderStateWrapper
    : replicated_state::document::DocumentLeaderState {
  DocumentLeaderStateWrapper(
      std::unique_ptr<replicated_state::document::DocumentCore> core,
      std::shared_ptr<replicated_state::document::IDocumentStateHandlersFactory>
          handlersFactory,
      transaction::IManager& transactionManager)
      : DocumentLeaderState(std::move(core), std::move(handlersFactory),
                            transactionManager) {}

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<replicated_state::document::DocumentCore> override {
    return std::move(*this).DocumentLeaderState::resign();
  }

  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override {
    return DocumentLeaderState::recoverEntries(std::move(ptr));
  }
};

struct MockProducerStream
    : streams::ProducerStream<replicated_state::document::DocumentLogEntry> {
  // Stream<T>
  MOCK_METHOD(futures::Future<WaitForResult>, waitFor, (LogIndex), (override));
  MOCK_METHOD(futures::Future<std::unique_ptr<Iterator>>, waitForIterator,
              (LogIndex), (override));
  MOCK_METHOD(void, release, (LogIndex), (override));
  // ProducerStream<T>
  MOCK_METHOD(LogIndex, insert,
              (replicated_state::document::DocumentLogEntry const&),
              (override));

  MockProducerStream() {
    ON_CALL(*this, insert)
        .WillByDefault(
            [this](replicated_state::document::DocumentLogEntry const& doc) {
              auto idx = current;
              current += 1;
              entries[idx] = doc;
              return idx;
            });
  }

  LogIndex current{1};
  std::map<LogIndex, replicated_state::document::DocumentLogEntry> entries;
};

struct DocumentLogEntryIterator
    : TypedLogRangeIterator<streams::StreamEntryView<
          replicated_state::document::DocumentLogEntry>> {
  explicit DocumentLogEntryIterator(
      std::vector<replicated_state::document::DocumentLogEntry> entries);

  auto next() -> std::optional<streams::StreamEntryView<
      replicated_state::document::DocumentLogEntry>> override;

  [[nodiscard]] auto range() const noexcept -> LogRange override;

  std::vector<replicated_state::document::DocumentLogEntry> entries;
  decltype(entries)::iterator iter;
};

}  // namespace arangodb::replication2::tests
