////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <cstddef>
#include <limits>

#include "Basics/Common.h"
#include "Containers/SmallVector.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#include <atomic>
#endif

struct TRI_vocbase_t;

namespace rocksdb {
class Iterator;
}  // namespace rocksdb

namespace arangodb {

namespace cache {
struct Transaction;
}

class LogicalCollection;
class RocksDBTransactionMethods;

/// @brief transaction type
class RocksDBTransactionState final : public TransactionState {
  friend class RocksDBTrxBaseMethods;

 public:
  RocksDBTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                          transaction::Options const& options);
  ~RocksDBTransactionState();

  /// @brief begin a transaction
  Result beginTransaction(transaction::Hints hints) override;

  /// @brief commit a transaction
  Result commitTransaction(transaction::Methods* trx) override;

  /// @brief abort a transaction
  Result abortTransaction(transaction::Methods* trx) override;

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  TRI_voc_tick_t lastOperationTick() const noexcept override;
  
  /// @brief number of commits, including intermediate commits
  uint64_t numCommits() const override;
  
  bool hasOperations() const noexcept;

  uint64_t numOperations() const noexcept;

  bool hasFailedOperations() const override {
    return (_status == transaction::Status::ABORTED) && hasOperations();
  }

  void beginQuery(bool isModificationQuery) override;
  void endQuery(bool isModificationQuery) noexcept override;

  bool iteratorMustCheckBounds(ReadOwnWrites readOwnWrites) const;

  void prepareOperation(DataSourceId cid, RevisionId rid,
                        TRI_voc_document_operation_e operationType);

  /// @brief undo the effects of the previous prepareOperation call
  void rollbackOperation(TRI_voc_document_operation_e operationType);

  /// @brief add an operation for a transaction collection
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed
  Result addOperation(DataSourceId collectionId, RevisionId revisionId,
                      TRI_voc_document_operation_e opType,
                      bool& hasPerformedIntermediateCommit);

  /// @brief return wrapper around rocksdb transaction
  RocksDBTransactionMethods* rocksdbMethods() {
    TRI_ASSERT(_rocksMethods);
    return _rocksMethods.get();
  }
  
  /// @brief acquire a database snapshot if we do not yet have one.
  /// Returns true if a snapshot was acquired, otherwise false (i.e., if we already had a snapshot)
  bool ensureSnapshot();
  
  static RocksDBTransactionState* toState(transaction::Methods* trx) {
    TRI_ASSERT(trx != nullptr);
    TransactionState* state = trx->state();
    TRI_ASSERT(state != nullptr);
    return static_cast<RocksDBTransactionState*>(state);
  }

  static RocksDBTransactionMethods* toMethods(transaction::Methods* trx) {
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

  RocksDBTransactionCollection::TrackedOperations& trackedOperations(DataSourceId cid);

  /// @brief Track documents inserted to the collection
  ///        Used to update the revision tree for replication after commit
  void trackInsert(DataSourceId cid, RevisionId rid);

  /// @brief Track documents removed from the collection
  ///        Used to update the revision tree for replication after commit
  void trackRemove(DataSourceId cid, RevisionId rid);

  /// @brief Every index can track hashes inserted into this index
  ///        Used to update the estimate after the trx committed
  void trackIndexInsert(DataSourceId cid, IndexId idxObjectId, uint64_t hash);

  /// @brief Every index can track hashes removed from this index
  ///        Used to update the estimate after the trx committed
  void trackIndexRemove(DataSourceId cid, IndexId idxObjectId, uint64_t hash);

  bool isOnlyExclusiveTransaction() const;

  rocksdb::SequenceNumber beginSeq() const;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  /// @brief only needed for RocksDBTransactionStateGuard
  void use() noexcept;
  void unuse() noexcept;
#endif

 private:
  rocksdb::SequenceNumber prepareCollections();
  void commitCollections(rocksdb::SequenceNumber lastWritten);
  void cleanupCollections();
  
  void maybeDisableIndexing();

  /// @brief delete transaction, snapshot and cache trx
  void cleanupTransaction() noexcept;

  /// @brief cache transaction to unblock banished keys
  cache::Transaction* _cacheTx;

  /// @brief wrapper to use outside this class to access rocksdb
  std::unique_ptr<RocksDBTransactionMethods> _rocksMethods;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::atomic<uint32_t> _users;
#endif

  /// @brief if true there key buffers will no longer be shared
  bool _parallel;
};

/// @brief a struct that makes sure that the same RocksDBTransactionState
/// is not used by different write operations in parallel. will only do
/// something in maintainer mode, and do nothing in release mode!
struct RocksDBTransactionStateGuard {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  explicit RocksDBTransactionStateGuard(RocksDBTransactionState* state) noexcept;
  ~RocksDBTransactionStateGuard();
  
  RocksDBTransactionState* _state;
#else
  explicit RocksDBTransactionStateGuard(RocksDBTransactionState* /*state*/) noexcept {}
  ~RocksDBTransactionStateGuard() = default;
#endif
};

class RocksDBKeyLeaser {
 public:
  explicit RocksDBKeyLeaser(transaction::Methods*);
  ~RocksDBKeyLeaser();
  inline RocksDBKey* builder() { return &_key; }
  inline RocksDBKey* operator->() { return &_key; }
  inline RocksDBKey const* operator->() const { return &_key; }
  inline RocksDBKey* get() { return &_key; }
  inline RocksDBKey const* get() const { return &_key; }
  inline RocksDBKey& ref() { return _key; }
  inline RocksDBKey const& ref() const { return _key; }

 private:
  transaction::Context* _ctx;
  RocksDBKey _key;
};

}  // namespace arangodb

