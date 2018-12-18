////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterTransactionCollection.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/Logger.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

ClusterTransactionCollection::ClusterTransactionCollection(
    TransactionState* trx, TRI_voc_cid_t cid, AccessMode::Type accessType,
    int nestingLevel)
    : TransactionCollection(trx, cid, accessType),
      _lockType(AccessMode::Type::NONE),
      _nestingLevel(nestingLevel),
      _usageLocked(false) {}

ClusterTransactionCollection::~ClusterTransactionCollection() {}

/// @brief whether or not any write operations for the collection happened
bool ClusterTransactionCollection::hasOperations() const {
  return false;//(_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
}

void ClusterTransactionCollection::freeOperations(
    transaction::Methods* /*activeTrx*/, bool /*mustRollback*/) {}

bool ClusterTransactionCollection::canAccess(
    AccessMode::Type accessType) const {
  // check if access type matches
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // type doesn't match. probably also a mistake by the caller
    return false;
  }

  return true;
}

int ClusterTransactionCollection::updateUsage(AccessMode::Type accessType,
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

int ClusterTransactionCollection::use(int nestingLevel) {
  if (_nestingLevel != nestingLevel) {
    // only process our own collections
    return TRI_ERROR_NO_ERROR;
  }

  if (_collection == nullptr) {
    // open the collection
    ClusterInfo* ci = ClusterInfo::instance();
    if (ci == nullptr) {
      return TRI_ERROR_SHUTTING_DOWN;
    }
    
    _collection = ci->getCollectionNT(_transaction->vocbase().name(), std::to_string(_cid));
    if (_collection == nullptr) {
      return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
    }

    if (!_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER) &&
        !_transaction->hasHint(transaction::Hints::Hint::NO_USAGE_LOCK)) {
      // use and usage-lock
      LOG_TRX(_transaction, nestingLevel) << "using collection " << _cid;
      _usageLocked = true;
    }
  }

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
  
  return TRI_ERROR_NO_ERROR;
}

void ClusterTransactionCollection::unuse(int nestingLevel) {
  // nothing to do here. we're postponing the unlocking until release()
}

void ClusterTransactionCollection::release() {
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
  }
}

/// @brief lock a collection
/// returns TRI_ERROR_LOCKED in case the lock was successfully acquired
/// returns TRI_ERROR_NO_ERROR in case the lock does not need to be acquired and no other error occurred
/// returns any other error code otherwise
int ClusterTransactionCollection::doLock(AccessMode::Type type,
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

  TRI_ASSERT(_collection);
  LOG_TRX(_transaction, nestingLevel) << "write-locking collection " << _cid;

  _lockType = type;
  // not an error, but we use TRI_ERROR_LOCKED to indicate that we actually acquired the lock ourselves
  return TRI_ERROR_LOCKED;
}

/// @brief unlock a collection
int ClusterTransactionCollection::doUnlock(AccessMode::Type type,
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
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "logic error in doUnlock";
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }

  TRI_ASSERT(_collection);

  _lockType = AccessMode::Type::NONE;

  return TRI_ERROR_NO_ERROR;
}
