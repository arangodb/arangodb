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

#ifndef ARANGOD_ROCKSDB_ROCKSDB_TRANSACTION_STATE_H
#define ARANGOD_ROCKSDB_ROCKSDB_TRANSACTION_STATE_H 1

#include "Basics/Common.h"
#include "Basics/SmallVector.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"

#include <rocksdb/status.h>
#include <rocksdb/options.h>

struct TRI_vocbase_t;
 
namespace rocksdb {
class Transaction;
class Slice;
class Iterator;
}

namespace arangodb {
class LogicalCollection;
struct RocksDBDocumentOperation;
class RocksDBWalMarker;
namespace transaction {
class Methods;
}
class TransactionCollection;
  
class RocksDBSavePoint {
public:
  explicit RocksDBSavePoint(rocksdb::Transaction* trx);
  ~RocksDBSavePoint();

  void commit();
  void rollback();

 private:
  rocksdb::Transaction* _trx;
  bool _committed;
};

/// @brief transaction type
class RocksDBTransactionState final : public TransactionState {
 public:
  explicit RocksDBTransactionState(TRI_vocbase_t* vocbase);
  ~RocksDBTransactionState();

  /// @brief begin a transaction
  int beginTransaction(transaction::Hints hints) override;

  /// @brief commit a transaction
  int commitTransaction(transaction::Methods* trx) override;

  /// @brief abort a transaction
  int abortTransaction(transaction::Methods* trx) override;

  bool hasFailedOperations() const override {
    return (_hasOperations && _status == transaction::Status::ABORTED);
  }

  /// @brief add a WAL operation for a transaction collection
  int addOperation(TRI_voc_rid_t, RocksDBDocumentOperation&, RocksDBWalMarker const* marker, bool&);
  
  rocksdb::Transaction* rocksTransaction() {
    TRI_ASSERT(_rocksTransaction != nullptr);
    return _rocksTransaction.get();
  }
  
  rocksdb::ReadOptions const& readOptions(){
    return _rocksReadOptions;
  }
  
 private:
  std::unique_ptr<rocksdb::Transaction> _rocksTransaction;
  rocksdb::WriteOptions _rocksWriteOptions;
  rocksdb::ReadOptions _rocksReadOptions;
  bool _hasOperations;
  
};
}

#endif
