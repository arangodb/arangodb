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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Containers/HashSet.h"
#include "IResearch/IResearchViewMeta.h"
#include "IResearch/IResearchKludge.h"
#include "Transaction/Status.h"
#include "VocBase/LogicalView.h"

namespace arangodb {

struct ViewFactory;  // forward declaration

}  // namespace arangodb

namespace arangodb {
namespace transaction {

class Methods;  // forward declaration

}  // namespace transaction
}  // namespace arangodb

namespace arangodb {
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                            Forward declarations
///////////////////////////////////////////////////////////////////////////////

class IResearchFeature;
class AsyncLinkHandle;
template<typename T>
class AsyncValue;

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
class IResearchView final : public arangodb::LogicalView {
  typedef std::shared_ptr<AsyncLinkHandle> AsyncLinkPtr;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief a snapshot representation of the view with ability to query for cid
  //////////////////////////////////////////////////////////////////////////////
  class Snapshot : public irs::index_reader {
   public:
    // @return cid of the sub-reader at operator['offset'] or 0 if undefined
    virtual DataSourceId cid(size_t offset) const noexcept = 0;
  };

  /// @enum snapshot getting mode
  enum class SnapshotMode {
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
  virtual ~IResearchView() override;

  using LogicalView::name;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief apply any changes to 'trx' required by this view
  /// @return success
  ///////////////////////////////////////////////////////////////////////////////
  bool apply(transaction::Methods& trx);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the factory for this type of view
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::ViewFactory const& factory();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief acquire locks on the specified 'link' during read-transactions
  ///        allowing retrieval of documents contained in the aforementioned
  ///        collection
  ///        also track 'cid' via the persisted list of tracked collection IDs
  /// @return the 'link' was newly added to the IResearch View
  //////////////////////////////////////////////////////////////////////////////
  Result link(AsyncLinkPtr const& link);

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief opens an existing view when the server is restarted
  ///////////////////////////////////////////////////////////////////////////////
  void open() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates properties of an existing view
  //////////////////////////////////////////////////////////////////////////////
  using LogicalDataSource::properties;
  virtual Result properties(velocypack::Slice properties, bool isUserRequest,
                            bool partialUpdate) override final;

  ////////////////////////////////////////////////////////////////////////////////
  /// @param shards the list of shard to restrict the snapshot to
  ///        nullptr == use all registered links
  ///        !nullptr && shard not registred then return nullptr
  ///        if mode == Find && list found doesn't match then return nullptr
  /// @param key the specified key will be as snapshot indentifier
  ///        in a transaction
  ///        (nullptr == view address will be used)
  /// @return pointer to an index reader containing the datastore record
  ///         snapshot associated with 'state'
  ///         (nullptr == no view snapshot associated with the specified state)
  ///         if force == true && no snapshot -> associate current snapshot
  ////////////////////////////////////////////////////////////////////////////////
  Snapshot const* snapshot(
      transaction::Methods& trx, SnapshotMode mode = SnapshotMode::Find,
      containers::HashSet<DataSourceId> const* shards = nullptr,
      void const* key = nullptr) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlink remove 'cid' from the persisted list of tracked collection
  ///        IDs
  /// @return success == view does not track collection
  //////////////////////////////////////////////////////////////////////////////
  Result unlink(DataSourceId cid) noexcept;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief visit all collection IDs that were added to the view
  /// @return 'visitor' success
  ///////////////////////////////////////////////////////////////////////////////
  bool visitCollections(CollectionVisitor const& visitor) const override;

  ///////////////////////////////////////////////////////////////////////////////
  /// @return primary sorting order of a view, empty -> use system order
  ///////////////////////////////////////////////////////////////////////////////
  IResearchViewSort const& primarySort() const noexcept {
    return _meta._primarySort;
  }

  ///////////////////////////////////////////////////////////////////////////////
  /// @return primary sort column compression method
  ///////////////////////////////////////////////////////////////////////////////
  auto const& primarySortCompression() const noexcept {
    return _meta._primarySortCompression;
  }

  ///////////////////////////////////////////////////////////////////////////////
  /// @return stored values from links collections
  ///////////////////////////////////////////////////////////////////////////////
  IResearchViewStoredValues const& storedValues() const noexcept {
    return _meta._storedValues;
  }

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchView object
  ///        only fields describing the view itself, not 'link' descriptions
  //////////////////////////////////////////////////////////////////////////////
  virtual Result appendVelocyPackImpl(velocypack::Builder& builder,
                                      Serialization context) const override;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief drop this IResearch View
  ///////////////////////////////////////////////////////////////////////////////
  Result dropImpl() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief renames implementation-specific parts of an existing view
  ///        including persistance of properties
  //////////////////////////////////////////////////////////////////////////////
  Result renameImpl(std::string const& oldName) override;

 private:
  typedef std::shared_ptr<AsyncValue<IResearchView>> AsyncViewPtr;
  struct ViewFactory;  // forward declaration

  AsyncViewPtr _asyncSelf;  // 'this' for the lifetime of the view (for use with
                            // asynchronous calls)
  std::unordered_map<DataSourceId, AsyncLinkPtr>
      _links;  // registered links (value may be nullptr on single-server if
               // link did not come up yet) FIXME TODO maybe this should be
               // asyncSelf?
  IResearchViewMeta _meta;  // the view configuration
  mutable arangodb::iresearch::kludge::read_write_mutex
      _mutex;                   // for use with member '_meta', '_links'
  std::mutex _updateLinksLock;  // prevents simultaneous 'updateLinks'
  std::function<void(transaction::Methods& trx, transaction::Status status)>
      _trxCallback;  // for snapshot(...)
  std::atomic<bool> _inRecovery;

  IResearchView(TRI_vocbase_t& vocbase, velocypack::Slice const& info,
                IResearchViewMeta&& meta);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief persist data store states for all known links to permanent storage
  //////////////////////////////////////////////////////////////////////////////
  Result commit();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief called when a view's properties are updated (i.e. delta-modified)
  //////////////////////////////////////////////////////////////////////////////
  Result updateProperties(velocypack::Slice slice, bool isUserRequest,
                          bool partialUpdate);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Called in post-recovery to remove any dangling documents old links
  //////////////////////////////////////////////////////////////////////////////
  void verifyKnownCollections();
};

}  // namespace iresearch
}  // namespace arangodb
