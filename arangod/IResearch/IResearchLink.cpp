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
#include "IResearchFeature.h"
#include "VelocyPackHelper.h"

#include "Basics/LocalTaskQueue.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include "IResearchLink.h"

NS_LOCAL

////////////////////////////////////////////////////////////////////////////////
/// @brief the string representing the link type
////////////////////////////////////////////////////////////////////////////////
static const std::string& LINK_TYPE = arangodb::iresearch::IResearchFeature::type();

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the iResearch Link definition denoting the
///        iResearch Link type
////////////////////////////////////////////////////////////////////////////////
static const std::string LINK_TYPE_FIELD("type");

////////////////////////////////////////////////////////////////////////////////
/// @brief the id of the field in the iResearch Link definition denoting the
///        corresponding iResearch View
////////////////////////////////////////////////////////////////////////////////
static const std::string VIEW_ID_FIELD("view");

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
  arangodb::LogicalCollection* collection
): _collection(collection),
   _defaultId(0), // 0 is never a valid id
   _id(iid),
   _view(NO_VIEW) {
}

IResearchLink::~IResearchLink() {
  unload(); // disassociate from view if it has not been done yet
}

bool IResearchLink::operator==(IResearchView const& view) const noexcept {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links
  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* thisView = _view->get();

  return thisView && thisView->id() == view.id();
}

bool IResearchLink::operator!=(IResearchView const& view) const noexcept {
  return !(*this == view);
}

bool IResearchLink::operator==(IResearchLinkMeta const& meta) const noexcept {
  return _meta == meta;
}

bool IResearchLink::operator!=(IResearchLinkMeta const& meta) const noexcept {
  return !(*this == meta);
}

bool IResearchLink::allowExpansion() const {
  return true; // maps to multivalued
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

  if (!_collection) {
    queue->setStatus(TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED); // '_collection' required

    return;
  }

  if (!trx) {
    queue->setStatus(TRI_ERROR_BAD_PARAMETER); // 'trx' required

    return;
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links
  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* view = _view->get();

  if (!view) {
    queue->setStatus(TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED); // IResearchView required

    return;
  }

  auto res = view->insert(*trx, _collection->cid(), batch, _meta);

  if (TRI_ERROR_NO_ERROR != res) {
    queue->setStatus(res);
  }
}

bool IResearchLink::canBeDropped() const {
  return true; // valid for a link to be dropped from an iResearch view
}

LogicalCollection* IResearchLink::collection() const noexcept {
  return _collection;
}

int IResearchLink::drop() {
  if (!_collection) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' required
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links
  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* view = _view->get();

  if (!view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // IResearchView required
  }

  // if the collection is in the process of being removed then drop it from the view
  if (_collection->deleted()) {
    auto result = view->updateLogicalProperties(emptyObjectSlice(), true, false); // revalidate all links

    if (!result.ok()) {
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "failed to force view link revalidation while unloading dropped IResearch link '" << _id << "' for IResearch view '" << view->id() << "'";

      return result.errorNumber();
    }
  }

  // FIXME TODO remove link via update properties on view
  return view->drop(_collection->cid());
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
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error parsing view link parameters from json: " << error;
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return false; // failed to parse metadata
  }

  if (collection() && definition.hasKey(VIEW_ID_FIELD)) {
    auto identifier = definition.get(VIEW_ID_FIELD);
    auto vocbase = collection()->vocbase();

    if (vocbase && identifier.isNumber() && uint64_t(identifier.getInt()) == identifier.getUInt()) {
      auto viewId = identifier.getUInt();

      // NOTE: this will cause a deadlock if registering a link while view is being created
      auto logicalView = vocbase->lookupView(viewId);

      if (!logicalView || IResearchView::type() != logicalView->type()) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error looking up view '" << viewId << "': no such view";
        return false; // no such view
      }

      // TODO FIXME find a better way to look up an iResearch View
      #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        auto* view = dynamic_cast<IResearchView*>(logicalView->getImplementation());
      #else
        auto* view = static_cast<IResearchView*>(logicalView->getImplementation());
      #endif

      if (!view) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error finding view: '" << viewId << "' for link '" << _id << "'";

        return false;
      }

      auto viewSelf = view->self();

      if (!viewSelf) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error getting view: '" << viewId << "' for link '" << _id << "'";

        return false;
      }

      _meta = std::move(meta);
      _view = std::move(viewSelf);

      // FIXME TODO remove once View::updateProperties(...) will be fixed to write
      // the update delta into the WAL marker instead of the full persisted state
      {
        auto* engine = arangodb::EngineSelectorFeature::ENGINE;

        if (engine && engine->inRecovery()) {
          _defaultId = view->id();
        }
      }

      return true;
    }
  }

  LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error finding view for link '" << _id << "'";
  TRI_set_errno(TRI_ERROR_ARANGO_VIEW_NOT_FOUND);

  return false;
}

Result IResearchLink::insert(
  transaction::Methods* trx,
  arangodb::LocalDocumentId const& documentId,
  VPackSlice const& doc,
  Index::OperationMode mode
) {
  if (!_collection) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' required
  }

  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links
  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* view = _view->get();

  if (!view) {
    return TRI_ERROR_ARANGO_INDEX_HANDLE_BAD; // IResearchView required
  }

  return view->insert(*trx, _collection->cid(), documentId, doc, _meta);
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

bool IResearchLink::json(
    arangodb::velocypack::Builder& builder,
    bool forPersistence
) const {
  if (!builder.isOpenObject() || !_meta.json(builder)) {
    return false;
  }

  builder.add("id", VPackValue(std::to_string(_id)));
  builder.add(LINK_TYPE_FIELD, VPackValue(typeName()));

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links
  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* view = _view->get();

  if (view) {
    builder.add(VIEW_ID_FIELD, VPackValue(view->id()));
  } else if (_defaultId) { // '0' _defaultId == no view name in source jSON
  //if (_defaultId && forPersistence) { // MMFilesCollection::saveIndex(...) does not set 'forPersistence'
    builder.add(VIEW_ID_FIELD, VPackValue(_defaultId));
  }

  return true;
}

void IResearchLink::load() {
}

bool IResearchLink::matchesDefinition(VPackSlice const& slice) const {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links

  if (slice.hasKey(VIEW_ID_FIELD)) {
    auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
    SCOPED_LOCK(viewMutex);
    auto* view = _view->get();

    if (!view) {
      return false; // slice has identifier but the current object does not
    }

    auto identifier = slice.get(VIEW_ID_FIELD);

    if (!identifier.isNumber() || uint64_t(identifier.getInt()) != identifier.getUInt() || identifier.getUInt() != view->id()) {
      return false; // iResearch View names of current object and slice do not match
    }
  } else if (_view->get()) { // do not need to lock since this is a single-call
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
  assert(_view); // NO_VIEW used for unasociated links
  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* view = _view->get();

  if (view) {
    size_t count = 0;

    // get a count of collections currently defined in the view
    view->visitCollections([&count](TRI_voc_cid_t)->bool {
      ++count;
      return true;
    });

    // <iResearch View size> / <number of tracked collection IDs>
    // a rough approximation of how much memory is used by each collection ID
    size += view->memory() / std::max(size_t(1), count);
  }

  return size;
}

Result IResearchLink::remove(
  transaction::Methods* trx,
  LocalDocumentId const& documentId,
  VPackSlice const& doc,
  Index::OperationMode mode
) {
  if (!_collection) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' required
  }

  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links
  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* view = _view->get();

  if (!view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // IResearchView required
  }

  // remove documents matching on cid and rid
  return view->remove(*trx, _collection->cid(), documentId);
}

Result IResearchLink::remove(
  transaction::Methods* trx,
  arangodb::LocalDocumentId const& documentId,
  Index::OperationMode mode
) {
  if (!_collection) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' required
  }

  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links
  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* view = _view->get();

  if (!view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // IResearchView required
  }

  // remove documents matching on cid and documentId
  return view->remove(*trx, _collection->cid(), documentId);
}

/*static*/ bool IResearchLink::setType(arangodb::velocypack::Builder& builder) {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add(LINK_TYPE_FIELD, arangodb::velocypack::Value(LINK_TYPE));

  return true;
}

/*static*/ bool IResearchLink::setView(
  arangodb::velocypack::Builder& builder,
  TRI_voc_cid_t value
) {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add(VIEW_ID_FIELD, arangodb::velocypack::Value(value));

  return true;
}

arangodb::Result IResearchLink::recover() {
  if (!_collection) {
    return {TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND}; // current link isn't associated with the collection
  }

  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* view = _view->get();

  if (!view) {
    return {TRI_ERROR_ARANGO_VIEW_NOT_FOUND}; // slice has identifier but the current object does not
  }

  arangodb::velocypack::Builder link;

  link.openObject();
  if (!json(link, false)) {
    return {TRI_ERROR_INTERNAL};
  }
  link.close();

  // re-insert link into the view
  return view->link(_collection->cid(), link.slice());
}

Index::IndexType IResearchLink::type() const {
  // TODO: don't use enum
  return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
}

char const* IResearchLink::typeName() const {
  return LINK_TYPE.c_str();
}

int IResearchLink::unload() {
  WriteMutex mutex(_mutex); // '_view' can be asynchronously read
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links
  auto viewCopy = _view; // retain a copy of the pointer for the case where nullyfying the original will call distructor of a locked mutex
  auto viewMutex = _view->mutex(); // IResearchView can be asynchronously deallocated
  SCOPED_LOCK(viewMutex);
  auto* view = _view->get();

  if (!view) {
    _view = NO_VIEW; // release reference to the IResearch View

    return TRI_ERROR_NO_ERROR;
  }

  _defaultId = view->id(); // remember view ID just in case (e.g. call to toVelocyPack(...) after unload())

  auto* col = collection();

  if (!col) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "failed finding collection while unloading IResearch link '" << _id << "'";

    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' required
  }

  // this code is used by the MMFilesEngine
  // if the collection is in the process of being removed then drop it from the view
  // FIXME TODO remove once LogicalCollection::drop(...) will drop its indexes explicitly
  if (col->deleted()) {
    auto res = drop();

    if (TRI_ERROR_NO_ERROR != res) {
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "failed to drop collection from view while unloading dropped IResearch link '" << _id << "' for IResearch view '" << view->id() << "'";

      return res;
    }
  }

  _view = NO_VIEW; // release reference to the iResearch View

  return TRI_ERROR_NO_ERROR;
}

const IResearchView* IResearchLink::view() const {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);
  assert(_view); // NO_VIEW used for unasociated links

  return _view->get();
}

int EnhanceJsonIResearchLink(
  VPackSlice const definition,
  VPackBuilder& builder,
  bool create
) noexcept {
  try {
    std::string error;
    IResearchLinkMeta meta;

    if (!meta.init(definition, error)) {
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error parsing view link parameters from json: " << error;

      return TRI_ERROR_BAD_PARAMETER;
    }

    if (definition.hasKey(VIEW_ID_FIELD)) {
      builder.add(VIEW_ID_FIELD, definition.get(VIEW_ID_FIELD)); // copy over iResearch View identifier
    }

    return meta.json(builder) ? TRI_ERROR_NO_ERROR : TRI_ERROR_BAD_PARAMETER;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error serializaing view link parameters to json: " << e.what();
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error serializaing view link parameters to json";
  }

  return TRI_ERROR_INTERNAL;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
