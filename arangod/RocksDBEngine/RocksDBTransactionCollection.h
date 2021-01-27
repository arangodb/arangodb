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

#ifndef ARANGOD_ROCKSDB_ROCKSDB_TRANSACTION_COLLECTION_H
#define ARANGOD_ROCKSDB_ROCKSDB_TRANSACTION_COLLECTION_H 1

#include "Basics/Common.h"
#include "StorageEngine/TransactionCollection.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
struct RocksDBDocumentOperation;
namespace transaction {
class Methods;
}
class TransactionState;

/// @brief collection used in a transaction
class RocksDBTransactionCollection final : public TransactionCollection {
 public:
  RocksDBTransactionCollection(TransactionState* trx, DataSourceId cid,
                               AccessMode::Type accessType);
  ~RocksDBTransactionCollection();

  /// @brief whether or not any write operations for the collection happened
  bool hasOperations() const override;

  bool canAccess(AccessMode::Type accessType) const override;
  Result lockUsage() override;
  void releaseUsage() override;

  RevisionId revision() const { return _revision; }
  uint64_t numberDocuments() const {
    return _initialNumberDocuments + _numInserts - _numRemoves;
  }
  uint64_t numInserts() const { return _numInserts; }
  uint64_t numUpdates() const { return _numUpdates; }
  uint64_t numRemoves() const { return _numRemoves; }

  /// @brief add an operation for a transaction collection
  void addOperation(TRI_voc_document_operation_e operationType, RevisionId revisionId);

  /**
   * @brief Prepare collection for commit by placing collection blockers
   * @param trxId    Active transaction ID
   * @param beginSeq Current seq/tick on transaction begin
   */
  void prepareTransaction(TransactionId trxId, uint64_t beginSeq);

  /**
   * @brief Signal upstream abort/rollback to clean up index blockers
   * @param trxId Active transaction ID
   */
  void abortCommit(TransactionId trxId);

  /**
   * @brief Commit collection counts and buffer tracked index updates
   * @param trxId     Active transaction ID
   * @param commitSeq Seq/tick immediately after upstream commit
   */
  void commitCounts(TransactionId trxId, uint64_t commitSeq);

  /// @brief Track documents inserted to the collection
  ///        Used to update the revision tree for replication after commit
  void trackInsert(RevisionId rid);

  /// @brief Track documents removed from the collection
  ///        Used to update the revision tree for replication after commit
  void trackRemove(RevisionId rid);

  struct TrackedOperations {
    std::vector<std::uint64_t> inserts;
    std::vector<std::uint64_t> removals;
    bool empty() const { return inserts.empty() && removals.empty(); }
    void clear() {
      inserts.clear();
      removals.clear();
    }
  };

  TrackedOperations& trackedOperations() { return _trackedOperations; }

  TrackedOperations stealTrackedOperations() {
    TrackedOperations empty;
    _trackedOperations.inserts.swap(empty.inserts);
    _trackedOperations.removals.swap(empty.removals);
    return empty;
  }

  /// @brief Every index can track hashes inserted into this index
  ///        Used to update the estimate after the trx commited
  void trackIndexInsert(IndexId iid, uint64_t hash);

  /// @brief Every index can track hashes removed from this index
  ///        Used to update the estimate after the trx commited
  void trackIndexRemove(IndexId iid, uint64_t hash);

  /// @brief tracked index operations
  struct TrackedIndexOperations {
    std::vector<uint64_t> inserts;
    std::vector<uint64_t> removals;
  };
  using IndexOperationsMap = std::unordered_map<IndexId, TrackedIndexOperations>;

  /// @brief steal the tracked operations from the map
  IndexOperationsMap stealTrackedIndexOperations() {
    IndexOperationsMap empty;
    _trackedIndexOperations.swap(empty);
    return empty;
  }

 private:
  /// @brief request a lock for a collection
  /// returns TRI_ERROR_LOCKED in case the lock was successfully acquired
  /// returns TRI_ERROR_NO_ERROR in case the lock does not need to be acquired
  /// and no other error occurred returns any other error code otherwise
  Result doLock(AccessMode::Type) override;

  /// @brief request an unlock for a collection
  Result doUnlock(AccessMode::Type) override;

 private:
  uint64_t _initialNumberDocuments;
  RevisionId _revision;
  uint64_t _numInserts;
  uint64_t _numUpdates;
  uint64_t _numRemoves;

  /// @brief A list where collection can store its document operations
  ///        Will be applied on commit and not applied on abort
  TrackedOperations _trackedOperations;

  /// @brief A list where all indexes with estimates can store their operations
  ///        Will be applied to the inserter on commit and not applied on abort
  IndexOperationsMap _trackedIndexOperations;
  
  bool _usageLocked;
  bool _exclusiveWrites;
};
}  // namespace arangodb

#endif
