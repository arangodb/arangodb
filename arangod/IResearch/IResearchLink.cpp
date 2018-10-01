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

#include "ApplicationServerHelper.h"
#include "IResearchCommon.h"
#include "IResearchViewDBServer.h"
#include "VelocyPackHelper.h"
#include "Basics/LocalTaskQueue.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include "IResearchLinkHelper.h"
#include "IResearchLink.h"

NS_LOCAL

////////////////////////////////////////////////////////////////////////////////
/// @brief an IResearchView token not associated with any view
///        used to reduce null checks by always having '_view' initialized
////////////////////////////////////////////////////////////////////////////////
static const arangodb::iresearch::IResearchView::AsyncSelf::ptr NO_VIEW =
  irs::memory::make_unique<arangodb::iresearch::IResearchView::AsyncSelf>(nullptr);

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchLink::IResearchLink(
  TRI_idx_iid_t iid,
  arangodb::LogicalCollection& collection
): _collection(collection),
   _defaultGuid(""), // "" is never a valid guid
   _dropCollectionInDestructor(false),
   _id(iid),
   _view(nullptr) {
  // IResearchLink is not intended to be used on a coordinator
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
}

IResearchLink::~IResearchLink() {
  if (_dropCollectionInDestructor) {
    drop();
  } else {
    unload(); // disassociate from view if it has not been done yet
  }
}

bool IResearchLink::operator==(LogicalView const& view) const noexcept {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  return _view && _view->id() == view.id();
}

bool IResearchLink::operator==(IResearchLinkMeta const& meta) const noexcept {
  return _meta == meta;
}

void IResearchLink::batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& batch,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue /*= nullptr*/
) {
  if (batch.empty()) {
    return; // nothing to do
  }

  if (!queue) {
    throw std::runtime_error(std::string("failed to report status during batch insert for iResearch link '") + arangodb::basics::StringUtils::itoa(_id) + "'");
  }

  if (!trx) {
    queue->setStatus(TRI_ERROR_BAD_PARAMETER); // 'trx' required

    return;
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_view) {
    queue->setStatus(TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED); // IResearchView required

    return;
  }

  auto res = _view->insert(*trx, _collection.id(), batch, _meta);

  if (TRI_ERROR_NO_ERROR != res) {
    queue->setStatus(res);
  }
}

bool IResearchLink::canBeDropped() const {
  return true; // valid for a link to be dropped from an iResearch view
}

int IResearchLink::drop() {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // IResearchView required
  }

  auto res = _view->drop(_collection.id());

  if (!res.ok()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to drop collection '" << _collection.name()
      << "' from IResearch View '" << _view->name() << "': " << res.errorMessage();

    return res.errorNumber();
  }

  _dropCollectionInDestructor = false; // will do drop now
  _defaultGuid = _view->guid(); // remember view ID just in case (e.g. call to toVelocyPack(...) after unload())

  TRI_voc_cid_t vid = _view->id();
  _view = nullptr; // mark as unassociated
  _viewLock.unlock(); // release read-lock on the IResearch View

  // FIXME TODO this workaround should be in ClusterInfo when moving 'Plan' to 'Current', i.e. IResearchViewDBServer::drop
  if (arangodb::ServerState::instance()->isDBServer()) {
    return _collection.vocbase().dropView(vid, true).errorNumber(); // cluster-view in ClusterInfo should already not have cid-view
  }

  return TRI_ERROR_NO_ERROR;
}

void IResearchLink::afterTruncate() {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_view) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED); // IResearchView required
  }

  auto res = _view->drop(_view->id(), false);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

bool IResearchLink::hasBatchInsert() const {
  return true;
}

bool IResearchLink::hasSelectivityEstimate() const {
  return false; // selectivity can only be determined per query since multiple fields are indexed
}

TRI_idx_iid_t IResearchLink::id() const noexcept {
  return _id;
}

bool IResearchLink::init(arangodb::velocypack::Slice const& definition) {
  // disassociate from view if it has not been done yet
  if (TRI_ERROR_NO_ERROR != unload()) {
    return false;
  }

  std::string error;
  IResearchLinkMeta meta;

  if (!meta.init(definition, error)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "error parsing view link parameters from json: " << error;
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return false; // failed to parse metadata
  }

  if (!definition.isObject()
      || !(definition.get(StaticStrings::ViewIdField).isString() ||
           definition.get(StaticStrings::ViewIdField).isNumber<TRI_voc_cid_t>())) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "error finding view for link '" << _id << "'";
    TRI_set_errno(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

    return false;
  }

  // we continue to support the old and new ID format
  auto idSlice = definition.get(StaticStrings::ViewIdField);
  std::string viewId = idSlice.isString() ? idSlice.copyString() : std::to_string(idSlice.getUInt());
  auto& vocbase = _collection.vocbase();
  auto logicalView = vocbase.lookupView(viewId); // will only contain IResearchView (even for a DBServer)

  // creation of link on a DBServer
  if (!logicalView && arangodb::ServerState::instance()->isDBServer()) {
    auto* ci = ClusterInfo::instance();

    if (!ci) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find 'ClusterInfo' instance for lookup of link '" << _id << "'";
      TRI_set_errno(TRI_ERROR_INTERNAL);

      return false;
    }

    auto logicalWiew = ci->getView(vocbase.name(), viewId);
    auto* wiew = LogicalView::cast<IResearchViewDBServer>(logicalWiew.get());
    if (wiew) {
      // FIXME figure out elegant way of testing for cluster wide LogicalCollection
      if (_collection.id() == _collection.planId() && _collection.isAStub()) {
      // this is a cluster-wide collection/index/link (per-cid view links have their corresponding collections in vocbase)
        auto clusterCol = ci->getCollectionCurrent(
          vocbase.name(), std::to_string(_collection.id())
        );

        if (clusterCol) {
          for (auto& entry: clusterCol->errorNum()) {
            auto collection = vocbase.lookupCollection(entry.first); // find shard collection

            if (collection) {
              // ensure the shard collection is registered with the cluster-wide view
              // required from creating snapshots for per-cid views loaded from WAL
              // only register existing per-cid view instances, do not create new per-cid view
              // instances since they will be created/registered  by their per-cid links just below
              wiew->ensure(collection->id(), false);
            }
          }
        }

        return true; // leave '_view' uninitialized to mark the index as unloaded/unusable
      }

      logicalView = wiew->ensure(_collection.id()); // repoint LogicalView at the per-cid instance
    }
  }

  if (!logicalView
      || arangodb::iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "error finding view: '" << viewId << "' for link '" << _id << "' : no such view";

    return false; // no such view
  }

  auto* view = LogicalView::cast<IResearchView>(logicalView.get());

  if (!view) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "error finding view: '" << viewId << "' for link '" << _id << "'";

    return false;
  }

  auto viewSelf = view->self();

  if (!viewSelf) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "error read-locking view: '" << viewId
      << "' for link '" << _id << "'";

    return false;
  }

  _viewLock = std::unique_lock<ReadMutex>(viewSelf->mutex()); // aquire read-lock before checking view

  if (!viewSelf->get()) {
    _viewLock.unlock();
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "error getting view: '" << viewId << "' for link '" << _id << "'";
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "have raw view '" << view->guid() << "'";

    return false;
  }

  _dropCollectionInDestructor = view->emplace(_collection.id()); // track if this is the instance that called emplace
  _meta = std::move(meta);
  _view = std::move(view);

  // FIXME TODO remove once View::updateProperties(...) will be fixed to write
  // the update delta into the WAL marker instead of the full persisted state
  {
    auto* engine = arangodb::EngineSelectorFeature::ENGINE;

    if (engine && engine->inRecovery()) {
      _defaultGuid = _view->guid();
    }
  }

  return true;
}

Result IResearchLink::insert(
  transaction::Methods* trx,
  arangodb::LocalDocumentId const& documentId,
  VPackSlice const& doc,
  Index::OperationMode mode
) {
  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_view) {
    return TRI_ERROR_ARANGO_INDEX_HANDLE_BAD; // IResearchView required
  }

  return _view->insert(*trx, _collection.id(), documentId, doc, _meta);
}

bool IResearchLink::isPersistent() const {
  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  // FIXME TODO remove once MMFilesEngine will fillIndex(...) during recovery
  // currently the index is created but fill is deffered untill the end of recovery
  // at the end of recovery only non-persistent indexes are filled
  if (engine && engine->inRecovery()) {
    return false;
  }

  return true; // records persisted into the iResearch view
}

bool IResearchLink::isSorted() const {
  return false; // iResearch does not provide a fixed default sort order
}

bool IResearchLink::json(arangodb::velocypack::Builder& builder) const {
  if (!builder.isOpenObject() || !_meta.json(builder)) {
    return false;
  }

  builder.add(
    arangodb::StaticStrings::IndexId,
    arangodb::velocypack::Value(std::to_string(_id))
  );
  builder.add(
    arangodb::StaticStrings::IndexType,
    arangodb::velocypack::Value(IResearchLinkHelper::type())
  );

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (_view) {
    builder.add(
      StaticStrings::ViewIdField, arangodb::velocypack::Value(_view->guid())
    );
  } else if (!_defaultGuid.empty()) { // _defaultGuid.empty() == no view name in source jSON
    builder.add(
      StaticStrings::ViewIdField, VPackValue(_defaultGuid)
    );
  }

  return true;
}

void IResearchLink::load() {
  // Note: this function is only used by RocksDB
}

bool IResearchLink::matchesDefinition(VPackSlice const& slice) const {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (slice.isObject() && slice.hasKey(StaticStrings::ViewIdField)) {
    if (!_view) {
      return false; // slice has identifier but the current object does not
    }

    auto identifier = slice.get(StaticStrings::ViewIdField);
    if (!((identifier.isString() && identifier.isEqualString(_view->guid())) ||
          (identifier.isNumber<TRI_voc_cid_t>() && identifier.getUInt() != _view->id()))) {
      return false;  // iResearch View names of current object and slice do not match
    }
  } else if (_view) {
    return false; // slice has no 'name' but the current object does
  }

  IResearchLinkMeta other;
  std::string errorField;

  return other.init(slice, errorField) && _meta == other;
}

size_t IResearchLink::memory() const {
  auto size = sizeof(IResearchLink); // includes empty members from parent

  size += _meta.memory();

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (_view) {
    size_t count = 0;

    // get a count of collections currently defined in the view
    _view->visitCollections([&count](TRI_voc_cid_t)->bool {
      ++count;
      return true;
    });

    // <iResearch View size> / <number of tracked collection IDs>
    // a rough approximation of how much memory is used by each collection ID
    size += _view->memory() / std::max(size_t(1), count);
  }

  return size;
}

Result IResearchLink::remove(
  transaction::Methods* trx,
  LocalDocumentId const& documentId,
  VPackSlice const& doc,
  Index::OperationMode mode
) {
  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // IResearchView required
  }

  // remove documents matching on cid and rid
  return _view->remove(*trx, _collection.id(), documentId);
}

Result IResearchLink::remove(
  transaction::Methods* trx,
  arangodb::LocalDocumentId const& documentId,
  Index::OperationMode mode
) {
  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // IResearchView required
  }

  // remove documents matching on cid and documentId
  return _view->remove(*trx, _collection.id(), documentId);
}

Index::IndexType IResearchLink::type() const {
  // TODO: don't use enum
  return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
}

char const* IResearchLink::typeName() const {
  return IResearchLinkHelper::type().c_str();
}

int IResearchLink::unload() {
  WriteMutex mutex(_mutex); // '_view' can be asynchronously read
  SCOPED_LOCK(mutex);

  if (!_view) {
    return TRI_ERROR_NO_ERROR;
  }

  // this code is used by the MMFilesEngine
  // if the collection is in the process of being removed then drop it from the view
  // FIXME TODO remove once LogicalCollection::drop(...) will drop its indexes explicitly
  if (_collection.deleted()
      || TRI_vocbase_col_status_e::TRI_VOC_COL_STATUS_DELETED == _collection.status()) {
    auto res = drop();

    LOG_TOPIC_if(WARN, arangodb::iresearch::TOPIC, TRI_ERROR_NO_ERROR != res)
        << "failed to drop collection from view while unloading dropped "
        << "IResearch link '" << _id "' for IResearch view '"
        << _view->name() << "'";
    }

    return res;
  }

  _dropCollectionInDestructor = false; // valid link (since unload(..) called), should not be dropped
  _defaultGuid = _view->guid(); // remember view ID just in case (e.g. call to toVelocyPack(...) after unload())
  _view = nullptr; // mark as unassociated
  _viewLock.unlock(); // release read-lock on the IResearch View

  return TRI_ERROR_NO_ERROR;
}

const IResearchView* IResearchLink::view() const {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  return _view;
}

/*static*/ std::shared_ptr<IResearchLink> IResearchLink::find(
    LogicalCollection const& collection,
    LogicalView const& view
) {
  for (auto& index : collection.getIndexes()) {
    if (!index || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
      continue; // not an iresearch Link
    }

    // TODO FIXME find a better way to retrieve an iResearch Link
    // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);

    if (link && *link == view) {
      return link; // found required link
    }
  }

  return nullptr;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
