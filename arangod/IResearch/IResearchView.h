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
#include "Basics/Thread.h"
#include "Transaction/Status.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalView.h"
#include "Utils/FlushTransaction.h"

#include "store/directory.hpp"
#include "index/index_writer.hpp"
#include "index/directory_reader.hpp"
#include "utils/async_utils.hpp"
#include "utils/utf8_path.hpp"

namespace {

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

}

namespace arangodb {

class DatabasePathFeature; // forward declaration
class TransactionState; // forward declaration
class ViewIterator; // forward declaration

namespace aql {

class Ast; // forward declaration
struct AstNode; // forward declaration
class SortCondition; // forward declaration
struct Variable; // forward declaration
class ExpressionContext; // forward declaration

} // aql

namespace transaction {

class Methods; // forward declaration

} // transaction

} // arangodb

namespace arangodb {
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                            Forward declarations
///////////////////////////////////////////////////////////////////////////////

class IResearchFeature; // forward declaratui
struct IResearchLinkMeta;

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                              utility constructs
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchViewMeta with an associated read-write mutex that can be
///        referenced by an std::unique_lock via read()/write()
////////////////////////////////////////////////////////////////////////////////
class AsyncMeta: public IResearchViewMeta {
 public:
  AsyncMeta(): _readMutex(_mutex), _writeMutex(_mutex) {}
  ReadMutex& read() const { return _readMutex; } // prevent modification
  WriteMutex& write() { return _writeMutex; } // exclusive modification

 private:
  irs::async_utils::read_write_mutex _mutex;
  mutable ReadMutex _readMutex; // object that can be referenced by std::unique_lock
  WriteMutex _writeMutex; // object that can be referenced by std::unique_lock
};

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
///        LogicalView interface
/// @note the responsibility of the IResearchView API is to only manage the
///       IResearch data store, i.e. insert/remove/query
///       the IResearchView API does not manage which and how the data gets
///       populated into and removed from the datatstore
///       therefore the API provides generic insert/remvoe/drop/query functions
///       which may be, but are not explicitly required to be, triggered via
///       the IResearchLink or IResearchViewBlock
///////////////////////////////////////////////////////////////////////////////
class IResearchView
  : public arangodb::LogicalViewStorageEngine,
    public arangodb::FlushTransaction {
 public:
  ///////////////////////////////////////////////////////////////////////////////
  /// @brief AsyncValue holding the view itself, modifiable by IResearchView
  ///////////////////////////////////////////////////////////////////////////////
  class AsyncSelf: public ResourceMutex {
   public:
    DECLARE_SHARED_PTR(AsyncSelf);
    explicit AsyncSelf(IResearchView* value): ResourceMutex(value) {}
    IResearchView* get() const {
      return static_cast<IResearchView*>(ResourceMutex::get());
    }
  };

  /// @enum snapshot getting mode
  enum class Snapshot {
    /// @brief lookup existing snapshot from a transaction
    Find,

    /// @brief lookup existing snapshop from a transaction
    /// or create if it doesn't exist
    FindOrCreate,

    /// @brief retrieve the latest view snapshot and cache
    /// it in a transaction
    SyncAndReplace
  };

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief destructor to clean up resources
  ///////////////////////////////////////////////////////////////////////////////
  virtual ~IResearchView();

  using arangodb::LogicalView::name;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief apply any changes to 'trx' required by this view
  /// @return success
  ///////////////////////////////////////////////////////////////////////////////
  bool apply(arangodb::transaction::Methods& trx);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief persist the specified WAL file into permanent storage
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result commit() override;

  using LogicalView::drop;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove all documents matching collection 'cid' from this IResearch
  ///        View and the underlying IResearch stores
  /// @param unlink remove 'cid' from the persisted list of tracked collection
  ///        IDs
  ////////////////////////////////////////////////////////////////////////////////
  arangodb::Result drop(TRI_voc_cid_t cid, bool unlink = true);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief acquire locks on the specified 'cid' during read-transactions
  ///        allowing retrieval of documents contained in the aforementioned
  ///        collection
  ///        also track 'cid' via the persisted list of tracked collection IDs
  /// @return the 'cid' was newly added to the IResearch View
  ////////////////////////////////////////////////////////////////////////////////
  bool emplace(TRI_voc_cid_t cid);

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

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief view factory
  /// @returns initialized view object
  ///////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<LogicalView> make(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    bool isNew,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& preCommit = {}
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
  PrimaryKeyIndexReader* snapshot(
    transaction::Methods& trx,
    Snapshot mode = Snapshot::Find
  ) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief wait for a flush of all index data to its respective stores
  /// @param maxMsec try not to exceed the specified time, casues partial sync
  ///                0 == full sync
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  bool sync(size_t maxMsec = 0);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates properties of an existing view
  //////////////////////////////////////////////////////////////////////////////
  using LogicalView::updateProperties;
  arangodb::Result updateProperties(std::shared_ptr<AsyncMeta> const& meta); // nullptr == TRI_ERROR_BAD_PARAMETER

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief visit all collection IDs that were added to the view
  /// @return 'visitor' success
  ///////////////////////////////////////////////////////////////////////////////
  bool visitCollections(CollectionVisitor const& visitor) const override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the number of documents indexed by the view
  //////////////////////////////////////////////////////////////////////////////
  size_t count();


 protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchView object
  ///        only fields describing the view itself, not 'link' descriptions
  //////////////////////////////////////////////////////////////////////////////
  virtual arangodb::Result appendVelocyPackDetailed(
    arangodb::velocypack::Builder& builder,
    bool forPersistence
  ) const override;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief drop this IResearch View
  ///////////////////////////////////////////////////////////////////////////////
  arangodb::Result dropImpl() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief called when a view's properties are updated (i.e. delta-modified)
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result updateProperties(
    arangodb::velocypack::Slice const& slice,
    bool partialUpdate
  ) override;

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

  struct PersistedStore: public DataStore {
    const irs::utf8_path _path;
    PersistedStore(irs::utf8_path&& path);
  };

  class ViewStateHelper; // forward declaration
  struct ViewStateRead; // forward declaration
  struct ViewStateWrite; // forward declaration

  struct FlushCallbackUnregisterer {
    void operator()(IResearchView* view) const noexcept;
  };

  struct MemoryStoreNode {
    MemoryStore _store;
    MemoryStoreNode* _next; // pointer to the next MemoryStore
    std::mutex _readMutex; // for use with obtaining _reader FIXME TODO find a better way
    std::mutex _reopenMutex; // for use with _reader.reopen() FIXME TODO find a better way
  };

  typedef std::unique_ptr<IResearchView, FlushCallbackUnregisterer> FlushCallback;
  typedef std::unique_ptr<
    arangodb::FlushTransaction, std::function<void(arangodb::FlushTransaction*)>
  > FlushTransactionPtr;

  IResearchView(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    arangodb::DatabasePathFeature const& dbPathFeature,
    uint64_t planVersion
  );

  MemoryStore& activeMemoryStore() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief registers a callback for flush feature
  ////////////////////////////////////////////////////////////////////////////////
  void registerFlushCallback();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Called in post-recovery to remove any dangling documents old links
  //////////////////////////////////////////////////////////////////////////////
  void verifyKnownCollections();

  IResearchFeature* _asyncFeature; // the feature where async jobs were registered (nullptr == no jobs registered)
  AsyncSelf::ptr _asyncSelf; // 'this' for the lifetime of the view (for use with asynchronous calls)
  std::atomic<bool> _asyncTerminate; // trigger termination of long-running async jobs
  std::shared_ptr<AsyncMeta> _meta; // the shared view configuration (never null!!!)
  IResearchViewMetaState _metaState; // the per-instance configuration state
  mutable irs::async_utils::read_write_mutex _mutex; // for use with member maps/sets and '_metaState'
  MemoryStoreNode _memoryNodes[2]; // 2 because we just swap them
  MemoryStoreNode* _memoryNode; // points to the current memory store
  MemoryStoreNode* _toFlush; // points to memory store to be flushed
  PersistedStore _storePersisted;
  FlushCallback _flushCallback; // responsible for flush callback unregistration
  std::function<void(arangodb::transaction::Methods& trx, arangodb::transaction::Status status)> _trxReadCallback; // for snapshot(...)
  std::function<void(arangodb::transaction::Methods& trx, arangodb::transaction::Status status)> _trxWriteCallback; // for insert(...)/remove(...)
  std::atomic<bool> _inRecovery;
};

} // iresearch
} // arangodb

#endif
