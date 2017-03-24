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

#include "RocksDBTransactionCollection.h"
#include "Basics/Exceptions.h"
#include "Cluster/CollectionLockState.h"
#include "Logger/Logger.h"
#include "RocksDBCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "Transaction/Hints.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/modes.h"

using namespace arangodb;

RocksDBTransactionCollection::RocksDBTransactionCollection(TransactionState* trx, 
                                                           TRI_voc_cid_t cid, 
                                                           AccessMode::Type accessType, 
                                                           int nestingLevel)
    : TransactionCollection(trx, cid),
      _firstTime(true),
      _waitForSync(false),
      _accessType(accessType), 
      _numOperations(0) {} 

RocksDBTransactionCollection::~RocksDBTransactionCollection() {}

/// @brief request a main-level lock for a collection
int RocksDBTransactionCollection::lock() { return TRI_ERROR_NO_ERROR; }

/// @brief request a lock for a collection
int RocksDBTransactionCollection::lock(AccessMode::Type accessType, int /*nestingLevel*/) {
  if (isWrite(accessType) && !isWrite(_accessType)) {
    // wrong lock type
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief request an unlock for a collection
int RocksDBTransactionCollection::unlock(AccessMode::Type accessType, int /*nestingLevel*/) {
  if (isWrite(accessType) && !isWrite(_accessType)) {
    // wrong lock type: write-unlock requested but collection is read-only
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief check if a collection is locked in a specific mode in a transaction
bool RocksDBTransactionCollection::isLocked(AccessMode::Type accessType, int /*nestingLevel*/) const {
  if (isWrite(accessType) && !isWrite(_accessType)) {
    // wrong lock type
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "logic error. checking wrong lock type";
    return false;
  }

  // TODO
  return false;
}

/// @brief check whether a collection is locked at all
bool RocksDBTransactionCollection::isLocked() const {
  // TODO
  return false;
}

/// @brief whether or not any write operations for the collection happened
bool RocksDBTransactionCollection::hasOperations() const {
  return (_numOperations > 0);
}

void RocksDBTransactionCollection::freeOperations(transaction::Methods* /*activeTrx*/, bool /*mustRollback*/) {}

bool RocksDBTransactionCollection::canAccess(AccessMode::Type accessType) const {
  // check if access type matches
  if (AccessMode::isWriteOrExclusive(accessType) && 
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // type doesn't match. probably also a mistake by the caller
    return false;
  }

  return true;
}

int RocksDBTransactionCollection::updateUsage(AccessMode::Type accessType, int nestingLevel) {
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

  // all correct
  return TRI_ERROR_NO_ERROR;
}

int RocksDBTransactionCollection::use(int /*nestingLevel*/) {
  if (_firstTime) {
    if (AccessMode::isWriteOrExclusive(_accessType) &&
        TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE &&
        !LogicalCollection::IsSystemName(_collection->name())) {
      return TRI_ERROR_ARANGO_READ_ONLY;
    }

    // store the waitForSync property
    _waitForSync = _collection->waitForSync();
  
    _firstTime = false;
  }

  return TRI_ERROR_NO_ERROR;
}

void RocksDBTransactionCollection::unuse(int /*nestingLevel*/) {}

// nothing to do here
void RocksDBTransactionCollection::release() {}
