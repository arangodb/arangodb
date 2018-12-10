////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBTransactionCollection.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

RocksDBTransactionCollection::RocksDBTransactionCollection(
    TransactionState* trx, TRI_voc_cid_t cid, AccessMode::Type accessType,
    int nestingLevel)
    : TransactionCollection(trx, cid, accessType),
      _nestingLevel(nestingLevel),
      _initialNumberDocuments(0),
      _revision(0),
      _numInserts(0),
      _numUpdates(0),
      _numRemoves(0),
      _usageLocked(false) {}

RocksDBTransactionCollection::~RocksDBTransactionCollection() = default;

/// @brief whether or not any write operations for the collection happened
bool RocksDBTransactionCollection::hasOperations() const {
  return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
}

void RocksDBTransactionCollection::freeOperations(
    transaction::Methods* /*activeTrx*/, bool /*mustRollback*/) {}

bool RocksDBTransactionCollection::canAccess(
    AccessMode::Type accessType) const {
  if (!_collection) {
    return false; // not opened. probably a mistake made by the caller
  }

  // check if access type matches
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // type doesn't match. probably also a mistake by the caller
    return false;
  }

  return true;
}

int RocksDBTransactionCollection::updateUsage(AccessMode::Type accessType,
                                              int nestingLevel) {
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    if (nestingLevel > 0) {
      // trying to write access a collection that is only marked with
      // read-access
      return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
    }

    TRI_ASSERT(nestingLevel == 0);

    // upgrade collection type to write-access
    _accessType = accessType;
  }

  if (nestingLevel < _nestingLevel) {
    _nestingLevel = nestingLevel;
  }

  // all correct
  return TRI_ERROR_NO_ERROR;
}

int RocksDBTransactionCollection::use(int nestingLevel) {
  if (_nestingLevel != nestingLevel) {
    // only process our own collections
    return TRI_ERROR_NO_ERROR;
  }

  bool doSetup = false;

  if (_collection == nullptr) {
    // open the collection
    if (!_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER) &&
        !_transaction->hasHint(transaction::Hints::Hint::NO_USAGE_LOCK)) {
      // use and usage-lock
      TRI_vocbase_col_status_e status;

      LOG_TRX(_transaction, nestingLevel) << "using collection " << _cid;
      TRI_set_errno(TRI_ERROR_NO_ERROR); // clear error state so can get valid error below
      _collection = _transaction->vocbase().useCollection(_cid, status);

      if (!_collection) {
        // must return an error
        return TRI_ERROR_NO_ERROR == TRI_errno()
          ? TRI_ERROR_INTERNAL : TRI_errno();
      }

      _usageLocked = true;
    } else {
      // use without usage-lock (lock already set externally)
      _collection = _transaction->vocbase().lookupCollection(_cid);

      if (_collection == nullptr) {
        return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
      }
    }

    doSetup = true;
  }

  TRI_ASSERT(_collection != nullptr);

  if (AccessMode::isWriteOrExclusive(_accessType) && !isLocked()) {
    // r/w lock the collection
    int res = doLock(_accessType, nestingLevel);

    if (res == TRI_ERROR_LOCKED) {
      // TRI_ERROR_LOCKED is not an error, but it indicates that the lock operation has actually acquired the lock
      // (and that the lock has not been held before)
      res = TRI_ERROR_NO_ERROR;
    } else if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  if (doSetup) {
    RocksDBCollection* rc =
        static_cast<RocksDBCollection*>(_collection->getPhysical());
    _initialNumberDocuments = rc->numberDocuments();
    _revision = rc->revision();
  }

  return TRI_ERROR_NO_ERROR;
}

void RocksDBTransactionCollection::unuse(int nestingLevel) {
  // nothing to do here. we're postponing the unlocking until release()
}

void RocksDBTransactionCollection::release() {
  if (isLocked()) {
    // unlock our own r/w locks
    doUnlock(_accessType, 0);
    _lockType = AccessMode::Type::NONE;
  }

  // the top level transaction releases all collections
  if (_collection != nullptr) {
    // unuse collection, remove usage-lock
    LOG_TRX(_transaction, 0) << "unusing collection " << _cid;

    if (_usageLocked) {
      _transaction->vocbase().releaseCollection(_collection.get());
      _usageLocked = false;
    }

    _collection = nullptr;
  } else {
    TRI_ASSERT(!_usageLocked);
  }
}

/// @brief add an operation for a transaction collection
void RocksDBTransactionCollection::addOperation(
    TRI_voc_document_operation_e operationType,
    TRI_voc_rid_t revisionId) {
  switch (operationType) {
    case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
      break;
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      ++_numInserts;
      _revision = revisionId;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      ++_numUpdates;
      _revision = revisionId;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
      ++_numRemoves;
      _revision = revisionId;
      break;
  }
}

void RocksDBTransactionCollection::prepareCommit(uint64_t trxId,
                                                 uint64_t preCommitSeq) {
  TRI_ASSERT(_collection != nullptr);
  if (hasOperations() || !_trackedIndexOperations.empty()) {
    RocksDBCollection* coll = static_cast<RocksDBCollection*>(_collection->getPhysical());
    coll->meta().placeBlocker(trxId, preCommitSeq);
  }
}

void RocksDBTransactionCollection::abortCommit(uint64_t trxId) {
  TRI_ASSERT(_collection != nullptr);
  if (hasOperations() || !_trackedIndexOperations.empty()) {
    RocksDBCollection* coll = static_cast<RocksDBCollection*>(_collection->getPhysical());
    coll->meta().removeBlocker(trxId);
  }
}

void RocksDBTransactionCollection::commitCounts(uint64_t trxId,
                                                uint64_t commitSeq) {
  TRI_ASSERT(_collection != nullptr);

  // Update the collection count
  int64_t const adjustment = _numInserts - _numRemoves;
  if (hasOperations()) {
    TRI_ASSERT(_revision != 0 && commitSeq != 0);
    RocksDBCollection* coll = static_cast<RocksDBCollection*>(_collection->getPhysical());
    coll->adjustNumberDocuments(_revision, adjustment); // update online count
    coll->meta().adjustNumberDocuments(commitSeq, _revision, adjustment); // buffer for recovery
  }

  // Update the index estimates.
  for (auto& pair : _trackedIndexOperations) {
    auto idx = _collection->lookupIndex(pair.first);
    if (idx == nullptr) {
      TRI_ASSERT(false); // Index reported estimates, but does not exist
      continue;
    }
    auto ridx = static_cast<RocksDBIndex*>(idx.get());
    auto est = ridx->estimator();
    if (ADB_LIKELY(est != nullptr)) {
      est->bufferUpdates(commitSeq, std::move(pair.second.inserts),
                         std::move(pair.second.removals));
    } else {
      TRI_ASSERT(false);
    }
  }
  
  if (hasOperations() || !_trackedIndexOperations.empty()) {
    RocksDBCollection* coll = static_cast<RocksDBCollection*>(_collection->getPhysical());
    coll->meta().removeBlocker(trxId);
  }

  _initialNumberDocuments += adjustment;
  _numInserts = 0;
  _numUpdates = 0;
  _numRemoves = 0;
  _trackedIndexOperations.clear();
}

void RocksDBTransactionCollection::trackIndexInsert(uint64_t idxObjectId,
                                                    uint64_t hash) {
  // First list is Inserts
  _trackedIndexOperations[idxObjectId].inserts.emplace_back(hash);
}

void RocksDBTransactionCollection::trackIndexRemove(uint64_t idxObjectId,
                                                    uint64_t hash) {
  // Second list is Removes
  _trackedIndexOperations[idxObjectId].removals.emplace_back(hash);
}

/// @brief lock a collection
/// returns TRI_ERROR_LOCKED in case the lock was successfully acquired
/// returns TRI_ERROR_NO_ERROR in case the lock does not need to be acquired and no other error occurred
/// returns any other error code otherwise
int RocksDBTransactionCollection::doLock(AccessMode::Type type,
                                         int nestingLevel) {
  if (!AccessMode::isWriteOrExclusive(type)) {
    _lockType = type;
    return TRI_ERROR_NO_ERROR;
  }

  if (_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER)) {
    // never lock
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(_collection != nullptr);

  std::string collName(_collection->name());
  if (_transaction->isLockedShard(collName)) {
    // do not lock by command
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(!isLocked());

  auto physical = static_cast<RocksDBCollection*>(_collection->getPhysical());
  TRI_ASSERT(physical != nullptr);

  double timeout = _transaction->timeout();
  if (_transaction->hasHint(transaction::Hints::Hint::TRY_LOCK)) {
    // give up early if we cannot acquire the lock instantly
    timeout = 0.00000001;
  }

  LOG_TRX(_transaction, nestingLevel) << "write-locking collection " << _cid;
  int res;
  if (AccessMode::isExclusive(type)) {
    // exclusive locking means we'll be acquiring the collection's RW lock in
    // write mode
    res = physical->lockWrite(timeout);
  } else {
    // write locking means we'll be acquiring the collection's RW lock in read
    // mode
    res = physical->lockRead(timeout);
  }

  if (res == TRI_ERROR_NO_ERROR) {
    _lockType = type;
    // not an error, but we use TRI_ERROR_LOCKED to indicate that we actually acquired the lock ourselves
    return TRI_ERROR_LOCKED;
  }

  if (res == TRI_ERROR_LOCK_TIMEOUT && timeout >= 0.1) {
    LOG_TOPIC(WARN, Logger::QUERIES)
        << "timed out after " << timeout << " s waiting for "
        << AccessMode::typeString(type) << "-lock on collection '"
        << _collection->name() << "'";
  }

  return res;
}

/// @brief unlock a collection
int RocksDBTransactionCollection::doUnlock(AccessMode::Type type,
                                           int nestingLevel) {
  if (!AccessMode::isWriteOrExclusive(type) ||
      !AccessMode::isWriteOrExclusive(_lockType)) {
    _lockType = AccessMode::Type::NONE;
    return TRI_ERROR_NO_ERROR;
  }

  if (_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER)) {
    // never unlock
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(_collection != nullptr);

  std::string collName(_collection->name());
  if (_transaction->isLockedShard(collName)) {
    // do not lock by command
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(isLocked());

  if (_nestingLevel < nestingLevel) {
    // only process our own collections
    return TRI_ERROR_NO_ERROR;
  }

  if (!AccessMode::isWriteOrExclusive(type) &&
      AccessMode::isWriteOrExclusive(_lockType)) {
    // do not remove a write-lock if a read-unlock was requested!
    return TRI_ERROR_NO_ERROR;
  }
  if (AccessMode::isWriteOrExclusive(type) &&
      !AccessMode::isWriteOrExclusive(_lockType)) {
    // we should never try to write-unlock a collection that we have only
    // read-locked
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES) << "logic error in doUnlock";
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }

  TRI_ASSERT(_collection);

  auto physical = static_cast<RocksDBCollection*>(_collection->getPhysical());
  TRI_ASSERT(physical != nullptr);

  LOG_TRX(_transaction, nestingLevel) << "write-unlocking collection " << _cid;
  if (AccessMode::isExclusive(type)) {
    // exclusive locking means we'll be releasing the collection's RW lock in
    // write mode
    physical->unlockWrite();
  } else {
    // write locking means we'll be releasing the collection's RW lock in read
    // mode
    physical->unlockRead();
  }

  _lockType = AccessMode::Type::NONE;

  return TRI_ERROR_NO_ERROR;
}
