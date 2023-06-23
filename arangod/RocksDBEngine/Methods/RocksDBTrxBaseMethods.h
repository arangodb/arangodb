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

#include "Aql/QueryContext.h"
#include "Containers/SmallVector.h"
#include "Logger/LogMacros.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"

#include <cstdint>
#include <memory>

namespace arangodb {

struct IMemoryTracker {
  virtual ~IMemoryTracker() {}
  virtual void reset() noexcept = 0;
  virtual void increaseMemoryUsage(std::uint64_t valueInBytes) = 0;
  virtual void decreaseMemoryUsage(std::uint64_t valueInBytes) noexcept = 0;

  virtual void setSavePoint() = 0;
  virtual void rollbackToSavePoint() noexcept = 0;
  virtual void popSavePoint() noexcept = 0;
};

class MemoryUsageTracker : public IMemoryTracker {
 public:
  MemoryUsageTracker(metrics::Gauge<uint64_t>* memoryTrackerMetric)
      : _memoryTrackerMetric(memoryTrackerMetric) {}
  ~MemoryUsageTracker() { TRI_ASSERT(_memoryTrackerMetric->load() == 0); }
  void reset() noexcept override {
    _memoryTrackerMetric->store(0, std::memory_order_relaxed);
    _savePoints.clear();
  }
  void increaseMemoryUsage(std::uint64_t valueInBytes) override {
    TRI_ASSERT(_memoryTrackerMetric != nullptr);
    LOG_DEVEL << "Will increase metric"
              << " " << _memoryTrackerMetric->name() << " by " << valueInBytes
              << "bytes";
    _memoryTrackerMetric->fetch_add(valueInBytes);
  }
  void decreaseMemoryUsage(std::uint64_t valueInBytes) noexcept override {
    TRI_ASSERT(_memoryTrackerMetric != nullptr);
    LOG_DEVEL << "Will decrease metric"
              << " " << _memoryTrackerMetric->name() << " by " << valueInBytes
              << "bytes";
    _memoryTrackerMetric->fetch_sub(valueInBytes);
  }

  void setSavePoint() override {
    //  LOG_DEVEL << "ADDING SAVEPOINT";
    _savePoints.push_back(_memoryTrackerMetric->load());
  }

  void rollbackToSavePoint() noexcept override {
    //  LOG_DEVEL << "ROLLING BACK TO SAVEPOINT";
    TRI_ASSERT(!_savePoints.empty());
    _memoryTrackerMetric->store(_savePoints.back(), std::memory_order_relaxed);
    _savePoints.pop_back();
  }

  void popSavePoint() noexcept override {
    //  LOG_DEVEL << "POPPING SAVEPOINT";
    TRI_ASSERT(!_savePoints.empty());
    _savePoints.pop_back();
  }

 private:
  metrics::Gauge<uint64_t>* _memoryTrackerMetric;
  containers::SmallVector<std::uint64_t, 4> _savePoints;
};

class AqlMemoryUsageTracker : public IMemoryTracker {
 public:
  AqlMemoryUsageTracker(metrics::Gauge<uint64_t>* memoryTrackerMetric,
                        ResourceMonitor& resourceMonitor)
      : _memoryTrackerMetric(memoryTrackerMetric),
        _resourceMonitor(resourceMonitor) {}
  ~AqlMemoryUsageTracker() { TRI_ASSERT(_memoryTrackerMetric->load() == 0); }

  void reset() noexcept override {
    _memoryTrackerMetric->store(0, std::memory_order_relaxed);
    _savePoints.clear();
    _resourceMonitor.clear();
  }
  void increaseMemoryUsage(std::uint64_t valueInBytes) override {
    TRI_ASSERT(_memoryTrackerMetric != nullptr);
    LOG_DEVEL << "Will increase metric"
              << " " << _memoryTrackerMetric->name() << " by " << valueInBytes
              << "bytes";
    _memoryTrackerMetric->fetch_add(valueInBytes);
    LOG_DEVEL << "Will increase resource monitor by " << valueInBytes
              << "bytes";
    _resourceMonitor.increaseMemoryUsage(valueInBytes);
  }
  void decreaseMemoryUsage(std::uint64_t valueInBytes) noexcept override {
    TRI_ASSERT(_memoryTrackerMetric != nullptr);
    LOG_DEVEL << "Will decrease metric"
              << " " << _memoryTrackerMetric->name() << " by " << valueInBytes
              << "bytes";
    _memoryTrackerMetric->fetch_sub(valueInBytes);
    LOG_DEVEL << "Will decrease resource monitor by " << valueInBytes
              << "bytes";
    _resourceMonitor.decreaseMemoryUsage(valueInBytes);
  }

  void setSavePoint() override {
    //  LOG_DEVEL << "ADDING SAVEPOINT";
    _savePoints.push_back(_memoryTrackerMetric->load());
  }

  void rollbackToSavePoint() noexcept override {
    //  LOG_DEVEL << "ROLLING BACK TO SAVEPOINT";
    TRI_ASSERT(!_savePoints.empty());
    _memoryTrackerMetric->store(_savePoints.back(), std::memory_order_relaxed);
    _savePoints.pop_back();
    _resourceMonitor.clear();
    _resourceMonitor.increaseMemoryUsage(_savePoints.back());
  }

  void popSavePoint() noexcept override {
    //  LOG_DEVEL << "POPPING SAVEPOINT";
    TRI_ASSERT(!_savePoints.empty());
    _savePoints.pop_back();
  }

 private:
  metrics::Gauge<uint64_t>* _memoryTrackerMetric;
  ResourceMonitor& _resourceMonitor;
  containers::SmallVector<std::uint64_t, 4> _savePoints;
};

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

  /// @brief assumed overhead for each appended entry to a rocksdb::WriteBuffer.
  /// this is not the actual per-entry overhead, but a good enough estimate.
  /// the actual overhead depends on a lot of factors, and we don't want to
  /// replicate rocksdb's internals here.
  static constexpr std::uint64_t writeBufferEntryOverhead = 12;
  /// @brief assumed additional overhead for each entry in a
  /// WriteBatchWithIndex. this is in addition to the actual WriteBuffer entry.
  static constexpr std::uint64_t indexingEntryOverhead = 32;
  /// @brief function to calculate overhead of a WriteBatchWithIndex entry,
  /// depending on keySize. will return 0 if indexing is disabled in the current
  /// transaction.
  std::uint64_t indexingOverhead(std::uint64_t keySize) const noexcept;

  IRocksDBTransactionCallback& _callback;

  rocksdb::TransactionDB* _db{nullptr};

  std::optional<std::reference_wrapper<ResourceMonitor>> _resourceMonitor;

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
  std::unique_ptr<IMemoryTracker> _memoryTracker;

  bool _indexingDisabled{false};
};

}  // namespace arangodb
