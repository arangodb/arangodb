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
#include "Cluster/CollectionLockState.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/modes.h"

using namespace arangodb;

RocksDBTransactionCollection::RocksDBTransactionCollection(
    TransactionState* trx, TRI_voc_cid_t cid, AccessMode::Type accessType,
    int nestingLevel)
    : TransactionCollection(trx, cid, accessType),
      _lockType(AccessMode::Type::NONE),
      _nestingLevel(nestingLevel),
      _initialNumberDocuments(0),
      _revision(0),
      _operationSize(0),
      _numInserts(0),
      _numUpdates(0),
      _numRemoves(0),
      _usageLocked(false) {}

RocksDBTransactionCollection::~RocksDBTransactionCollection() {}

/// @brief request a main-level lock for a collection
int RocksDBTransactionCollection::lock() { return lock(_accessType, 0); }

/// @brief request a lock for a collection
int RocksDBTransactionCollection::lock(AccessMode::Type accessType,
                                       int nestingLevel) {
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // wrong lock type
    return TRI_ERROR_INTERNAL;
  }

  if (isLocked()) {
    // already locked
    return TRI_ERROR_NO_ERROR;
  }

  return doLock(accessType, nestingLevel);
}

/// @brief request an unlock for a collection
int RocksDBTransactionCollection::unlock(AccessMode::Type accessType,
                                         int nestingLevel) {
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // wrong lock type: write-unlock requested but collection is read-only
    return TRI_ERROR_INTERNAL;
  }

  if (!isLocked()) {
    // already unlocked
    return TRI_ERROR_NO_ERROR;
  }

  return doUnlock(accessType, nestingLevel);
}

/// @brief check if a collection is locked in a specific mode in a transaction
bool RocksDBTransactionCollection::isLocked(AccessMode::Type accessType,
                                            int nestingLevel) const {
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // wrong lock type
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "logic error. checking wrong lock type";
    return false;
  }

  return isLocked();
}

/// @brief check whether a collection is locked at all
bool RocksDBTransactionCollection::isLocked() const {
  return (_lockType != AccessMode::Type::NONE);
}

/// @brief whether or not any write operations for the collection happened
bool RocksDBTransactionCollection::hasOperations() const {
  return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
}

void RocksDBTransactionCollection::freeOperations(
    transaction::Methods* /*activeTrx*/, bool /*mustRollback*/) {}

bool RocksDBTransactionCollection::canAccess(
    AccessMode::Type accessType) const {
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
      _collection = _transaction->vocbase()->useCollection(_cid, status);
      if (_collection != nullptr) {
        _usageLocked = true;

        // geo index needs exclusive write access
        RocksDBCollection* rc =
            static_cast<RocksDBCollection*>(_collection->getPhysical());
        if (AccessMode::isWrite(_accessType) && rc->hasGeoIndex()) {
          _accessType = AccessMode::Type::EXCLUSIVE;
        }
      }
    } else {
      // use without usage-lock (lock already set externally)
      _collection = _transaction->vocbase()->lookupCollection(_cid);

      if (_collection == nullptr) {
        return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
      }
    }

    if (_collection == nullptr) {
      int res = TRI_errno();
      if (res == TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED) {
        return res;
      }
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    if (AccessMode::isWriteOrExclusive(_accessType) &&
        TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE &&
        !LogicalCollection::IsSystemName(_collection->name())) {
      return TRI_ERROR_ARANGO_READ_ONLY;
    }

    doSetup = true;
  }

  TRI_ASSERT(_collection != nullptr);

  if (AccessMode::isWriteOrExclusive(_accessType) && !isLocked()) {
    // r/w lock the collection
    int res = doLock(_accessType, nestingLevel);

    if (res != TRI_ERROR_NO_ERROR) {
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
      _transaction->vocbase()->releaseCollection(_collection);
      _usageLocked = false;
    }
    _collection = nullptr;
  }
}

/// @brief add an operation for a transaction collection
void RocksDBTransactionCollection::addOperation(
    TRI_voc_document_operation_e operationType, uint64_t operationSize,
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
  _operationSize += operationSize;
}

void RocksDBTransactionCollection::commitCounts() {
  _initialNumberDocuments = _numInserts - _numRemoves;
  _operationSize = 0;
  _numInserts = 0;
  _numUpdates = 0;
  _numRemoves = 0;
}

/// @brief lock a collection
int RocksDBTransactionCollection::doLock(AccessMode::Type type,
                                         int nestingLevel) {
  if (!AccessMode::isWriteOrExclusive(type)) {
    return TRI_ERROR_NO_ERROR;
  }

  if (_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER)) {
    // never lock
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(_collection != nullptr);

  if (CollectionLockState::_noLockHeaders != nullptr) {
    std::string collName(_collection->name());
    auto it = CollectionLockState::_noLockHeaders->find(collName);
    if (it != CollectionLockState::_noLockHeaders->end()) {
      // do not lock by command
      return TRI_ERROR_NO_ERROR;
    }
  }

  TRI_ASSERT(!isLocked());

  LogicalCollection* collection = _collection;
  TRI_ASSERT(collection != nullptr);

  auto physical = static_cast<RocksDBCollection*>(collection->getPhysical());
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
  } else if (res == TRI_ERROR_LOCK_TIMEOUT && timeout >= 0.1) {
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
    return TRI_ERROR_NO_ERROR;
  }

  if (_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER)) {
    // never unlock
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(_collection != nullptr);

  if (CollectionLockState::_noLockHeaders != nullptr) {
    std::string collName(_collection->name());
    auto it = CollectionLockState::_noLockHeaders->find(collName);
    if (it != CollectionLockState::_noLockHeaders->end()) {
      // do not lock by command
      return TRI_ERROR_NO_ERROR;
    }
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
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "logic error in doUnlock";
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }

  LogicalCollection* collection = _collection;
  TRI_ASSERT(collection != nullptr);

  auto physical = static_cast<RocksDBCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);

  LOG_TRX(_transaction, nestingLevel) << "write-unlocking collection " << _cid;
  if (!AccessMode::isExclusive(type)) {
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
