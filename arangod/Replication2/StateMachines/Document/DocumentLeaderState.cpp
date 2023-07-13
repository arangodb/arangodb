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

#include "Basics/application-exit.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshotHandler.h"
#include "Transaction/Manager.h"

#include <Futures/Future.h>
#include <Logger/LogContextKeys.h>

namespace arangodb::replication2::replicated_state::document {

DocumentLeaderState::GuardedData::GuardedData(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory)
    : core(std::move(core)) {
  transactionHandler = handlersFactory->createTransactionHandler(
      this->core->getVocbase(), this->core->gid, this->core->getShardHandler());
}

DocumentLeaderState::DocumentLeaderState(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> handlersFactory,
    transaction::IManager& transactionManager)
    : gid(core->gid),
      loggerContext(
          core->loggerContext.with<logContextKeyStateComponent>("LeaderState")),
      _handlersFactory(std::move(handlersFactory)),
      _snapshotHandler(
          _handlersFactory->createSnapshotHandler(core->getVocbase(), gid)),
      _guardedData(std::move(core), this->_handlersFactory),
      _transactionManager(transactionManager) {}

auto DocumentLeaderState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  auto core = _guardedData.doUnderLock([&](auto& data) {
    ADB_PROD_ASSERT(!data.didResign())
        << "resigning leader " << gid
        << " is not possible because it is already resigned";

    auto abortAllRes = data.transactionHandler->applyEntry(
        ReplicatedOperation::buildAbortAllOngoingTrxOperation());
    ADB_PROD_ASSERT(abortAllRes.ok()) << abortAllRes;

    return std::move(data.core);
  });

  auto snapshotsGuard = _snapshotHandler.getLockedGuard();
  if (auto& snapshotHandler = snapshotsGuard.get(); snapshotHandler) {
    snapshotHandler->clear();
    // The snapshot handler holds a reference to the underlying vocbase. It must
    // not be accessed after resign, so the database can be dropped safely.
    snapshotHandler.reset();
  } else {
    TRI_ASSERT(false) << "Double-resign in document leader state " << gid;
  }

  // We are taking a copy of active transactions to avoid a deadlock in doAbort.
  // doAbort is called within abortManagedTrx below, and requires it's own lock
  // on the guarded data.
  auto activeTransactions = _activeTransactions.copy().getTransactions();
  for (auto const& trx : activeTransactions) {
    try {
      _transactionManager.abortManagedTrx(trx.first, gid.database);
    } catch (...) {
      LOG_CTX("7341f", WARN, loggerContext)
          << "failed to abort active transaction " << trx.first
          << " during resign";
    }
  }

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

    std::unordered_set<TransactionId> activeTransactions;
    while (auto entry = ptr->next()) {
      auto doc = entry->second;

      auto res = std::visit(
          overload{
              [&](ModifiesUserTransaction auto& op) -> Result {
                if (auto res = data.transactionHandler->validate(
                        doc.getInnerOperation());
                    res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
                  // The shard might've been dropped before doing recovery.
                  LOG_CTX("e76f8", INFO, self->loggerContext)
                      << "will not apply transaction " << op.tid
                      << " for shard " << op.shard
                      << " because it is not available";
                  return {};
                } else if (res.fail()) {
                  return res;
                }
                activeTransactions.insert(op.tid);
                return data.transactionHandler->applyEntry(op);
              },
              [&](FinishesUserTransactionOrIntermediate auto& op) -> Result {
                // There are two cases where we can end up here:
                // 1. After recovery, we did not get the beginning of the
                // transaction.
                // 2. We ignored all other operations for this transaction
                // because the shard was dropped.
                if (activeTransactions.erase(op.tid) == 0) {
                  return Result{};
                }
                return data.transactionHandler->applyEntry(op);
              },
              [&](ReplicatedOperation::DropShard& op) -> Result {
                // Abort all transactions for this shard.
                for (auto const& tid :
                     data.transactionHandler->getTransactionsForShard(
                         op.shard)) {
                  activeTransactions.erase(tid);
                  auto abortRes = data.transactionHandler->applyEntry(
                      ReplicatedOperation::buildAbortOperation(tid));
                  if (abortRes.fail()) {
                    LOG_CTX("3eb75", INFO, self->loggerContext)
                        << "failed to abort transaction " << tid
                        << " for shard " << op.shard
                        << " during recovery: " << abortRes;
                    return abortRes;
                  }
                }
                return data.transactionHandler->applyEntry(op);
              },
              [&](auto&& op) {
                return data.transactionHandler->applyEntry(op);
              }},
          doc.getInnerOperation());

      if (res.fail()) {
        LOG_CTX("cbc5b", FATAL, self->loggerContext)
            << "failed to apply entry " << doc << " during recovery: " << res;
        FATAL_ERROR_EXIT();
      }
    }

    auto abortAll = ReplicatedOperation::buildAbortAllOngoingTrxOperation();
    auto abortAllResFut =
        self->replicateOperation(abortAll, ReplicationOptions{});
    // Should finish immediately, because we are not waiting the operation to be
    // committed in the replicated log
    TRI_ASSERT(abortAllResFut.isReady());
    auto abortAllRes = abortAllResFut.get();
    if (abortAllRes.fail()) {
      LOG_CTX("b4217", FATAL, self->loggerContext)
          << "failed to replicate AbortAllOngoingTrx operation during "
             "recovery: "
          << abortAllRes.result();
      FATAL_ERROR_EXIT();
    }

    for (auto& [tid, trx] :
         data.transactionHandler->getUnfinishedTransactions()) {
      try {
        // the log entries contain follower ids, which is fine since during
        // recovery we apply the entries like a follower, but we have to
        // register tombstones in the trx managers for the leader trx id.
        self->_transactionManager.abortManagedTrx(tid.asLeaderTransactionId(),
                                                  self->gid.database);
      } catch (...) {
        LOG_CTX("894f1", WARN, self->loggerContext)
            << "failed to abort active transaction " << tid
            << " during recovery";
      }
    }

    auto abortAllResult = data.transactionHandler->applyEntry(abortAll);
    TRI_ASSERT(abortAllResult.ok());

    self->release(abortAllRes.get());

    return {TRI_ERROR_NO_ERROR};
  });
}

auto DocumentLeaderState::needsReplication(ReplicatedOperation const& op)
    -> bool {
  return std::visit(
      overload{
          [&](FinishesUserTransactionOrIntermediate auto const& op) -> bool {
            // An empty transaction is not active, therefore we can ignore it.
            // We are not replication commit or abort operations for empty
            // transactions.
            return _activeTransactions.getLockedGuard()
                ->getTransactions()
                .contains(op.tid);
          },
          [&](auto&&) -> bool { return true; },
      },
      op.operation);
}

auto DocumentLeaderState::replicateOperation(ReplicatedOperation op,
                                             ReplicationOptions opts)
    -> futures::Future<ResultT<LogIndex>> {
  auto const& stream = getStream();

  // We have to use follower IDs when replicating transactions
  auto entry = DocumentLogEntry{op};
  std::visit(
      [&](auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (UserTransaction<T>) {
          arg.tid = arg.tid.asFollowerTransactionId();
        }
      },
      entry.getInnerOperation());

  // Insert and emplace must happen atomically. The idx is strictly increasing,
  // which is required by markAsActive. Keeping the stream insertion and the
  // markAsActive call under the same lock ensures that we never call
  // markAsActive on a log index that is lower than the latest inserted one.
  auto&& insertionRes = basics::catchToResultT([&] {
    return _activeTransactions.doUnderLock([&](auto& activeTransactions) {
      auto idx = stream->insert(entry, opts.waitForSync);

      std::visit(overload{
                     [&](UserTransaction auto& op) {
                       activeTransactions.markAsActive(op.tid, idx);
                     },
                     [&](auto&&) { activeTransactions.markAsActive(idx); },
                 },
                 op.operation);

      return idx;
    });
  });
  if (insertionRes.fail()) {
    LOG_CTX("ffe2f", ERR, loggerContext)
        << "replicateOperation failed to insert into the stream: "
        << insertionRes.result();
    return insertionRes.result();
  }
  auto idx = insertionRes.get();

  if (opts.waitForCommit) {
    try {
      return stream->waitFor(idx).thenValue([idx](auto&& result) {
        return futures::Future<ResultT<LogIndex>>{idx};
      });
    } catch (basics::Exception const& e) {
      return Result{e.code(), e.what()};
    } catch (std::exception const& e) {
      return Result{TRI_ERROR_INTERNAL, e.what()};
    }
  }

  return LogIndex{idx};
}

auto DocumentLeaderState::release(LogIndex index) -> Result {
  auto const& stream = getStream();
  return basics::catchToResult([&]() -> Result {
    auto idx = _activeTransactions.doUnderLock([&](auto& activeTransactions) {
      activeTransactions.markAsInactive(index);
      return activeTransactions.getReleaseIndex().value_or(index);
    });
    stream->release(idx);
    return {TRI_ERROR_NO_ERROR};
  });
}

auto DocumentLeaderState::release(TransactionId tid, LogIndex index) -> Result {
  auto const& stream = getStream();
  return basics::catchToResult([&]() -> Result {
    auto idx = _activeTransactions.doUnderLock([&](auto& activeTransactions) {
      activeTransactions.markAsInactive(tid);
      return activeTransactions.getReleaseIndex().value_or(index);
    });
    stream->release(idx);
    return {TRI_ERROR_NO_ERROR};
  });
}

auto DocumentLeaderState::snapshotStart(SnapshotParams::Start const& params)
    -> ResultT<SnapshotConfig> {
  auto shardMap = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }
    return data.core->getShardHandler()->getShardMap();
  });

  return executeSnapshotOperation<ResultT<SnapshotConfig>>(
      [&](auto& handler) { return handler->create(shardMap, params); },
      [](auto& snapshot) { return snapshot->config(); });
}

auto DocumentLeaderState::snapshotNext(SnapshotParams::Next const& params)
    -> ResultT<SnapshotBatch> {
  return executeSnapshotOperation<ResultT<SnapshotBatch>>(
      [&](auto& handler) { return handler->find(params.id); },
      [](auto& snapshot) { return snapshot->fetch(); });
}

auto DocumentLeaderState::snapshotFinish(const SnapshotParams::Finish& params)
    -> Result {
  return _snapshotHandler.doUnderLock([&](auto& handler) -> Result {
    if (handler == nullptr) {
      return Result{
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED,
          fmt::format("Could not get snapshot status of {}, state resigned.",
                      gid)};
    }
    return handler->finish(params.id);
  });
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

auto DocumentLeaderState::getAssociatedShardList() const
    -> std::vector<ShardID> {
  auto shardMap =
      _guardedData.getLockedGuard()->core->getShardHandler()->getShardMap();

  std::vector<ShardID> shardIds;
  shardIds.reserve(shardMap.size());
  for (auto const& [id, props] : shardMap) {
    shardIds.emplace_back(id);
  }
  return shardIds;
}

auto DocumentLeaderState::createShard(ShardID shard, CollectionID collectionId,
                                      std::shared_ptr<VPackBuilder> properties)
    -> futures::Future<Result> {
  auto op = ReplicatedOperation::buildCreateShardOperation(
      std::move(shard), std::move(collectionId), std::move(properties));

  auto fut = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }

    return replicateOperation(
        op, ReplicationOptions{.waitForCommit = true, .waitForSync = true});
  });

  return std::move(fut).thenValue([self = shared_from_this(),
                                   op = std::move(op)](auto&& result) mutable {
    if (result.fail()) {
      return result.result();
    }
    auto logIndex = result.get();

    return self->_guardedData.doUnderLock([&](auto& data) -> Result {
      if (data.didResign()) {
        return TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED;
      }
      auto&& applyEntryRes = data.transactionHandler->applyEntry(std::move(op));
      if (applyEntryRes.fail()) {
        LOG_CTX("d11f0", FATAL, self->loggerContext)
            << "CreateShard operation failed on the leader, after being "
               "replicated to followers: "
            << applyEntryRes;
        FATAL_ERROR_EXIT();
      }

      if (auto releaseRes = self->release(logIndex); releaseRes.fail()) {
        LOG_CTX("ed8e5", ERR, self->loggerContext)
            << "Failed to call release: " << releaseRes;
      }
      return {};
    });
  });
}

auto DocumentLeaderState::dropShard(ShardID shard, CollectionID collectionId)
    -> futures::Future<Result> {
  ReplicatedOperation op = ReplicatedOperation::buildDropShardOperation(
      shard, std::move(collectionId));

  auto fut = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }

    return replicateOperation(
        op, ReplicationOptions{.waitForCommit = true, .waitForSync = true});
  });

  return std::move(fut).thenValue([self = shared_from_this(),
                                   shard = std::move(shard),
                                   op = std::move(op)](auto&& result) mutable {
    if (result.fail()) {
      return result.result();
    }
    auto logIndex = result.get();

    return self->_guardedData.doUnderLock([&](auto& data) -> Result {
      if (data.didResign()) {
        return TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED;
      }

      // This will release the shard lock. Currently ongoing snapshot transfers
      // will not suffer from it, they will simply stop receiving batches for
      // this shard. Later, when they start applying entries, it is going to be
      // dropped anyway.
      self->_snapshotHandler.getLockedGuard().get()->giveUpOnShard(shard);

      self->_activeTransactions.doUnderLock([&](auto& activeTransactions) {
        for (auto const& tid :
             data.transactionHandler->getTransactionsForShard(shard)) {
          activeTransactions.markAsInactive(tid);
        }
      });
      auto&& applyEntryRes = data.transactionHandler->applyEntry(std::move(op));
      if (applyEntryRes.fail()) {
        LOG_CTX("6865f", FATAL, self->loggerContext)
            << "DropShard operation failed on the leader, after being "
               "replicated to followers: "
            << applyEntryRes;
        FATAL_ERROR_EXIT();
      }

      if (auto releaseRes = self->release(logIndex); releaseRes.fail()) {
        LOG_CTX("6856b", ERR, self->loggerContext)
            << "Failed to call release: " << releaseRes;
      }
      return {};
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
                  "Could not get snapshot status of {}, state resigned.", gid)};
        }
        return getSnapshot(handler);
      });

  if (snapshotRes.fail()) {
    return snapshotRes.result();
  }
  if (auto snapshot = snapshotRes.get().lock(); snapshot != nullptr) {
    return processSnapshot(snapshot);
  }

  return Result(TRI_ERROR_INTERNAL,
                fmt::format("Snapshot not available for {}!", gid));
}

}  // namespace arangodb::replication2::replicated_state::document

#include "Replication2/ReplicatedState/ReplicatedStateImpl.tpp"
