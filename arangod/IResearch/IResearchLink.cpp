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
/// @brief the name of the field in the iResearch Link definition denoting the
///        corresponding iResearch View
////////////////////////////////////////////////////////////////////////////////
static const std::string VIEW_NAME_FIELD("name");

////////////////////////////////////////////////////////////////////////////////
/// @brief a flag in the iResearch Link definition, if present, denoting the
///        need to skip registration with the corresponding iResearch View
///        during construction if the object
///        this field is not persisted
////////////////////////////////////////////////////////////////////////////////
static const std::string SKIP_VIEW_REGISTRATION_FIELD("skipViewRegistration");

////////////////////////////////////////////////////////////////////////////////
/// @brief return a reference to a static VPackSlice of an empty index definition
////////////////////////////////////////////////////////////////////////////////
VPackSlice const& emptyParentSlice() {
  static const struct EmptySlice {
    VPackBuilder _builder;
    VPackSlice _slice;
    EmptySlice() {
      VPackBuilder fieldsBuilder;

      fieldsBuilder.openArray();
      fieldsBuilder.close(); // empty array
      _builder.openObject();
      _builder.add("fields", fieldsBuilder.slice()); // empty array
      _builder.add(LINK_TYPE_FIELD, VPackValue(LINK_TYPE)); // the index type required by Index
      _builder.close(); // object with just one field required by the Index constructor
      _slice = _builder.slice();
    }
  } emptySlice;

  return emptySlice._slice;
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchLink::IResearchLink(
  TRI_idx_iid_t iid,
  arangodb::LogicalCollection* collection,
  IResearchLinkMeta&& meta
) : Index(iid, collection, emptyParentSlice()),
    _meta(std::move(meta)),
    _view(nullptr) {
  _unique = false; // cannot be unique since multiple fields are indexed
  _sparse = true;  // always sparse
}

bool IResearchLink::operator==(IResearchView const& view) const noexcept {
  return _view && _view->name() == view.name();
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
  if (!queue) {
    throw std::runtime_error(std::string("failed to report status during batch insert for iResearch link '") + arangodb::basics::StringUtils::itoa(id()) + "'");
  }

  if (!_collection || !_view) {
    queue->setStatus(TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED); // '_collection' and '_view' required

    return;
  }

  if (!trx) {
    queue->setStatus(TRI_ERROR_BAD_PARAMETER); // 'trx' required

    return;
  }

  TRI_voc_fid_t fid = 0; // FIXME TODO find proper fid
  auto res = _view->insert(fid, *trx, _collection->cid(), batch, _meta);

  if (TRI_ERROR_NO_ERROR != res) {
    queue->setStatus(res);
  }
}

bool IResearchLink::canBeDropped() const {
  return true; // valid for a link to be dropped from an iResearch view
}

int IResearchLink::drop() {
  if (!_collection || !_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' and '_view' required
  }

  _view->linkUnregister(_collection->cid());

  return _view->drop(_collection->cid());
}

bool IResearchLink::hasBatchInsert() const {
  return true;
}

bool IResearchLink::hasSelectivityEstimate() const {
  return false; // selectivity can only be determined per query since multiple fields are indexed
}

// FIXME TODO for use by iresearch::kludge, remove once checkpoint ids are implemented
#if 1
Result IResearchLink::insert(
    transaction::Methods* trx,
    TRI_voc_rid_t rid,
    VPackSlice const& doc,
    bool isRollback
) {
  return TRI_ERROR_NO_ERROR; // NOOP because handled by iresearch::kludge::insert(...)
}

Result IResearchLink::insert(
    transaction::Methods* trx,
    TRI_voc_fid_t fid,
    TRI_voc_rid_t rid,
    VPackSlice const& doc
) {
  if (!_collection || !_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' and '_view' required
  }

  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  return _view->insert(fid, *trx, _collection->cid(), rid, doc, _meta);
}
#else
Result IResearchLink::insert(
  transaction::Methods* trx,
  TRI_voc_rid_t rid,
  VPackSlice const& doc,
  bool isRollback
) {
  if (!_collection || !_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' and '_view' required
  }

  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  TRI_voc_fid_t fid = 0; // FIXME TODO find proper fid

  return _view->insert(fid, *trx, _collection->cid(), rid, doc, _meta);
}
#endif

bool IResearchLink::isPersistent() const {
  return true; // records persisted into the iResearch view
}

bool IResearchLink::isSorted() const {
  return false; // iResearch does not provide a fixed default sort order
}

/*static*/ IResearchLink::ptr IResearchLink::make(
  TRI_idx_iid_t iid,
  arangodb::LogicalCollection* collection,
  VPackSlice const& definition
) noexcept {  // TODO: should somehow pass an error to the caller (return nullptr means "Out of memory")
  try {
    std::string error;
    IResearchLinkMeta meta;

    if (!meta.init(definition, error)) {
      LOG_TOPIC(WARN, Logger::FIXME) << "error parsing view link parameters from json: " << error;
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return nullptr; // failed to parse metadata
    }

    PTR_NAMED(IResearchLink, ptr, iid, collection, std::move(meta));

    if (definition.hasKey(SKIP_VIEW_REGISTRATION_FIELD)) {
      // TODO FIXME find a better way to remember view name for use with toVelocyPack(...)
      if (definition.hasKey(VIEW_NAME_FIELD)) {
        auto name = definition.get(VIEW_NAME_FIELD);

        if (!name.isString()) {
          LOG_TOPIC(WARN, Logger::FIXME) << "error parsing view name for link '" << iid << "'";
          TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

          return nullptr;
        }

        ptr->_defaultName = name.copyString();
      }

      return ptr;
    }

    if (collection && definition.hasKey(VIEW_NAME_FIELD)) {
      auto name = definition.get(VIEW_NAME_FIELD);
      auto vocbase = collection->vocbase();

      if (vocbase && name.isString()) {
        auto viewName = name.copyString();

        // NOTE: this will cause a deadlock if registering a link while view is being created
        auto logicalView = vocbase->lookupView(viewName);

        if (!logicalView || !logicalView->getPhysical() || IResearchView::type() != logicalView->type()) {
          return nullptr; // no such view
        }

        // TODO FIXME find a better way to look up an iResearch View
        #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto* view = dynamic_cast<IResearchView*>(logicalView->getImplementation());
        #else
          auto* view = static_cast<IResearchView*>(logicalView->getImplementation());
        #endif

        // on success this call will set the '_view' pointer
        if (!view || !view->linkRegister(ptr)) {
          LOG_TOPIC(WARN, Logger::FIXME) << "error finding view: '" << viewName << "' for link '" << iid << "'";

          return nullptr;
        }

        return ptr;
      }
    }

    LOG_TOPIC(WARN, Logger::FIXME) << "error finding view for link '" << iid << "'";
    TRI_set_errno(TRI_ERROR_ARANGO_VIEW_NOT_FOUND);
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::DEVEL) << "caught exception while creating view link '" << iid << "'" << e.what();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::DEVEL) << "caught exception while creating view link '" << iid << "'";
  }

  return nullptr;
}

bool IResearchLink::matchesDefinition(VPackSlice const& slice) const {
  if (slice.hasKey(VIEW_NAME_FIELD)) {
    if (!_view) {
      return false; // slice has 'name' but the current object does not
    }

    auto name = slice.get(VIEW_NAME_FIELD);
    VPackValueLength nameLength;
    auto nameValue = name.getString(nameLength);
    irs::string_ref sliceName(nameValue, nameLength);

    if (sliceName != _view->name()) {
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

  if (_view) {
    // <iResearch View size> / <number of link instances>
    size += _view->memory() / std::max(size_t(1), _view->linkCount());
  }

  return size;
}

// FIXME TODO for use by iresearch::kludge, remove once checkpoint ids are implemented
#if 1
Result IResearchLink::remove(
    transaction::Methods* trx,
    TRI_voc_rid_t rid,
    VPackSlice const& doc,
    bool isRollback
) {
  return TRI_ERROR_NO_ERROR; // NOOP because handled by iresearch::kludge::remove(...)
}

Result IResearchLink::remove(transaction::Methods* trx, TRI_voc_rid_t rid) {
#else
Result IResearchLink::remove(
  transaction::Methods* trx,
  TRI_voc_rid_t rid,
  VPackSlice const& doc,
  bool isRollback
) {
#endif
  if (!_collection || !_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // '_collection' and '_view' required
  }

  if (!trx) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' required
  }

  // remove documents matching on cid and rid
  return _view->remove(*trx, _collection->cid(), rid);
}

/*static*/ bool IResearchLink::setName(
  arangodb::velocypack::Builder& builder,
  std::string const& value
) {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add(VIEW_NAME_FIELD, arangodb::velocypack::Value(value));

  return true;
}

/*static*/ bool IResearchLink::setSkipViewRegistration(
  arangodb::velocypack::Builder& builder,
  std::string const& value
) {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add(SKIP_VIEW_REGISTRATION_FIELD, arangodb::velocypack::Value(true));

  return true;
}

/*static*/ bool IResearchLink::setType(arangodb::velocypack::Builder& builder) {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add(LINK_TYPE_FIELD, arangodb::velocypack::Value(LINK_TYPE));

  return true;
}

void IResearchLink::toVelocyPack(
    VPackBuilder& builder,
    bool withFigures,
    bool forPersistence) const {
  TRI_ASSERT(!builder.isOpenObject());
  builder.openObject();
  bool success = _meta.json(builder);
  TRI_ASSERT(success);

  builder.add("id", VPackValue(std::to_string(_iid)));
  builder.add(LINK_TYPE_FIELD, VPackValue(typeName()));

  if (_view) {
    builder.add(VIEW_NAME_FIELD, VPackValue(_view->name()));
  } else if (!_defaultName.empty()) { // empty _defaultName == no view name in source jSON
  // } else if (!_defaultName.empty() && forPersistence) { // MMFilesCollection::saveIndex(...) does not set 'forPersistence'
    builder.add(VIEW_NAME_FIELD, VPackValue(_defaultName));
  }

  if (withFigures) {
    VPackBuilder figuresBuilder;

    figuresBuilder.openObject();
    toVelocyPackFigures(figuresBuilder);
    figuresBuilder.close();
    builder.add("figures", figuresBuilder.slice());
  }
  builder.close();
}

Index::IndexType IResearchLink::type() const {
  // TODO: don't use enum
  return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
}

char const* IResearchLink::typeName() const {
  return LINK_TYPE.c_str();
}

int IResearchLink::load() {
  // TODO FIXME implement

  return TRI_ERROR_NO_ERROR;
}

int IResearchLink::unload() {
  _view = nullptr; // release reference to the iResearch View

  return TRI_ERROR_NO_ERROR;
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
      LOG_TOPIC(WARN, Logger::FIXME) << "error parsing view link parameters from json: " << error;

      return TRI_ERROR_BAD_PARAMETER;
    }

    if (definition.hasKey(VIEW_NAME_FIELD)) {
      builder.add(VIEW_NAME_FIELD, definition.get(VIEW_NAME_FIELD)); // copy over iResearch View name
    }

    return meta.json(builder) ? TRI_ERROR_NO_ERROR : TRI_ERROR_BAD_PARAMETER;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "error serializaing view link parameters to json: " << e.what();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "error serializaing view link parameters to json";
  }

  return TRI_ERROR_INTERNAL;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------