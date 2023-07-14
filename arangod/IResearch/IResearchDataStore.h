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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearchDataStoreMeta.h"
#include "Containers.h"
#include "IResearch/ResourceManager.hpp"
#include "IResearchCommon.h"
#include "IResearchVPackComparer.h"
#include "IResearchPrimaryKeyFilter.h"
#include "Indexes/Index.h"
#include "Metrics/Fwd.h"
#include "StorageEngine/TransactionState.h"
#include "RocksDBEngine/RocksDBIndex.h"

#include "store/directory_attributes.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/directory.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>

namespace arangodb {

struct FlushSubscription;
class DatabasePathFeature;
class StorageEngine;
class StorageSnapshot;
class ClusterInfo;

namespace iresearch {

struct MaintenanceState;
class IResearchFeature;
class IResearchDataStore;

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchDataStore handle to use with asynchronous tasks
////////////////////////////////////////////////////////////////////////////////
using AsyncLinkHandle = AsyncValue<IResearchDataStore>;
using LinkLock = AsyncLinkHandle::Value;

////////////////////////////////////////////////////////////////////////////////
/// @brief container storing the index state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct IResearchTrxState final : public TransactionState::Cookie {
  // prevent data-store deallocation (lock @ AsyncSelf)
  LinkLock _linkLock;  // should be first field to destroy last
  irs::IndexWriter::Transaction _ctx;
  std::shared_ptr<PrimaryKeysFilterBase> _removals;

  IResearchTrxState(LinkLock&& linkLock, irs::IndexWriter& writer,
                    std::shared_ptr<PrimaryKeysFilterBase>&& removals) noexcept
      : _linkLock{std::move(linkLock)},
        _ctx{writer.GetBatch()},
        _removals{std::move(removals)} {}

  ~IResearchTrxState() final {
    // TODO(MBkkt) Make Abort in ~Transaction()
    _ctx.Abort();
  }

  void remove(LocalDocumentId value) { _removals->emplace(value); }
};

void clusterCollectionName(LogicalCollection const& collection, ClusterInfo* ci,
                           uint64_t id, bool indexIdAttribute,
                           std::string& name);

class IResearchDataStore {
 public:
  using AsyncLinkPtr = std::shared_ptr<AsyncLinkHandle>;
  using InitCallback = std::function<irs::directory_attributes()>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a snapshot representation of the data-store
  ///        locked to prevent data store deallocation
  //////////////////////////////////////////////////////////////////////////////
  struct DataSnapshot {
    DataSnapshot(irs::DirectoryReader&& index,
                 std::shared_ptr<StorageSnapshot> db)
        : _reader(std::move(index)), _snapshot(std::move(db)) {
      TRI_ASSERT(_reader);
      // for now we require that each index has its own snapshot
      TRI_ASSERT(_snapshot);
    }
    irs::DirectoryReader _reader;
    std::shared_ptr<StorageSnapshot> _snapshot;
  };

  using DataSnapshotPtr = std::shared_ptr<DataSnapshot>;

  class Snapshot {
   public:
    Snapshot(Snapshot const&) = delete;
    Snapshot& operator=(Snapshot const&) = delete;
    Snapshot() = default;
    ~Snapshot() = default;
    Snapshot(LinkLock&& lock, DataSnapshotPtr&& snapshot) noexcept
        : _lock{std::move(lock)}, _snapshot{std::move(snapshot)} {}
    Snapshot(Snapshot&& rhs) noexcept
        : _lock{std::move(rhs._lock)}, _snapshot{std::move(rhs._snapshot)} {}
    Snapshot& operator=(Snapshot&& rhs) noexcept {
      if (this != &rhs) {
        _lock = std::move(rhs._lock);
        _snapshot = std::move(rhs._snapshot);
      }
      return *this;
    }

    [[nodiscard]] auto const& getDirectoryReader() const noexcept {
      return _snapshot->_reader;
    }

   private:
    // lock preventing data store deallocation
    LinkLock _lock;
    DataSnapshotPtr _snapshot;
  };

  explicit IResearchDataStore(ArangodServer& server,
                              LogicalCollection& collection);

  virtual ~IResearchDataStore();

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief 'this' for the lifetime of the link data-store
  ///        for use with asynchronous calls, e.g. callbacks, view
  ///////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] AsyncLinkPtr self() const { return _asyncSelf; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return pointer to an index reader containing the data store current
  ///         record snapshot
  ///         (nullptr == no data store snapshot available, e.g. error)
  //////////////////////////////////////////////////////////////////////////////
  Snapshot snapshot() const;
  static DataSnapshotPtr reader(LinkLock const& linkLock);

  [[nodiscard]] virtual Index& index() noexcept = 0;
  [[nodiscard]] virtual Index const& index() const noexcept = 0;
  [[nodiscard]] StorageEngine* engine() const noexcept { return _engine; }

  // valid for a link to be dropped from an ArangoSearch view
  static constexpr bool canBeDropped() noexcept { return true; }

  // selectivity can only be determined per query
  // since multiple fields are indexed
  static constexpr bool hasSelectivityEstimate() noexcept { return false; }

  bool hasNestedFields() const noexcept { return _hasNestedFields; }

  TruncateGuard truncateBegin() {
    _commitMutex.lock();
    return {TruncateGuard::Ptr{&_commitMutex}};
  }
  void truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                      transaction::Methods* trx);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief give derived class chance to fine-tune iresearch storage
  //////////////////////////////////////////////////////////////////////////////
  virtual void afterCommit() {}

  void finishCreation();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark the current data store state as the latest valid state
  /// @param wait even if other thread is committing
  //////////////////////////////////////////////////////////////////////////////
  Result commit(bool wait = true);
  static Result commit(LinkLock& linkLock, bool wait);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  void toVelocyPackStats(VPackBuilder& builder) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief lookup referenced analyzer
  ////////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] virtual AnalyzerPool::ptr findAnalyzer(
      AnalyzerPool const& analyzer) const = 0;

  uint64_t recoveryTickLow() const noexcept {
    return _dataStore._recoveryTickLow;
  }
  uint64_t recoveryTickHigh() const noexcept {
    return _dataStore._recoveryTickHigh;
  }

  IResearchTrxState* getContext(TransactionState& state);
  bool exists(LocalDocumentId documentId) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an ArangoDB document from an iResearch View
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result remove(transaction::Methods& trx, LocalDocumentId documentId);
  void recoveryRemove(LocalDocumentId documentId);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert an ArangoDB document into an iResearch View using '_meta'
  /// params
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  template<typename FieldIteratorType, typename MetaType>
  Result insert(transaction::Methods& trx, LocalDocumentId documentId,
                velocypack::Slice doc, MetaType const& meta);
  template<typename FieldIteratorType, typename MetaType>
  void recoveryInsert(uint64_t tick, LocalDocumentId documentId,
                      velocypack::Slice doc, MetaType const& meta);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update runtine data processing properties
  /// @return success
  //////////////////////////////////////////////////////////////////////////////
  Result properties(IResearchDataStoreMeta const& meta);
  static void properties(LinkLock linkLock, IResearchDataStoreMeta const& meta);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief index stats
  //////////////////////////////////////////////////////////////////////////////
  struct Stats {
    uint64_t numDocs{};
    uint64_t numLiveDocs{};
    uint64_t numPrimaryDocs{};
    uint64_t numSegments{};
    uint64_t numFiles{};
    uint64_t indexSize{};
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get index stats for current snapshot
  ////////////////////////////////////////////////////////////////////////////////
  virtual Stats stats() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set the data store to out of sync. if a data store is out of sync,
  /// it is known to have incomplete data and may refuse to serve queries
  /// (depending on settings). returns true if the call set the data store to
  /// out of sync, and false if the data store was already marked as out of
  /// sync before.
  //////////////////////////////////////////////////////////////////////////////
  bool setOutOfSync() noexcept;
  void markOutOfSyncUnsafe();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the data store is out of sync (i.e. has incomplete
  /// data)
  //////////////////////////////////////////////////////////////////////////////
  bool isOutOfSync() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not queries on this data store fail should fail with an
  /// error if the data store is out of sync.
  //////////////////////////////////////////////////////////////////////////////
  bool failQueriesOnOutOfSync() const noexcept;

 protected:
  friend struct CommitTask;
  friend struct ConsolidationTask;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief detailed commit result
  //////////////////////////////////////////////////////////////////////////////
  enum class CommitResult {
    ////////////////////////////////////////////////////////////////////////////
    /// @brief undefined state
    ////////////////////////////////////////////////////////////////////////////
    UNDEFINED = 0,

    ////////////////////////////////////////////////////////////////////////////
    /// @brief no changes were made
    ////////////////////////////////////////////////////////////////////////////
    NO_CHANGES,

    ////////////////////////////////////////////////////////////////////////////
    /// @brief another commit is in progress
    ////////////////////////////////////////////////////////////////////////////
    IN_PROGRESS,

    ////////////////////////////////////////////////////////////////////////////
    /// @brief commit is done
    ////////////////////////////////////////////////////////////////////////////
    DONE
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying iresearch data store
  //////////////////////////////////////////////////////////////////////////////
  struct DataStore {
    // runtime meta for a data store (not persisted)
    IResearchDataStoreMeta _meta;
    irs::directory::ptr _directory;
    // for use with member '_meta'
    basics::ReadWriteLock _mutex;
    std::filesystem::path _path;
    irs::IndexWriter::ptr _writer;
    // the tick at which data store was recovered
    uint64_t _recoveryTickLow{0};
    uint64_t _recoveryTickHigh{0};
    explicit operator bool() const noexcept { return _directory && _writer; }

    void resetDataStore() noexcept {
      // reset all underlying readers to release file handles
      storeSnapshot(nullptr);
      _writer.reset();
      _directory.reset();
    }

    [[nodiscard]] DataSnapshotPtr loadSnapshot() const noexcept {
      return std::atomic_load_explicit(&_snapshot, std::memory_order_acquire);
    }

    void storeSnapshot(DataSnapshotPtr snapshot) noexcept {
      std::atomic_store_explicit(&_snapshot, std::move(snapshot),
                                 std::memory_order_release);
    }

   private:
    DataSnapshotPtr _snapshot;
  };

  struct UnsafeOpResult {
    Result result;
    uint64_t timeMs;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run filesystem cleanup on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  UnsafeOpResult cleanupUnsafe();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark the current data store state as the latest valid state
  /// @param wait even if other thread is committing
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  UnsafeOpResult commitUnsafe(bool wait,
                              irs::ProgressReportCallback const& progress,
                              CommitResult& code);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run segment consolidation on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  UnsafeOpResult consolidateUnsafe(
      IResearchDataStoreMeta::ConsolidationPolicy const& policy,
      irs::MergeWriter::FlushProgress const& progress,
      bool& emptyConsolidation);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run filesystem cleanup on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  Result cleanupUnsafeImpl();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark the current data store state as the latest valid state
  /// @param wait even if other thread is committing
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  Result commitUnsafeImpl(bool wait,
                          irs::ProgressReportCallback const& progress,
                          CommitResult& code);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run segment consolidation on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  Result consolidateUnsafeImpl(
      IResearchDataStoreMeta::ConsolidationPolicy const& policy,
      irs::MergeWriter::FlushProgress const& progress,
      bool& emptyConsolidation);

  void initAsyncSelf();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the data store with a new or from an existing directory
  //////////////////////////////////////////////////////////////////////////////
  Result initDataStore(
      bool& pathExists, InitCallback const& initCallback, uint32_t version,
      bool sorted, bool nested,
      std::span<const IResearchViewStoredValues::StoredColumn> storedColumns,
      irs::type_info::type_id primarySortCompression,
      irs::IndexReaderOptions& readerOptions);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief schedule a commit job
  //////////////////////////////////////////////////////////////////////////////
  void scheduleCommit(std::chrono::milliseconds delay);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief schedule a consolidation job
  //////////////////////////////////////////////////////////////////////////////
  void scheduleConsolidation(std::chrono::milliseconds delay);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wait for all outstanding commit/consolidate operations and closes
  /// data store
  //////////////////////////////////////////////////////////////////////////////
  void shutdownDataStore() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wait for all outstanding commit/consolidate operations and remove
  /// data store
  //////////////////////////////////////////////////////////////////////////////
  Result deleteDataStore() noexcept;

#ifdef ARANGODB_USE_GOOGLE_TESTS
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get numbers of failed commit cleanup consolidation
  ////////////////////////////////////////////////////////////////////////////////
  std::tuple<uint64_t, uint64_t, uint64_t> numFailed() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get average time of commit cleanuo consolidation
  ////////////////////////////////////////////////////////////////////////////////
  std::tuple<uint64_t, uint64_t, uint64_t> avgTime() const;
#endif

  void recoveryCommit(uint64_t tick);

 protected:
  enum class DataStoreError : uint8_t {
    // data store has no issues
    kNoError = 0,
    // data store is out of sync
    kOutOfSync = 1,
    // data store is failed (currently not used)
    kFailed = 2,
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Update index stats for current snapshot
  /// @note Unsafe, can only be called is _asyncSelf is locked
  ////////////////////////////////////////////////////////////////////////////////
  Stats updateStatsUnsafe(DataSnapshotPtr data) const;

  void initClusterMetrics() const;

  // Return index writer options given the specified arguments.
  irs::IndexWriterOptions getWriterOptions(
      irs::IndexReaderOptions const& options, uint32_t version, bool sorted,
      bool nested,
      std::span<const IResearchViewStoredValues::StoredColumn> storedColumns,
      irs::type_info::type_id primarySortCompression);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief insert metrics to MetricsFeature
  //////////////////////////////////////////////////////////////////////////////
  virtual void insertMetrics() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove metrics from MetricsFeature
  //////////////////////////////////////////////////////////////////////////////
  virtual void removeMetrics() {}

  virtual void invalidateQueryCache(TRI_vocbase_t*) = 0;

  virtual irs::Comparer const* getComparator() const noexcept = 0;

  StorageEngine* _engine{nullptr};

  // the feature where async jobs were registered (nullptr == no jobs
  // registered)
  IResearchFeature* _asyncFeature;

  // 'this' for the lifetime of the link (for use with asynchronous calls)
  AsyncLinkPtr _asyncSelf;

  // the iresearch data store, protected by _asyncSelf->mutex()
  DataStore _dataStore;

  std::shared_ptr<FlushSubscription> _flushSubscription;
  std::shared_ptr<MaintenanceState> _maintenanceState;
  // data store error state
  std::atomic<DataStoreError> _error{DataStoreError::kNoError};
  bool _hasNestedFields{false};
  bool _isCreation{true};
#ifdef USE_ENTERPRISE
  std::atomic_bool _useSearchCache{true};
#endif

  // protected by _commitMutex
  uint64_t _lastCommittedTick{0};

  size_t _cleanupIntervalCount{0};

  // prevents data store sequential commits
  std::mutex _commitMutex;

  // for insert(...)/remove(...)
  irs::IndexWriter::Transaction _recoveryTrx;
  std::shared_ptr<PrimaryKeysFilterBase> _recoveryRemoves;
  TransactionState::BeforeCommitCallback _beforeCommitCallback;
  TransactionState::AfterCommitCallback _afterCommitCallback;

  irs::IResourceManager* _writersMemory{&irs::IResourceManager::kNoop};
  irs::IResourceManager* _readersMemory{&irs::IResourceManager::kNoop};
  irs::IResourceManager* _consolidationsMemory{&irs::IResourceManager::kNoop};
  irs::IResourceManager* _fileDescriptorsCount{&irs::IResourceManager::kNoop};

  metrics::Gauge<uint64_t>* _mappedMemory{nullptr};

  metrics::Gauge<uint64_t>* _numFailedCommits{nullptr};
  metrics::Gauge<uint64_t>* _numFailedCleanups{nullptr};
  metrics::Gauge<uint64_t>* _numFailedConsolidations{nullptr};

  std::atomic_uint64_t _commitTimeNum{0};
  metrics::Gauge<uint64_t>* _avgCommitTimeMs{nullptr};

  std::atomic_uint64_t _cleanupTimeNum{0};
  metrics::Gauge<uint64_t>* _avgCleanupTimeMs{nullptr};

  std::atomic_uint64_t _consolidationTimeNum{0};
  metrics::Gauge<uint64_t>* _avgConsolidationTimeMs{nullptr};

  metrics::Guard<Stats>* _metricStats{nullptr};
};

std::filesystem::path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                       IResearchDataStore const& dataStore);

}  // namespace iresearch
}  // namespace arangodb
