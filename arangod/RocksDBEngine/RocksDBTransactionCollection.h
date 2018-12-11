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

#ifndef ARANGOD_ROCKSDB_ROCKSDB_TRANSACTION_COLLECTION_H
#define ARANGOD_ROCKSDB_ROCKSDB_TRANSACTION_COLLECTION_H 1

#include "Basics/Common.h"
#include "StorageEngine/TransactionCollection.h"
#include "VocBase/AccessMode.h"
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
  RocksDBTransactionCollection(TransactionState* trx, TRI_voc_cid_t cid,
                               AccessMode::Type accessType, int nestingLevel);
  ~RocksDBTransactionCollection();

  /// @brief whether or not any write operations for the collection happened
  bool hasOperations() const override;

  void freeOperations(transaction::Methods* activeTrx,
                      bool mustRollback) override;

  bool canAccess(AccessMode::Type accessType) const override;
  int updateUsage(AccessMode::Type accessType, int nestingLevel) override;
  int use(int nestingLevel) override;
  void unuse(int nestingLevel) override;
  void release() override;

  TRI_voc_rid_t revision() const { return _revision; }
  uint64_t numberDocuments() const {
    return _initialNumberDocuments + _numInserts - _numRemoves;
  }
  uint64_t numInserts() const { return _numInserts; }
  uint64_t numUpdates() const { return _numUpdates; }
  uint64_t numRemoves() const { return _numRemoves; }

  /// @brief add an operation for a transaction collection
  void addOperation(TRI_voc_document_operation_e operationType,
                    TRI_voc_rid_t revisionId);
  
  /**
   * @brief Prepare collection for commit by placing index blockers
   * @param trxId        Active transaction ID
   * @param preCommitSeq Current seq/tick immediately before call
   */
  void prepareCommit(uint64_t trxId, uint64_t preCommitSeq);

  /**
   * @brief Signal upstream abort/rollback to clean up index blockers
   * @param trxId Active transaction ID
   */
  void abortCommit(uint64_t trxId);

  /**
   * @brief Commit collection counts and buffer tracked index updates
   * @param trxId     Active transaction ID
   * @param commitSeq Seq/tick immediately after upstream commit
   */
  void commitCounts(uint64_t trxId, uint64_t commitSeq);

  /// @brief Every index can track hashes inserted into this index
  ///        Used to update the estimate after the trx commited
  void trackIndexInsert(uint64_t idxObjectId, uint64_t hash);

  /// @brief Every index can track hashes removed from this index
  ///        Used to update the estimate after the trx commited
  void trackIndexRemove(uint64_t idxObjectId, uint64_t hash);

 private:
  /// @brief request a lock for a collection
  /// returns TRI_ERROR_LOCKED in case the lock was successfully acquired
  /// returns TRI_ERROR_NO_ERROR in case the lock does not need to be acquired and no other error occurred
  /// returns any other error code otherwise
  int doLock(AccessMode::Type, int nestingLevel) override;

  /// @brief request an unlock for a collection
  int doUnlock(AccessMode::Type, int nestingLevel) override;

 private:
  int _nestingLevel;  // the transaction level that added this collection
  uint64_t _initialNumberDocuments;
  TRI_voc_rid_t _revision;
  uint64_t _numInserts;
  uint64_t _numUpdates;
  uint64_t _numRemoves;
  bool _usageLocked;

  struct IndexOperations {
    std::vector<uint64_t> inserts;
    std::vector<uint64_t> removals;
  };
  
  /// @brief A list where all indexes with estimates can store their operations
  ///        Will be applied to the inserter on commit and not applied on abort
  std::unordered_map<uint64_t, IndexOperations> _trackedIndexOperations;
};
}

#endif
