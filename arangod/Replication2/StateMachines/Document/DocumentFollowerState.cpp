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
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"

#include <Basics/application-exit.h>
#include <Basics/Exceptions.h>
#include <Futures/Future.h>
#include <Logger/LogContextKeys.h>

using namespace arangodb::replication2::replicated_state::document;

DocumentFollowerState::DocumentFollowerState(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory)
    : loggerContext(core->loggerContext.with<logContextKeyStateComponent>(
          "FollowerState")),
      _networkHandler(handlersFactory->createNetworkHandler(core->gid)),
      _guardedData(std::move(core)) {}

DocumentFollowerState::~DocumentFollowerState() = default;

auto DocumentFollowerState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  return _guardedData.doUnderLock([](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_FOLLOWER);
    }
    return std::move(data.core);
  });
}

auto DocumentFollowerState::acquireSnapshot(ParticipantId const& destination,
                                            LogIndex waitForIndex) noexcept
    -> futures::Future<Result> {
  auto snapshotVersion = _guardedData.doUnderLock(
      [self = shared_from_this()](auto& data) -> ResultT<std::uint64_t> {
        if (data.didResign()) {
          return Result{TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }

        auto const& shardHandler = data.core->getShardHandler();
        if (auto dropAllRes = shardHandler->dropAllShards();
            dropAllRes.fail()) {
          return dropAllRes;
        }
        return ++data.currentSnapshotVersion;
      });

  if (snapshotVersion.fail()) {
    return snapshotVersion.result();
  }

  // A follower may request a snapshot before leadership has been
  // established. A retry will occur in that case.
  auto leader = _networkHandler->getLeaderInterface(destination);
  auto fut = leader->startSnapshot(waitForIndex);
  return handleSnapshotTransfer(leader, waitForIndex, snapshotVersion.get(),
                                std::move(fut))
      .thenValue([leader, self = shared_from_this()](
                     auto&& snapshotTransferResult) -> futures::Future<Result> {
        if (!snapshotTransferResult.snapshotId.has_value()) {
          TRI_ASSERT(snapshotTransferResult.res.fail());
          LOG_CTX("85628", ERR, self->loggerContext)
              << "Snapshot transfer failed: " << snapshotTransferResult.res;
          return snapshotTransferResult.res;
        }
        return leader->finishSnapshot(*snapshotTransferResult.snapshotId)
            .thenValue([snapshotTransferResult](auto&& res) {
              if (res.fail()) {
                LOG_DEVEL << "fuck " << res;
                LOG_TOPIC("0e168", ERR, Logger::REPLICATION2)
                    << "Failed to finish snapshot "
                    << *snapshotTransferResult.snapshotId << ": " << res;
              }

              TRI_ASSERT(snapshotTransferResult.res.fail() ||
                         (snapshotTransferResult.res.ok() &&
                          !snapshotTransferResult.reportFailure));

              if (snapshotTransferResult.reportFailure) {
                return snapshotTransferResult.res;
              }
              return Result{};
            });
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
            auto [index, doc] = *entry;

            auto currentReleaseIndex = std::visit(
                [&data,
                 index = index](auto&& op) -> ResultT<std::optional<LogIndex>> {
                  return data.applyEntry(op, index);
                },
                doc.getInnerOperation());

            if (currentReleaseIndex.fail()) {
              LOG_CTX("0aa2e", FATAL, self->loggerContext)
                  << "failed to apply entry " << doc
                  << " on follower: " << currentReleaseIndex.result();
              FATAL_ERROR_EXIT();
            }
            if (currentReleaseIndex->has_value()) {
              releaseIndex = std::move(currentReleaseIndex).get();
            }
          }

          return releaseIndex;
        });
      });

  if (applyResult.fail()) {
    return applyResult.result();
  }
  if (applyResult->has_value()) {
    // The follower might have resigned, so we need to be careful when accessing
    // the stream.
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

auto DocumentFollowerState::populateLocalShard(
    ShardID shardId, velocypack::SharedSlice slice,
    std::shared_ptr<IDocumentStateTransactionHandler> const& transactionHandler)
    -> Result {
  auto tid = TransactionId::createFollower();
  auto op = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_INSERT, tid, std::move(shardId),
      std::move(slice));
  if (auto applyRes = transactionHandler->applyEntry(op); applyRes.fail()) {
    transactionHandler->removeTransaction(tid);
    return applyRes;
  }
  auto commit = ReplicatedOperation::buildCommitOperation(tid);
  return transactionHandler->applyEntry(commit);
}

auto DocumentFollowerState::handleSnapshotTransfer(
    std::shared_ptr<IDocumentStateLeaderInterface> leader,
    LogIndex waitForIndex, std::uint64_t snapshotVersion,
    futures::Future<ResultT<SnapshotConfig>>&& snapshotFuture) noexcept
    -> futures::Future<SnapshotTransferResult> {
  return std::move(snapshotFuture)
      .then([weak = weak_from_this(), leader = std::move(leader), waitForIndex,
             snapshotVersion](
                futures::Try<ResultT<SnapshotConfig>>&& tryResult) mutable
            -> futures::Future<SnapshotTransferResult> {
        auto catchRes =
            basics::catchToResultT([&] { return std::move(tryResult).get(); });
        if (catchRes.fail()) {
          return SnapshotTransferResult{catchRes.result(), true, {}};
        }

        auto snapshotRes = catchRes.get();
        if (snapshotRes.fail()) {
          return SnapshotTransferResult{.res = snapshotRes.result(),
                                        .reportFailure = true};
        }

        auto self = weak.lock();
        if (self == nullptr) {
          // The follower resigned, there is no need to continue.
          return SnapshotTransferResult{
              .res = TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
              .snapshotId = snapshotRes->snapshotId};
        }

        if (snapshotRes->shards.empty()) {
          // There are no shards, no need to continue.
          return SnapshotTransferResult{.snapshotId = snapshotRes->snapshotId};
        }

        auto res = self->_guardedData.doUnderLock(
            [self, snapshotId = snapshotRes->snapshotId,
             shards = std::move(snapshotRes->shards),
             snapshotVersion](auto& data) -> Result {
              if (data.didResign()) {
                return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
              }

              if (data.currentSnapshotVersion != snapshotVersion) {
                return {TRI_ERROR_INTERNAL,
                        "Snapshot transfer cancelled because a "
                        "new one was started!"};
              }

              auto transactionHandler = data.core->getTransactionHandler();
              for (auto const& [shardId, properties] : shards) {
                auto res = transactionHandler->applyEntry(
                    ReplicatedOperation::buildCreateShardOperation(
                        shardId, properties.collection, properties.properties));
                if (res.fail()) {
                  LOG_CTX("bd17c", ERR, self->loggerContext)
                      << "Failed to ensure shard " << shardId << " " << res;
                  FATAL_ERROR_EXIT();
                }
              }

              return Result{};
            });

        if (res.fail()) {
          // If we got here, it means the snapshot transfer is no longer needed.
          return SnapshotTransferResult{.snapshotId = snapshotRes->snapshotId};
        }

        auto fut = leader->nextSnapshotBatch(snapshotRes->snapshotId);
        return self->handleSnapshotTransfer(snapshotRes->snapshotId,
                                            std::move(leader), waitForIndex,
                                            snapshotVersion, std::move(fut));
      });
}

auto DocumentFollowerState::handleSnapshotTransfer(
    SnapshotId snapshotId,
    std::shared_ptr<IDocumentStateLeaderInterface> leader,
    LogIndex waitForIndex, std::uint64_t snapshotVersion,
    futures::Future<ResultT<SnapshotBatch>>&& snapshotFuture) noexcept
    -> futures::Future<SnapshotTransferResult> {
  return std::move(snapshotFuture)
      .then([weak = weak_from_this(), leader = std::move(leader), waitForIndex,
             snapshotVersion, snapshotId](
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

        TRI_ASSERT(snapshotRes->snapshotId == snapshotId);

        auto self = weak.lock();
        if (self == nullptr) {
          // The follower resigned, no need to continue the transfer.
          return SnapshotTransferResult{.snapshotId = snapshotId};
        }

        if (snapshotRes->shardId.has_value()) {
          bool reportingFailure = false;
          auto insertRes = self->_guardedData.doUnderLock([&self, &snapshotRes,
                                                           &reportingFailure,
                                                           snapshotVersion](
                                                              auto& data)
                                                              -> Result {
            if (data.didResign()) {
              return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
            }

            // The user may remove and add the server again. The leader
            // might do a compaction which the follower won't notice. Hence, a
            // new snapshot is required. This can happen so quick, that one
            // snapshot transfer is not yet completed before another one is
            // requested. Before populating the shard, we have to make sure
            // there's no new snapshot transfer in progress.
            if (data.currentSnapshotVersion != snapshotVersion) {
              return {
                  TRI_ERROR_INTERNAL,
                  "Snapshot transfer cancelled because a new one was started!"};
            }

            if (auto localShardRes = self->populateLocalShard(
                    *snapshotRes->shardId, snapshotRes->payload,
                    data.core->getTransactionHandler());
                localShardRes.fail()) {
              reportingFailure = true;
              return localShardRes;
            }

            return Result{};
          });
          if (insertRes.fail()) {
            return SnapshotTransferResult{.res = insertRes,
                                          .reportFailure = reportingFailure,
                                          .snapshotId = snapshotId};
          }
        } else {
          TRI_ASSERT(!snapshotRes->hasMore);
        }

        if (snapshotRes->hasMore) {
          auto fut = leader->nextSnapshotBatch(snapshotId);
          return self->handleSnapshotTransfer(snapshotId, std::move(leader),
                                              waitForIndex, snapshotVersion,
                                              std::move(fut));
        }

        return SnapshotTransferResult{.snapshotId = snapshotId};
      });
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ModifiesUserTransaction auto const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  auto const& transactionHandler = core->getTransactionHandler();
  if (auto validationRes = transactionHandler->validate(op);
      validationRes.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    //  Even though a shard was dropped before acquiring the
    //  snapshot, we could still see transactions referring to
    //  that shard.
    LOG_CTX("e1edb", INFO, core->loggerContext)
        << "will not apply transaction " << op.tid << " on shard " << op.shard
        << " because the shard is not available";
    return ResultT<std::optional<LogIndex>>::success(std::nullopt);
  } else if (validationRes.fail()) {
    return validationRes;
  }

  activeTransactions.markAsActive(op.tid, index);
  if (auto res = transactionHandler->applyEntry(op); res.fail()) {
    return res;
  }

  // We don't need to update the release index after such operations.
  return ResultT<std::optional<LogIndex>>::success(std::nullopt);
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::IntermediateCommit const& op, LogIndex)
    -> ResultT<std::optional<LogIndex>> {
  if (!activeTransactions.getTransactions().contains(op.tid)) {
    LOG_CTX("b41dc", INFO, core->loggerContext)
        << "will not apply intermediate commit for transaction " << op.tid
        << " because it is not active";
    return ResultT<std::optional<LogIndex>>{std::nullopt};
  }
  auto const& transactionHandler = core->getTransactionHandler();
  if (auto res = transactionHandler->applyEntry(op); res.fail()) {
    return res;
  }

  // We don't need to update the release index after an intermediate commit.
  // However, we could release everything in this transaction up to this point
  // and update the start LogIndex of this transaction to the current log index.
  return ResultT<std::optional<LogIndex>>::success(std::nullopt);
}

auto DocumentFollowerState::GuardedData::applyEntry(
    FinishesUserTransaction auto const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  if (!activeTransactions.getTransactions().contains(op.tid)) {
    // Single commit/abort operations are possible.
    LOG_CTX("cf7ea", INFO, core->loggerContext)
        << "will not finish transaction " << op.tid
        << " because it is not active";
    return ResultT<std::optional<LogIndex>>{std::nullopt};
  }

  auto const& transactionHandler = core->getTransactionHandler();
  if (auto res = transactionHandler->applyEntry(op); res.fail()) {
    return res;
  }

  activeTransactions.markAsInactive(op.tid);
  // We can now potentially release past the finished transaction.
  return ResultT<std::optional<LogIndex>>::success(
      activeTransactions.getReleaseIndex().value_or(index));
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::AbortAllOngoingTrx const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  auto const& transactionHandler = core->getTransactionHandler();
  if (auto res = transactionHandler->applyEntry(op); res.fail()) {
    return res;
  }

  activeTransactions.clear();
  // Since everything was aborted, we can release all of it.
  return ResultT<std::optional<LogIndex>>::success(index);
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::DropShard const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  // We first have to abort all transactions for this shard.
  auto const& transactionHandler = core->getTransactionHandler();
  for (auto const& tid :
       transactionHandler->getTransactionsForShard(op.shard)) {
    auto abortRes = transactionHandler->applyEntry(
        ReplicatedOperation::buildAbortOperation(tid));
    if (abortRes.fail()) {
      LOG_CTX("aa36c", INFO, core->loggerContext)
          << "failed to abort transaction " << tid << " for shard " << op.shard
          << " before dropping the shard: " << abortRes.errorMessage();
      return abortRes;
    }
    activeTransactions.markAsInactive(tid);
  }

  if (auto res = transactionHandler->applyEntry(op); res.fail()) {
    return res;
  }

  // Since some transactions were aborted, we could potentially increase the
  // release index.
  return ResultT<std::optional<LogIndex>>::success(
      activeTransactions.getReleaseIndex().value_or(index));
}

auto DocumentFollowerState::GuardedData::applyEntry(
    ReplicatedOperation::CreateShard const& op, LogIndex index)
    -> ResultT<std::optional<LogIndex>> {
  auto const& transactionHandler = core->getTransactionHandler();
  if (auto res = transactionHandler->applyEntry(op); res.fail()) {
    return res;
  }

  // Once the shard has been created, we can release the entry.
  return ResultT<std::optional<LogIndex>>::success(
      activeTransactions.getReleaseIndex().value_or(index));
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
