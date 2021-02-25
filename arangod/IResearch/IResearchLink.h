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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_LINK_H
#define ARANGOD_IRESEARCH__IRESEARCH_LINK_H 1

#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/directory.hpp"
#include "utils/utf8_path.hpp"

#include "Indexes/Index.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchViewMeta.h"
#include "RestServer/DatabasePathFeature.h"
#include "Transaction/Status.h"
#include "Utils/OperationOptions.h"
#include "VocBase/Identifiers/IndexId.h"

namespace arangodb {

struct FlushSubscription;

namespace iresearch {

struct MaintenanceState;
class IResearchFeature;
class IResearchView;
class IResearchLink;
template<typename T> class TypedResourceMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief IResarchLink handle to use with asynchronous tasks
////////////////////////////////////////////////////////////////////////////////
class AsyncLinkHandle {
 public:
  explicit AsyncLinkHandle(IResearchLink* link);
  ~AsyncLinkHandle();
  IResearchLink* get() noexcept { return _link.get(); }
  bool empty() const { return _link.empty(); }
  std::unique_lock<ReadMutex> lock() { return _link.lock(); }
  std::unique_lock<ReadMutex> try_lock() noexcept { return _link.try_lock(); }
  bool terminationRequested() const noexcept { return _asyncTerminate.load(); }

 private:
  AsyncLinkHandle(AsyncLinkHandle const&) = delete;
  AsyncLinkHandle(AsyncLinkHandle&&) = delete;
  AsyncLinkHandle& operator=(AsyncLinkHandle const&) = delete;
  AsyncLinkHandle& operator=(AsyncLinkHandle&&) = delete;

  friend class IResearchLink;
  void reset();

  ResourceMutexT<IResearchLink> _link;
  std::atomic<bool> _asyncTerminate{false}; // trigger termination of long-running async jobs
}; // AsyncLinkHandle

////////////////////////////////////////////////////////////////////////////////
/// @brief common base class for functionality required to link an ArangoDB
///        LogicalCollection with an IResearchView
////////////////////////////////////////////////////////////////////////////////
class IResearchLink {
 public:
  using AsyncLinkPtr = std::shared_ptr<AsyncLinkHandle>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a snapshot representation of the data-store
  ///        locked to prevent data store deallocation
  //////////////////////////////////////////////////////////////////////////////
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

  virtual ~IResearchLink();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this IResearch Link reference the supplied view
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(LogicalView const& view) const noexcept;
  bool operator!=(LogicalView const& view) const noexcept {
    return !(*this == view);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this iResearch Link match the meta definition
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(IResearchLinkMeta const& meta) const noexcept;
  bool operator!=(IResearchLinkMeta const& meta) const noexcept {
    return !(*this == meta);
  }

  void afterTruncate(TRI_voc_tick_t tick,
                     transaction::Methods* trx); // arangodb::Index override

  bool canBeDropped() const {
    // valid for a link to be dropped from an ArangoSearch view
    return true;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @return the associated collection
  /// @note arangodb::Index override
  //////////////////////////////////////////////////////////////////////////////
  LogicalCollection& collection() const noexcept {
    return _collection;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mark the current data store state as the latest valid state
  /// @param wait even if other thread is committing
  //////////////////////////////////////////////////////////////////////////////
  Result commit(bool wait = true);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is dropped
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result drop();

  bool hasSelectivityEstimate() const; // arangodb::Index override

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the identifier for this link
  //////////////////////////////////////////////////////////////////////////////
  IndexId id() const noexcept { return _id; }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert an ArangoDB document into an iResearch View using '_meta' params
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result insert(transaction::Methods& trx,
                LocalDocumentId const& documentId,
                velocypack::Slice const doc);

  bool isHidden() const;  // arangodb::Index override
  bool isSorted() const; // arangodb::Index override

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is loaded into memory
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  void load();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief index comparator, used by the coordinator to detect if the specified
  ///        definition is the same as this link
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  bool matchesDefinition(velocypack::Slice const& slice) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  void toVelocyPackStats(VPackBuilder& builder) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a jSON description of a IResearchLink object
  ///        elements are appended to an existing object
  //////////////////////////////////////////////////////////////////////////////
  Result properties(velocypack::Builder& builder, bool forPersistence) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update runtine data processing properties (not persisted)
  /// @return success
  //////////////////////////////////////////////////////////////////////////////
  Result properties(IResearchViewMeta const& meta);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an ArangoDB document from an iResearch View
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result remove(transaction::Methods& trx,
                LocalDocumentId const& documentId,
                velocypack::Slice const doc);

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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief ArangoSearch Link index type enum value
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Index::IndexType type() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief ArangoSearch Link index type string value
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  char const* typeName() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is unloaded from memory
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result unload();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief lookup referenced analyzer
  ////////////////////////////////////////////////////////////////////////////////
  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const;

  typedef std::function<void(irs::directory&)> InitCallback;
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition used in make(...)
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  Result init(velocypack::Slice const& definition,
              InitCallback const& initCallback = {});
              
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get stored values
  ////////////////////////////////////////////////////////////////////////////////
  IResearchViewStoredValues const& storedValues() const noexcept;

  /// @brief sets the _collectionName in Link meta. Used in cluster only to store
  /// linked collection name (as shard name differs from the cluster-wide collection name)
  /// @param name  collectioName to set. Should match existing value of  the _collectionName
  /// if it is not empty. 
  /// @return true if name not existed in link before and was actually set by this call,
  /// false otherwise
  bool setCollectionName(irs::string_ref name) noexcept;

 protected:
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

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...)
  /// after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLink(IndexId iid, LogicalCollection& collection);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief link was created during recovery
  ////////////////////////////////////////////////////////////////////////////////
  bool createdInRecovery() const noexcept { return _createdInRecovery; }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get index stats for current snapshot
  ////////////////////////////////////////////////////////////////////////////////
  Stats stats() const;

 private:
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
  }; // CommitResult

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying iresearch data store
  //////////////////////////////////////////////////////////////////////////////
  struct DataStore {
    IResearchViewMeta _meta; // runtime meta for a data store (not persisted)
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
    InitCallback const& initCallback, bool sorted,
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

  StorageEngine* _engine;
  VPackComparer _comparer;
  IResearchFeature* _asyncFeature; // the feature where async jobs were registered (nullptr == no jobs registered)
  AsyncLinkPtr _asyncSelf; // 'this' for the lifetime of the link (for use with asynchronous calls)
  LogicalCollection& _collection; // the linked collection
  DataStore _dataStore; // the iresearch data store, protected by _asyncSelf->mutex()
  std::shared_ptr<FlushSubscription> _flushSubscription;
  std::shared_ptr<MaintenanceState> _maintenanceState;
  IndexId const _id;                 // the index identifier
  TRI_voc_tick_t _lastCommittedTick; // protected by _commitMutex
  IResearchLinkMeta const _meta; // how this collection should be indexed (read-only, set via init())
  std::mutex _commitMutex; // prevents data store sequential commits
  std::function<void(transaction::Methods& trx, transaction::Status status)> _trxCallback; // for insert(...)/remove(...)
  std::string const _viewGuid; // the identifier of the desired view (read-only, set via init())
  bool _createdInRecovery; // link was created based on recovery marker
};  // IResearchLink

irs::utf8_path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                iresearch::IResearchLink const& link);

}  // namespace iresearch
}  // namespace arangodb

#endif
