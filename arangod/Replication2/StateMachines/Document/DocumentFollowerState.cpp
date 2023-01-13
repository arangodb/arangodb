////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
    : shardId(core->getShardId()),
      loggerContext(core->loggerContext.with<logContextKeyStateComponent>(
          "FollowerState")),
      _networkHandler(handlersFactory->createNetworkHandler(core->getGid())),
      _transactionHandler(
          handlersFactory->createTransactionHandler(core->getGid())),
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
  auto truncateRes = _guardedData.doUnderLock(
      [self = shared_from_this()](auto& data) -> Result {
        if (data.didResign()) {
          return Result{TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }
        return self->truncateLocalShard();
      });
  if (truncateRes.fail()) {
    return truncateRes;
  }

  // A follower may request a snapshot before leadership has been
  // established. A retry will occur in that case.
  auto leader = _networkHandler->getLeaderInterface(destination);
  auto fut = leader->startSnapshot(waitForIndex);
  return handleSnapshotTransfer(std::move(leader), waitForIndex,
                                std::move(fut));
}

auto DocumentFollowerState::applyEntries(
    std::unique_ptr<EntryIterator> ptr) noexcept -> futures::Future<Result> {
  auto result = _guardedData.doUnderLock(
      [self = shared_from_this(),
       ptr = std::move(ptr)](auto& data) -> ResultT<std::optional<LogIndex>> {
        if (data.didResign()) {
          return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }

        if (self->_transactionHandler == nullptr) {
          // TODO this is a temporary fix, see CINFRA-588
          LOG_CTX("cfd76", ERR, self->loggerContext) << fmt::format(
              "Transaction handler is missing from "
              "DocumentFollowerState {}! This happens if the vocbase cannot be "
              "found during DocumentState construction.",
              self->shardId);
          return Result{};
        }

        return basics::catchToResultT([&]() -> std::optional<LogIndex> {
          auto releaseIndex = std::make_optional<LogIndex>();

          while (auto entry = ptr->next()) {
            auto doc = entry->second;
            auto res = self->_transactionHandler->applyEntry(doc);
            if (res.fail()) {
              LOG_CTX("d82d4", FATAL, self->loggerContext)
                  << "Failed to apply entry " << entry->first
                  << " to local shard " << self->shardId
                  << " with error: " << res;
              FATAL_ERROR_EXIT();
            }

            if (doc.operation == OperationType::kAbortAllOngoingTrx) {
              self->_activeTransactions.clear();
              if (!releaseIndex.has_value() ||
                  releaseIndex.value() < entry->first) {
                releaseIndex = entry->first;
              }
            } else if (doc.operation == OperationType::kCommit ||
                       doc.operation == OperationType::kAbort) {
              self->_activeTransactions.erase(doc.tid);
              if (!releaseIndex.has_value() ||
                  releaseIndex.value() < entry->first) {
                releaseIndex = entry->first;
              }
            } else {
              self->_activeTransactions.emplace(doc.tid, entry->first);
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

/**
 * @brief Using the underlying transaction handler to apply a local transaction,
 * for this follower only.
 */
auto DocumentFollowerState::forceLocalTransaction(OperationType opType,
                                                  velocypack::SharedSlice slice)
    -> Result {
  if (_transactionHandler == nullptr) {
    // TODO this is a temporary fix, see CINFRA-588
    LOG_CTX("cfd76", ERR, loggerContext) << fmt::format(
        "Transaction handler is missing from "
        "DocumentFollowerState {}! This happens if the vocbase cannot be found "
        "during DocumentState construction.",
        shardId);
    return Result{};
  }
  auto trxId = TransactionId::createFollower();
  auto doc =
      DocumentLogEntry{std::string(shardId), opType, std::move(slice), trxId};
  if (auto applyRes = _transactionHandler->applyEntry(doc); applyRes.fail()) {
    _transactionHandler->removeTransaction(trxId);
    return applyRes;
  }
  auto commit =
      DocumentLogEntry{std::string(shardId), OperationType::kCommit, {}, trxId};
  return _transactionHandler->applyEntry(commit);
}

auto DocumentFollowerState::truncateLocalShard() -> Result {
  VPackBuilder b;
  b.openObject();
  b.add("collection", VPackValue(shardId));
  b.close();
  return forceLocalTransaction(OperationType::kTruncate, b.sharedSlice());
}

auto DocumentFollowerState::populateLocalShard(velocypack::SharedSlice slice)
    -> Result {
  return forceLocalTransaction(OperationType::kInsert, std::move(slice));
}

auto DocumentFollowerState::handleSnapshotTransfer(
    std::shared_ptr<IDocumentStateLeaderInterface> leader,
    LogIndex waitForIndex,
    futures::Future<ResultT<SnapshotBatch>>&& snapshotFuture) noexcept
    -> futures::Future<Result> {
  return std::move(snapshotFuture)
      .then([weak = weak_from_this(), leader = std::move(leader), waitForIndex](
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

        // Will be removed once we introduce collection groups
        TRI_ASSERT(snapshotRes->shardId == self->shardId);

        auto& docs = snapshotRes->payload;
        auto insertRes = self->_guardedData.doUnderLock(
            [&self, &docs](auto& data) -> Result {
              if (data.didResign()) {
                return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
              }
              return self->populateLocalShard(docs);
            });
        if (insertRes.fail()) {
          return insertRes;
        }

        if (snapshotRes->hasMore) {
          auto fut = leader->nextSnapshotBatch(snapshotRes->snapshotId);
          return self->handleSnapshotTransfer(std::move(leader), waitForIndex,
                                              std::move(fut));
        }

        return leader->finishSnapshot(snapshotRes->snapshotId);
      });
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
