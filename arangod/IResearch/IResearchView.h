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

#include "Containers.h"
#include "IResearchViewMeta.h"

#include "VocBase/LocalDocumentId.h"
#include "VocBase/ViewImplementation.h"
#include "Utils/FlushTransaction.h"

#include "store/directory.hpp"
#include "index/index_writer.hpp"
#include "index/directory_reader.hpp"
#include "utils/async_utils.hpp"

NS_BEGIN(arangodb)

class TransactionState; // forward declaration
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

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                            Forward declarations
///////////////////////////////////////////////////////////////////////////////

struct IResearchLinkMeta;

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                              utility constructs
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation with a cached primary-key reader lambda
////////////////////////////////////////////////////////////////////////////////
class PrimaryKeyIndexReader: public irs::index_reader {
 public:
  virtual irs::sub_reader const& operator[](
    size_t subReaderId
  ) const noexcept = 0;
  virtual irs::columnstore_reader::values_reader_f const& pkColumn(
    size_t subReaderId
  ) const noexcept = 0;
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
  /// @brief AsyncValue holding the view itself, modifiable by IResearchView
  ///////////////////////////////////////////////////////////////////////////////
  class AsyncSelf: public AsyncValue<IResearchView*> {
    friend IResearchView;
   public:
    DECLARE_SPTR(AsyncSelf);
    explicit AsyncSelf(IResearchView* value): AsyncValue(value) {}
  };

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief destructor to clean up resources
  ///////////////////////////////////////////////////////////////////////////////
  virtual ~IResearchView();

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief apply any changes to 'state' required by this view
  ///////////////////////////////////////////////////////////////////////////////
  void apply(arangodb::TransactionState& state);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief persist the specified WAL file into permanent storage
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result commit() override;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief drop this iResearch View
  ///////////////////////////////////////////////////////////////////////////////
  void drop() override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove all documents matching collection 'cid' from this IResearch
  ///        View and the underlying IResearch stores
  ///        also remove 'cid' from the runtime list of tracked collection IDs
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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief link the specified 'cid' to the view using the specified 'link'
  ///        definition (!link.isObject() == remove only)
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result link(
    TRI_voc_cid_t cid,
    arangodb::velocypack::Slice const link
  );

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

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief 'this' for the lifetime of the view
  ///        for use with asynchronous calls, e.g. callbacks, links
  ///////////////////////////////////////////////////////////////////////////////
  AsyncSelf::ptr self() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @return pointer to an index reader containing the datastore record snapshot
  ///         associated with 'state'
  ///         (nullptr == no view snapshot associated with the specified state)
  ///         if force == true && no snapshot -> associate current snapshot
  ////////////////////////////////////////////////////////////////////////////////
  PrimaryKeyIndexReader* snapshot(TransactionState& state, bool force = false);

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
  /// @brief update the view properties via the LogicalView allowing for tracking
  ///        update via WAL entries
  ///////////////////////////////////////////////////////////////////////////////
  arangodb::Result updateLogicalProperties(
    arangodb::velocypack::Slice const& slice,
    bool partialUpdate,
    bool doSync
  );

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief called when a view's properties are updated (i.e. delta-modified)
  ///////////////////////////////////////////////////////////////////////////////
  arangodb::Result updateProperties(
    arangodb::velocypack::Slice const& slice,
    bool partialUpdate,
    bool doSync
  ) override;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief visit all collection IDs that were added to the view
  /// @return 'visitor' success
  ///////////////////////////////////////////////////////////////////////////////
  bool visitCollections(
    std::function<bool(TRI_voc_cid_t)> const& visitor
  ) const override;

 private:
  struct DataStore {
    irs::directory::ptr _directory;
    irs::directory_reader _reader;
    std::atomic<size_t> _segmentCount{}; // total number of segments in the writer
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

  struct TidStore {
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Called in post-recovery to remove any dangling documents old links
  //////////////////////////////////////////////////////////////////////////////
  void verifyKnownCollections();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief process a finished transaction and release resources held by it
  ////////////////////////////////////////////////////////////////////////////////
  int finish(TRI_voc_tid_t tid, bool commit);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief registers a callback for flush feature
  ////////////////////////////////////////////////////////////////////////////////
  void registerFlushCallback();

  MemoryStore& activeMemoryStore() const;

  std::condition_variable _asyncCondition; // trigger reload of timeout settings for async jobs
  std::atomic<size_t> _asyncMetaRevision; // arbitrary meta modification id, async jobs should reload if different
  std::mutex _asyncMutex; // mutex used with '_asyncCondition' and associated timeouts
  AsyncSelf::ptr _asyncSelf; // 'this' for the lifetime of the view (for use with asynchronous calls)
  std::mutex _trxStoreMutex; // mutex used to protect '_storeByTid' against multiple insertions
  std::atomic<bool> _asyncTerminate; // trigger termination of long-running async jobs
  IResearchViewMeta _meta;
  mutable irs::async_utils::read_write_mutex _mutex; // for use with member maps/sets and '_meta'
  MemoryStoreNode _memoryNodes[2]; // 2 because we just swap them
  MemoryStoreNode* _memoryNode; // points to the current memory store
  MemoryStoreNode* _toFlush; // points to memory store to be flushed
  MemoryStoreByTid _storeByTid;
  DataStore _storePersisted;
  FlushCallback _flushCallback; // responsible for flush callback unregistration
  irs::async_utils::thread_pool _threadPool;
  std::function<void(arangodb::TransactionState& state)> _trxReadCallback; // for snapshot(...)
  std::function<void(arangodb::TransactionState& state)> _trxWriteCallback; // for insert(...)/remove(...)
  std::atomic<bool> _inRecovery;
};

NS_END // iresearch
NS_END // arangodb
#endif