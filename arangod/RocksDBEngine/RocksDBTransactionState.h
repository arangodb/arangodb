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

#include <rocksdb/options.h>
#include <rocksdb/status.h>

#include "Basics/Common.h"
#include "Containers/SmallVector.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace rocksdb {

class Transaction;
class Slice;
class Iterator;

}  // namespace rocksdb

namespace arangodb {

namespace cache {
struct Transaction;
}

class LogicalCollection;
struct RocksDBDocumentOperation;
class RocksDBMethods;

/// @brief transaction type
class RocksDBTransactionState final : public TransactionState {
  friend class RocksDBMethods;
  friend class RocksDBReadOnlyMethods;
  friend class RocksDBTrxMethods;
  friend class RocksDBBatchedMethods;
  friend class RocksDBBatchedWithIndexMethods;

 public:
  RocksDBTransactionState(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
                          transaction::Options const& options);
  ~RocksDBTransactionState();

  /// @brief begin a transaction
  Result beginTransaction(transaction::Hints hints) override;

  /// @brief commit a transaction
  Result commitTransaction(transaction::Methods* trx) override;

  /// @brief abort a transaction
  Result abortTransaction(transaction::Methods* trx) override;

  /// @brief number of commits, including intermediate commits
  uint64_t numCommits() const override { return _numCommits; }

  /// @brief number of insert operations
  uint64_t numInserts() const { return _numInserts; }
  /// @brief number of update/replace operations
  uint64_t numUpdates() const { return _numUpdates; }
  /// @brief number of remove operations
  uint64_t numRemoves() const { return _numRemoves; }

  inline bool hasOperations() const {
    return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
  }

  bool hasFailedOperations() const override {
    return (_status == transaction::Status::ABORTED) && hasOperations();
  }

  void prepareOperation(TRI_voc_cid_t cid, TRI_voc_rid_t rid,
                        TRI_voc_document_operation_e operationType);

  /// @brief undo the effects of the previous prepareOperation call
  void rollbackOperation(TRI_voc_document_operation_e operationType);

  /// @brief add an operation for a transaction collection
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed
  Result addOperation(TRI_voc_cid_t collectionId, TRI_voc_rid_t revisionId,
                      TRI_voc_document_operation_e opType,
                      bool& hasPerformedIntermediateCommit);

  /// @brief return wrapper around rocksdb transaction
  RocksDBMethods* rocksdbMethods() {
    TRI_ASSERT(_rocksMethods);
    return _rocksMethods.get();
  }
  
  /// @brief Rocksdb sequence number of snapshot. Works while trx
  ///        has either a snapshot or a transaction
  rocksdb::SequenceNumber sequenceNumber() const;
  
  /// @brief acquire a database snapshot
  bool setSnapshotOnReadOnly();

  static RocksDBTransactionState* toState(transaction::Methods* trx) {
    TRI_ASSERT(trx != nullptr);
    TransactionState* state = trx->state();
    TRI_ASSERT(state != nullptr);
    return static_cast<RocksDBTransactionState*>(state);
  }

  static RocksDBMethods* toMethods(transaction::Methods* trx) {
    TRI_ASSERT(trx != nullptr);
    TransactionState* state = trx->state();
    TRI_ASSERT(state != nullptr);
    return static_cast<RocksDBTransactionState*>(state)->rocksdbMethods();
  }

  /// @brief make some internal preparations for accessing this state in
  /// parallel from multiple threads. READ-ONLY transactions
  void prepareForParallelReads() { _parallel = true; }
  /// @brief in parallel mode. READ-ONLY transactions
  bool inParallelMode() const { return _parallel; }

  RocksDBTransactionCollection::TrackedOperations& trackedOperations(TRI_voc_cid_t cid);

  /// @brief Track documents inserted to the collection
  ///        Used to update the revision tree for replication after commit
  void trackInsert(TRI_voc_cid_t cid, TRI_voc_rid_t rid);

  /// @brief Track documents removed from the collection
  ///        Used to update the revision tree for replication after commit
  void trackRemove(TRI_voc_cid_t cid, TRI_voc_rid_t rid);

  /// @brief Every index can track hashes inserted into this index
  ///        Used to update the estimate after the trx committed
  void trackIndexInsert(TRI_voc_cid_t cid, IndexId idxObjectId, uint64_t hash);

  /// @brief Every index can track hashes removed from this index
  ///        Used to update the estimate after the trx committed
  void trackIndexRemove(TRI_voc_cid_t cid, IndexId idxObjectId, uint64_t hash);

  bool isOnlyExclusiveTransaction() const;

  rocksdb::SequenceNumber beginSeq() const;

 private:
  /// @brief create a new rocksdb transaction
  void createTransaction();

  void prepareCollections();
  void commitCollections(rocksdb::SequenceNumber lastWritten);
  void cleanupCollections();

  /// @brief delete transaction, snapshot and cache trx
  void cleanupTransaction() noexcept;
  /// @brief internally commit a transaction
  arangodb::Result internalCommit();

  /// @brief Trigger an intermediate commit.
  /// Handle with care if failing after this commit it will only
  /// be rolled back until this point of time.
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed Not thread safe
  Result triggerIntermediateCommit(bool& hasPerformedIntermediateCommit);

  /// @brief check sizes and call internalCommit if too big
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed
  Result checkIntermediateCommit(uint64_t newSize, bool& hasPerformedIntermediateCommit);

  /// @brief rocksdb transaction may be null for read only transactions
  rocksdb::Transaction* _rocksTransaction;
  /// @brief used for read-only trx and intermediate commits
  /// For intermediate commits this MUST ONLY be used for iteratos
  rocksdb::Snapshot const* _readSnapshot;
  /// @brief shared read options which can be used by operations
  /// For intermediate commits iterators MUST use the _readSnapshot
  rocksdb::ReadOptions _rocksReadOptions;

  /// @brief cache transaction to unblock blacklisted keys
  cache::Transaction* _cacheTx;
  /// @brief wrapper to use outside this class to access rocksdb
  std::unique_ptr<RocksDBMethods> _rocksMethods;

  bool _blockers = false;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  /// store the number of log entries in WAL
  uint64_t _numLogdata = 0;
#endif
  /// @brief number of commits, including intermediate commits
  uint64_t _numCommits;
  // if a transaction gets bigger than these values then an automatic
  // intermediate commit will be done
  uint64_t _numInserts;
  uint64_t _numUpdates;
  uint64_t _numRemoves;

  /// @brief if true there key buffers will no longer be shared
  bool _parallel;
};

class RocksDBKeyLeaser {
 public:
  explicit RocksDBKeyLeaser(transaction::Methods*);
  ~RocksDBKeyLeaser();
  inline RocksDBKey* builder() { return &_key; }
  inline RocksDBKey* operator->() { return &_key; }
  inline RocksDBKey* get() { return &_key; }
  inline RocksDBKey& ref() { return _key; }

 private:
  transaction::Context* _ctx;
  RocksDBKey _key;
};

}  // namespace arangodb

#endif
