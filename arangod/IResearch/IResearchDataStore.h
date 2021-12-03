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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IResearchViewMeta.h"
#include "Containers.h"
#include "IResearchCommon.h"
#include "IResearchPrimaryKeyFilter.h"
#include "IResearchVPackComparer.h"
#include "Indexes/Index.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/TransactionState.h"

#include "store/directory_attributes.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/directory.hpp"
#include "utils/utf8_path.hpp"

namespace arangodb {

struct FlushSubscription;

namespace iresearch {

struct MaintenanceState;
class IResearchFeature;
class IResearchView;
class IResearchDataStore;
class IResearchLink;

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchDataStore handle to use with asynchronous tasks
////////////////////////////////////////////////////////////////////////////////
class AsyncLinkHandle final {
 public:
  explicit AsyncLinkHandle(IResearchDataStore* link);
  ~AsyncLinkHandle();
  bool empty() const noexcept { return _link.empty(); }
  auto lock() noexcept { return _link.lock(); }
  auto tryLock() noexcept { return _link.try_lock(); }
  bool terminationRequested() const noexcept {
    return _asyncTerminate.load(std::memory_order_acquire);
  }

  AsyncLinkHandle(AsyncLinkHandle const&) = delete;
  AsyncLinkHandle(AsyncLinkHandle&&) = delete;
  AsyncLinkHandle& operator=(AsyncLinkHandle const&) = delete;
  AsyncLinkHandle& operator=(AsyncLinkHandle&&) = delete;

 private:
  friend class IResearchLink;
  friend class IResearchDataStore;

  void reset();

  AsyncValue<IResearchDataStore> _link;
  std::atomic_bool _asyncTerminate{false};  // trigger termination of long-running async jobs
};


////////////////////////////////////////////////////////////////////////////////
/// @brief container storing the index state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct IResearchTrxState final : public TransactionState::Cookie {
  irs::index_writer::documents_context _ctx;
  AsyncValue<IResearchDataStore>::Value _linkLock; // prevent data-store deallocation (lock @ AsyncSelf)
  PrimaryKeyFilterContainer _removals;  // list of document removals

  IResearchTrxState(AsyncValue<IResearchDataStore>::Value&& linkLock,
               irs::index_writer& writer) noexcept
      : _ctx(writer.documents()), _linkLock(std::move(linkLock)) {
    TRI_ASSERT(_linkLock.ownsLock());
  }

  virtual ~IResearchTrxState() noexcept {
    if (_removals.empty()) {
      return;  // nothing to do
    }

    try {
      // hold references even after transaction
      auto filter = std::make_unique<PrimaryKeyFilterContainer>(std::move(_removals));
      _ctx.remove(std::unique_ptr<irs::filter>(std::move(filter)));
    } catch (std::exception const& e) {
      LOG_TOPIC("eb463", ERR, arangodb::iresearch::TOPIC)
          << "caught exception while applying accumulated removals: " << e.what();
    } catch (...) {
      LOG_TOPIC("14917", WARN, arangodb::iresearch::TOPIC)
          << "caught exception while applying accumulated removals";
    }
  }

  operator irs::index_writer::documents_context&() noexcept { return _ctx; }

  void remove(StorageEngine& engine, LocalDocumentId const& value) {
    _ctx.remove(_removals.emplace(engine, value));
  }

  void reset() noexcept {
    _removals.clear();
    _ctx.reset();
  }
};

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
    Snapshot(AsyncValue<IResearchDataStore>::Value&& lock, irs::directory_reader&& reader) noexcept
        : _lock{std::move(lock)}, _reader{std::move(reader)} {
      TRI_ASSERT(_lock.ownsLock());
    }
    Snapshot(Snapshot&& rhs) noexcept
        : _lock{std::move(rhs._lock)}, _reader{std::move(rhs._reader)} {
      TRI_ASSERT(_lock.ownsLock());
    }
    Snapshot& operator=(Snapshot&& rhs) noexcept {
      if (this != &rhs) {
        _lock = std::move(rhs._lock);
        _reader = std::move(rhs._reader);
      }
      TRI_ASSERT(_lock.ownsLock());
      return *this;
    }

    irs::directory_reader const& getDirectoryReader() const noexcept {
      return _reader;
    }

   private:
    AsyncValue<IResearchDataStore>::Value _lock;  // lock preventing data store deallocation
    irs::directory_reader _reader;
  };

  IResearchDataStore(IndexId iid, LogicalCollection& collection);

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief 'this' for the lifetime of the link data-store
  ///        for use with asynchronous calls, e.g. callbacks, view
  ///////////////////////////////////////////////////////////////////////////////
  AsyncLinkPtr self() const { return _asyncSelf; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return pointer to an index reader containing the data store current
  ///         record snapshot
  ///         (nullptr == no data store snapshot availabe, e.g. error)
  //////////////////////////////////////////////////////////////////////////////
  Snapshot snapshot() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the identifier for this link/index
  //////////////////////////////////////////////////////////////////////////////
  IndexId id() const noexcept { return _id; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return the associated collection
  /// @note arangodb::Index override
  //////////////////////////////////////////////////////////////////////////////
  LogicalCollection& collection() const noexcept {
    return _collection;
  }

  static bool hasSelectivityEstimate();  // arangodb::Index override

  
  void afterTruncate(TRI_voc_tick_t tick,
                     transaction::Methods* trx); // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @brief give derived class chance to fine-tune iresearch storage
  //////////////////////////////////////////////////////////////////////////////
  virtual void afterCommit() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark the current data store state as the latest valid state
  /// @param wait even if other thread is committing
  //////////////////////////////////////////////////////////////////////////////
  Result commit(bool wait = true);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  void toVelocyPackStats(VPackBuilder& builder) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief lookup referenced analyzer
  ////////////////////////////////////////////////////////////////////////////////
  virtual AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const = 0;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an ArangoDB document from an iResearch View
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result remove(transaction::Methods& trx,
                LocalDocumentId const& documentId,
                velocypack::Slice const doc);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert an ArangoDB document into an iResearch View using '_meta' params
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  template<typename FieldIteratorType, typename MetaType>
  Result insert(transaction::Methods& trx,
                LocalDocumentId const& documentId,
                velocypack::Slice const doc,
                MetaType const& meta);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update runtine data processing properties
  /// @return success
  //////////////////////////////////////////////////////////////////////////////
  Result properties(IResearchViewMeta const& meta);
 
 protected:
  friend struct CommitTask;
  friend struct ConsolidationTask;


  //////////////////////////////////////////////////////////////////////////////
  /// @brief index stats
  //////////////////////////////////////////////////////////////////////////////
  struct Stats {
    uint64_t numBufferedDocs{};
    uint64_t numDocs{};
    uint64_t numLiveDocs{};
    uint64_t numSegments{};
    uint64_t numFiles{};
    uint64_t indexSize{};
  };

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
  }; // CommitResult

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying iresearch data store
  //////////////////////////////////////////////////////////////////////////////
  struct DataStore {
    IResearchViewMeta _meta;  // runtime meta for a data store (not persisted)
    irs::directory::ptr _directory;
    basics::ReadWriteLock _mutex;  // for use with member '_meta'
    irs::utf8_path _path;
    irs::directory_reader _reader;
    irs::index_writer::ptr _writer;
    TRI_voc_tick_t _recoveryTick{0};  // the tick at which data store was recovered
    std::atomic_bool _inRecovery{false};  // data store is in recovery
    explicit operator bool() const noexcept { return _directory && _writer; }

    void resetDataStore() noexcept {  // reset all underlying readers to release file handles
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
  UnsafeOpResult commitUnsafe(bool wait, CommitResult* code);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run segment consolidation on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  UnsafeOpResult consolidateUnsafe(IResearchViewMeta::ConsolidationPolicy const& policy,
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
  Result commitUnsafeImpl(bool wait, CommitResult* code);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run segment consolidation on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  Result consolidateUnsafeImpl(IResearchViewMeta::ConsolidationPolicy const& policy,
                               irs::merge_writer::flush_progress_t const& progress,
                               bool& emptyConsolidation);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the data store with a new or from an existing directory
  //////////////////////////////////////////////////////////////////////////////
  Result initDataStore(InitCallback const& initCallback, uint32_t version, bool sorted,
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
  /// @brief wait for all outstanding commit/consolidate operations and closes data store
  //////////////////////////////////////////////////////////////////////////////
  Result shutdownDataStore();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wait for all outstanding commit/consolidate operations and remove data store
  //////////////////////////////////////////////////////////////////////////////
  Result deleteDataStore();

 public:  // TODO public only for tests, make protected
  
  // TODO: Generalize for Link/Index
  struct LinkStats : Stats {
    void needName() const;
    void toPrometheus(std::string& result, std::string_view globals,
                      std::string_view labels) const;

   private:
    mutable bool _needName{false};
  };

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

  Stats statsSynced() const;
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get index stats for current snapshot
  /// @note Unsafe, can only be called is _asyncSelf is locked
  ////////////////////////////////////////////////////////////////////////////////
  Stats statsUnsafe() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief insert statistic to MetricsFeature
  //////////////////////////////////////////////////////////////////////////////
  virtual void insertStats() {};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove statistic from MetricsFeature
  //////////////////////////////////////////////////////////////////////////////
  virtual void removeStats() {};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update store statistics in MetricsFeature
  //////////////////////////////////////////////////////////////////////////////
  virtual void updateStats(Stats const&) {};

  virtual void invalidateQueryCache(TRI_vocbase_t*) = 0;

  StorageEngine* _engine;
  VPackComparer _comparer;
  IResearchFeature* _asyncFeature;  // the feature where async jobs were registered (nullptr == no jobs registered)
  AsyncLinkPtr _asyncSelf;  // 'this' for the lifetime of the link (for use with asynchronous calls)
  LogicalCollection& _collection;  // the linked collection
  DataStore _dataStore;  // the iresearch data store, protected by _asyncSelf->mutex()
  std::shared_ptr<FlushSubscription> _flushSubscription;
  std::shared_ptr<MaintenanceState> _maintenanceState;
  IndexId const _id;                  // the index identifier
  TRI_voc_tick_t _lastCommittedTick;  // protected by _commitMutex
  size_t _cleanupIntervalCount;
  std::mutex _commitMutex;  // prevents data store sequential commits
  std::function<void(transaction::Methods& trx, transaction::Status status)> _trxCallback;  // for insert(...)/remove(...)
  
  Gauge<uint64_t>* _numFailedCommits;
  Gauge<uint64_t>* _numFailedCleanups;
  Gauge<uint64_t>* _numFailedConsolidations;

  std::atomic_uint64_t _commitTimeNum;
  Gauge<uint64_t>* _avgCommitTimeMs;

  std::atomic_uint64_t _cleanupTimeNum;
  Gauge<uint64_t>* _avgCleanupTimeMs;

  std::atomic_uint64_t _consolidationTimeNum;
  Gauge<uint64_t>* _avgConsolidationTimeMs;
};

irs::utf8_path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                IResearchDataStore const& link);
} // namespace iresearch
} // namespace arangodb
