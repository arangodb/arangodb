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

#include "ReplicatedRocksDBTransactionCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
#include "RocksDBEngine/Methods/RocksDBReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationTrxMethods.h"
#include "RocksDBEngine/Methods/RocksDBTrxMethods.h"
#include "RocksDBEngine/ReplicatedRocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"

#include <algorithm>

using namespace arangodb;

ReplicatedRocksDBTransactionCollection::ReplicatedRocksDBTransactionCollection(
    ReplicatedRocksDBTransactionState* trx, DataSourceId cid,
    AccessMode::Type accessType)
    : RocksDBTransactionCollection(trx, cid, accessType) {}

ReplicatedRocksDBTransactionCollection::
    ~ReplicatedRocksDBTransactionCollection() {}

Result ReplicatedRocksDBTransactionCollection::beginTransaction() {
  auto* trx = static_cast<RocksDBTransactionState*>(_transaction);
  auto& selector = trx->vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();

  if (trx->isReadOnlyTransaction()) {
    if (trx->isSingleOperation()) {
      _rocksMethods =
          std::make_unique<RocksDBSingleOperationReadOnlyMethods>(trx, db);
    } else {
      _rocksMethods = std::make_unique<RocksDBReadOnlyMethods>(trx, db);
    }
  } else {
    if (trx->isSingleOperation()) {
      _rocksMethods =
          std::make_unique<RocksDBSingleOperationTrxMethods>(trx, *this, db);
    } else {
      _rocksMethods = std::make_unique<RocksDBTrxMethods>(trx, *this, db);
    }
  }

  auto res = _rocksMethods->beginTransaction();
  if (res.ok()) {
    maybeDisableIndexing();
  }

  return res;
}

void ReplicatedRocksDBTransactionCollection::maybeDisableIndexing() {
  if (!_transaction->hasHint(transaction::Hints::Hint::NO_INDEXING)) {
    return;
  }

  TRI_ASSERT(!_transaction->isReadOnlyTransaction());
  // do not track our own writes... we can only use this in very
  // specific scenarios, i.e., when we are sure that we will have a
  // single operation transaction or we are sure we are writing
  // unique keys

  auto const indexes = collection()->getIndexes();

  bool disableIndexing =
      !AccessMode::isWriteOrExclusive(accessType()) ||
      !std::any_of(indexes.begin(), indexes.end(), [](auto& idx) {
        // primary index is unique, but we can ignore it here.
        // for secondary unique indexes we need to turn off the
        // NO_INDEXING optimization
        return idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX &&
               idx->unique();
      });

  if (disableIndexing) {
    // only turn it on when safe...
    _rocksMethods->DisableIndexing();
  }
}

/// @brief commit a transaction
Result ReplicatedRocksDBTransactionCollection::commitTransaction() {
  LOG_DEVEL << "commit";
  auto lock = static_cast<ReplicatedRocksDBTransactionState*>(_transaction)
                  ->lockCommit();
  return _rocksMethods->commitTransaction();
}

/// @brief abort and rollback a transaction
Result ReplicatedRocksDBTransactionCollection::abortTransaction() {
  return _rocksMethods->abortTransaction();
}

void ReplicatedRocksDBTransactionCollection::beginQuery(
    bool isModificationQuery) {
  auto* trxMethods = dynamic_cast<RocksDBTrxMethods*>(_rocksMethods.get());
  if (trxMethods) {
    trxMethods->beginQuery(isModificationQuery);
  }
}

void ReplicatedRocksDBTransactionCollection::endQuery(
    bool isModificationQuery) noexcept {
  auto* trxMethods = dynamic_cast<RocksDBTrxMethods*>(_rocksMethods.get());
  if (trxMethods) {
    trxMethods->endQuery(isModificationQuery);
  }
}

TRI_voc_tick_t ReplicatedRocksDBTransactionCollection::lastOperationTick()
    const noexcept {
  return _rocksMethods->lastOperationTick();
}

uint64_t ReplicatedRocksDBTransactionCollection::numCommits() const noexcept {
  return _rocksMethods->numCommits();
}

uint64_t ReplicatedRocksDBTransactionCollection::numIntermediateCommits()
    const noexcept {
  return _rocksMethods->numIntermediateCommits();
}

uint64_t ReplicatedRocksDBTransactionCollection::numOperations()
    const noexcept {
  return _rocksMethods->numOperations();
}

uint64_t ReplicatedRocksDBTransactionCollection::numPrimitiveOperations()
    const noexcept {
  return _rocksMethods->numPrimitiveOperations();
}

bool ReplicatedRocksDBTransactionCollection::ensureSnapshot() {
  return _rocksMethods->ensureSnapshot();
}

auto ReplicatedRocksDBTransactionCollection::leaderState() -> std::shared_ptr<
    replication2::replicated_state::document::DocumentLeaderState> {
  // leaderState should only be requested in cases where we are expected to be
  // leader, in which case _leaderState should always be initialized!
  ADB_PROD_ASSERT(_leaderState != nullptr);
  return _leaderState;
}

rocksdb::SequenceNumber ReplicatedRocksDBTransactionCollection::prepare() {
  auto* trx = static_cast<RocksDBTransactionState*>(_transaction);
  auto& engine = trx->vocbase()
                     .server()
                     .getFeature<EngineSelectorFeature>()
                     .engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  rocksdb::SequenceNumber preSeq = db->GetLatestSequenceNumber();
  rocksdb::SequenceNumber seq = prepareTransaction(_transaction->id());
  return std::max(seq, preSeq);
}

void ReplicatedRocksDBTransactionCollection::commit(
    rocksdb::SequenceNumber lastWritten) {
  TRI_ASSERT(lastWritten > 0);

  // we need this in case of an intermediate commit. The number of
  // initial documents is adjusted and numInserts / removes is set to 0
  // index estimator updates are buffered
  commitCounts(_transaction->id(), lastWritten);
}

void ReplicatedRocksDBTransactionCollection::cleanup() {
  abortCommit(_transaction->id());
}

auto ReplicatedRocksDBTransactionCollection::ensureCollection() -> Result {
  auto res = RocksDBTransactionCollection::ensureCollection();

  if (res.fail()) {
    return res;
  }

  // We only need to fetch the leaderState for non-read accesses. Note that this
  // also covers the case that ReplicatedRocksDBTransactionState instances can
  // be created on followers (but just for read-only access) in which case we
  // obviously must not attempt to fetch the leaderState.
  if (accessType() != AccessMode::Type::READ &&
      // index creation is read-only, but might still use an exclusive lock
      !_transaction->hasHint(transaction::Hints::Hint::INDEX_CREATION) &&
      _leaderState == nullptr) {
    // Note that doing this here is only correct as long as we're not supporting
    // distributeShardsLike.
    // Later, we must make sure to get the very same state for all collections
    // (shards) belonging to the same collection group (shard sheaf) (i.e.
    // belong to the same distributeShardsLike group) See
    // https://arangodb.atlassian.net/browse/CINFRA-294.
    _leaderState = _collection->getDocumentStateLeader();
    ADB_PROD_ASSERT(_leaderState != nullptr);
  }

  return res;
}

futures::Future<Result>
ReplicatedRocksDBTransactionCollection::performIntermediateCommitIfRequired() {
  if (_rocksMethods->isIntermediateCommitNeeded()) {
    auto leader = leaderState();
    auto operation = replication2::replicated_state::document::OperationType::
        kIntermediateCommit;
    auto options = replication2::replicated_state::document::ReplicationOptions{
        .waitForCommit = true};
    try {
      return leader
          ->replicateOperation(velocypack::SharedSlice{}, operation,
                               _transaction->id(), options)
          .thenValue([state = _transaction->shared_from_this(),
                      this](auto&& res) -> Result {
            return _rocksMethods->triggerIntermediateCommit();
          });
    } catch (basics::Exception const& e) {
      return Result{e.code(), e.what()};
    } catch (std::exception const& e) {
      return Result{TRI_ERROR_INTERNAL, e.what()};
    }
  }
  return Result{};
}
