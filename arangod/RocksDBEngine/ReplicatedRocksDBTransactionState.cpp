////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedRocksDBTransactionState.h"

#include "Basics/application-exit.h"
#include "Futures/Utilities.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"
#include "RocksDBEngine/ReplicatedRocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <rocksdb/types.h>

#include <algorithm>
#include <limits>
#include <numeric>

using namespace arangodb;

ReplicatedRocksDBTransactionState::ReplicatedRocksDBTransactionState(
    TRI_vocbase_t& vocbase, TransactionId tid,
    transaction::Options const& options,
    transaction::OperationOrigin operationOrigin)
    : RocksDBTransactionState(vocbase, tid, options, operationOrigin) {}

ReplicatedRocksDBTransactionState::~ReplicatedRocksDBTransactionState() {}

futures::Future<Result> ReplicatedRocksDBTransactionState::beginTransaction(
    transaction::Hints hints) {
  TRI_ASSERT(!_hasActiveTrx);
  auto res = co_await RocksDBTransactionState::beginTransaction(hints);
  if (!res.ok()) {
    co_return res;
  }

  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  for (auto& col : _collections) {
    res = static_cast<ReplicatedRocksDBTransactionCollection&>(*col)
              .beginTransaction();
    if (!res.ok()) {
      co_return res;
    }
  }
  _hasActiveTrx = true;
  co_return res;
}

/// @brief commit a transaction
futures::Future<Result> ReplicatedRocksDBTransactionState::doCommit() {
  _hasActiveTrx = false;

  if (!mustBeReplicated()) {
    Result res;
    RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
    for (auto& col : _collections) {
      res = static_cast<ReplicatedRocksDBTransactionCollection&>(*col)
                .commitTransaction();
      if (!res.ok()) {
        break;
      }
    }
    return res;
  }

  auto tid = id().asFollowerTransactionId();
  auto operation = replication2::replicated_state::document::
      ReplicatedOperation::buildCommitOperation(tid);
  auto options = replication2::replicated_state::document::ReplicationOptions{
      .waitForCommit = true};
  std::vector<futures::Future<Result>> commits;

  // We need the guard to ensure that the underlying collections are available
  // in replicateOperation futures, even if we return early
  ScopeGuard guard{[&]() noexcept {
    try {
      futures::collectAll(commits).waitAndGet();
    } catch (std::exception const&) {
    }
  }};

  // Due to distributeShardsLike, multiple collections can have the same log
  // leader. In this case, we are going to commit the same transaction multiple
  // times in the same log. This is ok, because followers know how to handle
  // this situation.
  allCollections([&](TransactionCollection& tc) {
    auto& rtc = static_cast<ReplicatedRocksDBTransactionCollection&>(tc);
    // We have to write to the log and wait for the log entry to be
    // committed (in the log sense), before we can commit locally.
    if ((rtc.accessType() != AccessMode::Type::READ)) {
      auto leader = rtc.leaderState();
      if (!leader->needsReplication(operation)) {
        // For transactions that have no operations, we only
        // have to commit locally. It is a nop, but helps with cleanup.
        commits.emplace_back(rtc.commitTransaction());
        return true;
      }

      commits.emplace_back(
          leader->replicateOperation(operation, options)
              .thenValue([&rtc, leader,
                          tid](auto&& res) -> ResultT<replication2::LogIndex> {
                if (res.fail()) {
                  LOG_CTX("57328", WARN, leader->loggerContext)
                      << "Failed to replicate commit of transaction (follower "
                         "ID) "
                      << tid << " on collection " << rtc.collectionName()
                      << ": " << res.result();
                  return res;
                }
                if (auto localCommitRes = rtc.commitTransaction();
                    localCommitRes.fail()) {
                  LOG_CTX("e8dd4", ERR, leader->loggerContext)
                      << "Failed to commit transaction (follower ID) " << tid
                      << " locally on collection " << rtc.collectionName()
                      << ": " << res.result();
                  return localCommitRes;
                }
                return res;
              })
              .thenValue([leader, tid](auto&& res) -> Result {
                if (res.fail()) {
                  return res.result();
                }
                auto logIndex = res.get();
                if (auto releaseRes = leader->release(tid, logIndex);
                    releaseRes.fail()) {
                  LOG_CTX("4a744", ERR, leader->loggerContext)
                      << "Failed to call release: " << releaseRes;
                }
                return Result{};
              }));
    } else {
      // For read-only transactions the commit is a no-op, but we still have
      // to call it to ensure cleanup.
      rtc.commitTransaction();
    }
    return true;
  });

  guard.cancel();

  // We are capturing a shared pointer to this state so we prevent reclamation
  // while we are waiting for the commit operations.
  return futures::collectAll(commits).thenValue([self = shared_from_this(),
                                                 tid](std::vector<
                                                      futures::Try<Result>>&&
                                                          results) -> Result {
    std::vector<Result> partialResults;
    partialResults.reserve(results.size());
    for (auto& res : results) {
      partialResults.emplace_back(
          basics::catchToResult([&]() { return res.get(); }));
    }

    if (std::all_of(
            partialResults.begin(), partialResults.end(), [](auto const& r) {
              return r.ok() ||
                     r.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
            })) {
      if (partialResults.empty() || partialResults.front().ok()) {
        return {};
      }

      if (std::all_of(
              partialResults.begin(), partialResults.end(), [](auto const& r) {
                return r.is(
                    TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
              })) {
        // Although on this server, the transaction has not made any progress
        // locally, it may have been committed by other replicated log leaders,
        // if they are located on other servers. This problem could be fixed by
        // distributed transactions.
        return Result{
            TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED,
            fmt::format(
                "All the replicated log leaders involved in transaction {} "
                "have resigned before the commit operation could be "
                "replicated. The transaction has neither been committed "
                "locally nor replicated, and it is going to be aborted.",
                tid.asCoordinatorTransactionId().id())};
      }

      auto warningMsg = fmt::format(
          "Some replicated log leaders have resigned before replicating "
          "the commit operation of transaction {}. The transaction may "
          "have been successfully applied only on some of the shards.",
          tid.asCoordinatorTransactionId().id());
      LOG_TOPIC("6d1ce", ERR, Logger::REPLICATED_STATE) << warningMsg;
      // This is expected behaviour. The transaction is committed on
      // some but not all leaders.
      return {TRI_ERROR_TRANSACTION_INTERNAL, warningMsg};
    }

    LOG_TOPIC("8ebc0", FATAL, Logger::REPLICATED_STATE)
        << "Failed to commit replicated transaction locally (partial "
           "commits detected): "
        << partialResults;
    TRI_ASSERT(false) << partialResults;
    // The leader is out of sync.
    // It makes sense to crash here, in the hopes that this server
    // becomes a follower and it re-applies the transaction successfully.
    FATAL_ERROR_EXIT();
  });
}

std::lock_guard<std::mutex> ReplicatedRocksDBTransactionState::lockCommit() {
  return std::lock_guard(_commitLock);
}

/// @brief abort and rollback a transaction
Result ReplicatedRocksDBTransactionState::doAbort() {
  _hasActiveTrx = false;

  if (!mustBeReplicated()) {
    Result res;
    RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
    for (auto& col : _collections) {
      auto& rtc = static_cast<ReplicatedRocksDBTransactionCollection&>(*col);
      res = rtc.abortTransaction();
      if (!res.ok()) {
        break;
      }
    }
    return res;
  }

  auto tid = id().asFollowerTransactionId();
  auto operation = replication2::replicated_state::document::
      ReplicatedOperation::buildAbortOperation(tid);
  auto options = replication2::replicated_state::document::ReplicationOptions{};

  // The following code has been simplified based on this assertion.
  TRI_ASSERT(options.waitForCommit == false);

  // Due to distributeShardsLike, multiple collections can have the same log
  // leader. In this case, we are going to abort the same transaction multiple
  // times in the same log. This is ok, because followers know how to handle
  // this situation.
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  for (auto& col : _collections) {
    auto& rtc = static_cast<ReplicatedRocksDBTransactionCollection&>(*col);

    if (rtc.accessType() != AccessMode::Type::READ) {
      auto leader = rtc.leaderState();
      auto needsReplication = leader->needsReplication(operation);
      if (!needsReplication) {
        if (auto r = rtc.abortTransaction(); r.fail()) {
          return r;
        }
        continue;
      }
      auto res = leader->replicateOperation(operation, options).waitAndGet();
      bool resigned = false;
      if (res.fail()) {
        if (res.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED)) {
          // During the resign procedure, the stream becomes unavailable, hence
          // any insertion attempts will be rejected. This means that
          // replication is expected to fail. In that case, we no longer have to
          // worry about the followers. If they are going to resign too, they'll
          // abort any unfinished transactions themselves. Otherwise, a new
          // leader will replicated an abort-all entry during recovery.
          resigned = true;
        } else {
          return res.result();
        }
      }

      if (auto r = rtc.abortTransaction(); r.fail()) {
        return r;
      }

      if (resigned) {
        // Skip the release step because it is not going to work anyway, and
        // that's ok.
        continue;
      }

      if (auto releaseRes = leader->release(tid, res.get());
          releaseRes.fail()) {
        LOG_CTX("0279d", ERR, leader->loggerContext)
            << "Failed to call release: " << releaseRes;
      }
    } else {
      if (auto r = rtc.abortTransaction(); r.fail()) {
        return r;
      }
    }
  }

  return {};
}

RocksDBTransactionMethods* ReplicatedRocksDBTransactionState::rocksdbMethods(
    DataSourceId collectionId) const {
  auto* coll = static_cast<ReplicatedRocksDBTransactionCollection*>(
      findCollection(collectionId));
  if (coll == nullptr) {
    std::string message = "collection '" + std::to_string(collectionId.id()) +
                          "' not found in transaction state";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }
  auto* result = coll->rocksdbMethods();
  TRI_ASSERT(result != nullptr);
  return result;
}

void ReplicatedRocksDBTransactionState::beginQuery(
    std::shared_ptr<ResourceMonitor> resourceMonitor,
    bool isModificationQuery) {
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  for (auto& col : _collections) {
    static_cast<ReplicatedRocksDBTransactionCollection&>(*col).beginQuery(
        resourceMonitor, isModificationQuery);
  }
}

void ReplicatedRocksDBTransactionState::endQuery(
    bool isModificationQuery) noexcept {
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  for (auto& col : _collections) {
    static_cast<ReplicatedRocksDBTransactionCollection&>(*col).endQuery(
        isModificationQuery);
  }
}

TRI_voc_tick_t ReplicatedRocksDBTransactionState::lastOperationTick()
    const noexcept {
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  return std::accumulate(
      _collections.begin(), _collections.end(), (TRI_voc_tick_t)0,
      [](auto maxTick, auto& col) {
        return std::max(
            maxTick, static_cast<ReplicatedRocksDBTransactionCollection&>(*col)
                         .lastOperationTick());
      });
}

uint64_t ReplicatedRocksDBTransactionState::numCommits() const noexcept {
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  return std::accumulate(
      _collections.begin(), _collections.end(), (uint64_t)0,
      [](auto sum, auto& col) {
        return sum +
               static_cast<ReplicatedRocksDBTransactionCollection const&>(*col)
                   .numCommits();
      });
}

uint64_t ReplicatedRocksDBTransactionState::numIntermediateCommits()
    const noexcept {
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  return std::accumulate(
      _collections.begin(), _collections.end(), (uint64_t)0,
      [](auto sum, auto& col) {
        return sum +
               static_cast<ReplicatedRocksDBTransactionCollection const&>(*col)
                   .numIntermediateCommits();
      });
}

void ReplicatedRocksDBTransactionState::addIntermediateCommits(uint64_t value) {
  // this is not supposed to be called, ever
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid call to addIntermediateCommits");
}

arangodb::Result
ReplicatedRocksDBTransactionState::triggerIntermediateCommit() {
  ADB_PROD_ASSERT(false) << "triggerIntermediateCommit is not supported in "
                            "ReplicatedRocksDBTransactionState";
  return arangodb::Result{TRI_ERROR_INTERNAL};
}

futures::Future<Result>
ReplicatedRocksDBTransactionState::performIntermediateCommitIfRequired(
    DataSourceId cid) {
  auto* coll =
      static_cast<ReplicatedRocksDBTransactionCollection*>(findCollection(cid));
  return coll->performIntermediateCommitIfRequired();
}

bool ReplicatedRocksDBTransactionState::hasOperations() const noexcept {
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  return std::any_of(
      _collections.begin(), _collections.end(), [](auto const& col) {
        return static_cast<ReplicatedRocksDBTransactionCollection const&>(*col)
            .hasOperations();
      });
}

uint64_t ReplicatedRocksDBTransactionState::numOperations() const noexcept {
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  return std::accumulate(
      _collections.begin(), _collections.end(), (uint64_t)0,
      [](auto ops, auto& col) {
        return ops +
               static_cast<ReplicatedRocksDBTransactionCollection const&>(*col)
                   .numOperations();
      });
}

uint64_t ReplicatedRocksDBTransactionState::numPrimitiveOperations()
    const noexcept {
  return 0;
}

bool ReplicatedRocksDBTransactionState::ensureSnapshot() {
  bool result = false;
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  for (auto& col : _collections) {
    result |= static_cast<ReplicatedRocksDBTransactionCollection&>(*col)
                  .ensureSnapshot();
  };
  return result;
}

std::unique_ptr<TransactionCollection>
ReplicatedRocksDBTransactionState::createTransactionCollection(
    DataSourceId cid, AccessMode::Type accessType) {
  auto result = std::make_unique<ReplicatedRocksDBTransactionCollection>(
      this, cid, accessType);
  if (_hasActiveTrx) {
    result->beginTransaction();  // TODO - find a better solution!
  }
  return result;
}

rocksdb::SequenceNumber ReplicatedRocksDBTransactionState::beginSeq() const {
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  auto seq = std::accumulate(
      _collections.begin(), _collections.end(),
      std::numeric_limits<rocksdb::SequenceNumber>::max(),
      [](auto seq, auto& col) {
        return std::min(
            seq,
            static_cast<ReplicatedRocksDBTransactionCollection const&>(*col)
                .rocksdbMethods()
                ->GetSequenceNumber());
      });
  TRI_ASSERT(seq != std::numeric_limits<rocksdb::SequenceNumber>::max());
  return seq;
}

bool ReplicatedRocksDBTransactionState::mustBeReplicated() const {
  auto isIndexCreation = _hints.has(transaction::Hints::Hint::INDEX_CREATION);
  return !isReadOnlyTransaction() && !isIndexCreation;
}
