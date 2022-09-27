////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "IResearchCommon.h"
#include "IResearchVPackComparer.h"
#include "IResearchPrimaryKeyFilter.h"
#include "Indexes/Index.h"
#include "Metrics/Fwd.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/TransactionState.h"

#include "store/directory_attributes.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/directory.hpp"
#include "utils/utf8_path.hpp"

#include <atomic>

namespace arangodb {

struct FlushSubscription;

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
  irs::index_writer::documents_context _ctx;
  PrimaryKeyFilterContainer _removals;  // list of document removals

  IResearchTrxState(LinkLock&& linkLock, irs::index_writer& writer) noexcept
      : _linkLock{std::move(linkLock)}, _ctx{writer.documents()} {}

  ~IResearchTrxState() final {
    if (_removals.empty()) {
      return;  // nothing to do
    }
    try {
      // hold references even after transaction
      auto filter =
          std::make_unique<PrimaryKeyFilterContainer>(std::move(_removals));
      _ctx.remove(std::unique_ptr<irs::filter>(std::move(filter)));
    } catch (std::exception const& e) {
      LOG_TOPIC("eb463", ERR, arangodb::iresearch::TOPIC)
          << "caught exception while applying accumulated removals: "
          << e.what();
    } catch (...) {
      LOG_TOPIC("14917", ERR, arangodb::iresearch::TOPIC)
          << "caught exception while applying accumulated removals";
    }
  }

  void remove(StorageEngine& engine, LocalDocumentId const& value,
              bool nested) {
    _ctx.remove(_removals.emplace(engine, value, nested));
  }

  void reset() noexcept {
    _removals.clear();
    _ctx.reset();
  }
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
  // TODO Refactor irs::directory_reader ctor, now it doesn't have move
  class Snapshot {
   public:
    Snapshot(Snapshot const&) = delete;
    Snapshot& operator=(Snapshot const&) = delete;
    Snapshot() = default;
    ~Snapshot() = default;
    Snapshot(LinkLock&& lock, irs::directory_reader&& reader) noexcept
        : _lock{std::move(lock)}, _reader{std::move(reader)} {}
    Snapshot(Snapshot&& rhs) noexcept
        : _lock{std::move(rhs._lock)}, _reader{std::move(rhs._reader)} {}
    Snapshot& operator=(Snapshot&& rhs) noexcept {
      if (this != &rhs) {
        _lock = std::move(rhs._lock);
        _reader = std::move(rhs._reader);
      }
      return *this;
    }

    [[nodiscard]] auto const& getDirectoryReader() const noexcept {
      return _reader;
    }

   private:
    // lock preventing data store deallocation
    LinkLock _lock;
    irs::directory_reader _reader;
  };

  IResearchDataStore(IndexId iid, LogicalCollection& collection);

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
  static irs::directory_reader reader(LinkLock const& linkLock);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the identifier for this link/index
  //////////////////////////////////////////////////////////////////////////////
  IndexId id() const noexcept { return _id; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return the associated collection
  /// @note arangodb::Index override
  //////////////////////////////////////////////////////////////////////////////
  LogicalCollection& collection() const noexcept { return _collection; }

  static bool hasSelectivityEstimate();  // arangodb::Index override

  bool hasNestedFields() const noexcept { return _hasNestedFields; }

  void afterTruncate(TRI_voc_tick_t tick,
                     transaction::Methods* trx);  // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @brief give derived class chance to fine-tune iresearch storage
  //////////////////////////////////////////////////////////////////////////////
  virtual void afterCommit() {}

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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an ArangoDB document from an iResearch View
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result remove(transaction::Methods& trx, LocalDocumentId documentId,
                bool nested);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert an ArangoDB document into an iResearch View using '_meta'
  /// params
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  template<typename FieldIteratorType, typename MetaType>
  Result insert(transaction::Methods& trx, LocalDocumentId documentId,
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
    irs::utf8_path _path;
    irs::directory_reader _reader;
    irs::index_writer::ptr _writer;
    // the tick at which data store was recovered
    TRI_voc_tick_t _recoveryTick{0};
    // data store is in recovery
    std::atomic_bool _inRecovery{false};
    explicit operator bool() const noexcept { return _directory && _writer; }

    void resetDataStore() noexcept {
      // reset all underlying readers to release file handles
      _reader.reset();
      _writer.reset();
      _directory.reset();
    }
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
  UnsafeOpResult commitUnsafe(
      bool wait, irs::index_writer::progress_report_callback const& progress,
      CommitResult& code);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run segment consolidation on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  UnsafeOpResult consolidateUnsafe(
      IResearchDataStoreMeta::ConsolidationPolicy const& policy,
      irs::merge_writer::flush_progress_t const& progress,
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
  Result commitUnsafeImpl(
      bool wait, irs::index_writer::progress_report_callback const& progress,
      CommitResult& code);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run segment consolidation on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  Result consolidateUnsafeImpl(
      IResearchDataStoreMeta::ConsolidationPolicy const& policy,
      irs::merge_writer::flush_progress_t const& progress,
      bool& emptyConsolidation);

  void initAsyncSelf();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the data store with a new or from an existing directory
  //////////////////////////////////////////////////////////////////////////////
  Result initDataStore(
      bool& pathExists, InitCallback const& initCallback, uint32_t version,
      bool sorted, bool nested,
      std::vector<IResearchViewStoredValues::StoredColumn> const& storedColumns,
      irs::type_info::type_id primarySortCompression);

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

 public:  // TODO(MBkkt) public only for tests, make protected
  // These methods only for tests
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get numbers of failed commit cleanup consolidation
  ////////////////////////////////////////////////////////////////////////////////
  std::tuple<uint64_t, uint64_t, uint64_t> numFailed() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get average time of commit cleanuo consolidation
  ////////////////////////////////////////////////////////////////////////////////
  std::tuple<uint64_t, uint64_t, uint64_t> avgTime() const;

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
  Stats updateStatsUnsafe() const;

  void initClusterMetrics() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief insert metrics to MetricsFeature
  //////////////////////////////////////////////////////////////////////////////
  virtual void insertMetrics() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove metrics from MetricsFeature
  //////////////////////////////////////////////////////////////////////////////
  virtual void removeMetrics() {}

  virtual void invalidateQueryCache(TRI_vocbase_t*) = 0;

  virtual irs::comparer const* getComparator() const noexcept = 0;

  StorageEngine* _engine;

  // the feature where async jobs were registered (nullptr == no jobs
  // registered)
  IResearchFeature* _asyncFeature;

  // 'this' for the lifetime of the link (for use with asynchronous calls)
  AsyncLinkPtr _asyncSelf;

  // the linked collection
  LogicalCollection& _collection;

  // the iresearch data store, protected by _asyncSelf->mutex()
  DataStore _dataStore;

  // data store error state
  std::atomic<DataStoreError> _error;

  std::shared_ptr<FlushSubscription> _flushSubscription;
  std::shared_ptr<MaintenanceState> _maintenanceState;
  IndexId const _id;
  // protected by _commitMutex
  TRI_voc_tick_t _lastCommittedTick;
  size_t _cleanupIntervalCount;

  bool _hasNestedFields{false};

  // prevents data store sequential commits
  std::mutex _commitMutex;

  // for insert(...)/remove(...)
  std::function<void(transaction::Methods& trx, transaction::Status status)>
      _trxCallback;

  metrics::Gauge<uint64_t>* _numFailedCommits;
  metrics::Gauge<uint64_t>* _numFailedCleanups;
  metrics::Gauge<uint64_t>* _numFailedConsolidations;

  std::atomic_uint64_t _commitTimeNum;
  metrics::Gauge<uint64_t>* _avgCommitTimeMs;

  std::atomic_uint64_t _cleanupTimeNum;
  metrics::Gauge<uint64_t>* _avgCleanupTimeMs;

  std::atomic_uint64_t _consolidationTimeNum;
  metrics::Gauge<uint64_t>* _avgConsolidationTimeMs;

  metrics::Guard<Stats>* _metricStats;
};

irs::utf8_path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                IResearchDataStore const& link);

}  // namespace iresearch
}  // namespace arangodb
