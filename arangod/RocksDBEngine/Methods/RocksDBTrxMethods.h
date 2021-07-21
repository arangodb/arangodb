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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBTransactionMethods.h"

namespace arangodb {
  
/// transaction wrapper, uses the current rocksdb transaction
class RocksDBTrxMethods : public RocksDBTransactionMethods {
 public:
  explicit RocksDBTrxMethods(RocksDBTransactionState*, rocksdb::TransactionDB* db);
  
  ~RocksDBTrxMethods();

  virtual bool isIndexingDisabled() const override{ return _indexingDisabled; }

  /// @brief returns true if indexing was disabled by this call
  bool DisableIndexing() override;

  bool EnableIndexing() override;

  Result beginTransaction() override;
  
  Result commitTransaction() override;

  Result abortTransaction() override;
  
  uint64_t numCommits() const override { return _numCommits; }

  rocksdb::ReadOptions iteratorReadOptions() const override;
  
  bool ensureSnapshot() override;

  rocksdb::SequenceNumber GetSequenceNumber() const override;

  bool hasOperations() const noexcept override {
    return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
  }
  
  uint64_t numOperations() const noexcept override {
    return _numInserts + _numUpdates + _numRemoves;
  }
  
  void prepareOperation(DataSourceId cid, RevisionId rid, TRI_voc_document_operation_e operationType) override;

  /// @brief undo the effects of the previous prepareOperation call
  void rollbackOperation(TRI_voc_document_operation_e operationType) override;

  /// @brief add an operation for a transaction collection
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed
  Result addOperation(DataSourceId collectionId, RevisionId revisionId,
                      TRI_voc_document_operation_e opType,
                      bool& hasPerformedIntermediateCommit) override;
                                           
  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*, rocksdb::Slice const& key,
                      rocksdb::PinnableSlice* val) override;
  rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                               rocksdb::Slice const&,
                               rocksdb::PinnableSlice*) override;
  rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                      rocksdb::Slice const& val, bool assume_tracked) override;
  rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                               rocksdb::Slice const& val) override;
  rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key) override;
  rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*, RocksDBKey const&) override;
  void PutLogData(rocksdb::Slice const&) override;

  std::unique_ptr<rocksdb::Iterator> NewIterator(rocksdb::ReadOptions const&,
                                                 rocksdb::ColumnFamilyHandle*) override;

  void SetSavePoint() override;
  rocksdb::Status RollbackToSavePoint() override;
  rocksdb::Status RollbackToWriteBatchSavePoint() override;
  void PopSavePoint() override;

 private:
  void cleanupTransaction();

  /// @brief create a new rocksdb transaction
  void createTransaction();
  
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
    
  rocksdb::TransactionDB* _db{nullptr};

  /// @brief used for read-only trx and intermediate commits
  /// For intermediate commits this MUST ONLY be used for iterators
  rocksdb::Snapshot const* _readSnapshot{nullptr};

  /// @brief shared read options which can be used by operations
  /// For intermediate commits iterators MUST use the _readSnapshot
  rocksdb::ReadOptions _readOptions{};

  /// @brief rocksdb transaction may be null for read only transactions
  rocksdb::Transaction* _rocksTransaction{nullptr};
  
    /// store the number of log entries in WAL
  uint64_t _numLogdata{0};
  
  /// @brief number of commits, including intermediate commits
  uint64_t _numCommits{0};
  // if a transaction gets bigger than these values then an automatic
  // intermediate commit will be done
  uint64_t _numInserts{0};
  uint64_t _numUpdates{0};
  uint64_t _numRemoves{0};

  /// @brief number of rollbacks performed in current transaction. not
  /// resetted on intermediate commit
  uint64_t _numRollbacks{0};

  bool _indexingDisabled{false};
};

}  // namespace arangodb

