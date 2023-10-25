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

#include "Basics/overload.h"
#include "Basics/ResultT.h"
#include "Basics/voc-errors.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"

#include "Replication2/StateMachines/Document/ReplicatedOperation.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <Basics/application-exit.h>
#include <Basics/Exceptions.h>
#include <Futures/Future.h>
#include <Logger/LogContextKeys.h>

#include <functional>
#include <optional>
#include <ranges>
#include <type_traits>

namespace arangodb::replication2::replicated_state::document {

struct DocumentFollowerState::EntryProcessor {
  explicit EntryProcessor(DocumentFollowerState& state) : _state(state) {}

  auto collectResults() {
    std::vector<futures::Future<ResultT<std::optional<LogIndex>>>> futures;
    for (auto& [tid, f] : _activeTransactions) {
      futures.emplace_back(std::move(f));
    }
    _activeTransactions.clear();
    auto future = futures::collectAll(futures.begin(), futures.end());
    auto results = future.get();

    for (auto&& tryResult : results) {
      TRI_ASSERT(tryResult.valid());
      try {
        auto currentReleaseIndex = std::move(tryResult).get();
        TRI_ASSERT(currentReleaseIndex.ok());
        if (currentReleaseIndex->has_value()) {
          auto newReleaseIndex = currentReleaseIndex.get().value();
          if (_releaseIndex <= newReleaseIndex) {
            _releaseIndex = newReleaseIndex;
          }
        }
      } catch (std::exception& e) {
        LOG_DEVEL << "caught exception while applying entries: " << e.what();
        throw;
      } catch (...) {
        LOG_DEVEL << "caught unknown exception while applying entries";
        throw;
      }
    }
    return _releaseIndex;
  }

  auto applyEntry(DataDefinitionOperation auto const& op, LogIndex index,
                  DocumentLogEntry const& doc)
      -> ResultT<std::optional<LogIndex>> {
    waitForPendingOperations();
    return _state._guardedData.doUnderLock(
        [&](auto& data) -> ResultT<std::optional<LogIndex>> {
          return applyDataDefinitionEntry(data, op, index);
        });
  }

  auto applyEntry(ReplicatedOperation::AbortAllOngoingTrx const& op,
                  LogIndex index, DocumentLogEntry const& doc)
      -> ResultT<std::optional<LogIndex>> {
    waitForPendingOperations();
    if (auto res = _state._transactionHandler->applyEntry(op); res.fail()) {
      return res;
    }

    return _state._guardedData.doUnderLock([&](auto& data) {
      data.activeTransactions.clear();
      // Since everything was aborted, we can release all of it.
      return ResultT<std::optional<LogIndex>>::success(index);
    });
  }

  auto applyEntry(UserTransaction auto const& op, LogIndex index,
                  DocumentLogEntry const& doc)
      -> ResultT<std::optional<LogIndex>> {
    using OpType = std::remove_cvref_t<decltype(op)>;
    if constexpr (FinishesUserTransaction<OpType>) {
      auto it = _activeTransactions.find(op.tid);
      if (it != _activeTransactions.end()) {
        it->second =
            std::move(it->second)
                .thenValue([this, index, doc = doc](
                               auto _res) -> ResultT<std::optional<LogIndex>> {
                  auto& op = std::get<OpType>(doc.getInnerOperation());
                  if (auto res = beforeApplyEntry(op, index); res.fail()) {
                    return res;
                  }
                  if (auto res = _state._transactionHandler->applyEntry(op);
                      res.fail()) {
                    return res;
                  }
                  return afterApplyEntry(op, index);
                });
        return ResultT<std::optional<LogIndex>>::success(std::nullopt);
      }
    }
    auto res = scheduleApplyEntry(op, index, doc);
    if (res.fail()) {
      return res;
    }
    return ResultT<std::optional<LogIndex>>::success(std::nullopt);
  }

  template<class T>
  auto scheduleApplyEntry(T const& op, LogIndex index,
                          DocumentLogEntry const& doc) -> Result {
    auto res = beforeApplyEntry(op, index);
    if (res.fail()) {
      return res;
    }

    auto applyEntry = [self = _state.shared_from_this(), index,
                       doc =
                           doc]() mutable -> ResultT<std::optional<LogIndex>> {
      if (self->_guardedData.getLockedGuard()->didResign()) {
        return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
      }

      auto& op = std::get<T>(doc.getInnerOperation());
      try {
        auto currentReleaseIndex =
            std::invoke([&]() -> ResultT<std::optional<LogIndex>> {
              if (auto res = self->_transactionHandler->applyEntry(op);
                  res.fail()) {
                return res;
              }
              EntryProcessor processor{*self};
              auto res = processor.afterApplyEntry(op, index);
              return res;
            });

        if (currentReleaseIndex.fail()) {
          LOG_CTX("0aa2e", FATAL, self->loggerContext)
              << "failed to apply entry " << doc
              << " on follower: " << currentReleaseIndex.result();
          FATAL_ERROR_EXIT();
        }
        return currentReleaseIndex;
      } catch (std::exception& e) {
        LOG_DEVEL << "caught exception while applying entry " << index << ": "
                  << e.what();
        throw;
      } catch (...) {
        LOG_DEVEL << "caught unknown exception while "
                     "applying entry "
                  << index;
        throw;
      }
    };

    // TODO lane
    _activeTransactions.emplace(
        op.tid, SchedulerFeature::SCHEDULER->queueWithFuture(
                    RequestLane::CLIENT_FAST, std::move(applyEntry)));

    return Result{};
  }

 private:
  DocumentFollowerState& _state;
  std::optional<LogIndex> _releaseIndex;
  std::unordered_map<TransactionId,
                     futures::Future<ResultT<std::optional<LogIndex>>>>
      _activeTransactions;

  void waitForPendingOperations() { collectResults(); }

  auto applyDataDefinitionEntry(GuardedData& data,
                                ReplicatedOperation::DropShard const& op,
                                LogIndex index)
      -> ResultT<std::optional<LogIndex>> {
    // We first have to abort all transactions for this shard.
    for (auto const& tid :
         _state._transactionHandler->getTransactionsForShard(op.shard)) {
      auto abortRes = _state._transactionHandler->applyEntry(
          ReplicatedOperation::buildAbortOperation(tid));
      if (abortRes.fail()) {
        LOG_CTX("aa36c", INFO, data.core->loggerContext)
            << "failed to abort transaction " << tid << " for shard "
            << op.shard
            << " before dropping the shard: " << abortRes.errorMessage();
        return abortRes;
      }
      data.activeTransactions.markAsInactive(tid);
    }

    // Since some transactions were aborted, we could potentially
    // increase the release index.
    return ResultT<std::optional<LogIndex>>::success(
        data.activeTransactions.getReleaseIndex().value_or(index));
  }

  auto applyDataDefinitionEntry(GuardedData& data,
                                ReplicatedOperation::ModifyShard const& op,
                                LogIndex index)
      -> ResultT<std::optional<LogIndex>> {
    // Note that locking the shard is not necessary on the
    // follower. However, we still do it for safety reasons.
    auto shardHandler = data.core->getShardHandler();
    auto origin = transaction::OperationOriginREST{
        "follower collection properties update"};
    auto trxLock = shardHandler->lockShard(
        op.shard, AccessMode::Type::EXCLUSIVE, std::move(origin));
    if (trxLock.fail()) {
      return trxLock.result();
    }

    return applyEntryAndReleaseIndex(data, op, index);
  }

  template<class T>
  auto applyDataDefinitionEntry(GuardedData& data, T const& op, LogIndex index)
      -> ResultT<std::optional<LogIndex>>
  requires IsAnyOf<T, ReplicatedOperation::CreateShard,
                   ReplicatedOperation::CreateIndex,
                   ReplicatedOperation::DropIndex> {
    return applyEntryAndReleaseIndex(data, op, index);
  }

  template<class T>
  auto applyEntryAndReleaseIndex(GuardedData& data, T const& op, LogIndex index)
      -> ResultT<std::optional<LogIndex>> {
    if (auto res = _state._transactionHandler->applyEntry(op); res.fail()) {
      return res;
    }

    return ResultT<std::optional<LogIndex>>::success(
        data.activeTransactions.getReleaseIndex().value_or(index));
  }

  auto beforeApplyEntry(ModifiesUserTransaction auto const& op, LogIndex index)
      -> Result {
    return _state._guardedData.doUnderLock([&](auto& data) -> Result {
      if (auto validationRes = _state._transactionHandler->validate(op);
          validationRes.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
        //  Even though a shard was dropped before acquiring the
        //  snapshot, we could still see transactions referring to
        //  that shard.
        LOG_CTX("e1edb", INFO, data.core->loggerContext)
            << "will not apply transaction " << op.tid << " on shard "
            << op.shard << " because the shard is not available";
        return Result(TRI_ERROR_INTERNAL,
                      fmt::format("shard {} is not available", op.shard));
      } else if (validationRes.fail()) {
        return validationRes;
      }
      data.activeTransactions.markAsActive(op.tid, index);
      return {};
    });
  }

  auto afterApplyEntry(ModifiesUserTransaction auto const& op, LogIndex index)
      -> ResultT<std::optional<LogIndex>> {
    // We don't need to update the release index after such operations.
    return ResultT<std::optional<LogIndex>>::success(std::nullopt);
  }

  auto beforeApplyEntry(ReplicatedOperation::IntermediateCommit const& op,
                        LogIndex) -> Result {
    return _state._guardedData.doUnderLock([&](auto& data) -> Result {
      if (!data.activeTransactions.getTransactions().contains(op.tid)) {
        LOG_CTX("b41dc", INFO, data.core->loggerContext)
            << "will not apply intermediate commit for transaction " << op.tid
            << " because it is not active";
        return Result(TRI_ERROR_INTERNAL,
                      fmt::format("transaction {} is not active", op.tid.id()));
      }
      return {};
    });
  }

  auto afterApplyEntry(ReplicatedOperation::IntermediateCommit const& op,
                       LogIndex) -> ResultT<std::optional<LogIndex>> {
    // We don't need to update the release index after an intermediate commit.
    // However, we could release everything in this transaction up to this
    // point and update the start LogIndex of this transaction to the current
    // log index.
    return ResultT<std::optional<LogIndex>>::success(std::nullopt);
  }

  auto beforeApplyEntry(FinishesUserTransaction auto const& op, LogIndex index)
      -> Result {
    return _state._guardedData.doUnderLock([&](auto& data) -> Result {
      if (!data.activeTransactions.getTransactions().contains(op.tid)) {
        // Single commit/abort operations are possible.
        LOG_CTX("cf7ea", INFO, data.core->loggerContext)
            << "will not finish transaction " << op.tid
            << " because it is not active";
        return Result(TRI_ERROR_INTERNAL,
                      fmt::format("transaction {} is not active", op.tid.id()));
      }
      return {};
    });
  }

  auto afterApplyEntry(FinishesUserTransaction auto const& op, LogIndex index)
      -> ResultT<std::optional<LogIndex>> {
    return _state._guardedData.doUnderLock([&](auto& data) {
      data.activeTransactions.markAsInactive(op.tid);
      // We can now potentially release past the finished transaction.
      return ResultT<std::optional<LogIndex>>::success(
          data.activeTransactions.getReleaseIndex().value_or(index));
    });
  }

  auto beforeApplyEntry(ReplicatedOperation::AbortAllOngoingTrx const&,
                        LogIndex) -> Result {
    return {};
  }

  auto afterApplyEntry(ReplicatedOperation::AbortAllOngoingTrx const& op,
                       LogIndex index) -> ResultT<std::optional<LogIndex>> {
    return _state._guardedData.doUnderLock([&](auto& data) {
      data.activeTransactions.clear();
      // Since everything was aborted, we can release all of it.
      return ResultT<std::optional<LogIndex>>::success(index);
    });
  }
};

DocumentFollowerState::GuardedData::GuardedData(
    std::unique_ptr<DocumentCore> core)
    : core(std::move(core)), currentSnapshotVersion{0} {}

DocumentFollowerState::DocumentFollowerState(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory)
    : loggerContext(core->loggerContext.with<logContextKeyStateComponent>(
          "FollowerState")),
      _networkHandler(handlersFactory->createNetworkHandler(core->gid)),
      _transactionHandler(handlersFactory->createTransactionHandler(
          core->getVocbase(), core->gid, core->getShardHandler())),
      _guardedData(std::move(core)) {}

DocumentFollowerState::~DocumentFollowerState() = default;

auto DocumentFollowerState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  _resigning = true;
  return _guardedData.doUnderLock([this](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_FOLLOWER);
    }

    auto abortAllRes = _transactionHandler->applyEntry(
        ReplicatedOperation::buildAbortAllOngoingTrxOperation());
    ADB_PROD_ASSERT(abortAllRes.ok()) << abortAllRes;

    return std::move(data.core);
  });
}

auto DocumentFollowerState::getAssociatedShardList() const
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

auto DocumentFollowerState::acquireSnapshot(
    ParticipantId const& destination) noexcept -> futures::Future<Result> {
  LOG_CTX("1f67d", DEBUG, loggerContext)
      << "Trying to acquire snapshot from destination " << destination;

  auto snapshotVersion =
      _guardedData.doUnderLock([this](auto& data) -> ResultT<std::uint64_t> {
        if (data.didResign()) {
          return Result{TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }

        auto abortAllRes = _transactionHandler->applyEntry(
            ReplicatedOperation::buildAbortAllOngoingTrxOperation());
        ADB_PROD_ASSERT(abortAllRes.ok()) << abortAllRes;

        auto const& shardHandler = data.core->getShardHandler();
        if (auto dropAllRes = shardHandler->dropAllShards();
            dropAllRes.fail()) {
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
  auto fut = leader->startSnapshot();
  return handleSnapshotTransfer(leader, snapshotVersion.get(), std::move(fut))
      .thenValue([leader, self = shared_from_this()](
                     auto&& snapshotTransferResult) -> futures::Future<Result> {
        if (!snapshotTransferResult.snapshotId.has_value()) {
          TRI_ASSERT(snapshotTransferResult.res.fail());
          LOG_CTX("85628", ERR, self->loggerContext)
              << "Snapshot transfer failed: " << snapshotTransferResult.res;
          return snapshotTransferResult.res;
        }

        LOG_CTX("b4fcb", DEBUG, self->loggerContext)
            << "Snapshot " << *snapshotTransferResult.snapshotId
            << " data transfer over, will send finish request: "
            << snapshotTransferResult.res;

        return leader->finishSnapshot(*snapshotTransferResult.snapshotId)
            .then([snapshotTransferResult](auto&& tryRes) {
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
                          !snapshotTransferResult.reportFailure));

              if (snapshotTransferResult.reportFailure) {
                LOG_TOPIC("2883c", DEBUG, Logger::REPLICATION2)
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

auto DocumentFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  auto applyResult = basics::catchToResultT([&]() -> std::optional<LogIndex> {
    EntryProcessor processor{*this};

    int cnt = 0;
    while (auto entry = ptr->next()) {
      ++cnt;
      if (_resigning) {
        // We have not officially resigned yet, but we are about to. So,
        // we can just stop here.
        break;
      }

      auto& e = entry.value();
      LogIndex index = e.first;
      auto const& doc = entry->second;
      auto res = std::visit(
          [&](auto&& op) -> ResultT<std::optional<LogIndex>> {
            return processor.applyEntry(op, index, doc);
          },
          doc.getInnerOperation());

      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res.result());
      }
    }

    return processor.collectResults();
  });

  if (_resigning) {
    return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
  }

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

auto DocumentFollowerState::populateLocalShard(ShardID shardId,
                                               velocypack::SharedSlice slice)
    -> Result {
  auto tid = TransactionId::createFollower();
  auto op = ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_INSERT, tid, std::move(shardId),
      std::move(slice));
  if (auto applyRes = _transactionHandler->applyEntry(op); applyRes.fail()) {
    _transactionHandler->removeTransaction(tid);
    return applyRes;
  }
  auto commit = ReplicatedOperation::buildCommitOperation(tid);
  return _transactionHandler->applyEntry(commit);
}

auto DocumentFollowerState::handleSnapshotTransfer(
    std::shared_ptr<IDocumentStateLeaderInterface> leader,
    std::uint64_t snapshotVersion,
    futures::Future<ResultT<SnapshotConfig>>&& snapshotFuture) noexcept
    -> futures::Future<SnapshotTransferResult> {
  return std::move(snapshotFuture)
      .then([weak = weak_from_this(), leader = std::move(leader),
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
              .reportFailure = true,
              .snapshotId = snapshotRes->snapshotId};
        }

        if (snapshotRes->shards.empty()) {
          LOG_CTX("289dc", DEBUG, self->loggerContext)
              << "No shards to transfer for snapshot "
              << snapshotRes->snapshotId;
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

              std::stringstream ss;
              for (auto const& [shardId, _] : shards) {
                ss << shardId << " ";
              }
              LOG_CTX("2410c", DEBUG, self->loggerContext)
                  << "While starting snapshot " << snapshotId
                  << ", the follower tries to create the following shards: "
                  << ss.str();

              for (auto const& [shardId, properties] : shards) {
                auto res = self->_transactionHandler->applyEntry(
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
          if (res.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED)) {
            return SnapshotTransferResult{
                .res = res,
                .reportFailure = true,
                .snapshotId = snapshotRes->snapshotId};
          }
          // If we got here, it means the snapshot transfer is no longer needed.
          return SnapshotTransferResult{.res = res,
                                        .snapshotId = snapshotRes->snapshotId};
        }

        LOG_CTX("d6666", DEBUG, self->loggerContext)
            << "Trying to get first batch of snapshot: "
            << snapshotRes->snapshotId;
        auto fut = leader->nextSnapshotBatch(snapshotRes->snapshotId);
        return self->handleSnapshotTransfer(snapshotRes->snapshotId,
                                            std::move(leader), snapshotVersion,
                                            std::nullopt, std::move(fut));
      });
}

auto DocumentFollowerState::handleSnapshotTransfer(
    SnapshotId snapshotId,
    std::shared_ptr<IDocumentStateLeaderInterface> leader,
    std::uint64_t snapshotVersion, std::optional<ShardID> currentShard,
    futures::Future<ResultT<SnapshotBatch>>&& snapshotFuture) noexcept
    -> futures::Future<SnapshotTransferResult> {
  return std::move(snapshotFuture)
      .then([weak = weak_from_this(), leader = std::move(leader),
             snapshotVersion, snapshotId,
             currentShard = std::move(currentShard)](
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
          return SnapshotTransferResult{
              .res = TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
              .reportFailure = true,
              .snapshotId = snapshotId};
        }

        if (currentShard.has_value()) {
          if (currentShard != snapshotRes->shardId) {
            LOG_CTX("9f630", DEBUG, self->loggerContext)
                << "Snapshot transfer " << snapshotId
                << " completed all batches for shard " << *currentShard;
            if (snapshotRes->shardId.has_value()) {
              LOG_CTX("2add0", DEBUG, self->loggerContext)
                  << "Snapshot transfer " << snapshotId
                  << " beginning to transfer batches for shard "
                  << *snapshotRes->shardId;
              currentShard = snapshotRes->shardId;
            } else {
              LOG_CTX("ee8f7", DEBUG, self->loggerContext)
                  << "Snapshot transfer " << snapshotId
                  << " does not get any more batches";
            }
          }
        } else {
          currentShard = snapshotRes->shardId;
        }

        if (snapshotRes->shardId.has_value()) {
          bool reportingFailure = false;
          auto insertRes = self->_guardedData.doUnderLock(
              [&self, &snapshotRes, &reportingFailure,
               snapshotVersion](auto& data) -> Result {
                if (data.didResign()) {
                  reportingFailure = true;
                  return {
                      TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
                }

                // The user may remove and add the server again. The leader
                // might do a compaction which the follower won't notice. Hence,
                // a new snapshot is required. This can happen so quick, that
                // one snapshot transfer is not yet completed before another one
                // is requested. Before populating the shard, we have to make
                // sure there's no new snapshot transfer in progress.
                if (data.currentSnapshotVersion != snapshotVersion) {
                  return {TRI_ERROR_INTERNAL,
                          "Snapshot transfer cancelled because a new one was "
                          "started!"};
                }

                LOG_CTX("c1d58", DEBUG, self->loggerContext)
                    << "Trying to insert " << snapshotRes->payload.length()
                    << " documents with " << snapshotRes->payload.byteSize()
                    << " bytes into shard " << *snapshotRes->shardId;

                // Note that when the DocumentStateSnapshot::infiniteSnapshot
                // failure point is enabled, the payload is an empty array.
                if (auto localShardRes = self->populateLocalShard(
                        *snapshotRes->shardId, snapshotRes->payload);
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
          LOG_CTX("a732f", DEBUG, self->loggerContext)
              << "Trying to fetch the next batch of snapshot: "
              << snapshotRes->snapshotId;
          auto fut = leader->nextSnapshotBatch(snapshotId);
          return self->handleSnapshotTransfer(
              snapshotId, std::move(leader), snapshotVersion,
              std::move(currentShard), std::move(fut));
        }

        LOG_CTX("742df", DEBUG, self->loggerContext)
            << "Leader informed the follower there is no more data to be sent "
               "for snapshot "
            << snapshotRes->snapshotId;
        return SnapshotTransferResult{.snapshotId = snapshotId};
      });
}

}  // namespace arangodb::replication2::replicated_state::document

#include "Replication2/ReplicatedState/ReplicatedStateImpl.tpp"
