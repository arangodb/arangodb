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

RocksDBTransactionCollection::RocksDBTransactionCollection(TransactionState* trx, TRI_voc_cid_t cid, AccessMode::Type accessType, int nestingLevel)
    : TransactionCollection(trx, cid) {
}

RocksDBTransactionCollection::~RocksDBTransactionCollection() {
}

/// @brief request a main-level lock for a collection
int RocksDBTransactionCollection::lock() {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

/// @brief request a lock for a collection
int RocksDBTransactionCollection::lock(AccessMode::Type accessType,
                                       int nestingLevel) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

/// @brief request an unlock for a collection
int RocksDBTransactionCollection::unlock(AccessMode::Type accessType,
                                         int nestingLevel) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

/// @brief check if a collection is locked in a specific mode in a transaction
bool RocksDBTransactionCollection::isLocked(AccessMode::Type accessType, int nestingLevel) const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

/// @brief check whether a collection is locked at all
bool RocksDBTransactionCollection::isLocked() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

/// @brief whether or not any write operations for the collection happened
bool RocksDBTransactionCollection::hasOperations() const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

void RocksDBTransactionCollection::freeOperations(transaction::Methods* activeTrx, bool mustRollback) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

bool RocksDBTransactionCollection::canAccess(AccessMode::Type accessType) const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

int RocksDBTransactionCollection::updateUsage(AccessMode::Type accessType, int nestingLevel) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

int RocksDBTransactionCollection::use(int nestingLevel) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}

void RocksDBTransactionCollection::unuse(int nestingLevel) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

void RocksDBTransactionCollection::release() {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

