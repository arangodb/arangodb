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

#include "SimpleRocksDBTransactionState.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RocksDBEngine/Methods/RocksDBReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationTrxMethods.h"
#include "RocksDBEngine/Methods/RocksDBTrxMethods.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;

SimpleRocksDBTransactionState::SimpleRocksDBTransactionState(
    TRI_vocbase_t& vocbase, TransactionId tid,
    transaction::Options const& options,
    transaction::OperationOrigin operationOrigin)
    : RocksDBTransactionState(vocbase, tid, options, operationOrigin) {}

SimpleRocksDBTransactionState::~SimpleRocksDBTransactionState() {}

futures::Future<Result> SimpleRocksDBTransactionState::beginTransaction(
    transaction::Hints hints) {
  auto res = co_await RocksDBTransactionState::beginTransaction(hints);
  if (!res.ok()) {
    co_return res;
  }

  rocksdb::TransactionDB* db = vocbase().engine<RocksDBEngine>().db();

  TRI_ASSERT(_rocksMethods == nullptr);

  if (isReadOnlyTransaction()) {
    if (isSingleOperation()) {
      _rocksMethods =
          std::make_unique<RocksDBSingleOperationReadOnlyMethods>(this, db);
    } else {
      _rocksMethods = std::make_unique<RocksDBReadOnlyMethods>(this, db);
    }
  } else {
    if (isSingleOperation()) {
      _rocksMethods =
          std::make_unique<RocksDBSingleOperationTrxMethods>(this, *this, db);
    } else {
      _rocksMethods = std::make_unique<RocksDBTrxMethods>(this, *this, db);
    }
  }

  TRI_ASSERT(_rocksMethods != nullptr);

  res = _rocksMethods->beginTransaction();
  if (res.ok()) {
    maybeDisableIndexing();
  }

  co_return res;
}

void SimpleRocksDBTransactionState::maybeDisableIndexing() {
  if (!hasHint(transaction::Hints::Hint::NO_INDEXING)) {
    return;
  }

  TRI_ASSERT(!isReadOnlyTransaction());
  // do not track our own writes... we can only use this in very
  // specific scenarios, i.e. when we are sure that we will have a
  // single operation transaction or we are sure we are writing
  // unique keys

  // we must check if there is a unique secondary index for any of the
  // collections we write into in case it is, we must disable NO_INDEXING
  // here, as it wouldn't be safe
  bool disableIndexing = true;

  {
    RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
    for (auto& trxCollection : _collections) {
      if (!AccessMode::isWriteOrExclusive(trxCollection->accessType())) {
        continue;
      }
      auto indexes =
          trxCollection->collection()->getPhysical()->getAllIndexes();
      for (auto const& idx : indexes) {
        if (idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
          // primary index is unique, but we can ignore it here.
          // we are only looking for secondary indexes
          continue;
        }
        if (idx->unique()) {
          // found secondary unique index. we need to turn off the
          // NO_INDEXING optimization now
          disableIndexing = false;
          break;
        }
      }
    }
  }

  if (disableIndexing) {
    // only turn it on when safe...
    _rocksMethods->DisableIndexing();
  }
}

/// @brief commit a transaction
futures::Future<Result> SimpleRocksDBTransactionState::doCommit() {
  return _rocksMethods->commitTransaction();
}

/// @brief abort and rollback a transaction
Result SimpleRocksDBTransactionState::doAbort() {
  return _rocksMethods->abortTransaction();
}

void SimpleRocksDBTransactionState::beginQuery(
    std::shared_ptr<ResourceMonitor> resourceMonitor,
    bool isModificationQuery) {
  auto* trxMethods = dynamic_cast<RocksDBTrxMethods*>(_rocksMethods.get());
  if (trxMethods) {
    trxMethods->beginQuery(std::move(resourceMonitor), isModificationQuery);
  }
}

void SimpleRocksDBTransactionState::endQuery(
    bool isModificationQuery) noexcept {
  auto* trxMethods = dynamic_cast<RocksDBTrxMethods*>(_rocksMethods.get());
  if (trxMethods) {
    trxMethods->endQuery(isModificationQuery);
  }
}

TRI_voc_tick_t SimpleRocksDBTransactionState::lastOperationTick()
    const noexcept {
  return _rocksMethods->lastOperationTick();
}

uint64_t SimpleRocksDBTransactionState::numCommits() const noexcept {
  return _rocksMethods->numCommits();
}

uint64_t SimpleRocksDBTransactionState::numIntermediateCommits()
    const noexcept {
  return _rocksMethods->numIntermediateCommits();
}

void SimpleRocksDBTransactionState::addIntermediateCommits(uint64_t value) {
  // this is not supposed to be called, ever
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid call to addIntermediateCommits");
}

bool SimpleRocksDBTransactionState::hasOperations() const noexcept {
  return _rocksMethods->hasOperations();
}

uint64_t SimpleRocksDBTransactionState::numOperations() const noexcept {
  return _rocksMethods->numOperations();
}

uint64_t SimpleRocksDBTransactionState::numPrimitiveOperations()
    const noexcept {
  return _rocksMethods->numPrimitiveOperations();
}

bool SimpleRocksDBTransactionState::ensureSnapshot() {
  return _rocksMethods->ensureSnapshot();
}

rocksdb::SequenceNumber SimpleRocksDBTransactionState::beginSeq() const {
  return _rocksMethods->GetSequenceNumber();
}

std::string SimpleRocksDBTransactionState::debugInfo() const {
  // serialize transaction options
  velocypack::Builder builder;
  builder.openObject();
  options().toVelocyPack(builder);
  builder.close();

  return absl::StrCat(
      "num operations: ", numOperations(), ", tid: ", id().id(),
      ", transaction options: ", builder.slice().toJson(),
      ", transaction hints: ", _hints.toString(), ", actor: ", actorName(),
      ", num collections: ", numCollections(),
      ", num primitive operations: ", numPrimitiveOperations(),
      ", num commits: ", numCommits(),
      ", num intermediate commits: ", numIntermediateCommits(),
      ", is follower trx: ", (isFollowerTransaction() ? "yes" : "no"),
      ", is read only trx: ", (isReadOnlyTransaction() ? "yes" : "no"),
      ", is single: ", (isSingleOperation() ? "yes" : "no"),
      ", is only exclusive: ", (isOnlyExclusiveTransaction() ? "yes" : "no"),
      ", is indexing disabled: ",
      (_rocksMethods->isIndexingDisabled() ? "yes" : "no"));
}

std::unique_ptr<TransactionCollection>
SimpleRocksDBTransactionState::createTransactionCollection(
    DataSourceId cid, AccessMode::Type accessType) {
  return std::unique_ptr<TransactionCollection>(
      new RocksDBTransactionCollection(this, cid, accessType));
}

rocksdb::SequenceNumber SimpleRocksDBTransactionState::prepare() {
  rocksdb::TransactionDB* db = vocbase().engine<RocksDBEngine>().db();

  rocksdb::SequenceNumber preSeq = db->GetLatestSequenceNumber();

  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  for (auto& trxColl : _collections) {
    auto* coll = static_cast<RocksDBTransactionCollection*>(trxColl);

    rocksdb::SequenceNumber seq = coll->prepareTransaction(id());
    preSeq = std::max(seq, preSeq);
  }

  return preSeq;
}

void SimpleRocksDBTransactionState::commit(
    rocksdb::SequenceNumber lastWritten) {
  TRI_ASSERT(lastWritten > 0);
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  for (auto& trxColl : _collections) {
    auto* coll = static_cast<RocksDBTransactionCollection*>(trxColl);
    // we need this in case of an intermediate commit. The number of
    // initial documents is adjusted and numInserts / removes is set to 0
    // index estimator updates are buffered
    coll->commitCounts(id(), lastWritten);
  }
}

void SimpleRocksDBTransactionState::cleanup() {
  RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
  for (auto& trxColl : _collections) {
    auto* coll = static_cast<RocksDBTransactionCollection*>(trxColl);
    coll->abortCommit(id());
  }
}

arangodb::Result SimpleRocksDBTransactionState::triggerIntermediateCommit() {
  return _rocksMethods->triggerIntermediateCommit();
}

futures::Future<Result>
SimpleRocksDBTransactionState::performIntermediateCommitIfRequired(
    DataSourceId cid) {
  if (_rocksMethods->isIntermediateCommitNeeded()) {
    return triggerIntermediateCommit();
  }
  return Result{};
}
