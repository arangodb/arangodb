////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "SimpleRocksDBTransactionState.h"

#include "RocksDBEngine/Methods/RocksDBReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationTrxMethods.h"
#include "RocksDBEngine/Methods/RocksDBTrxMethods.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"

using namespace arangodb;

SimpleRocksDBTransactionState::SimpleRocksDBTransactionState(TRI_vocbase_t& vocbase,
                                                             TransactionId tid,
                                                             transaction::Options const& options)
    : RocksDBTransactionState(vocbase, tid, options) {}

SimpleRocksDBTransactionState::~SimpleRocksDBTransactionState() {}

Result SimpleRocksDBTransactionState::beginTransaction(transaction::Hints hints) {
  auto res = RocksDBTransactionState::beginTransaction(hints);
  if (!res.ok()) {
    return res;
  }

  auto& selector = vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();

  if (isReadOnlyTransaction()) {
    if (isSingleOperation()) {
      _rocksMethods = std::make_unique<RocksDBSingleOperationReadOnlyMethods>(this, db);
    } else {
      _rocksMethods = std::make_unique<RocksDBReadOnlyMethods>(this, db);
    }
  } else {
    if (isSingleOperation()) {
      _rocksMethods = std::make_unique<RocksDBSingleOperationTrxMethods>(this, db);
    } else {
      _rocksMethods = std::make_unique<RocksDBTrxMethods>(this, db);
    }
  }
  
  res = _rocksMethods->beginTransaction();
  if (res.ok()) {
    maybeDisableIndexing();
  }
  
  return res;
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

  for (auto& trxCollection : _collections) {
    if (!AccessMode::isWriteOrExclusive(trxCollection->accessType())) {
      continue;
    }
    auto indexes = trxCollection->collection()->getIndexes();
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

  if (disableIndexing) {
    // only turn it on when safe...
    _rocksMethods->DisableIndexing();
  }
}

/// @brief commit a transaction
Result SimpleRocksDBTransactionState::doCommit() {
  return _rocksMethods->commitTransaction();  
}

/// @brief abort and rollback a transaction
Result SimpleRocksDBTransactionState::doAbort() {
  return _rocksMethods->abortTransaction();
}

void SimpleRocksDBTransactionState::beginQuery(bool isModificationQuery) {
  auto* trxMethods = dynamic_cast<RocksDBTrxMethods*>(_rocksMethods.get());
  if (trxMethods) {
    trxMethods->beginQuery(isModificationQuery);
  }
}

void SimpleRocksDBTransactionState::endQuery(bool isModificationQuery) noexcept {
  auto* trxMethods = dynamic_cast<RocksDBTrxMethods*>(_rocksMethods.get());
  if (trxMethods) {
    trxMethods->endQuery(isModificationQuery);
  }
}

TRI_voc_tick_t SimpleRocksDBTransactionState::lastOperationTick() const noexcept {
  return _rocksMethods->lastOperationTick();
}
  
uint64_t SimpleRocksDBTransactionState::numCommits() const {
  return _rocksMethods->numCommits();
}

bool SimpleRocksDBTransactionState::hasOperations() const noexcept {
  return _rocksMethods->hasOperations();
}

uint64_t SimpleRocksDBTransactionState::numOperations() const noexcept {
  return _rocksMethods->numOperations();
}

bool SimpleRocksDBTransactionState::ensureSnapshot() {
  return _rocksMethods->ensureSnapshot();
}

rocksdb::SequenceNumber SimpleRocksDBTransactionState::beginSeq() const {
  return _rocksMethods->GetSequenceNumber();
}

std::unique_ptr<TransactionCollection> SimpleRocksDBTransactionState::createTransactionCollection(
    DataSourceId cid, AccessMode::Type accessType) {
  return std::unique_ptr<TransactionCollection>(
      new RocksDBTransactionCollection(this, cid, accessType));
}
