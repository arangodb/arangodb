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

#include "TransactionCollection.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionHints.h"
#include "Utils/TransactionState.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

static bool IsWrite(AccessMode::Type type) {
  return (type == AccessMode::Type::WRITE || type == AccessMode::Type::EXCLUSIVE);
}

/// @brief whether or not a specific hint is set for the transaction
static inline bool HasHint(TransactionState const* trx,
                           TransactionHints::Hint hint) {
  return trx->_hints.has(hint);
}

/// @brief whether or not a transaction consists of a single operation
static inline bool IsSingleOperationTransaction(TransactionState const* trx) {
  return HasHint(trx, TransactionHints::Hint::SINGLE_OPERATION);
}

/// @brief request a lock for a collection
int TransactionCollection::lock(AccessMode::Type accessType,
                                int nestingLevel) {
  if (IsWrite(accessType) && !IsWrite(_accessType)) {
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
int TransactionCollection::unlock(AccessMode::Type accessType,
                                  int nestingLevel) {
  if (IsWrite(accessType) && !IsWrite(_accessType)) {
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
bool TransactionCollection::isLocked(AccessMode::Type accessType, int nestingLevel) const {
  if (IsWrite(accessType) && !IsWrite(_accessType)) {
    // wrong lock type
    LOG(WARN) << "logic error. checking wrong lock type";
    return false;
  }

  return isLocked();
}
  
/// @brief check whether a collection is locked at all
bool TransactionCollection::isLocked() const {
  return (_lockType != AccessMode::Type::NONE);
}

/// @brief lock a collection
int TransactionCollection::doLock(AccessMode::Type type, int nestingLevel) {
  TransactionState* trx = _transaction;

  if (HasHint(trx, TransactionHints::Hint::LOCK_NEVER)) {
    // never lock
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(_collection != nullptr);

  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(_collection->name());
    auto it = arangodb::Transaction::_makeNolockHeaders->find(collName);
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "LockCollection blocked: " << collName << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }

  TRI_ASSERT(!isLocked());

  LogicalCollection* collection = _collection;
  TRI_ASSERT(collection != nullptr);
  double timeout = trx->_timeout;
  if (HasHint(_transaction, TransactionHints::Hint::TRY_LOCK)) {
    // give up early if we cannot acquire the lock instantly
    timeout = 0.00000001;
  }
  
  bool const useDeadlockDetector = !IsSingleOperationTransaction(trx);

  int res;
  if (!IsWrite(type)) {
    LOG_TRX(trx, nestingLevel) << "read-locking collection " << _cid;
    res = collection->beginReadTimed(useDeadlockDetector, timeout);
  } else { // WRITE or EXCLUSIVE
    LOG_TRX(trx, nestingLevel) << "write-locking collection " << _cid;
    res = collection->beginWriteTimed(useDeadlockDetector, timeout);
  }

  if (res == TRI_ERROR_NO_ERROR) {
    _lockType = type;
  }

  return res;
}

/// @brief unlock a collection
int TransactionCollection::doUnlock(AccessMode::Type type, int nestingLevel) {
  if (HasHint(_transaction, TransactionHints::Hint::LOCK_NEVER)) {
    // never unlock
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(_collection != nullptr);

  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(_collection->name());
    auto it = arangodb::Transaction::_makeNolockHeaders->find(collName);
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "UnlockCollection blocked: " << collName << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }

  TRI_ASSERT(isLocked());

  if (_nestingLevel < nestingLevel) {
    // only process our own collections
    return TRI_ERROR_NO_ERROR;
  }

  if (!IsWrite(type) && IsWrite(_lockType)) {
    // do not remove a write-lock if a read-unlock was requested!
    return TRI_ERROR_NO_ERROR;
  } else if (IsWrite(type) && !IsWrite(_lockType)) {
    // we should never try to write-unlock a collection that we have only
    // read-locked
    LOG(ERR) << "logic error in UnlockCollection";
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }

  TransactionState* trx = _transaction;
  bool const useDeadlockDetector = !IsSingleOperationTransaction(trx);

  LogicalCollection* collection = _collection;
  TRI_ASSERT(collection != nullptr);
  if (!IsWrite(_lockType)) {
    LOG_TRX(_transaction, nestingLevel) << "read-unlocking collection " << _cid;
    collection->endRead(useDeadlockDetector);
  } else { // WRITE or EXCLUSIVE
    LOG_TRX(_transaction, nestingLevel) << "write-unlocking collection " << _cid;
    collection->endWrite(useDeadlockDetector);
  }

  _lockType = AccessMode::Type::NONE;

  return TRI_ERROR_NO_ERROR;
}

