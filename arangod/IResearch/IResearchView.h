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

NS_END // aql

NS_BEGIN(transaction)

class Methods; // forward declaration

NS_END // transaction

NS_END // arangodb

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                            Forward declarations
///////////////////////////////////////////////////////////////////////////////

class IResearchLink;
struct IResearchLinkMeta;
class IResearchView;

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                                   IResearchView
///////////////////////////////////////////////////////////////////////////////

// Note, that currenly ArangoDB uses only 1 FlushThread for flushing the views
// In case if number of threads will be increased each thread has to receive
// it's own FlushTransaction object

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
  /// @brief drop this iResearch View
  ///////////////////////////////////////////////////////////////////////////////
  void drop() override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief drop collection matching 'cid' from the iResearch View
  ////////////////////////////////////////////////////////////////////////////////
  int drop(TRI_voc_cid_t cid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief persist the specified WAL file into permanent storage
  ////////////////////////////////////////////////////////////////////////////////
  int finish();

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchView object
  ///        only fields describing the view itself, not 'link' descriptions
  ////////////////////////////////////////////////////////////////////////////////
  void getPropertiesVPack(arangodb::velocypack::Builder& builder,
			  bool forPersistence) const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the id identifying the current iResearch View or '0' if unknown
  ////////////////////////////////////////////////////////////////////////////////
  TRI_voc_cid_t id() const noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert a document into the iResearch View
  ///        to be done in the scope of transaction 'tid' and 'meta'
  ////////////////////////////////////////////////////////////////////////////////
  int insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid,
    arangodb::velocypack::Slice const& doc,
    IResearchLinkMeta const& meta
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert a batch of documents into the iResearch View
  ///        to be done in the scope of transaction 'tid' and 'meta'
  ///        'Itrator.first' == TRI_voc_rid_t
  ///        'Itrator.second' == arangodb::velocypack::Slice
  ///        terminate on first failure
  ////////////////////////////////////////////////////////////////////////////////
  int insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const& batch,
    IResearchLinkMeta const& meta
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called at query execution, when the AQL query engine requests data
  ///        from the view
  ///        the call will get the specialized filter condition and the sort
  ///        condition from the previous calls
  /// @return a ViewIterator which the AQL query engine will use for fetching
  ///         results from the view
  ////////////////////////////////////////////////////////////////////////////////
  virtual arangodb::ViewIterator* iteratorForCondition(
    transaction::Methods* trx,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition
  ) override;

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
  /// @brief remove documents matching 'cid' and 'rid' from the iResearch View
  ///        to be done in the scope of transaction 'tid'
  ////////////////////////////////////////////////////////////////////////////////
  int remove(transaction::Methods& trx, TRI_voc_cid_t cid, TRI_voc_rid_t rid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called for a filter condition that was previously handed to the view
  ///        in `supportsFilterCondition`, and for which the view has claimed to
  ///        support it (at least partially)
  ///        this call gives the view the chance to filter out all parts of the
  ///        filter condition that it cannot handle itself
  ///        if the view does not support the entire filter condition (or needs to
  ///        rewrite the filter condition somehow), it may return a new AstNode,
  ///        it is not allowed to modify the AstNode
  ////////////////////////////////////////////////////////////////////////////////
  virtual arangodb::aql::AstNode* specializeCondition(
    arangodb::aql::Ast* ast,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference
  ) override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called by the AQL optimizer to check if the view supports a filter
  ///        condition (as identified by the AST node `node`) at least partially
  ///        the AQL variable `reference` is the variable that was used for
  ///        accessing the view in the AQL query
  /// @return boolean
  ///         can provide an estimate for the number of items to return and
  ///         can provide an estimated cost value
  ///         Note: that these return values are informational only (e.g. for
  ///               displaying them in the AQL explain output)
  ////////////////////////////////////////////////////////////////////////////////
  virtual bool supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    size_t& estimatedItems,
    double& estimatedCost
  ) const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called by the AQL optimizer to check if the view supports a
  ///        particular sort condition (as identifier by the `sortCondition`
  ///        passed) at least partially
  /// @param reference identifies the variable in the AQL query that is used to
  ///        access the view
  /// @return boolean
  ///         can provide an estimated cost value
  ///         Note: that this return value is informational only (e.g. for
  ///               displaying them in the AQL explain output)
  ///         it must also return the number of sort condition parts that are
  ///         covered by the view, from left to right, starting at the first sort
  ///         condition part
  ///         if the first sort condition part is not covered by the view, then
  ///         `coveredAttributes` must be `0`
  ////////////////////////////////////////////////////////////////////////////////
  virtual bool supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference,
    double& estimatedCost,
    size_t& coveredAttributes
  ) const override;

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

  arangodb::Result commit() override;

 private:
  struct DataStore {
    irs::directory::ptr _directory;
    irs::directory_reader _reader;
    irs::index_writer::ptr _writer;
    DataStore() = default;
    DataStore(DataStore&& other) noexcept;
    DataStore& operator=(DataStore&& other) noexcept;
    operator bool() const noexcept;
  };

  struct MemoryStore: public DataStore {
    MemoryStore(); // initialize _directory and _writer during allocation
  };

  struct SyncState {
    struct PolicyState {
      size_t _intervalCount;
      size_t _intervalStep;

      std::shared_ptr<irs::index_writer::consolidation_policy_t> _policy;
      PolicyState(size_t intervalStep, std::shared_ptr<irs::index_writer::consolidation_policy_t> policy);
    };

    size_t _cleanupIntervalCount;
    size_t _cleanupIntervalStep;
    std::vector<PolicyState> _consolidationPolicies;

    SyncState() noexcept;
    SyncState(IResearchViewMeta::CommitBaseMeta const& meta);
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
  };

  typedef std::unordered_map<TRI_voc_tid_t, TidStore> MemoryStoreByTid;
  typedef std::unique_ptr<IResearchView, FlushCallbackUnregisterer> FlushCallback;
  typedef std::unique_ptr<
    arangodb::FlushTransaction, std::function<void(arangodb::FlushTransaction*)>
  > FlushTransactionPtr;

  std::condition_variable _asyncCondition; // trigger reload of timeout settings for async jobs
  std::atomic<size_t> _asyncMetaRevision; // arbitrary meta modification id, async jobs should reload if different
  std::mutex _asyncMutex; // mutex used with '_asyncCondition' and associated timeouts
  std::atomic<bool> _asyncTerminate; // trigger termination of long-running async jobs
  int (*_beforeInsert)(IResearchView&, arangodb::transaction::Methods&, TRI_voc_cid_t, TRI_voc_rid_t); // call before insertion
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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief process a finished transaction and release resources held by it
  ////////////////////////////////////////////////////////////////////////////////
  int finish(TRI_voc_tid_t tid, bool commit);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief wait for a flush of all index data to its respective stores
  /// @param maxMsec try not to exceed the specified time, casues partial sync
  ///                0 == full sync
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  bool sync(bool includePersisted, size_t maxMsec = 0);

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
};

NS_END // iresearch
NS_END // arangodb
#endif