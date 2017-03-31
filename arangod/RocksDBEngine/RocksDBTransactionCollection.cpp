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
#include "Transaction/Methods.h"
#include "Transaction/Hints.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/modes.h"

using namespace arangodb;

RocksDBTransactionCollection::RocksDBTransactionCollection(TransactionState* trx, 
                                                           TRI_voc_cid_t cid, 
                                                           AccessMode::Type accessType) 
    : TransactionCollection(trx, cid),
      _accessType(accessType), 
      _initialNumberDocuments(0),
      _initialRevision(0),
      _operationSize(0),
      _numInserts(0),
      _numUpdates(0),
      _numRemoves(0) {}

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
  return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
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

int RocksDBTransactionCollection::use(int nestingLevel) {
  if (_collection == nullptr) {
    TRI_vocbase_col_status_e status;
    LOG_TRX(_transaction, nestingLevel) << "using collection " << _cid;
    _collection = _transaction->vocbase()->useCollection(_cid, status);
      
    if (_collection == nullptr) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }
    
    if (AccessMode::isWriteOrExclusive(_accessType) &&
        TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE &&
        !LogicalCollection::IsSystemName(_collection->name())) {
      return TRI_ERROR_ARANGO_READ_ONLY;
    }

    _initialNumberDocuments = static_cast<RocksDBCollection*>(_collection->getPhysical())->numberDocuments();
  }

  return TRI_ERROR_NO_ERROR;
}

void RocksDBTransactionCollection::unuse(int /*nestingLevel*/) {}

void RocksDBTransactionCollection::release() {
  // the top level transaction releases all collections
  if (_collection != nullptr) {
    // unuse collection, remove usage-lock
    LOG_TRX(_transaction, 0) << "unusing collection " << _cid;

    _transaction->vocbase()->releaseCollection(_collection);
    _collection = nullptr;
  }
}
  
/// @brief add an operation for a transaction collection
void RocksDBTransactionCollection::addOperation(TRI_voc_document_operation_e operationType,
                                                uint64_t operationSize) {
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

  _operationSize += operationSize;
}
