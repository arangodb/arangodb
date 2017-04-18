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
#include "RocksDBEngine/RocksDBCommon.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"

#include <rocksdb/options.h>
#include <rocksdb/status.h>

struct TRI_vocbase_t;

namespace rocksdb {
class Transaction;
class Slice;
class Iterator;
}

namespace arangodb {
namespace cache {
struct Transaction;
}
class LogicalCollection;
struct RocksDBDocumentOperation;
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
  explicit RocksDBTransactionState(TRI_vocbase_t* vocbase,
                                   uint64_t maxOperationSize,
                                   bool intermediateTransactionEnabled,
                                   uint64_t intermediateTransactionSize,
                                   uint64_t intermediateTransactionNumber);
  ~RocksDBTransactionState();

  /// @brief begin a transaction
  Result beginTransaction(transaction::Hints hints) override;

  /// @brief commit a transaction
  Result commitTransaction(transaction::Methods* trx) override;

  /// @brief abort a transaction
  Result abortTransaction(transaction::Methods* trx) override;

  uint64_t numInserts() const { return _numInserts; }
  uint64_t numUpdates() const { return _numUpdates; }
  uint64_t numRemoves() const { return _numRemoves; }

  inline bool hasOperations() const {
    return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
  }

  bool hasFailedOperations() const override {
    return (_status == transaction::Status::ABORTED) && hasOperations();
  }

  /// @brief add an operation for a transaction collection
  RocksDBOperationResult addOperation(
      TRI_voc_cid_t collectionId, TRI_voc_rid_t revisionId,
      TRI_voc_document_operation_e operationType, uint64_t operationSize,
      uint64_t keySize);

  rocksdb::Transaction* rocksTransaction() {
    TRI_ASSERT(_rocksTransaction != nullptr);
    return _rocksTransaction.get();
  }

  rocksdb::ReadOptions const& readOptions() { return _rocksReadOptions; }

  uint64_t sequenceNumber() const;

  void reset();

 private:
  std::unique_ptr<rocksdb::Transaction> _rocksTransaction;
  rocksdb::WriteOptions _rocksWriteOptions;
  rocksdb::ReadOptions _rocksReadOptions;
  cache::Transaction* _cacheTx;
  // current transaction size
  uint64_t _transactionSize;
  // a transaction may not become bigger than this value
  uint64_t _maxTransactionSize;
  // if a transaction gets bigger than  this value and intermediate transactions
  // are enabled then a commit will be done
  uint64_t _intermediateTransactionSize;
  uint64_t _intermediateTransactionNumber;
  uint64_t _numInserts;
  uint64_t _numUpdates;
  uint64_t _numRemoves;
  bool _intermediateTransactionEnabled;
};
}

#endif
