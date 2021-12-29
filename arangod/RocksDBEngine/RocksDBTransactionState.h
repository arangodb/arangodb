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
class RocksDBTransactionState : public TransactionState {
  friend class RocksDBTrxBaseMethods;

 public:
  RocksDBTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                          transaction::Options const& options);
  ~RocksDBTransactionState() override;

  /// @brief begin a transaction
  [[nodiscard]] Result beginTransaction(transaction::Hints hints) override;

  /// @brief commit a transaction
  [[nodiscard]] Result commitTransaction(transaction::Methods* trx) override;

  /// @brief abort a transaction
  [[nodiscard]] Result abortTransaction(transaction::Methods* trx) override;

  [[nodiscard]] virtual bool hasOperations() const noexcept = 0;

  [[nodiscard]] virtual uint64_t numOperations() const noexcept = 0;

  [[nodiscard]] bool hasFailedOperations() const override;

  [[nodiscard]] bool iteratorMustCheckBounds(DataSourceId cid,
                                             ReadOwnWrites readOwnWrites) const;

  void prepareOperation(DataSourceId cid, RevisionId rid,
                        TRI_voc_document_operation_e operationType);

  /// @brief add an operation for a transaction collection
  [[nodiscard]] Result addOperation(DataSourceId collectionId,
                                    RevisionId revisionId,
                                    TRI_voc_document_operation_e opType);

  [[nodiscard]] Result performIntermediateCommitIfRequired(
      DataSourceId collectionId) override;

  /// @brief return wrapper around rocksdb transaction
  [[nodiscard]] virtual RocksDBTransactionMethods* rocksdbMethods(
      DataSourceId collectionId) const = 0;

  /// @brief acquire a database snapshot if we do not yet have one.
  /// Returns true if a snapshot was acquired, otherwise false (i.e., if we
  /// already had a snapshot)
  [[nodiscard]] virtual bool ensureSnapshot() = 0;

  [[nodiscard]] static RocksDBTransactionState* toState(
      transaction::Methods* trx);

  [[nodiscard]] static RocksDBTransactionMethods* toMethods(
      transaction::Methods* trx, DataSourceId collectionId);

  /// @brief make some internal preparations for accessing this state in
  /// parallel from multiple threads. READ-ONLY transactions
  void prepareForParallelReads();
  /// @brief in parallel mode. READ-ONLY transactions
  [[nodiscard]] bool inParallelMode() const;

  [[nodiscard]] RocksDBTransactionCollection::TrackedOperations&
  trackedOperations(DataSourceId cid);

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

  /// @brief whether or not a transaction only has exclusive or read accesses
  bool isOnlyExclusiveTransaction() const noexcept;

  [[nodiscard]] virtual rocksdb::SequenceNumber beginSeq() const = 0;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  /// @brief only needed for RocksDBTransactionStateGuard
  void use() noexcept;
  void unuse() noexcept;
#endif

 protected:
  virtual Result doCommit() = 0;
  virtual Result doAbort() = 0;

 private:
  rocksdb::SequenceNumber prepareCollections();
  void commitCollections(rocksdb::SequenceNumber lastWritten);
  void cleanupCollections();

  void maybeDisableIndexing();

  /// @brief delete transaction, snapshot and cache trx
  void cleanupTransaction() noexcept;

  /// @brief cache transaction to unblock banished keys
  cache::Transaction* _cacheTx;

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
  explicit RocksDBTransactionStateGuard(
      RocksDBTransactionState* state) noexcept;
  ~RocksDBTransactionStateGuard();

  RocksDBTransactionState* _state;
#else
  explicit RocksDBTransactionStateGuard(
      RocksDBTransactionState* /*state*/) noexcept {}
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
