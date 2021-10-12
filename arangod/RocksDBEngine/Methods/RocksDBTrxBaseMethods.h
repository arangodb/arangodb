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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBTransactionMethods.h"

namespace arangodb {
  
/// transaction wrapper, uses the current rocksdb transaction
class RocksDBTrxBaseMethods : public RocksDBTransactionMethods {
 public:
  RocksDBTrxBaseMethods(TRI_vocbase_t& vocbase, transaction::Options options,
                        TransactionId tid, transaction::Hints hints,
                        rocksdb::TransactionDB* db);

  ~RocksDBTrxBaseMethods();

  virtual bool isIndexingDisabled() const final override{ return _indexingDisabled; }

  /// @brief returns true if indexing was disabled by this call
  bool DisableIndexing() final override;

  bool EnableIndexing() final override;

  Result beginTransaction() override;
  
  Result commitTransaction() final override;

  Result abortTransaction() final override;
  
  TRI_voc_tick_t lastOperationTick() const noexcept override;

  uint64_t numCommits() const noexcept final override { return _numCommits; }
  
  bool ensureSnapshot() final override;

  rocksdb::SequenceNumber GetSequenceNumber() const noexcept final override;

  bool hasOperations() const noexcept final override {
    return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
  }
  
  uint64_t numOperations() const noexcept final override {
    return _numInserts + _numUpdates + _numRemoves;
  }

  /// @brief add an operation for a transaction collection
  /// sets hasPerformedIntermediateCommit to true if an intermediate commit was
  /// performed
  Result addOperation(DataSourceId collectionId, RevisionId revisionId,
                      TRI_voc_document_operation_e opType) override;

  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*,
                      rocksdb::Slice const&, rocksdb::PinnableSlice*, ReadOwnWrites) override;
  rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                               rocksdb::Slice const&,
                               rocksdb::PinnableSlice*) final override;
  rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                      rocksdb::Slice const& val, bool assume_tracked) final override;
  rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                               rocksdb::Slice const& val) final override;
  rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key) final override;
  rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*, RocksDBKey const&) final override;
  void PutLogData(rocksdb::Slice const&) final override;

  void SetSavePoint() final override;
  rocksdb::Status RollbackToSavePoint() final override;
  rocksdb::Status RollbackToWriteBatchSavePoint() final override;
  void PopSavePoint() final override;

 protected:
  virtual void cleanupTransaction();

  /// @brief create a new rocksdb transaction
  virtual void createTransaction();
  
  arangodb::Result doCommit();

  rocksdb::TransactionDB* _db{nullptr};

  /// @brief shared read options which can be used by operations
  ReadOptions _readOptions{};

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

  /// @brief tick of last added & written operation
  TRI_voc_tick_t _lastWrittenOperationTick{0};
  
  bool _indexingDisabled{false};
};

}  // namespace arangodb

