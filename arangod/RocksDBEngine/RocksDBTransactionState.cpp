////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBTransactionState.h"
#include "Aql/QueryCache.h"
#include "Basics/Exceptions.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Manager.h"
#include "Cache/Transaction.h"
#include "Logger/Logger.h"
#include "RestServer/TransactionManagerFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCounterManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/TransactionManager.h"
#include "VocBase/modes.h"
#include "VocBase/ticks.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>

using namespace arangodb;

struct RocksDBTransactionData final : public TransactionData {};

RocksDBSavePoint::RocksDBSavePoint(rocksdb::Transaction* trx)
    : _trx(trx), _committed(false) {
  _trx->SetSavePoint();
}

RocksDBSavePoint::~RocksDBSavePoint() {
  if (!_committed) {
    rollback();
  }
}

void RocksDBSavePoint::commit() {
  _committed = true;  // this will prevent the rollback
}

void RocksDBSavePoint::rollback() {
  _trx->RollbackToSavePoint();
  _committed = true;  // in order to not roll back again by accident
}

/// @brief transaction type
RocksDBTransactionState::RocksDBTransactionState(
    TRI_vocbase_t* vocbase, uint64_t maxTransSize,
    bool intermediateTransactionEnabled, uint64_t intermediateTransactionSize,
    uint64_t intermediateTransactionNumber)
    : TransactionState(vocbase),
      _rocksReadOptions(),
      _cacheTx(nullptr),
      _transactionSize(0),
      _maxTransactionSize(maxTransSize),
      _intermediateTransactionSize(intermediateTransactionSize),
      _intermediateTransactionNumber(intermediateTransactionNumber),
      _numInserts(0),
      _numUpdates(0),
      _numRemoves(0),
      _intermediateTransactionEnabled(intermediateTransactionEnabled) {}

/// @brief free a transaction container
RocksDBTransactionState::~RocksDBTransactionState() {
  if (_cacheTx != nullptr) {
    // note: endTransaction() will delete _cacheTrx!
    CacheManagerFeature::MANAGER->endTransaction(_cacheTx);
    _cacheTx = nullptr;
  }
}

/// @brief start a transaction
Result RocksDBTransactionState::beginTransaction(transaction::Hints hints) {
  LOG_TRX(this, _nestingLevel) << "beginning " << AccessMode::typeString(_type)
                               << " transaction";
  if (_nestingLevel == 0) {
    TRI_ASSERT(_status == transaction::Status::CREATED);

    // get a new id
    _id = TRI_NewTickServer();

    // register a protector
    auto data =
        std::make_unique<RocksDBTransactionData>();  // intentionally empty
    TransactionManagerFeature::MANAGER->registerTransaction(_id,
                                                            std::move(data));

    TRI_ASSERT(_rocksTransaction == nullptr);
    TRI_ASSERT(_cacheTx == nullptr);

    // start cache transaction
    _cacheTx =
        CacheManagerFeature::MANAGER->beginTransaction(isReadOnlyTransaction());

    // start rocks transaction
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    rocksdb::TransactionDB* db = static_cast<RocksDBEngine*>(engine)->db();
    _rocksTransaction.reset(db->BeginTransaction(
        _rocksWriteOptions, rocksdb::TransactionOptions()));
    _rocksTransaction->SetSnapshot();
    _rocksReadOptions.snapshot = _rocksTransaction->GetSnapshot();
  } else {
    TRI_ASSERT(_status == transaction::Status::RUNNING);
  }

  Result result = useCollections(_nestingLevel);

  if (result.ok()) {
    // all valid
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::RUNNING);
    }
  } else {
    // something is wrong
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::ABORTED);
    }

    // free what we have got so far
    unuseCollections(_nestingLevel);
  }

  return result;
}

/// @brief commit a transaction
Result RocksDBTransactionState::commitTransaction(
    transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel) << "committing " << AccessMode::typeString(_type)
                               << " transaction";

  TRI_ASSERT(_status == transaction::Status::RUNNING);
  TRI_IF_FAILURE("TransactionWriteCommitMarker") {
    return Result(TRI_ERROR_DEBUG);
  }

  arangodb::Result result;

  if (_nestingLevel == 0) {
    if (_rocksTransaction != nullptr) {
      // set wait for sync flag if required
      if (waitForSync()) {
        _rocksWriteOptions.sync = true;
      }

      // TODO wait for response on github issue to see how we can use the
      // sequence number
      result = rocksutils::convertStatus(_rocksTransaction->Commit());
      rocksdb::SequenceNumber latestSeq =
          rocksutils::globalRocksDB()->GetLatestSequenceNumber();
      if (!result.ok()) {
        abortTransaction(activeTrx);
        return result;
      }

      if (_cacheTx != nullptr) {
        // note: endTransaction() will delete _cacheTx!
        CacheManagerFeature::MANAGER->endTransaction(_cacheTx);
        _cacheTx = nullptr;
      }

      // LOG_TOPIC(ERR, Logger::FIXME) << "#" << _id << " COMMIT";

      rocksdb::Snapshot const* snap = this->_rocksReadOptions.snapshot;
      TRI_ASSERT(snap != nullptr);

      for (auto& trxCollection : _collections) {
        RocksDBTransactionCollection* collection =
            static_cast<RocksDBTransactionCollection*>(trxCollection);
        int64_t adjustment =
            collection->numInserts() - collection->numRemoves();

        if (collection->numInserts() != 0 || collection->numRemoves() != 0 ||
            collection->revision() != 0) {
          RocksDBCollection* coll = static_cast<RocksDBCollection*>(
              trxCollection->collection()->getPhysical());
          coll->adjustNumberDocuments(adjustment);
          coll->setRevision(collection->revision());
          RocksDBEngine* engine =
              static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);

          RocksDBCounterManager::CounterAdjustment update(latestSeq,
                                                      collection->numInserts(),
                                                      collection->numRemoves(),
                                                      collection->revision());
          engine->counterManager()->updateCounter(coll->objectId(), update);
        }
      }

      _rocksTransaction.reset();
    }

    updateStatus(transaction::Status::COMMITTED);

    // if a write query, clear the query cache for the participating collections
    if (AccessMode::isWriteOrExclusive(_type) && !_collections.empty() &&
        arangodb::aql::QueryCache::instance()->mayBeActive()) {
      clearQueryCache();
    }
  }

  unuseCollections(_nestingLevel);

  return result;
}

/// @brief abort and rollback a transaction
Result RocksDBTransactionState::abortTransaction(
    transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel) << "aborting " << AccessMode::typeString(_type)
                               << " transaction";
  TRI_ASSERT(_status == transaction::Status::RUNNING);
  Result result;

  if (_nestingLevel == 0) {
    if (_rocksTransaction != nullptr) {
      rocksdb::Status status = _rocksTransaction->Rollback();
      result = rocksutils::convertStatus(status);
      _rocksTransaction.reset();
    }

    if (_cacheTx != nullptr) {
      // note: endTransaction() will delete _cacheTx!
      CacheManagerFeature::MANAGER->endTransaction(_cacheTx);
      _cacheTx = nullptr;
    }

    // LOG_TOPIC(ERR, Logger::FIXME) << "#" << _id << " ABORT";

    updateStatus(transaction::Status::ABORTED);

    if (hasOperations()) {
      // must clean up the query cache because the transaction
      // may have queried something via AQL that is now rolled back
      clearQueryCache();
    }
  }

  unuseCollections(_nestingLevel);

  return result;
}

/// @brief add an operation for a transaction collection
RocksDBOperationResult RocksDBTransactionState::addOperation(
    TRI_voc_cid_t cid, TRI_voc_rid_t revisionId,
    TRI_voc_document_operation_e operationType, uint64_t operationSize,
    uint64_t keySize) {
  RocksDBOperationResult res;

  uint64_t newSize = _transactionSize + operationSize + keySize;
  if (_maxTransactionSize < newSize) {
    // we hit the transaction size limit
    std::string message = "maximal transaction size limit of " + std::to_string(_maxTransactionSize) + " bytes reached!";
    res.reset(TRI_ERROR_RESOURCE_LIMIT, message);
    return res;
  }

  auto collection =
      static_cast<RocksDBTransactionCollection*>(findCollection(cid));

  if (collection == nullptr) {
    std::string message = "collection '" + collection->collectionName() + "' not found in transaction state";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  // should not fail or fail with exception
  collection->addOperation(operationType, operationSize, revisionId);

  switch (operationType) {
    case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
      break;
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      ++_numInserts;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      ++_numUpdates;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
      ++_numRemoves;
      break;
  }

  _transactionSize = newSize;
  auto numOperations = _numInserts + _numUpdates + _numRemoves;

  // signal if intermediate commit is required
  // this will be done if intermediate transactions are enabled
  // and either the "number of operations" or the "transaction size"
  // has reached the limit
  if (_intermediateTransactionEnabled &&
      (_intermediateTransactionNumber <= numOperations ||
       _intermediateTransactionSize <= newSize)) {
    res.commitRequired(true);
  }

  return res;
}
  
void RocksDBTransactionState::reset() {
  //only reset when already commited
  TRI_ASSERT(_status == transaction::Status::COMMITTED);
  //reset count
  _transactionSize = 0;
  _numInserts = 0;
  _numUpdates = 0;
  _numRemoves = 0;
  unuseCollections(_nestingLevel);
  for (auto it = _collections.rbegin(); it != _collections.rend(); ++it) {
    (static_cast<RocksDBTransactionCollection*>(*it))->resetCounts();
  }
  _nestingLevel = 0;

  updateStatus(transaction::Status::CREATED);

  // start new transaction
  beginTransaction(transaction::Hints());
}
