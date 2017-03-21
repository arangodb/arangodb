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
#include "Logger/Logger.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/modes.h"
#include "VocBase/ticks.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>

using namespace arangodb;

/// @brief transaction type
RocksDBTransactionState::RocksDBTransactionState(TRI_vocbase_t* vocbase)
    : TransactionState(vocbase),
      _rocksTransaction(nullptr),
      _beginWritten(false),
      _hasOperations(false) {}

/// @brief free a transaction container
RocksDBTransactionState::~RocksDBTransactionState() {
}

/// @brief get (or create) a rocksdb WriteTransaction
rocksdb::Transaction* RocksDBTransactionState::rocksTransaction() {
  throw std::runtime_error("not implemented");
  return nullptr;
}

/// @brief start a transaction
int RocksDBTransactionState::beginTransaction(transaction::Hints hints) {
  throw std::runtime_error("not implemented");
  return 0;
}

/// @brief commit a transaction
int RocksDBTransactionState::commitTransaction(transaction::Methods* activeTrx) {
  throw std::runtime_error("not implemented");
  return 0;
}

/// @brief abort and rollback a transaction
int RocksDBTransactionState::abortTransaction(transaction::Methods* activeTrx) {
  throw std::runtime_error("not implemented");
  return 0;
}

/// @brief add a WAL operation for a transaction collection
int RocksDBTransactionState::addOperation(TRI_voc_rid_t revisionId,
                                   RocksDBDocumentOperation& operation,
                                   RocksDBWalMarker const* marker,
                                   bool& waitForSync) {
  throw std::runtime_error("not implemented");
  return 0;
}

/// @brief free all operations for a transaction
void RocksDBTransactionState::freeOperations(transaction::Methods* activeTrx) {
  throw std::runtime_error("not implemented");
}

/// @brief write WAL begin marker
int RocksDBTransactionState::writeBeginMarker() {
  throw std::runtime_error("not implemented");
  return 0;
}

/// @brief write WAL abort marker
int RocksDBTransactionState::writeAbortMarker() {
  throw std::runtime_error("not implemented");
  return 0;
}
