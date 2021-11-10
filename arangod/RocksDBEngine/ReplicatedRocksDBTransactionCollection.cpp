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

#include <algorithm>

#include "RocksDBEngine/RocksDBTransactionCollection.h"
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

#include "ReplicatedRocksDBTransactionCollection.h"
#include "RocksDBEngine/Methods/RocksDBReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationReadOnlyMethods.h"
#include "RocksDBEngine/Methods/RocksDBSingleOperationTrxMethods.h"
#include "RocksDBEngine/Methods/RocksDBTrxMethods.h"
#include "RocksDBEngine/ReplicatedRocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"

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
          std::make_unique<RocksDBSingleOperationTrxMethods>(trx, db);
    } else {
      _rocksMethods = std::make_unique<RocksDBTrxMethods>(trx, db);
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

  bool disableIndexing =
      !AccessMode::isWriteOrExclusive(accessType()) ||
      !std::any_of(collection()->getIndexes().begin(),
                   collection()->getIndexes().end(), [](auto& idx) {
                     // primary index is unique, but we can ignore it here.
                     // for secondary unique indexes we need to turn off the
                     // NO_INDEXING optimization
                     return idx->type() !=
                                Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX &&
                            idx->unique();
                   });

  if (disableIndexing) {
    // only turn it on when safe...
    _rocksMethods->DisableIndexing();
  }
}

/// @brief commit a transaction
Result ReplicatedRocksDBTransactionCollection::commitTransaction() {
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

uint64_t ReplicatedRocksDBTransactionCollection::numCommits() const {
  return _rocksMethods->numCommits();
}

uint64_t ReplicatedRocksDBTransactionCollection::numOperations()
    const noexcept {
  return _rocksMethods->numOperations();
}

bool ReplicatedRocksDBTransactionCollection::ensureSnapshot() {
  return _rocksMethods->ensureSnapshot();
}
