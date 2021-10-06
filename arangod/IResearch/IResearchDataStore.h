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
template<typename T> class TypedResourceMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief IResarchLink handle to use with asynchronous tasks
////////////////////////////////////////////////////////////////////////////////
class AsyncLinkHandle {
 public:
  explicit AsyncLinkHandle(IResearchDataStore* link);
  ~AsyncLinkHandle();
  IResearchDataStore* get() noexcept { return _link.get(); }
  bool empty() const { return _link.empty(); }
  std::unique_lock<ReadMutex> lock() { return _link.lock(); }
  std::unique_lock<ReadMutex> try_lock() noexcept { return _link.try_lock(); }
  bool terminationRequested() const noexcept { return _asyncTerminate.load(); }

 private:
  AsyncLinkHandle(AsyncLinkHandle const&) = delete;
  AsyncLinkHandle(AsyncLinkHandle&&) = delete;
  AsyncLinkHandle& operator=(AsyncLinkHandle const&) = delete;
  AsyncLinkHandle& operator=(AsyncLinkHandle&&) = delete;

  friend class IResearchDataStore;
  friend class IResearchLink;

  void reset();

  ResourceMutexT<IResearchDataStore> _link;
  std::atomic<bool> _asyncTerminate{false}; // trigger termination of long-running async jobs
}; // AsyncLinkHandle


////////////////////////////////////////////////////////////////////////////////
/// @brief container storing the link state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct IResearchTrxState final : public TransactionState::Cookie {
  irs::index_writer::documents_context _ctx;
  std::unique_lock<ReadMutex> _linkLock; // prevent data-store deallocation (lock @ AsyncSelf)
  PrimaryKeyFilterContainer _removals;  // list of document removals

  IResearchTrxState(std::unique_lock<ReadMutex>&& linkLock,
               irs::index_writer& writer) noexcept
      : _ctx(writer.documents()), _linkLock(std::move(linkLock)) {
    TRI_ASSERT(_linkLock.owns_lock());
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

  class Snapshot {
   public:
    Snapshot() = default;
    Snapshot(std::unique_lock<ReadMutex>&& lock,
              irs::directory_reader&& reader) noexcept
        : _lock(std::move(lock)), _reader(std::move(reader)) {
      TRI_ASSERT(_lock.owns_lock());
    }
    Snapshot(Snapshot&& rhs) noexcept
      : _lock(std::move(rhs._lock)),
        _reader(std::move(rhs._reader)) {
      TRI_ASSERT(_lock.owns_lock());
    }
    Snapshot& operator=(Snapshot&& rhs) noexcept {
      if (this != &rhs) {
        _lock = std::move(rhs._lock);
        _reader = std::move(rhs._reader);
      }
      TRI_ASSERT(_lock.owns_lock());
      return *this;
    }
    operator irs::directory_reader const&() const noexcept {
      return _reader;
    }

   private:
    std::unique_lock<ReadMutex> _lock; // lock preventing data store dealocation
    irs::directory_reader _reader;
  };

  IResearchDataStore(IndexId iid, LogicalCollection& collection);
  //IResearchDataStore(IndexId iid, LogicalCollection& collection, IResearchLinkMeta&& meta);
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
  /// @brief the identifier for this link
  //////////////////////////////////////////////////////////////////////////////
  IndexId id() const noexcept { return _id; }

  //////////////////////////////////////////////////////////////////////////////
  /// @return the associated collection
  /// @note arangodb::Index override
  //////////////////////////////////////////////////////////////////////////////
  LogicalCollection& collection() const noexcept {
    return _collection;
  }

  
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
    size_t docsCount{};       // total number of documents
    size_t liveDocsCount{};   // number of live documents
    size_t numBufferedDocs{}; // number of buffered docs
    size_t indexSize{};       // size of the index in bytes
    size_t numSegments{};     // number of segments
    size_t numFiles{};        // number of files
  };

  
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get index stats for current snapshot
  ////////////////////////////////////////////////////////////////////////////////
  Stats stats() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying iresearch data store
  //////////////////////////////////////////////////////////////////////////////
  struct DataStore {
    arangodb::iresearch::IResearchViewMeta _meta; // runtime meta for a data store (not persisted) // FIXME: move to simpler struct?
    irs::directory::ptr _directory;
    basics::ReadWriteLock _mutex; // for use with member '_meta'
    irs::utf8_path _path;
    irs::directory_reader _reader;
    irs::index_writer::ptr _writer;
    TRI_voc_tick_t _recoveryTick{ 0 }; // the tick at which data store was recovered
    std::atomic<bool> _inRecovery{ false }; // data store is in recovery
    operator bool() const noexcept { return _directory && _writer; }

    void resetDataStore() noexcept { // reset all underlying readers to release file handles 
      _reader.reset(); 
      _writer.reset();
      _directory.reset();
    }
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
  /// @brief schedule a commit job
  //////////////////////////////////////////////////////////////////////////////
  void scheduleCommit(std::chrono::milliseconds delay);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief schedule a consolidation job
  //////////////////////////////////////////////////////////////////////////////
  void scheduleConsolidation(std::chrono::milliseconds delay);

  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief run filesystem cleanup on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  Result cleanupUnsafe();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark the current data store state as the latest valid state
  /// @param wait even if other thread is committing
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  Result commitUnsafe(bool wait, CommitResult* code);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run segment consolidation on the data store
  /// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
  //////////////////////////////////////////////////////////////////////////////
  Result consolidateUnsafe(
    IResearchViewMeta::ConsolidationPolicy const& policy,
    irs::merge_writer::flush_progress_t const& progress,
    bool& emptyConsolidation);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the data store with a new or from an existing directory
  //////////////////////////////////////////////////////////////////////////////
  Result initDataStore(
    InitCallback const& initCallback,
    uint32_t version,
    bool sorted,
    std::vector<IResearchViewStoredValues::StoredColumn> const& storedColumns,
    irs::type_info::type_id primarySortCompression);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wait for all outstanding commit/consolidate operations and closes data store
  //////////////////////////////////////////////////////////////////////////////
  Result shutdownDataStore();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wait for all outstanding commit/consolidate operations and remove data store
  //////////////////////////////////////////////////////////////////////////////
  Result deleteDataStore();


  LogicalCollection& _collection; // the linked collection
  IndexId const _id;                 // the index identifier
  StorageEngine* _engine;
  std::shared_ptr<FlushSubscription> _flushSubscription;
  std::shared_ptr<MaintenanceState> _maintenanceState;
  IResearchFeature* _asyncFeature; // the feature where async jobs were registered (nullptr == no jobs registered)
  AsyncLinkPtr _asyncSelf; // 'this' for the lifetime of the link (for use with asynchronous calls)
  DataStore _dataStore; // the iresearch data store, protected by _asyncSelf->mutex()
  TRI_voc_tick_t _lastCommittedTick; // protected by _commitMutex
  std::mutex _commitMutex; // prevents data store sequential commits
  std::function<void(transaction::Methods& trx, transaction::Status status)> _trxCallback; // for insert(...)/remove(...)
  VPackComparer _comparer;
};

irs::utf8_path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                IResearchDataStore const& link);
} // namespace iresearch
} // namespace arangodb
