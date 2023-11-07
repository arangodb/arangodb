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

#include "Replication2/StateMachines/Document/DocumentFollowerState.h"

#include "Replication2/StateMachines/Document/DocumentLogEntry.h"
#include "Replication2/StateMachines/Document/DocumentStateErrorHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "VocBase/LogicalCollection.h"

#include <Basics/application-exit.h>
#include <Basics/Exceptions.h>
#include <Futures/Future.h>
#include <Logger/LogContextKeys.h>

namespace arangodb::replication2::replicated_state::document {

DocumentFollowerState::GuardedData::GuardedData(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory,
    LoggerContext const& loggerContext,
    std::shared_ptr<IDocumentStateErrorHandler> errorHandler)
    : loggerContext(loggerContext),
      errorHandler(std::move(errorHandler)),
      core(std::move(core)),
      currentSnapshotVersion{0},
      shardHandler(handlersFactory->createShardHandler(this->core->getVocbase(),
                                                       this->core->gid)),
      transactionHandler(handlersFactory->createTransactionHandler(
          this->core->getVocbase(), this->core->gid, shardHandler)) {}

DocumentFollowerState::DocumentFollowerState(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory)
    : gid(core->gid),
      loggerContext(handlersFactory->createLogger(core->gid)
                        .with<logContextKeyStateComponent>("FollowerState")),
      _networkHandler(handlersFactory->createNetworkHandler(core->gid)),
      _shardHandler(
          handlersFactory->createShardHandler(core->getVocbase(), core->gid)),
      _errorHandler(handlersFactory->createErrorHandler(core->gid)),
      _guardedData(std::move(core), handlersFactory, loggerContext,
                   _errorHandler) {}

DocumentFollowerState::~DocumentFollowerState() = default;

auto DocumentFollowerState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  _resigning = true;
  return _guardedData.doUnderLock([&](auto& data) {
    ADB_PROD_ASSERT(!data.didResign())
        << "Follower " << gid << " already resigned!";

    auto abortAllRes = data.transactionHandler->applyEntry(
        ReplicatedOperation::buildAbortAllOngoingTrxOperation());
    ADB_PROD_ASSERT(abortAllRes.ok())
        << "Failed to abort ongoing transactions while resigning follower "
        << gid << ": " << abortAllRes;

    LOG_CTX("ed901", DEBUG, loggerContext)
        << "All ongoing transactions were aborted, as follower  resigned";

    return std::move(data.core);
  });
}

auto DocumentFollowerState::getAssociatedShardList() const
    -> std::vector<ShardID> {
  auto collections = _shardHandler->getAvailableShards();

  std::vector<ShardID> shardIds;
  shardIds.reserve(collections.size());
  for (auto const& shard : collections) {
    shardIds.emplace_back(shard->name());
  }
  return shardIds;
}

auto DocumentFollowerState::acquireSnapshot(
    ParticipantId const& destination) noexcept -> futures::Future<Result> {
  LOG_CTX("1f67d", INFO, loggerContext)
      << "Trying to acquire snapshot from destination " << destination;

  auto snapshotVersion = _guardedData.doUnderLock(
      [self =
           shared_from_this()](auto& data) noexcept -> ResultT<std::uint64_t> {
        if (data.didResign()) {
          return Result{TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }

        if (auto abortAllRes = data.transactionHandler->applyEntry(
                ReplicatedOperation::buildAbortAllOngoingTrxOperation());
            abortAllRes.fail()) {
          LOG_CTX("c863a", ERR, self->loggerContext)
              << "Failed to abort ongoing transactions before acquiring "
                 "snapshot: "
              << abortAllRes;
          return abortAllRes;
        }
        LOG_CTX("529bb", DEBUG, self->loggerContext)
            << "All ongoing transactions aborted before acquiring snapshot";

        if (auto dropAllRes = self->_shardHandler->dropAllShards();
            dropAllRes.fail()) {
          LOG_CTX("ae182", ERR, self->loggerContext)
              << "Failed to drop shards before acquiring snapshot: "
              << dropAllRes;
          return dropAllRes;
        }

        return ++data.currentSnapshotVersion;
      });

  if (snapshotVersion.fail()) {
    LOG_CTX("5ef29", DEBUG, loggerContext)
        << "Aborting snapshot transfer before contacting destination "
        << destination << ": " << snapshotVersion.result();
    return snapshotVersion.result();
  }

  // A follower may request a snapshot before leadership has been
  // established. A retry will occur in that case.
  auto leader = _networkHandler->getLeaderInterface(destination);
  auto snapshotStartRes =
      basics::catchToResultT([&leader]() { return leader->startSnapshot(); });
  if (snapshotStartRes.fail()) {
    LOG_CTX("954e3", DEBUG, loggerContext)
        << "Failed to start snapshot transfer with destination " << destination
        << ": " << snapshotStartRes.result();
    return snapshotStartRes.result();
  }

  return handleSnapshotTransfer(std::nullopt, leader, snapshotVersion.get(),
                                std::move(snapshotStartRes.get()))
      .then([leader, destination, self = shared_from_this()](
                auto&& tryResult) -> futures::Future<Result> {
        auto snapshotTransferResult =
            basics::catchToResultT([&] { return tryResult.get(); });
        if (snapshotTransferResult.fail()) {
          LOG_CTX("0c6d9", ERR, self->loggerContext)
              << "Snapshot transfer failed: "
              << snapshotTransferResult.result();
          return snapshotTransferResult.result();
        }

        if (!snapshotTransferResult->snapshotId.has_value()) {
          TRI_ASSERT(snapshotTransferResult->res.fail()) << self->gid;
          LOG_CTX("85628", ERR, self->loggerContext)
              << "Snapshot transfer failed: " << snapshotTransferResult->res;
          return snapshotTransferResult->res;
        }

        LOG_CTX("b4fcb", DEBUG, self->loggerContext)
            << "Snapshot " << *snapshotTransferResult->snapshotId
            << " data transfer over, will send finish request: "
            << snapshotTransferResult->res;

        auto snapshotFinishRes = basics::catchToResultT(
            [&leader, snapshotId = *snapshotTransferResult->snapshotId]() {
              return leader->finishSnapshot(snapshotId);
            });
        if (snapshotFinishRes.fail()) {
          LOG_CTX("4404d", ERR, self->loggerContext)
              << "Failed to initiate snapshot finishing procedure with "
                 "destination "
              << destination << ": " << snapshotFinishRes.result();
          return snapshotFinishRes.result();
        }

        return std::move(snapshotFinishRes.get())
            .then([snapshotTransferResult =
                       std::move(snapshotTransferResult).get()](auto&& tryRes) {
              auto res = basics::catchToResult([&] { return tryRes.get(); });
              if (res.fail()) {
                LOG_TOPIC("0e168", ERR, Logger::REPLICATION2)
                    << "Failed to finish snapshot "
                    << *snapshotTransferResult.snapshotId << ": " << res;
              }

              LOG_TOPIC("42ffd", DEBUG, Logger::REPLICATION2)
                  << "Successfully sent finish command for snapshot  "
                  << *snapshotTransferResult.snapshotId;

              TRI_ASSERT(snapshotTransferResult.res.fail() ||
                         (snapshotTransferResult.res.ok() &&
                          !snapshotTransferResult.reportFailure))
                  << snapshotTransferResult.res << " "
                  << snapshotTransferResult.reportFailure;

              if (snapshotTransferResult.reportFailure) {
                // Some failures don't need to be reported. For example, it's
                // totally fine for the follower to interrupt a snapshot
                // transfer while resigning, because there's no point in
                // continuing it.
                LOG_TOPIC("2883c", WARN, Logger::REPLICATION2)
                    << "During the processing of snapshot "
                    << *snapshotTransferResult.snapshotId
                    << ", the following problem occurred on the follower: "
                    << snapshotTransferResult.res;
                return snapshotTransferResult.res;
              }

              LOG_TOPIC("d73cb", DEBUG, Logger::REPLICATION2)
                  << "Snapshot " << *snapshotTransferResult.snapshotId
                  << " finished: " << snapshotTransferResult.res;
              return Result{};
            });
      });
}

auto DocumentFollowerState::handleSnapshotTransfer(
    std::optional<SnapshotId> snapshotId,
    std::shared_ptr<IDocumentStateLeaderInterface> leader,
    std::uint64_t snapshotVersion,
    futures::Future<ResultT<SnapshotBatch>>&& snapshotFuture) noexcept
    -> futures::Future<SnapshotTransferResult> {
  return std::move(snapshotFuture)
      .then([weak = weak_from_this(), leader = std::move(leader), snapshotId,
             snapshotVersion](
                futures::Try<ResultT<SnapshotBatch>>&& tryResult) mutable
            -> futures::Future<SnapshotTransferResult> {
        auto catchRes =
            basics::catchToResultT([&] { return std::move(tryResult).get(); });
        if (catchRes.fail()) {
          return SnapshotTransferResult{.res = catchRes.result(),
                                        .reportFailure = true,
                                        .snapshotId = snapshotId};
        }

        auto snapshotRes = catchRes.get();
        if (snapshotRes.fail()) {
          return SnapshotTransferResult{.res = snapshotRes.result(),
                                        .reportFailure = true,
                                        .snapshotId = snapshotId};
        }

        if (snapshotId.has_value()) {
          if (snapshotId != snapshotRes->snapshotId) {
            auto err = fmt::format("Expected snapshot id {} but got {}",
                                   *snapshotId, snapshotRes->snapshotId);
            TRI_ASSERT(snapshotId == snapshotRes->snapshotId) << err;
            return SnapshotTransferResult{
                .res = {TRI_ERROR_INTERNAL, err},
                .reportFailure = true,
                .snapshotId = snapshotRes->snapshotId};
          }
        } else {
          // First batch of this snapshot, we got the ID now.
          snapshotId = snapshotRes->snapshotId;
        }

        auto self = weak.lock();
        if (self == nullptr) {
          // The follower resigned, there is no need to continue.
          return SnapshotTransferResult{
              .res = TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
              .reportFailure = true,
              .snapshotId = snapshotId};
        }

        // Apply operations locally
        bool reportingFailure = false;
        auto applyOperationsRes = self->_guardedData.doUnderLock(
            [&self, &snapshotRes, &reportingFailure,
             snapshotVersion](auto& data) -> Result {
              if (data.didResign() || self->_resigning) {
                reportingFailure = true;
                return TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED;
              }

              // The user may remove and add the server again. The leader
              // might do a compaction which the follower won't notice.
              // Hence, a new snapshot is required. This can happen so
              // quick, that one snapshot transfer is not yet completed
              // before another one is requested. Before populating the
              // shard, we have to make sure there's no new snapshot
              // transfer in progress.
              if (data.currentSnapshotVersion != snapshotVersion) {
                return {TRI_ERROR_INTERNAL,
                        "Snapshot transfer cancelled because a new one was "
                        "started!"};
              }

              LOG_CTX("c1d58", DEBUG, self->loggerContext)
                  << "Trying to apply " << snapshotRes->operations.size()
                  << " operations during snapshot transfer "
                  << snapshotRes->snapshotId;
              LOG_CTX("fcc92", TRACE, self->loggerContext)
                  << snapshotRes->snapshotId
                  << " operations: " << snapshotRes->operations;

              for (auto const& op : snapshotRes->operations) {
                if (auto applyRes = data.transactionHandler->applyEntry(op);
                    applyRes.fail()) {
                  reportingFailure = true;
                  return applyRes;
                }
              }

              return {};
            });
        if (applyOperationsRes.fail()) {
          return SnapshotTransferResult{.res = applyOperationsRes,
                                        .reportFailure = reportingFailure,
                                        .snapshotId = snapshotId};
        }

        // If there are more batches to come, fetch the next one
        if (snapshotRes->hasMore) {
          auto nextBatchRes = basics::catchToResultT([&leader, snapshotId]() {
            return leader->nextSnapshotBatch(*snapshotId);
          });
          if (nextBatchRes.fail()) {
            LOG_CTX("a732f", ERR, self->loggerContext)
                << "Failed to fetch the next batch of snapshot: "
                << *snapshotId;
            return SnapshotTransferResult{.res = nextBatchRes.result(),
                                          .reportFailure = true,
                                          .snapshotId = snapshotId};
          }
          return self->handleSnapshotTransfer(snapshotId, std::move(leader),
                                              snapshotVersion,
                                              std::move(nextBatchRes.get()));
        }

        // Snapshot transfer completed
        LOG_CTX("742df", DEBUG, self->loggerContext)
            << "Leader informed the follower there is no more data to be sent "
               "for snapshot "
            << snapshotRes->snapshotId;
        return SnapshotTransferResult{.snapshotId = snapshotId};
      });
}

auto DocumentFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  auto applyResult = _guardedData.doUnderLock(
      [self = shared_from_this(),
       ptr = std::move(ptr)](auto& data) -> ResultT<std::optional<LogIndex>> {
        if (data.didResign()) {
          return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }

        return basics::catchToResultT([&]() -> std::optional<LogIndex> {
          std::optional<LogIndex> releaseIndex;

          while (auto entry = ptr->next()) {
            if (self->_resigning) {
              // We have not officially resigned yet, but we are about to.
              // So, we can just stop here.
              break;
            }
            auto [index, doc] = *entry;

            auto currentReleaseIndex = std::visit(
                [&data, index = index](
                    auto const& op) -> ResultT<std::optional<LogIndex>> {
                  return data.applyEntry(op, index);
                },
                doc.getInnerOperation());

            if (currentReleaseIndex.fail()) {
              TRI_ASSERT(self->_errorHandler
                             ->handleOpResult(doc.getInnerOperation(),
                                              currentReleaseIndex.result())
                             .fail())
                  << currentReleaseIndex.result()
                  << " should have been already handled for operation "
                  << doc.getInnerOperation()
                  << " during applyEntries of follower " << self->gid;
              LOG_CTX("0aa2e", FATAL, self->loggerContext)
                  << "failed to apply entry " << doc
                  << " on follower: " << currentReleaseIndex.result();
              TRI_ASSERT(false) << currentReleaseIndex.result();
              FATAL_ERROR_EXIT();
            }
            if (currentReleaseIndex->has_value()) {
              releaseIndex = std::move(currentReleaseIndex).get();
            }
          }

          return releaseIndex;
        });
      });

  if (_resigning) {
    return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
  }

  if (applyResult.fail()) {
    return applyResult.result();
  }
  if (applyResult->has_value()) {
    // The follower might have resigned, so we need to be careful when
    // accessing the stream.
    auto releaseRes = basics::catchVoidToResult([&] {
      auto const& stream = getStream();
      stream->release(applyResult->value());
    });
    if (releaseRes.fail()) {
      LOG_CTX("10f07", ERR, loggerContext)
          << "Failed to get stream! " << releaseRes;
    }
  }

  return {TRI_ERROR_NO_ERROR};
}

template<class T>
auto DocumentFollowerState::GuardedData::applyAndRelease(
    T const& op, std::optional<LogIndex> index,
    std::optional<fu2::unique_function<void(Result&&)>> fun)
    -> ResultT<std::optional<LogIndex>> {
  auto originalRes = transactionHandler->applyEntry(op);
  auto res = errorHandler->handleOpResult(op, originalRes);
  if (res.fail()) {
    return res;
  }

  if (fun.has_value()) {
    fun->operator()(std::move(originalRes));
  }

  if (index.has_value()) {
    return ResultT<std::optional<LogIndex>>::success(
        activeTransactions.getReleaseIndex().value_or(*index));
  }

  return ResultT<std::optional<LogIndex>>::success(std::nullopt);
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ModifiesUserTransaction auto const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  activeTransactions.markAsActive(op.tid, index);
  // Will not release the index until the transaction is finished
  return applyAndRelease(op, std::nullopt, [&](Result&& res) {
    if (res.fail()) {
      // If the transaction could not be applied, we have to mark it as inactive
      activeTransactions.markAsInactive(op.tid);
    }
  });
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::IntermediateCommit const& op, LogIndex)
    -> ResultT<std::optional<LogIndex>> {
  if (!activeTransactions.getTransactions().contains(op.tid)) {
    LOG_CTX("b41dc", INFO, loggerContext)
        << "will not apply intermediate commit for transaction " << op.tid
        << " because it is not active";
    return ResultT<std::optional<LogIndex>>{std::nullopt};
  }

  // We don't need to update the release index after an intermediate
  // commit. However, we could release everything in this transaction up
  // to this point and update the start LogIndex of this transaction to
  // the current log index.
  return applyAndRelease(op);
}

auto DocumentFollowerState::GuardedData::applyEntry(
    FinishesUserTransaction auto const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  if (!activeTransactions.getTransactions().contains(op.tid)) {
    // Single commit/abort operations are possible.
    LOG_CTX("cf7ea", INFO, loggerContext)
        << "will not finish transaction " << op.tid
        << " because it is not active";
    return ResultT<std::optional<LogIndex>>{std::nullopt};
  }

  return applyAndRelease(
      op, index, [&](Result&&) { activeTransactions.markAsInactive(op.tid); });
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::AbortAllOngoingTrx const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  // Since everything was aborted, we can release all of it.
  return applyAndRelease(op, index,
                         [&](Result&&) { activeTransactions.clear(); });
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::DropShard const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  // We first have to abort all transactions for this shard.
  // This stunt may seem unnecessary, as the leader counterpart takes care of
  // this by replicating the abort operations. However, because we're currently
  // replicating the "DropShard" operation first, "Abort" operations come later.
  // Hence, we need to abort transactions manually for now.
  for (auto const& tid :
       transactionHandler->getTransactionsForShard(op.shard)) {
    auto abortRes = transactionHandler->applyEntry(
        ReplicatedOperation::buildAbortOperation(tid));
    if (abortRes.fail()) {
      LOG_CTX("aa36c", INFO, loggerContext)
          << "Failed to abort transaction " << tid << " for shard " << op.shard
          << " before dropping the shard: " << abortRes.errorMessage();
      return abortRes;
    }
    activeTransactions.markAsInactive(tid);
  }

  return applyAndRelease(op, index);
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::CreateShard const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  return applyAndRelease(op, index);
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::CreateIndex const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  return applyAndRelease(op, index);
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::DropIndex const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  return applyAndRelease(op, index);
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::ModifyShard const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  // Note that locking the shard is not necessary on the follower.
  // However, we still do it for safety reasons.
  auto origin =
      transaction::OperationOriginREST{"follower collection properties update"};
  auto trxLock = shardHandler->lockShard(op.shard, AccessMode::Type::EXCLUSIVE,
                                         std::move(origin));
  if (trxLock.fail()) {
    auto res = errorHandler->handleOpResult(op, trxLock.result());

    // If the shard was not found, we can ignore this operation and release it.
    if (res.ok()) {
      return ResultT<std::optional<LogIndex>>::success(
          activeTransactions.getReleaseIndex().value_or(index));
    }

    return res;
  }

  return applyAndRelease(op, index);
}

}  // namespace arangodb::replication2::replicated_state::document

#include "Replication2/ReplicatedState/ReplicatedStateImpl.tpp"
