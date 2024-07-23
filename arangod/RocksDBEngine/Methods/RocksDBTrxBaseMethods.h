////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Aql/QueryContext.h"
#include "Containers/SmallVector.h"
#include "RocksDBEngine/RocksDBMethodsMemoryTracker.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"

#include <cstdint>

namespace arangodb {
struct ResourceMonitor;

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

  bool isIndexingDisabled() const noexcept final override {
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

  rocksdb::Status SingleGet(rocksdb::Snapshot const* snapshot,
                            rocksdb::ColumnFamilyHandle& family,
                            rocksdb::Slice const& key,
                            rocksdb::PinnableSlice& value) final;
  void MultiGet(rocksdb::Snapshot const* snapshot,
                rocksdb::ColumnFamilyHandle& family, size_t count,
                rocksdb::Slice const* keys, rocksdb::PinnableSlice* values,
                rocksdb::Status* statuses) final;
  void MultiGet(rocksdb::ColumnFamilyHandle& family, size_t count,
                rocksdb::Slice const* keys, rocksdb::PinnableSlice* values,
                rocksdb::Status* statuses, ReadOwnWrites) override;

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

  virtual void beginQuery(std::shared_ptr<ResourceMonitor> resourceMonitor,
                          bool isModificationQuery);
  virtual void endQuery(bool isModificationQuery) noexcept;

 protected:
  virtual void cleanupTransaction();

  /// @brief create a new rocksdb transaction
  virtual void createTransaction();

  Result doCommit();
  Result doCommitImpl();

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
  RocksDBMethodsMemoryTracker _memoryTracker;

  bool _indexingDisabled{false};
};

}  // namespace arangodb
