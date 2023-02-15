////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "Replication2/StateMachines/Document/DocumentLeaderState.h"

#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Transaction/Manager.h"

#include <Futures/Future.h>
#include <Logger/LogContextKeys.h>

namespace arangodb::replication2::replicated_state::document {

DocumentLeaderState::DocumentLeaderState(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> handlersFactory,
    transaction::IManager& transactionManager)
    : gid(core->getGid()),
      loggerContext(
          core->loggerContext.with<logContextKeyStateComponent>("LeaderState")),
      shardId(core->getShardId()),
      _handlersFactory(std::move(handlersFactory)),
      _snapshotHandler(
          _handlersFactory->createSnapshotHandler(core->getVocbase(), gid)),
      _guardedData(std::move(core)),
      _transactionManager(transactionManager),
      _isResigning(false) {
  ADB_PROD_ASSERT(_snapshotHandler.getLockedGuard().get() != nullptr);
}

auto DocumentLeaderState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  _isResigning.store(true);

  auto core = _guardedData.doUnderLock([&](auto& data) {
    TRI_ASSERT(!data.didResign());
    if (data.didResign()) {
      LOG_CTX("f9c79", ERR, loggerContext)
          << "resigning leader " << gid
          << " is not possible because it is already resigned";
    }
    return std::move(data.core);
  });

  auto snapshotsGuard = _snapshotHandler.getLockedGuard();
  if (auto& snapshotHandler = snapshotsGuard.get(); snapshotHandler) {
    snapshotHandler->clear();
    // The snapshot handler holds a reference to the underlying vocbase. It must
    // not be accessed after a resign, so the database can be dropped safely.
    snapshotHandler.reset();
  } else {
    TRI_ASSERT(false) << "Double-resign in document leader state " << gid;
  }

  _activeTransactions.doUnderLock([&](auto& activeTransactions) {
    for (auto const& trx : activeTransactions.getTransactions()) {
      try {
        _transactionManager.abortManagedTrx(trx.first, gid.database);
      } catch (...) {
        LOG_CTX("7341f", WARN, loggerContext)
            << "failed to abort active transaction " << gid << " during resign";
      }
    }
  });

  return core;
}

auto DocumentLeaderState::recoverEntries(std::unique_ptr<EntryIterator> ptr)
    -> futures::Future<Result> {
  return _guardedData.doUnderLock([self = shared_from_this(),
                                   ptr = std::move(ptr)](
                                      auto& data) -> futures::Future<Result> {
    if (data.didResign()) {
      return TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED;
    }

    auto transactionHandler = self->_handlersFactory->createTransactionHandler(
        data.core->getVocbase(), self->gid);
    ADB_PROD_ASSERT(transactionHandler != nullptr);

    while (auto entry = ptr->next()) {
      auto doc = entry->second;
      if (doc.operation == OperationType::kCreateShard ||
          doc.operation == OperationType::kDropShard) {
        // TODO
        // If the maintenance sees that shards are missing, it is going to
        // create them on the leader.
        // Perhaps we should check that they're already created.
        continue;
      }

      // Note that the shard could've been dropped before doing recovery.
      if (!data.core->isShardAvailable(doc.shardId)) {
        LOG_CTX("1970d", DEBUG, self->loggerContext)
            << "Will not apply transaction " << doc.tid << " for shard "
            << doc.shardId << " because it is not available";
        continue;
      }

      auto res = transactionHandler->applyEntry(doc);
      if (res.fail()) {
        return res;
      }
    }

    auto doc = DocumentLogEntry{{},  // this affects all shards
                                OperationType::kAbortAllOngoingTrx,
                                {},
                                TransactionId{0},
                                {}};
    auto stream = self->getStream();
    stream->insert(doc);

    for (auto& [tid, trx] : transactionHandler->getUnfinishedTransactions()) {
      try {
        // the log entries contain follower ids, which is fine since during
        // recovery we apply the entries like a follower, but we have to
        // register tombstones in the trx managers for the leader trx id.
        self->_transactionManager.abortManagedTrx(tid.asLeaderTransactionId(),
                                                  self->gid.database);
      } catch (...) {
        LOG_CTX("894f1", WARN, self->loggerContext)
            << "failed to abort active transaction " << self->gid
            << " during recovery";
      }
    }

    return {TRI_ERROR_NO_ERROR};
  });
}

auto DocumentLeaderState::replicateOperation(velocypack::SharedSlice payload,
                                             OperationType operation,
                                             TransactionId transactionId,
                                             ShardID shard,
                                             ReplicationOptions opts)
    -> futures::Future<LogIndex> {
  if (_isResigning.load()) {
    LOG_CTX("ffe2f", INFO, loggerContext)
        << "replicateOperation called on a resigning leader, will not "
           "replicate";
    return LogIndex{};  // TODO - can we do this differently instead of
                        // returning a dummy index
  }

  TRI_ASSERT(operation != OperationType::kAbortAllOngoingTrx);
  if ((operation == OperationType::kCommit ||
       operation == OperationType::kAbort) &&
      !_activeTransactions.getLockedGuard()->erase(transactionId)) {
    // we have not replicated anything for a transaction with this id, so
    // there is no need to replicate the abort/commit operation
    return LogIndex{};  // TODO - can we do this differently instead of
                        // returning a dummy index
  }

  auto const& stream = getStream();
  auto entry = DocumentLogEntry{shard,
                                operation,
                                std::move(payload),
                                transactionId.asFollowerTransactionId(),
                                {}};

  // Insert and emplace must happen atomically
  auto idx = _activeTransactions.doUnderLock([&](auto& activeTransactions) {
    auto idx = stream->insert(entry);
    if (operation != OperationType::kCommit &&
        operation != OperationType::kAbort) {
      activeTransactions.emplace(transactionId, idx);
    } else {
      stream->release(activeTransactions.getReleaseIndex(idx));
    }
    return idx;
  });

  if (opts.waitForCommit) {
    return stream->waitFor(idx).thenValue(
        [idx](auto&& result) { return futures::Future<LogIndex>{idx}; });
  }

  return LogIndex{idx};
}

auto DocumentLeaderState::snapshotStart(SnapshotParams::Start const& params)
    -> ResultT<SnapshotBatch> {
  return executeSnapshotOperation<ResultT<SnapshotBatch>>(
      [&](auto& handler) { return handler->create(shardId); },
      [](auto& snapshot) { return snapshot->fetch(); });
}

auto DocumentLeaderState::snapshotNext(SnapshotParams::Next const& params)
    -> ResultT<SnapshotBatch> {
  return executeSnapshotOperation<ResultT<SnapshotBatch>>(
      [&](auto& handler) { return handler->find(params.id); },
      [](auto& snapshot) { return snapshot->fetch(); });
}

auto DocumentLeaderState::snapshotFinish(const SnapshotParams::Finish& params)
    -> Result {
  return executeSnapshotOperation<Result>(
      [&](auto& handler) { return handler->find(params.id); },
      [](auto& snapshot) { return snapshot->finish(); });
}

auto DocumentLeaderState::snapshotStatus(SnapshotId id)
    -> ResultT<SnapshotStatus> {
  return executeSnapshotOperation<ResultT<SnapshotStatus>>(
      [&](auto& handler) { return handler->find(id); },
      [](auto& snapshot) { return snapshot->status(); });
}

auto DocumentLeaderState::allSnapshotsStatus() -> ResultT<AllSnapshotsStatus> {
  return _snapshotHandler.doUnderLock(
      [&](auto& handler) -> ResultT<AllSnapshotsStatus> {
        if (handler) {
          return handler->status();
        }
        return Result{
            TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED,
            fmt::format(
                "Could not get snapshot statuses of {}, state resigned.",
                to_string(gid))};
      });
}

auto DocumentLeaderState::createShard(ShardID shard, CollectionID collectionId,
                                      velocypack::SharedSlice properties)
    -> futures::Future<Result> {
  DocumentLogEntry entry;
  entry.operation = OperationType::kCreateShard;
  entry.shardId = shard;
  entry.data = properties;
  entry.collectionId = collectionId;

  // TODO actually we have to block this log entry from release until the
  // collection is actually created
  auto const& stream = getStream();
  auto idx = stream->insert(entry);

  return stream->waitFor(idx).thenValue([=, self = shared_from_this()](auto&&) {
    return _guardedData.doUnderLock([&](auto& data) -> Result {
      if (data.didResign()) {
        return TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED;
      }
      return data.core->createShard(std::move(shard), std::move(collectionId),
                                    std::move(properties));
    });
  });
}

template<class ResultType, class GetFunc, class ProcessFunc>
auto DocumentLeaderState::executeSnapshotOperation(GetFunc getSnapshot,
                                                   ProcessFunc processSnapshot)
    -> ResultType {
  auto snapshotRes = _snapshotHandler.doUnderLock(
      [&](auto& handler) -> ResultT<std::weak_ptr<Snapshot>> {
        if (handler == nullptr) {
          return Result{
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED,
              fmt::format(
                  "Could not get snapshot statuses of {}, state resigned.",
                  to_string(gid))};
        }
        if (_isResigning.load()) {
          return ResultT<std::weak_ptr<Snapshot>>::error(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED,
              fmt::format("Leader resigned for shard {}", shardId));
        }
        return getSnapshot(handler);
      });

  if (snapshotRes.fail()) {
    return snapshotRes.result();
  }
  if (auto snapshot = snapshotRes.get().lock()) {
    return processSnapshot(snapshot);
  }
  return Result(
      TRI_ERROR_INTERNAL,
      fmt::format("Snapshot not available for {}! Most often this happens "
                  "because the leader resigned in the meantime!",
                  shardId));
}

}  // namespace arangodb::replication2::replicated_state::document

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
