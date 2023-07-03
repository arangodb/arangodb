////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>
#include <memory>

namespace arangodb {
struct RocksDBMethodsMemoryTracker;

struct IRocksDBTransactionCallback {
  virtual ~IRocksDBTransactionCallback() = default;
  virtual rocksdb::SequenceNumber prepare() = 0;
  virtual void cleanup() = 0;
  virtual void commit(rocksdb::SequenceNumber lastWritten) = 0;
};

/// transaction wrapper, uses the current rocksdb transaction
class RocksDBTrxBaseMethods : public RocksDBTransactionMethods {
 public:
  explicit RocksDBTrxBaseMethods(RocksDBTransactionState* state,
                                 IRocksDBTransactionCallback& callback,
                                 rocksdb::TransactionDB* db);

  ~RocksDBTrxBaseMethods() override;

  virtual bool isIndexingDisabled() const final override {
    return _indexingDisabled;
  }

  /// @brief returns true if indexing was disabled by this call
  bool DisableIndexing() final override;

  bool EnableIndexing() final override;

  Result beginTransaction() override;

  Result commitTransaction() final override;

  Result abortTransaction() final override;

  TRI_voc_tick_t lastOperationTick() const noexcept override;

  uint64_t numCommits() const noexcept final override { return _numCommits; }

  uint64_t numIntermediateCommits() const noexcept final override {
    return _numIntermediateCommits;
  }

  bool ensureSnapshot() final override;

  rocksdb::SequenceNumber GetSequenceNumber() const noexcept final override;

  bool hasOperations() const noexcept final override {
    return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
  }

  uint64_t numOperations() const noexcept final override {
    return _numInserts + _numUpdates + _numRemoves;
  }

  uint64_t numPrimitiveOperations() const noexcept final override {
    return _numInserts + 2 * _numUpdates + _numRemoves;
  }

  /// @brief add an operation for a transaction
  Result addOperation(TRI_voc_document_operation_e opType) override;

  rocksdb::Status GetFromSnapshot(rocksdb::ColumnFamilyHandle* family,
                                  rocksdb::Slice const& slice,
                                  rocksdb::PinnableSlice* pinnable,
                                  ReadOwnWrites rw,
                                  rocksdb::Snapshot const* snapshot) override;
  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*, rocksdb::Slice const&,
                      rocksdb::PinnableSlice*, ReadOwnWrites) override;
  rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                               rocksdb::Slice const&,
                               rocksdb::PinnableSlice*) final override;
  rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                      rocksdb::Slice const& val,
                      bool assume_tracked) final override;
  rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*,
                               RocksDBKey const& key,
                               rocksdb::Slice const& val) final override;
  rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*,
                         RocksDBKey const& key) final override;
  rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*,
                               RocksDBKey const&) final override;
  void PutLogData(rocksdb::Slice const&) final override;

  void SetSavePoint() final override;
  rocksdb::Status RollbackToSavePoint() final override;
  rocksdb::Status RollbackToWriteBatchSavePoint() final override;
  void PopSavePoint() final override;

 protected:
  virtual void cleanupTransaction();

  /// @brief create a new rocksdb transaction
  virtual void createTransaction();

  Result doCommit();
  Result doCommitImpl();

  /// @brief assumed additional indexing overhead for each entry in a
  /// WriteBatchWithIndex. this is in addition to the actual WriteBuffer entry.
  /// the WriteBatchWithIndex keeps all entries (which are pointers) in a
  /// skiplist. it is unclear from the outside how much memory the skiplist
  /// will use per entry, so this value here is just a guess.
  static constexpr size_t fixedIndexingEntryOverhead = 32;

  /// @brief assumed additional overhead for each lock that is held by the
  /// transaction. the overhead is computed as follows:
  /// locks are stored in rocksdb in a hash table, which maps from the locked
  /// key to a LockInfo struct. The LockInfo struct is 120 bytes big.
  /// we also assume some more overhead for the hash table entries and some
  /// general overhead because the hash table is never assumed to be completely
  /// full (load factor < 1). thus we assume 64 bytes of overhead for each
  /// entry. this is an arbitrary value.
  static constexpr size_t fixedLockEntryOverhead = 120 + 64;

  /// @brief function to calculate overhead of a WriteBatchWithIndex entry,
  /// depending on keySize. will return 0 if indexing is disabled in the current
  /// transaction.
  size_t indexingOverhead(size_t keySize) const noexcept;

  /// @brief function to calculate overhead of a lock entry, depending on
  /// keySize. will return 0 if no locks are used by the current transaction
  /// (e.g. if the transaction is using an exclusive lock)
  size_t lockOverhead(size_t keySize) const noexcept;

  /// @brief returns the payload size of the transaction's WriteBatch. this
  /// excludes locks and any potential indexes (i.e. WriteBatchWithIndex).
  size_t currentWriteBatchSize() const noexcept;

  IRocksDBTransactionCallback& _callback;

  rocksdb::TransactionDB* _db{nullptr};

  /// @brief shared read options which can be used by operations
  ReadOptions _readOptions{};

  /// @brief rocksdb transaction may be null for read only transactions
  rocksdb::Transaction* _rocksTransaction{nullptr};

  /// store the number of log entries in WAL
  std::uint64_t _numLogdata{0};

  /// @brief number of commits, including intermediate commits
  std::uint64_t _numCommits{0};
  /// @brief number of intermediate commits
  std::uint64_t _numIntermediateCommits{0};
  std::uint64_t _numInserts{0};
  std::uint64_t _numUpdates{0};
  std::uint64_t _numRemoves{0};

  /// @brief number of rollbacks performed in current transaction. not
  /// resetted on intermediate commit
  std::uint64_t _numRollbacks{0};

  /// @brief tick of last added & written operation
  TRI_voc_tick_t _lastWrittenOperationTick{0};

  /// @brief object used for tracking memory usage
  std::unique_ptr<RocksDBMethodsMemoryTracker> _memoryTracker;

  bool _indexingDisabled{false};
};

}  // namespace arangodb
