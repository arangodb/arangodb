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

#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

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
      _networkHandler(handlersFactory->createNetworkHandler(core->getGid())),
      _transactionHandler(handlersFactory->createTransactionHandler(
          core->getVocbase(), core->getGid())),
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

        auto count = ++data.currentSnapshotVersion;

        // Drop all shards, as we're going to recreate them with the first
        // transfer
        if (auto dropAllRes = data.core->dropAllShards(); dropAllRes.fail()) {
          return dropAllRes;
        }
        return count;
      });

  if (snapshotVersion.fail()) {
    return snapshotVersion.result();
  }

  // A follower may request a snapshot before leadership has been
  // established. A retry will occur in that case.
  auto leader = _networkHandler->getLeaderInterface(destination);
  auto fut = leader->startSnapshot(waitForIndex);
  return handleSnapshotTransfer(std::move(leader), waitForIndex,
                                snapshotVersion.get(), std::move(fut));
}

auto DocumentFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  auto result = _guardedData.doUnderLock([self = shared_from_this(),
                                          ptr = std::move(ptr)](auto& data)
                                             -> ResultT<
                                                 std::optional<LogIndex>> {
    if (data.didResign()) {
      return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
    }

    return basics::catchToResultT([&]() -> std::optional<LogIndex> {
      std::optional<LogIndex> releaseIndex;

      while (auto entry = ptr->next()) {
        auto doc = entry->second;

        auto res = std::visit(
            [&](auto& op) -> Result {
              using T = std::decay_t<decltype(op)>;
              if constexpr (std::is_same_v<T, ReplicatedOperation::Truncate> ||
                            std::is_same_v<T, ReplicatedOperation::Insert> ||
                            std::is_same_v<T, ReplicatedOperation::Update> ||
                            std::is_same_v<T, ReplicatedOperation::Replace> ||
                            std::is_same_v<T, ReplicatedOperation::Remove>) {
                // Even though a shard was dropped before acquiring the
                // snapshot, we could still see transactions referring to that
                // shard.
                if (!data.core->isShardAvailable(op.shard)) {
                  LOG_CTX("bee57", INFO, self->loggerContext)
                      << "will not apply transaction " << op.tid
                      << " for shard " << op.shard
                      << " because it is not available";
                  return {};
                }

                self->_activeTransactions.emplace(op.tid, entry->first);
                return self->_transactionHandler->applyEntry(doc);
              } else if constexpr (std::is_same_v<
                                       T, ReplicatedOperation::Commit> ||
                                   std::is_same_v<T,
                                                  ReplicatedOperation::Abort>) {
                self->_activeTransactions.erase(op.tid);
                releaseIndex =
                    self->_activeTransactions.getReleaseIndex(entry->first);
                return self->_transactionHandler->applyEntry(doc);
              } else if constexpr (std::is_same_v<T, ReplicatedOperation::
                                                         IntermediateCommit>) {
                return self->_transactionHandler->applyEntry(doc);
              } else if constexpr (std::is_same_v<T, ReplicatedOperation::
                                                         AbortAllOngoingTrx>) {
                self->_activeTransactions.clear();
                releaseIndex =
                    self->_activeTransactions.getReleaseIndex(entry->first);
                return self->_transactionHandler->applyEntry(doc);
              } else if constexpr (std::is_same_v<
                                       T, ReplicatedOperation::CreateShard>) {
                return data.core->ensureShard(op.shard, op.collection,
                                              op.properties);
              } else if constexpr (std::is_same_v<
                                       T, ReplicatedOperation::DropShard>) {
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
              << "failed to apply entry " << doc << " on follower: " << res;
          FATAL_ERROR_EXIT();
        }
      }

      return releaseIndex;
    });
  });

  if (result.fail()) {
    return result.result();
  }
  if (result->has_value()) {
    // The follower might have resigned, so we need to be careful when accessing
    // the stream.
    auto releaseRes = basics::catchVoidToResult([&] {
      auto const& stream = getStream();
      stream->release(result->value());
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
  auto doc = DocumentLogEntry{ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_INSERT, tid, std::move(shardId),
      std::move(slice))};
  if (auto applyRes = _transactionHandler->applyEntry(doc); applyRes.fail()) {
    _transactionHandler->removeTransaction(tid);
    return applyRes;
  }
  auto commit =
      DocumentLogEntry{ReplicatedOperation::buildCommitOperation(tid)};
  return _transactionHandler->applyEntry(commit);
}

auto DocumentFollowerState::handleSnapshotTransfer(
    std::shared_ptr<IDocumentStateLeaderInterface> leader,
    LogIndex waitForIndex, std::uint64_t snapshotVersion,
    futures::Future<ResultT<SnapshotConfig>>&& snapshotFuture) noexcept
    -> futures::Future<Result> {
  return std::move(snapshotFuture)
      .then([weak = weak_from_this(), leader = std::move(leader), waitForIndex,
             snapshotVersion](
                futures::Try<ResultT<SnapshotConfig>>&& tryResult) mutable
            -> futures::Future<Result> {
        auto self = weak.lock();
        if (self == nullptr) {
          return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }

        auto catchRes =
            basics::catchToResultT([&] { return std::move(tryResult).get(); });
        if (catchRes.fail()) {
          return catchRes.result();
        }

        auto snapshotRes = catchRes.get();
        if (snapshotRes.fail()) {
          return snapshotRes.result();
        }

        if (snapshotRes->shards.empty()) {
          // Nothing to do, just call finish
          return leader->finishSnapshot(snapshotRes->snapshotId);
        }

        auto res = self->_guardedData.doUnderLock([self,
                                                   shards = std::move(
                                                       snapshotRes->shards),
                                                   snapshotVersion](
                                                      auto& data) -> Result {
          if (data.didResign()) {
            return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
          }

          if (data.currentSnapshotVersion != snapshotVersion) {
            return {
                TRI_ERROR_INTERNAL,
                "Snapshot transfer cancelled because a new one was started!"};
          }

          for (auto const& [shardId, properties] : shards) {
            auto res =
                data.core->ensureShard(shardId, properties.collectionId,
                                       properties.properties->sharedSlice());
            if (res.fail()) {
              LOG_CTX("bd17c", ERR, self->loggerContext)
                  << "Failed to ensure shard " << shardId << " " << res;
              FATAL_ERROR_EXIT();
            }
          }

          return {};
        });

        if (res.fail()) {
          LOG_CTX("d82d4", ERR, self->loggerContext) << res;
          // TODO Handle resign
          if (res.isNot(
                  TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED)) {
            return leader->finishSnapshot(snapshotRes->snapshotId);
          }
          return res;
        }

        auto fut = leader->nextSnapshotBatch(snapshotRes->snapshotId);
        return self->handleSnapshotTransfer(std::move(leader), waitForIndex,
                                            snapshotVersion, std::move(fut));
      });
}

auto DocumentFollowerState::handleSnapshotTransfer(
    std::shared_ptr<IDocumentStateLeaderInterface> leader,
    LogIndex waitForIndex, std::uint64_t snapshotVersion,
    futures::Future<ResultT<SnapshotBatch>>&& snapshotFuture) noexcept
    -> futures::Future<Result> {
  return std::move(snapshotFuture)
      .then([weak = weak_from_this(), leader = std::move(leader), waitForIndex,
             snapshotVersion](
                futures::Try<ResultT<SnapshotBatch>>&& tryResult) mutable
            -> futures::Future<Result> {
        auto self = weak.lock();
        if (self == nullptr) {
          return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }

        auto catchRes =
            basics::catchToResultT([&] { return std::move(tryResult).get(); });
        if (catchRes.fail()) {
          return catchRes.result();
        }

        auto snapshotRes = catchRes.get();
        if (snapshotRes.fail()) {
          return snapshotRes.result();
        }

        if (snapshotRes->shardId.has_value()) {
          auto& docs = snapshotRes->payload;
          auto insertRes = self->_guardedData.doUnderLock([&self, &docs,
                                                           &snapshotRes,
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
            return self->populateLocalShard(*snapshotRes->shardId, docs);
          });
          if (insertRes.fail()) {
            LOG_CTX("d8b8a", ERR, self->loggerContext)
                << "Failed to populate local shard: " << insertRes;
            if (insertRes.isNot(
                    TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED)) {
              // TODO return result and let the leader clear the failed snapshot
              // itself, or send an abort instead of finish?
              return leader->finishSnapshot(snapshotRes->snapshotId);
            }
            return insertRes;
          }
        } else {
          TRI_ASSERT(!snapshotRes->hasMore);
        }

        if (snapshotRes->hasMore) {
          auto fut = leader->nextSnapshotBatch(snapshotRes->snapshotId);
          return self->handleSnapshotTransfer(std::move(leader), waitForIndex,
                                              snapshotVersion, std::move(fut));
        }

        return leader->finishSnapshot(snapshotRes->snapshotId);
      });
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
