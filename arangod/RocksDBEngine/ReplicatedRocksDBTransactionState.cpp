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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedRocksDBTransactionState.h"
#include <rocksdb/types.h>

#include <algorithm>
#include <limits>
#include <numeric>

#include "Futures/Utilities.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "RocksDBEngine/Methods/RocksDBReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationTrxMethods.h"
#include "RocksDBEngine/Methods/RocksDBTrxMethods.h"
#include "RocksDBEngine/ReplicatedRocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <Basics/application-exit.h>

using namespace arangodb;

ReplicatedRocksDBTransactionState::ReplicatedRocksDBTransactionState(
    TRI_vocbase_t& vocbase, TransactionId tid,
    transaction::Options const& options)
    : RocksDBTransactionState(vocbase, tid, options) {}

ReplicatedRocksDBTransactionState::~ReplicatedRocksDBTransactionState() {}

Result ReplicatedRocksDBTransactionState::beginTransaction(
    transaction::Hints hints) {
  TRI_ASSERT(!_hasActiveTrx);
  auto res = RocksDBTransactionState::beginTransaction(hints);
  if (!res.ok()) {
    return res;
  }

  for (auto& col : _collections) {
    res = static_cast<ReplicatedRocksDBTransactionCollection&>(*col)
              .beginTransaction();
    if (!res.ok()) {
      return res;
    }
  }
  _hasActiveTrx = true;
  return res;
}

/// @brief commit a transaction
futures::Future<Result> ReplicatedRocksDBTransactionState::doCommit() {
  _hasActiveTrx = false;

  if (!mustBeReplicated()) {
    Result res;
    for (auto& col : _collections) {
      res = static_cast<ReplicatedRocksDBTransactionCollection&>(*col)
                .commitTransaction();
      if (!res.ok()) {
        break;
      }
    }
    return res;
  }

  auto operation =
      replication2::replicated_state::document::OperationType::kCommit;
  auto options = replication2::replicated_state::document::ReplicationOptions{
      .waitForCommit = true};
  std::vector<futures::Future<Result>> commits;

  // We need the guard to ensure that the underlying collections are available
  // in replicateOperation futures, even if we return early
  ScopeGuard guard{[&]() noexcept {
    try {
      futures::collectAll(commits).get();
    } catch (std::exception const&) {
    }
  }};

  try {
    allCollections([&](TransactionCollection& tc) {
      auto& rtc = static_cast<ReplicatedRocksDBTransactionCollection&>(tc);
      if (rtc.accessType() != AccessMode::Type::READ) {
        // We have to write to the log and wait for the log entry to be
        // committed (in the log sense), before we can commit locally.
        auto leader = rtc.leaderState();
        commits.emplace_back(leader
                                 ->replicateOperation(velocypack::SharedSlice{},
                                                      operation, id(), options)
                                 .thenValue([&rtc](auto&& res) -> Result {
                                   return rtc.commitTransaction();
                                 }));
      } else {
        // For read-only transactions the commit is a no-op, but we still have
        // to call it to ensure cleanup.
        rtc.commitTransaction();
      }
      return true;
    });
  } catch (basics::Exception const& e) {
    return Result{e.code(), e.what()};
  } catch (std::exception const& e) {
    return Result{TRI_ERROR_INTERNAL, e.what()};
  }

  guard.cancel();

  // We are capturing a shared pointer to this state so we prevent reclamation
  // while we are waiting for the commit operations.
  return futures::collectAll(commits).thenValue(
      [self = shared_from_this()](
          std::vector<futures::Try<Result>>&& results) -> Result {
        for (auto& res : results) {
          auto result = res.get();
          if (result.fail()) {
            LOG_TOPIC("8ebc0", FATAL, Logger::REPLICATION2)
                << "Failed to commit replicated transaction locally: "
                << result;
            FATAL_ERROR_EXIT();
          }
        }
        return {};
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
    for (auto& col : _collections) {
      auto& rtc = static_cast<ReplicatedRocksDBTransactionCollection&>(*col);
      res = rtc.abortTransaction();
      if (!res.ok()) {
        break;
      }
    }
    return res;
  }

  auto operation =
      replication2::replicated_state::document::OperationType::kAbort;
  auto options = replication2::replicated_state::document::ReplicationOptions{};

  // The following code has been simplified based on this assertion.
  TRI_ASSERT(options.waitForCommit == false);
  for (auto& col : _collections) {
    auto& rtc = static_cast<ReplicatedRocksDBTransactionCollection&>(*col);
    if (rtc.accessType() != AccessMode::Type::READ) {
      auto leader = rtc.leaderState();
      try {
        leader->replicateOperation(velocypack::SharedSlice{}, operation, id(),
                                   options);
      } catch (basics::Exception const& e) {
        return Result{e.code(), e.what()};
      } catch (std::exception const& e) {
        return Result{TRI_ERROR_INTERNAL, e.what()};
      }
    }
    auto r = rtc.abortTransaction();
    if (r.fail()) {
      return r;
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

void ReplicatedRocksDBTransactionState::beginQuery(bool isModificationQuery) {
  for (auto& col : _collections) {
    static_cast<ReplicatedRocksDBTransactionCollection&>(*col).beginQuery(
        isModificationQuery);
  }
}

void ReplicatedRocksDBTransactionState::endQuery(
    bool isModificationQuery) noexcept {
  for (auto& col : _collections) {
    static_cast<ReplicatedRocksDBTransactionCollection&>(*col).endQuery(
        isModificationQuery);
  }
}

TRI_voc_tick_t ReplicatedRocksDBTransactionState::lastOperationTick()
    const noexcept {
  return std::accumulate(
      _collections.begin(), _collections.end(), (TRI_voc_tick_t)0,
      [](auto maxTick, auto& col) {
        return std::max(
            maxTick, static_cast<ReplicatedRocksDBTransactionCollection&>(*col)
                         .lastOperationTick());
      });
}

uint64_t ReplicatedRocksDBTransactionState::numCommits() const noexcept {
  // TODO
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
  return std::any_of(
      _collections.begin(), _collections.end(), [](auto const& col) {
        return static_cast<ReplicatedRocksDBTransactionCollection const&>(*col)
            .hasOperations();
      });
}

uint64_t ReplicatedRocksDBTransactionState::numOperations() const noexcept {
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
