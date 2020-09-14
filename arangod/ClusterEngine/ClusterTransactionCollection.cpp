////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterTransactionCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

ClusterTransactionCollection::ClusterTransactionCollection(TransactionState* trx,
                                                           DataSourceId cid,
                                                           AccessMode::Type accessType)
    : TransactionCollection(trx, cid, accessType) {}

ClusterTransactionCollection::~ClusterTransactionCollection() = default;

/// @brief whether or not any write operations for the collection happened
bool ClusterTransactionCollection::hasOperations() const {
  return false;  
}

bool ClusterTransactionCollection::canAccess(AccessMode::Type accessType) const {
  // check if access type matches
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // type doesn't match. probably also a mistake by the caller
    return false;
  }

  return true;
}

// simon: actually probably never called on coordinator
Result ClusterTransactionCollection::lockUsage() {
  if (_collection == nullptr) {
    // open the collection
    if (_transaction->vocbase().server().isStopping()) {
      return {TRI_ERROR_SHUTTING_DOWN};
    }
    ClusterInfo& ci =
        _transaction->vocbase().server().getFeature<ClusterFeature>().clusterInfo();

    _collection = ci.getCollectionNT(_transaction->vocbase().name(),
                                     std::to_string(_cid.id()));
    if (_collection == nullptr) {
      return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
    }

    if (!_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER) &&
        !_transaction->hasHint(transaction::Hints::Hint::NO_USAGE_LOCK)) {
      // use and usage-lock
      LOG_TRX("8154f", TRACE, _transaction) << "using collection " << _cid;
    }
  }

  if (AccessMode::isWriteOrExclusive(_accessType) && !isLocked()) {
    // r/w lock the collection
    Result res = doLock(_accessType);

    // TRI_ERROR_LOCKED is not an error, but it indicates that the lock
    // operation has actually acquired the lock (and that the lock has not
    // been held before)
    if (res.fail() && res.isNot(TRI_ERROR_LOCKED)) {
      return res;
    }
  }

  return {};
}

void ClusterTransactionCollection::releaseUsage() {
  if (isLocked()) {
    // unlock our own r/w locks
    doUnlock(_accessType);
    _lockType = AccessMode::Type::NONE;
  }

  // the top level transaction releases all collections
  if (_collection != nullptr) {
    // unuse collection, remove usage-lock
    LOG_TRX("1cb8d", TRACE, _transaction) << "unusing collection " << _cid;
    _collection = nullptr;
  }
}

/// @brief lock a collection
/// returns TRI_ERROR_LOCKED in case the lock was successfully acquired
/// returns TRI_ERROR_NO_ERROR in case the lock does not need to be acquired and
/// no other error occurred returns any other error code otherwise
Result ClusterTransactionCollection::doLock(AccessMode::Type type) {
  if (!AccessMode::isWriteOrExclusive(type)) {
    _lockType = type;
    return {};
  }

  if (_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER)) {
    // never lock
    return {};
  }

  TRI_ASSERT(_collection != nullptr);

  TRI_ASSERT(!isLocked());

  TRI_ASSERT(_collection);
  LOG_TRX("b4a05", TRACE, _transaction) << "write-locking collection " << _cid;

  _lockType = type;
  // not an error, but we use TRI_ERROR_LOCKED to indicate that we actually
  // acquired the lock ourselves
  return {TRI_ERROR_LOCKED};
}

/// @brief unlock a collection
Result ClusterTransactionCollection::doUnlock(AccessMode::Type type) {
  if (!AccessMode::isWriteOrExclusive(type) || !AccessMode::isWriteOrExclusive(_lockType)) {
    _lockType = AccessMode::Type::NONE;
    return {};
  }

  if (_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER)) {
    // never unlock
    return {};
  }

  TRI_ASSERT(_collection != nullptr);

  TRI_ASSERT(isLocked());

  if (!AccessMode::isWriteOrExclusive(type) && AccessMode::isWriteOrExclusive(_lockType)) {
    // do not remove a write-lock if a read-unlock was requested!
    return {};
  }
  if (AccessMode::isWriteOrExclusive(type) && !AccessMode::isWriteOrExclusive(_lockType)) {
    // we should never try to write-unlock a collection that we have only
    // read-locked
    LOG_TOPIC("e8aab", ERR, arangodb::Logger::FIXME) << "logic error in doUnlock";
    TRI_ASSERT(false);
    return {TRI_ERROR_INTERNAL, "logic error in doUnlock"};
  }

  TRI_ASSERT(_collection);

  _lockType = AccessMode::Type::NONE;

  return {};
}
