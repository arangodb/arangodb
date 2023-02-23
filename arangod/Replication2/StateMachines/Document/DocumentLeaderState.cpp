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
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
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

// TODO make sure we call release during recovery
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

      auto res = std::visit(
          [&](auto& op) -> Result {
            using T = std::decay_t<decltype(op)>;
            if constexpr (ModifiesUserTransaction<T>) {
              // Note that the shard could've been dropped before doing
              // recovery.
              if (!data.core->isShardAvailable(op.shard)) {
                LOG_CTX("bee57", INFO, self->loggerContext)
                    << "will not apply transaction " << op.tid << " for shard "
                    << op.shard << " because it is not available";
                return {};
              }

              return transactionHandler->applyEntry(doc);
            } else if constexpr (
                std::is_same_v<T, ReplicatedOperation::Commit> ||
                std::is_same_v<T, ReplicatedOperation::Abort> ||
                std::is_same_v<T, ReplicatedOperation::IntermediateCommit> ||
                std::is_same_v<T, ReplicatedOperation::AbortAllOngoingTrx>) {
              return transactionHandler->applyEntry(doc);
            } else if constexpr (std::is_same_v<
                                     T, ReplicatedOperation::CreateShard>) {
              return data.core->ensureShard(op.shard, op.collection,
                                            op.properties);
            } else if constexpr (std::is_same_v<
                                     T, ReplicatedOperation::DropShard>) {
              transactionHandler->abortTransactionsForShard(op.shard);
              return data.core->dropShard(op.shard, op.collection);
            } else {
              return Result{
                  TRI_ERROR_INTERNAL,
                  fmt::format("unknown operation: {}", doc.operation)};
            }
          },
          doc.getInnerOperation());
      if (res.fail()) {
        LOG_CTX("0aa2e", FATAL, self->loggerContext)
            << "failed to apply entry " << doc << " during recovery: " << res;
        FATAL_ERROR_EXIT();
      }
    }

    auto abortAll = DocumentLogEntry{
        ReplicatedOperation::buildAbortAllOngoingTrxOperation()};
    auto stream = self->getStream();
    stream->insert(abortAll);

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

// TODO make everything go through replicateOperation, including AbortAll and
// shard operations
auto DocumentLeaderState::replicateOperation(ReplicatedOperation op,
                                             ReplicationOptions opts)
    -> futures::Future<LogIndex> {
  if (_isResigning.load()) {
    LOG_CTX("ffe2f", INFO, loggerContext)
        << "replicateOperation called on a resigning leader, will not "
           "replicate";
    return LogIndex{};  // TODO - can we do this differently instead of
                        // returning a dummy index
  }

  // No user transaction should ever try to replicate an AbortAllOngoingTrx
  // operation.
  TRI_ASSERT(!std::holds_alternative<ReplicatedOperation::AbortAllOngoingTrx>(
      op.operation));

  std::visit(
      [&](auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ReplicatedOperation::Commit> ||
                      std::is_same_v<T, ReplicatedOperation::Abort> ||
                      std::is_same_v<T,
                                     ReplicatedOperation::IntermediateCommit> ||
                      std::is_same_v<T, ReplicatedOperation::Truncate> ||
                      std::is_same_v<T, ReplicatedOperation::Insert> ||
                      std::is_same_v<T, ReplicatedOperation::Update> ||
                      std::is_same_v<T, ReplicatedOperation::Replace> ||
                      std::is_same_v<T, ReplicatedOperation::Remove>) {
          arg.tid = arg.tid.asFollowerTransactionId();
        }
      },
      op.operation);

  auto needsReplication = std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ReplicatedOperation::Commit> ||
                      std::is_same_v<T, ReplicatedOperation::Abort>) {
          // We don't replicate single abort/commit operations in case we have
          // not replicated anything else for a transaction.
          return _activeTransactions.getLockedGuard()->erase(arg.tid);
        } else {
          return true;
        }
      },
      op.operation);

  if (!needsReplication) {
    // we have not replicated anything for a transaction with this id, so
    // there is no need to replicate the abort/commit operation
    return LogIndex{};  // TODO - can we do this differently instead of
                        // returning a dummy index
  }

  auto const& stream = getStream();
  auto entry = DocumentLogEntry{op};

  // Insert and emplace must happen atomically
  auto idx = _activeTransactions.doUnderLock([&](auto& activeTransactions) {
    auto idx = stream->insert(entry);

    std::visit(
        [&](auto& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, ReplicatedOperation::Truncate> ||
                        std::is_same_v<T, ReplicatedOperation::Insert> ||
                        std::is_same_v<T, ReplicatedOperation::Update> ||
                        std::is_same_v<T, ReplicatedOperation::Replace> ||
                        std::is_same_v<
                            T, ReplicatedOperation::IntermediateCommit> ||
                        std::is_same_v<T, ReplicatedOperation::Remove>) {
            activeTransactions.emplace(arg.tid, idx);
          } else if (std::is_same_v<T, ReplicatedOperation::Commit> ||
                     std::is_same_v<T, ReplicatedOperation::Abort>) {
            // TODO - must not release commit entry before trx has actually been
            // committed!
            stream->release(activeTransactions.getReleaseIndex(idx));
          }
        },
        op.operation);

    return idx;
  });

  if (opts.waitForCommit) {
    return stream->waitFor(idx).thenValue(
        [idx](auto&& result) { return futures::Future<LogIndex>{idx}; });
  }

  return LogIndex{idx};
}

auto DocumentLeaderState::snapshotStart(SnapshotParams::Start const&)
    -> ResultT<SnapshotConfig> {
  auto const& shardMap = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }
    return data.core->getShardMap();
  });

  return executeSnapshotOperation<ResultT<SnapshotConfig>>(
      [&](auto& handler) { return handler->create(shardMap); },
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
  auto fut = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }

    auto entry =
        DocumentLogEntry{ReplicatedOperation::buildCreateShardOperation(
            shard, collectionId, properties)};

    // TODO actually we have to block this log entry from release until the
    // collection is actually created
    auto const& stream = getStream();
    auto idx = stream->insert(entry);

    return stream->waitFor(idx);
  });

  return std::move(fut).thenValue(
      [self = shared_from_this(), shard = std::move(shard),
       collectionId = std::move(collectionId),
       properties = std::move(properties)](auto&&) mutable {
        return self->_guardedData.doUnderLock([&](auto& data) -> Result {
          if (data.didResign()) {
            return TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED;
          }
          return data.core->createShard(
              std::move(shard), std::move(collectionId), std::move(properties));
        });
      });
}

auto DocumentLeaderState::dropShard(ShardID shard, CollectionID collectionId)
    -> futures::Future<Result> {
  auto fut = _guardedData.doUnderLock([&](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
    }

    ReplicatedOperation op =
        ReplicatedOperation::buildDropShardOperation(collectionId, shard);
    auto entry = DocumentLogEntry{op};

    // TODO actually we have to block this log entry from release until the
    // collection is actually created
    auto const& stream = getStream();
    auto idx = stream->insert(entry);
    return stream->waitFor(idx);
  });

  return std::move(fut).thenValue(
      [self = shared_from_this(), shard = std::move(shard),
       collectionId = std::move(collectionId)](auto&&) mutable {
        return self->_guardedData.doUnderLock([&](auto& data) -> Result {
          if (data.didResign()) {
            return TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED;
          }
          // TODO we clear snapshot here, to release the shard lock. This is
          //   very invasive and aborts all snapshot transfers. Maybe there is
          //   a better solution?
          self->_snapshotHandler.getLockedGuard().get()->clear();
          auto result =
              data.core->dropShard(std::move(shard), std::move(collectionId));
          return result;
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
                  gid)};
        }
        if (_isResigning.load()) {
          return ResultT<std::weak_ptr<Snapshot>>::error(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED,
              fmt::format("Leader resigned for state {}", gid));
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
                  gid));
}

}  // namespace arangodb::replication2::replicated_state::document

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
