////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Containers/FlatHashSet.h"
#include "Containers/FlatHashMap.h"
#include "IResearch/IResearchViewMeta.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/ViewSnapshot.h"
#include "Transaction/Status.h"
#include "VocBase/LogicalView.h"

#include <boost/thread/v2/shared_mutex.hpp>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace arangodb {

struct ViewFactory;

namespace transaction {

class Methods;

}  // namespace transaction
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                            Forward declarations
///////////////////////////////////////////////////////////////////////////////

class IResearchFeature;
template<typename T>
class AsyncValue;

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                                   IResearchView
///////////////////////////////////////////////////////////////////////////////

// Note, that currently ArangoDB uses only 1 FlushThread for flushing the views
// In case if number of threads will be increased each thread has to receive
// it's own FlushTransaction object

///////////////////////////////////////////////////////////////////////////////
/// @brief an abstraction over the IResearch index implementing the
///        LogicalView interface
/// @note the responsibility of the IResearchView API is to only manage the
///       IResearch data store, i.e. insert/remove/query
///       the IResearchView API does not manage which and how the data gets
///       populated into and removed from the datastore
///       therefore the API provides generic insert/remove/drop/query functions
///       which may be, but are not explicitly required to be, triggered via
///       the IResearchLink or IResearchViewBlock
///////////////////////////////////////////////////////////////////////////////
class IResearchView final : public LogicalView {
  using AsyncLinkPtr = std::shared_ptr<AsyncLinkHandle>;

 public:
  static constexpr std::pair<ViewType, std::string_view> typeInfo() noexcept {
    return {ViewType::kArangoSearch, StaticStrings::ViewArangoSearchType};
  }

  ~IResearchView() final;

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
  void open() final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates properties of an existing view
  //////////////////////////////////////////////////////////////////////////////
  using LogicalDataSource::properties;
  Result properties(velocypack::Slice properties, bool isUserRequest,
                    bool partialUpdate) final;

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
  bool visitCollections(CollectionVisitor const& visitor) const final;

  IResearchViewMeta const& meta() const noexcept { return _meta; }

  ///////////////////////////////////////////////////////////////////////////////
  /// @return primary sorting order of a view, empty -> use system order
  ///////////////////////////////////////////////////////////////////////////////
  IResearchViewSort const& primarySort() const noexcept {
    return _meta._primarySort;
  }

  ///////////////////////////////////////////////////////////////////////////////
  /// @return primary sort column compression method
  ///////////////////////////////////////////////////////////////////////////////
  irs::type_info::type_id const& primarySortCompression() const noexcept {
    return _meta._primarySortCompression;
  }

  ///////////////////////////////////////////////////////////////////////////////
  /// @return stored values from links collections
  ///////////////////////////////////////////////////////////////////////////////
  IResearchViewStoredValues const& storedValues() const noexcept {
    return _meta._storedValues;
  }

  bool pkCache() const noexcept {
#ifdef USE_ENTERPRISE
    return _meta._pkCache;
#else
    return false;
#endif
  }

  bool sortCache() const noexcept {
#ifdef USE_ENTERPRISE
    return _meta._sortCache;
#else
    return false;
#endif
  }

  auto linksReadLock() const noexcept {
    return std::shared_lock<boost::upgrade_mutex>{_mutex};
  }

  LinkLock linkLock(std::shared_lock<boost::upgrade_mutex> const& guard,
                    DataSourceId cid) const noexcept;

  ViewSnapshot::Links getLinks(
      containers::FlatHashSet<DataSourceId> const* sources) const noexcept;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchView object
  ///        only fields describing the view itself, not 'link' descriptions
  //////////////////////////////////////////////////////////////////////////////
  Result appendVPackImpl(velocypack::Builder& build, Serialization ctx,
                         bool safe) const final;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief drop this IResearch View
  ///////////////////////////////////////////////////////////////////////////////
  Result dropImpl() final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief renames implementation-specific parts of an existing view
  ///        including persistance of properties
  //////////////////////////////////////////////////////////////////////////////
  Result renameImpl(std::string const& oldName) final;

  bool isBuilding() const final;

  using AsyncViewPtr = std::shared_ptr<AsyncValue<IResearchView>>;
  struct ViewFactory;

  AsyncViewPtr _asyncSelf;
  // 'this' for the lifetime of the view (for use with asynchronous calls)
  // (AsyncLinkPtr may be nullptr on single-server if link did not come up yet)
  using Indexes = containers::FlatHashMap<DataSourceId, AsyncLinkPtr>;
  Indexes _links;
  IResearchViewMeta _meta;              // the view configuration
  mutable boost::upgrade_mutex _mutex;  // for '_meta', '_links'
  std::mutex _updateLinksLock;          // prevents simultaneous 'updateLinks'
  std::function<void(transaction::Methods& trx, transaction::Status status)>
      _trxCallback;  // for snapshot(...)
  std::atomic<bool> _inRecovery;

  IResearchView(TRI_vocbase_t& vocbase, velocypack::Slice info,
                IResearchViewMeta&& meta, bool isUserRequest);

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
