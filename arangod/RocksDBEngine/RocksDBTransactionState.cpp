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
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "RestServer/TransactionManagerFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/TransactionManager.h"
#include "VocBase/modes.h"
#include "VocBase/ticks.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>

using namespace arangodb;

struct RocksDBTransactionData final : public TransactionData {
};

/// @brief transaction type
RocksDBTransactionState::RocksDBTransactionState(TRI_vocbase_t* vocbase)
    : TransactionState(vocbase),
      _beginWritten(false),
      _hasOperations(false) {
  LOG_TOPIC(ERR, Logger::FIXME) << "ctor rocksdb transaction state: " << this;
}

/// @brief free a transaction container
RocksDBTransactionState::~RocksDBTransactionState() {
  LOG_TOPIC(ERR, Logger::FIXME) << "dtor rocksdb transaction state: " << this;
}

/// @brief start a transaction
int RocksDBTransactionState::beginTransaction(transaction::Hints hints) {
  LOG_TRX(this, _nestingLevel) << "beginning " << AccessMode::typeString(_type) << " transaction";

  if (_nestingLevel == 0) {
    TRI_ASSERT(_status == transaction::Status::CREATED);
    
    // get a new id
    _id = TRI_NewTickServer();

    // register a protector
    auto data = std::make_unique<RocksDBTransactionData>(); // intentionally empty
    TransactionManagerFeature::MANAGER->registerTransaction(_id, std::move(data));

    TRI_ASSERT(_rocksTransaction == nullptr);

    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    rocksdb::TransactionDB* db = static_cast<RocksDBEngine*>(engine)->db();
    _rocksTransaction.reset(db->BeginTransaction(rocksdb::WriteOptions(), rocksdb::TransactionOptions()));
  } else {
    TRI_ASSERT(_status == transaction::Status::RUNNING);
  }
  
  int res = useCollections(_nestingLevel);

  LOG_TOPIC(ERR, Logger::FIXME) << "USE COLLECTIONS RETURNED: " << res << ", NESTING: " << _nestingLevel;

  if (res == TRI_ERROR_NO_ERROR) {
    // all valid
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::RUNNING);
    }
  } else {
    // something is wrong
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::ABORTED);
    }

    // free what we have got so far
    unuseCollections(_nestingLevel);
  }
    
  return res;
}

/// @brief commit a transaction
int RocksDBTransactionState::commitTransaction(transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel) << "committing " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(_status == transaction::Status::RUNNING);

  int res = TRI_ERROR_NO_ERROR;

  if (_nestingLevel == 0) {
    if (_rocksTransaction != nullptr) {
      auto status = _rocksTransaction->Commit();

      if (!status.ok()) {
        // TODO: translate status
        res = TRI_ERROR_INTERNAL;
        abortTransaction(activeTrx);
        return res;
      }
      _rocksTransaction.reset();
    }

    updateStatus(transaction::Status::COMMITTED);

    // if a write query, clear the query cache for the participating collections
    if (AccessMode::isWriteOrExclusive(_type) &&
        !_collections.empty() &&
        arangodb::aql::QueryCache::instance()->mayBeActive()) {
      clearQueryCache();
    }
  }

  unuseCollections(_nestingLevel);

  return res;
}

/// @brief abort and rollback a transaction
int RocksDBTransactionState::abortTransaction(transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel) << "aborting " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(_status == transaction::Status::RUNNING);

  int res = TRI_ERROR_NO_ERROR;

  if (_nestingLevel == 0) {
    if (_rocksTransaction != nullptr) {
      _rocksTransaction->Rollback();
      _rocksTransaction.reset();
    }

    updateStatus(transaction::Status::ABORTED);

    if (_hasOperations) {
      // must clean up the query cache because the transaction
      // may have queried something via AQL that is now rolled back
      clearQueryCache();
    }
  }

  unuseCollections(_nestingLevel);

  return res;
}

/// @brief add a WAL operation for a transaction collection
int RocksDBTransactionState::addOperation(TRI_voc_rid_t revisionId,
                                   RocksDBDocumentOperation& operation,
                                   RocksDBWalMarker const* marker,
                                   bool& waitForSync) {
  _hasOperations = true;
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return 0;
}
