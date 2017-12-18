//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_VIEW_H
#define ARANGOD_IRESEARCH__IRESEARCH_VIEW_H 1

#include "IResearchViewMeta.h"

#include "VocBase/LocalDocumentId.h"
#include "VocBase/ViewImplementation.h"
#include "Utils/FlushTransaction.h"

#include "store/directory.hpp"
#include "index/index_writer.hpp"
#include "index/directory_reader.hpp"
#include "utils/async_utils.hpp"

NS_BEGIN(arangodb)

class ViewIterator; // forward declaration

NS_BEGIN(aql)

class Ast; // forward declaration
struct AstNode; // forward declaration
class SortCondition; // forward declaration
struct Variable; // forward declaration
class ExpressionContext; // forward declaration

NS_END // aql

NS_BEGIN(transaction)

class Methods; // forward declaration

NS_END // transaction

NS_END // arangodb

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

struct QueryContext;

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                            Forward declarations
///////////////////////////////////////////////////////////////////////////////

class IResearchLink;
struct IResearchLinkMeta;

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                              utility constructs
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// @brief a cookie/flag denoting validitiy with a read-write mutex to lock
///        scope for the duration of required operations after ensuring validity
///////////////////////////////////////////////////////////////////////////////
struct AsyncValid {
  irs::async_utils::read_write_mutex _mutex; // read-lock to prevent flag modification
  bool _valid; // do not need atomic because need to hold lock anyway
  explicit AsyncValid(bool valid): _valid(valid) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple directory readers
////////////////////////////////////////////////////////////////////////////////
class CompoundReader final : public irs::index_reader {
 public:
  CompoundReader(CompoundReader&& other) noexcept;
  irs::sub_reader const& operator[](size_t subReaderId) const noexcept {
    return *(_subReaders[subReaderId].first);
  }
  void add(irs::directory_reader const& reader);
  virtual reader_iterator begin() const override;
  virtual uint64_t docs_count(const irs::string_ref& field) const override;
  virtual uint64_t docs_count() const override;
  virtual reader_iterator end() const override;
  virtual uint64_t live_docs_count() const override;

  irs::columnstore_reader::values_reader_f const& pkColumn(
      size_t subReaderId
  ) const noexcept {
    return _subReaders[subReaderId].second;
  }

  virtual size_t size() const noexcept override {
    return _subReaders.size();
  }

 private:
  CompoundReader(irs::async_utils::read_write_mutex& mutex);

  friend class IResearchView;

  typedef std::vector<
    std::pair<irs::sub_reader*, irs::columnstore_reader::values_reader_f>
  > SubReadersType;

  class IteratorImpl final : public irs::index_reader::reader_iterator_impl {
   public:
    explicit IteratorImpl(SubReadersType::const_iterator const& itr)
      : _itr(itr) {
    }

    virtual void operator++() noexcept override {
      ++_itr;
    }
    virtual reference operator*() noexcept override {
      return *(_itr->first);
    }
    virtual const_reference operator*() const noexcept override {
      return *(_itr->first);
    }
    virtual bool operator==(const reader_iterator_impl& other) noexcept override {
      return static_cast<IteratorImpl const&>(other)._itr == _itr;
    }

   private:
    SubReadersType::const_iterator _itr;
  };

  // order is important
  ReadMutex _mutex;
  std::unique_lock<ReadMutex> _lock;
  std::vector<irs::directory_reader> _readers;
  SubReadersType _subReaders;
};

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                                   IResearchView
///////////////////////////////////////////////////////////////////////////////

// Note, that currenly ArangoDB uses only 1 FlushThread for flushing the views
// In case if number of threads will be increased each thread has to receive
// it's own FlushTransaction object

///////////////////////////////////////////////////////////////////////////////
/// @brief an abstraction over the IResearch index implementing the
///        ViewImplementation interface
/// @note the responsibility of the IResearchView API is to only manage the
///       IResearch data store, i.e. insert/remove/query
///       the IResearchView API does not manage which and how the data gets
///       populated into and removed from the datatstore
///       therefore the API provides generic insert/remvoe/drop/query functions
///       which may be, but are not explicitly required to be, triggered via
///       the IResearchLink or IResearchViewBlock
///////////////////////////////////////////////////////////////////////////////
class IResearchView final: public arangodb::ViewImplementation,
                           public arangodb::FlushTransaction {
 public:
  typedef std::unique_ptr<arangodb::ViewImplementation> ptr;
  typedef std::shared_ptr<IResearchView> sptr;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief destructor to clean up resources
  ///////////////////////////////////////////////////////////////////////////////
  virtual ~IResearchView();

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief append all know collection IDs to the set (from meta and from store)
  /// @return success
  ///////////////////////////////////////////////////////////////////////////////
  bool appendKnownCollections(std::unordered_set<TRI_voc_cid_t>& set) const;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief drop this iResearch View
  ///////////////////////////////////////////////////////////////////////////////
  void drop() override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove all documents matching collection 'cid' from this IResearch
  ///        View and the underlying IResearch stores
  ////////////////////////////////////////////////////////////////////////////////
  int drop(TRI_voc_cid_t cid);

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchView object
  ///        only fields describing the view itself, not 'link' descriptions
  ////////////////////////////////////////////////////////////////////////////////
  void getPropertiesVPack(
    arangodb::velocypack::Builder& builder,
    bool forPersistence
  ) const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the id identifying the current iResearch View or '0' if unknown
  ////////////////////////////////////////////////////////////////////////////////
  TRI_voc_cid_t id() const noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert a document into this IResearch View and the underlying
  ///        IResearch stores
  ///        to be done in the scope of transaction 'tid' and 'meta'
  ////////////////////////////////////////////////////////////////////////////////
  int insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    arangodb::LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    IResearchLinkMeta const& meta
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert a batch of documents into the IResearch View and the
  ///        underlying IResearch stores
  ///        to be done in the scope of transaction 'tid' and 'meta'
  ///        'Itrator.first' == TRI_voc_rid_t
  ///        'Itrator.second' == arangodb::velocypack::Slice
  ///        terminate on first failure
  ////////////////////////////////////////////////////////////////////////////////
  int insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& batch,
    IResearchLinkMeta const& meta
  );

  CompoundReader snapshot(transaction::Methods& trx);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief count of known links registered with this view
  ////////////////////////////////////////////////////////////////////////////////
  size_t linkCount() const noexcept;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief register an iResearch Link with the specified view
  ///        on success this call will set the '_view' pointer in the link
  /// @return new registration was successful
  ///////////////////////////////////////////////////////////////////////////////
  bool linkRegister(IResearchLink& link);

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief view factory
  /// @returns initialized view object
  ///////////////////////////////////////////////////////////////////////////////
  static ptr make(
    arangodb::LogicalView* view,
    arangodb::velocypack::Slice const& info,
    bool isNew
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const;
  ///////////////////////////////////////////////////////////////////////////////
  /// @brief opens an existing view when the server is restarted
  ///////////////////////////////////////////////////////////////////////////////
  void open() override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove documents matching 'cid' and 'rid' from the IResearch View
  ///        and the underlying IResearch stores
  ///        to be done in the scope of transaction 'tid'
  ////////////////////////////////////////////////////////////////////////////////
  int remove(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    arangodb::LocalDocumentId const& documentId
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief wait for a flush of all index data to its respective stores
  /// @param maxMsec try not to exceed the specified time, casues partial sync
  ///                0 == full sync
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  bool sync(size_t maxMsec = 0);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the view type as used when selecting which view to instantiate
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const& type() noexcept;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief called when a view's properties are updated (i.e. delta-modified)
  ///////////////////////////////////////////////////////////////////////////////
  arangodb::Result updateProperties(
    arangodb::velocypack::Slice const& slice,
    bool partialUpdate,
    bool doSync
  ) override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief persist the specified WAL file into permanent storage
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result commit() override;

 private:
  struct DataStore {
    irs::directory::ptr _directory;
    irs::directory_reader _reader;
    irs::index_writer::ptr _writer;
    DataStore() = default;
    DataStore(DataStore&& other) noexcept;
    DataStore& operator=(DataStore&& other) noexcept;
    operator bool() const noexcept {
      return _directory && _writer;
    }
    void sync();
  };

  struct MemoryStore: public DataStore {
    MemoryStore(); // initialize _directory and _writer during allocation
  };

  struct SyncState {
    struct PolicyState {
      size_t _intervalCount;
      size_t _intervalStep;

      std::shared_ptr<irs::index_writer::consolidation_policy_t> _policy;
      PolicyState(
        size_t intervalStep,
        const std::shared_ptr<irs::index_writer::consolidation_policy_t>& policy
      );
    };

    size_t _cleanupIntervalCount;
    size_t _cleanupIntervalStep;
    std::vector<PolicyState> _consolidationPolicies;

    SyncState() noexcept;
    explicit SyncState(IResearchViewMeta::CommitMeta const& meta);
  };

  struct TidStore {
    TidStore(
      transaction::Methods& trx,
      std::function<void(transaction::Methods*)> const& trxCallback
    );

    mutable std::mutex _mutex; // for use with '_removals' (allow use in const functions)
    std::vector<std::shared_ptr<irs::filter>> _removals; // removal filters to be applied to during merge
    MemoryStore _store;
  };

  struct FlushCallbackUnregisterer {
    void operator()(IResearchView* view) const noexcept;
  };

  struct MemoryStoreNode {
    MemoryStore _store;
    MemoryStoreNode* _next; // pointer to the next MemoryStore
    std::mutex _readMutex; // for use with obtaining _reader FIXME TODO find a better way
    std::mutex _reopenMutex; // for use with _reader.reopen() FIXME TODO find a better way
  };

  typedef std::unordered_map<TRI_voc_tid_t, TidStore> MemoryStoreByTid;
  typedef std::unique_ptr<IResearchView, FlushCallbackUnregisterer> FlushCallback;
  typedef std::unique_ptr<
    arangodb::FlushTransaction, std::function<void(arangodb::FlushTransaction*)>
  > FlushTransactionPtr;

  IResearchView(
    arangodb::LogicalView*,
    arangodb::velocypack::Slice const& info
  );

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief run cleaners on data directories to remove unused files
  /// @param maxMsec try not to exceed the specified time, casues partial cleanup
  ///                0 == full cleanup
  /// @return success
  ///////////////////////////////////////////////////////////////////////////////
  bool cleanup(size_t maxMsec = 0);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Called in post-recovery to remove any dangling documents old links
  //////////////////////////////////////////////////////////////////////////////
  void verifyKnownCollections();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief process a finished transaction and release resources held by it
  ////////////////////////////////////////////////////////////////////////////////
  int finish(TRI_voc_tid_t tid, bool commit);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief wait for a flush of all index data to its respective stores
  /// @param meta configuraton to use for sync
  /// @param maxMsec try not to exceed the specified time, casues partial sync
  ///                0 == full sync
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  bool sync(SyncState& state, size_t maxMsec = 0);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief registers a callback for flush feature
  ////////////////////////////////////////////////////////////////////////////////
  void registerFlushCallback();

  MemoryStore& activeMemoryStore() const;

  std::condition_variable _asyncCondition; // trigger reload of timeout settings for async jobs
  std::atomic<size_t> _asyncMetaRevision; // arbitrary meta modification id, async jobs should reload if different
  std::mutex _asyncMutex; // mutex used with '_asyncCondition' and associated timeouts
  std::mutex _trxStoreMutex; // mutex used to protect '_storeByTid' against multiple insertions
  std::atomic<bool> _asyncTerminate; // trigger termination of long-running async jobs
  IResearchViewMeta _meta;
  mutable irs::async_utils::read_write_mutex _mutex; // for use with member maps/sets and '_meta'
  MemoryStoreNode _memoryNodes[2]; // 2 because we just swap them
  MemoryStoreNode* _memoryNode; // points to the current memory store
  std::unordered_map<TRI_voc_cid_t, IResearchLink*> _registeredLinks; // links that have been registered with this view (used for duplicate registration detection)
  MemoryStoreNode* _toFlush; // points to memory store to be flushed
  MemoryStoreByTid _storeByTid;
  DataStore _storePersisted;
  FlushCallback _flushCallback; // responsible for flush callback unregistration
  irs::async_utils::thread_pool _threadPool;
  std::function<void(transaction::Methods* trx)> _transactionCallback;
  std::atomic<bool> _inRecovery;
  std::shared_ptr<AsyncValid> _valid; // true for the lifetime of the view (for use with asynchronous callbacks)
};

NS_END // iresearch
NS_END // arangodb
#endif