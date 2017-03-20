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
//#include "RocksDB/RocksDBDatafileHelper.h"
//#include "RocksDB/RocksDBDocumentOperation.h"
//#include "RocksDB/RocksDBLogfileManager.h"
//#include "RocksDB/RocksDBPersistentIndexFeature.h"
//#include "RocksDB/RocksDBTransactionCollection.h"
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
}

/// @brief start a transaction
int RocksDBTransactionState::beginTransaction(transaction::Hints hints) {
}

/// @brief commit a transaction
int RocksDBTransactionState::commitTransaction(transaction::Methods* activeTrx) {
}

/// @brief abort and rollback a transaction
int RocksDBTransactionState::abortTransaction(transaction::Methods* activeTrx) {
}

/// @brief add a WAL operation for a transaction collection
int RocksDBTransactionState::addOperation(TRI_voc_rid_t revisionId,
                                   RocksDBDocumentOperation& operation,
                                   RocksDBWalMarker const* marker,
                                   bool& waitForSync) {
}

/// @brief free all operations for a transaction
void RocksDBTransactionState::freeOperations(transaction::Methods* activeTrx) {
}

/// @brief write WAL begin marker
int RocksDBTransactionState::writeBeginMarker() {
}

/// @brief write WAL abort marker
int RocksDBTransactionState::writeAbortMarker() {
}
