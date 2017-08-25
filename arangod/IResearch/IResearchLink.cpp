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
#include "IResearchAnalyzerFeature.h"
#include "IResearchFeature.h"

#include "Basics/LocalTaskQueue.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include "IResearchLink.h"

NS_LOCAL

////////////////////////////////////////////////////////////////////////////////
/// @brief the string representing the link type
////////////////////////////////////////////////////////////////////////////////
static const std::string LINK_TYPE("iresearch");

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

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve in IResearchAnalyzerFeature all tokenizers regsitered with
///        the IResearchLinkMeta
/// @return success
////////////////////////////////////////////////////////////////////////////////
bool reserveAnalyzers(
    arangodb::iresearch::IResearchLinkMeta const& meta,
    TRI_voc_cid_t cid,
    TRI_idx_iid_t iid
) {
  auto* analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

  if (!analyzers) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to retrieve Analyzer Feature while registering analyzers for IResearch view '" << cid << "' IResearch link '" << iid << "'";

    return false;
  }

  // reserve all tokenizers registered by link meta
  for (auto& entry: meta._tokenizers) {
    if (entry) {
      // FIXME TODO revert on failure or send iterator to reserve
      analyzers->reserve(entry->name());
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release from IResearchAnalyzerFeature all tokenizers regsitered with
///        the IResearchLinkMeta
/// @return success
////////////////////////////////////////////////////////////////////////////////
bool releaseAnalyzers(
    arangodb::iresearch::IResearchLinkMeta const& meta,
    TRI_voc_cid_t cid,
    TRI_idx_iid_t iid
) {
  auto* analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

  if (!analyzers) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to retrieve Analyzer Feature while unregistering analyzers for IResearch view '" << cid << "' IResearch link '" << iid << "'";

    return false;
  }

  // release all tokenizers reserved by link meta
  for (auto& entry: meta._tokenizers) {
    if (entry) {
      // FIXME TODO revert on failure or send iterator to release
      analyzers->release(entry->name());
    }
  }

  return true;
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchLink::IResearchLink(
  TRI_idx_iid_t iid,
  arangodb::LogicalCollection* collection
): _collection(collection),
   _defaultId(0), // 0 is never a valid id
   _id(iid),
   _view(nullptr) {
}

IResearchLink::~IResearchLink() {
  unload(); // disassociate from view if it has not been done yet
}

bool IResearchLink::operator==(IResearchView const& view) const noexcept {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  return _view && _view->id() == view.id();
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
    std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const& batch,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue /*= nullptr*/
) {
  if (batch.empty()) {
    return; // nothing to do
  }

  if (!queue) {
    throw std::runtime_error(std::string("failed to report status during batch insert for iResearch link '") + arangodb::basics::StringUtils::itoa(_id) + "'");
  }

  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_collection || !_view) {
    queue->setStatus(TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED); // '_collection' and '_view' required

    return;
  }

  if (!trx) {
    queue->setStatus(TRI_ERROR_BAD_PARAMETER); // 'trx' required

    return;
  }

  auto res = _view->insert(*trx, _collection->cid(), batch, _meta);

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
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_collection || !_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' and '_view' required
  }

  if (!releaseAnalyzers(_meta, _view->id(), _id)) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "failed to release tokenizers while dropping IResearch link '" << _id << "' for IResearch view '" << _view->id() << "'";

    return TRI_ERROR_INTERNAL;
  }

  return _view->drop(_collection->cid());
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

      // on success this call will set the '_view' pointer
      if (!view->linkRegister(*this)) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error registering link '" << _id << "' for view: '" << viewId;
        return false;
      }

      _meta = std::move(meta);

      return true;
    }
  }

  LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "error finding view for link '" << _id << "'";
  TRI_set_errno(TRI_ERROR_ARANGO_VIEW_NOT_FOUND);

  return false;
}

Result IResearchLink::insert(
  transaction::Methods* trx,
  TRI_voc_rid_t rid,
  VPackSlice const& doc,
  bool isRollback
) {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_collection || !_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' and '_view' required
  }

  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  return _view->insert(*trx, _collection->cid(), rid, doc, _meta);
}

bool IResearchLink::isPersistent() const {
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

  if (_view) {
    builder.add(VIEW_ID_FIELD, VPackValue(_view->id()));
  } else if (_defaultId) { // '0' _defaultId == no view name in source jSON
  // } else if (_defaultId && forPersistence) { // MMFilesCollection::saveIndex(...) does not set 'forPersistence'
    builder.add(VIEW_ID_FIELD, VPackValue(_defaultId));
  }

  return true;
}

bool IResearchLink::matchesDefinition(VPackSlice const& slice) const {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (slice.hasKey(VIEW_ID_FIELD)) {
    if (!_view) {
      return false; // slice has identifier but the current object does not
    }

    auto identifier = slice.get(VIEW_ID_FIELD);

    if (!identifier.isNumber() || uint64_t(identifier.getInt()) != identifier.getUInt() || identifier.getUInt() != _view->id()) {
      return false; // iResearch View names of current object and slice do not match
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
    // <iResearch View size> / <number of link instances>
    size += _view->memory() / std::max(size_t(1), _view->linkCount());
  }

  return size;
}

Result IResearchLink::remove(
  transaction::Methods* trx,
  TRI_voc_rid_t rid,
  VPackSlice const& doc,
  bool isRollback
) {
  ReadMutex mutex(_mutex); // '_view' can be asynchronously modified
  SCOPED_LOCK(mutex);

  if (!_collection || !_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' and '_view' required
  }

  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  // remove documents matching on cid and rid
  return _view->remove(*trx, _collection->cid(), rid);
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

  if (_view) {
    _defaultId = _view->id(); // remember view ID just in case (e.g. call to toVelocyPack(...) after unload())

    auto* col = collection();

    if (!col) {
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "failed finding collection while unloading IResearch link '" << _id << "'";

      return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' required
    }

    // if the collection is in the process of being removed then drop it from the view
    if (col->deleted()) {
      auto res = _view->drop(col->cid());

      if (TRI_ERROR_NO_ERROR != res) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "failed to drop collection from view while unloading dropped IResearch link '" << _id << "' for IResearch view '" << _view->id() << "'";

        return res;
      }

      if (!releaseAnalyzers(_meta, _view->id(), _id)) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH) << "failed to release tokenizers while unloading dropped IResearch link '" << _id << "' for IResearch view '" << _view->id() << "'";

        return TRI_ERROR_INTERNAL;
      }
    }
  }

  _view = nullptr; // release reference to the iResearch View

  return TRI_ERROR_NO_ERROR;
}

IResearchView::sptr IResearchLink::updateView(
    IResearchView::sptr const& view,
    bool isNew /*= false*/
) {
  WriteMutex mutex(_mutex); // '_view' can be asynchronously read
  SCOPED_LOCK(mutex);
  auto previous = _view;

  _view = view;

  if (isNew && _view) {
    reserveAnalyzers(_meta, _view->id(), _id);
  }

  return previous;
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