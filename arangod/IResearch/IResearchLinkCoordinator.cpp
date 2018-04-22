////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "IResearchLinkCoordinator.h"
#include "IResearchCommon.h"
#include "IResearchLinkHelper.h"
#include "IResearchFeature.h"
#include "IResearchViewCoordinator.h"
#include "VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/LogicalCollection.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

namespace {

using namespace arangodb::iresearch;
using namespace arangodb::basics;

class IResearchLinkMMFilesCoordinator final
    : public arangodb::Index,
      public IResearchLinkCoordinator {
 public:
  IResearchLinkMMFilesCoordinator(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection
  ) : arangodb::Index(iid, collection, IResearchLinkHelper::emptyIndexSlice()),
      IResearchLinkCoordinator(iid, collection) {
    _unique = false; // cannot be unique since multiple fields are indexed
    _sparse = true;  // always sparse
  }

  bool allowExpansion() const noexcept {
    return true;
  }

  void batchInsert(
      arangodb::transaction::Methods*,
      std::vector<std::pair<arangodb::LocalDocumentId, VPackSlice>> const&,
      std::shared_ptr<LocalTaskQueue>
  ) override {
  }

  bool canBeDropped() const noexcept override {
    return true;
  }

  int drop() {
    return IResearchLinkCoordinator::drop().errorNumber();
  }

  bool hasBatchInsert() const noexcept override {
    return true;
  }

  bool hasSelectivityEstimate() const noexcept override {
    // selectivity can only be determined per query since multiple fields are indexed
    return false;
  }

  arangodb::Result insert(
      arangodb::transaction::Methods* trx,
      arangodb::LocalDocumentId const& documentId,
      VPackSlice const& doc,
      Index::OperationMode mode
  ) override {
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  bool isPersistent() const noexcept override {
    return true;
  }

  bool isSorted() const noexcept override {
    // IResearch does not provide a fixed default sort order
    return false;
  }

  void load() noexcept {
    // NOOP
  }

  bool matchesDefinition(VPackSlice const& slice) const override {
    return IResearchLinkCoordinator::matchesDefinition(slice);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const noexcept override {
    return IResearchLinkCoordinator::memory();
  }

  arangodb::Result remove(
      arangodb::transaction::Methods*,
      arangodb::LocalDocumentId const&,
      VPackSlice const&,
      Index::OperationMode
  ) override {
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type enum value
  ////////////////////////////////////////////////////////////////////////////////
  Index::IndexType type() const noexcept override {
    return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type string value
  ////////////////////////////////////////////////////////////////////////////////
  char const* typeName() const noexcept override {
    return IResearchLinkHelper::type().c_str();
  }

  void toVelocyPack(
      arangodb::velocypack::Builder& builder,
      bool withFigures,
      bool /*forPersistence*/
  ) const {
    TRI_ASSERT(!builder.isOpenObject());
    builder.openObject();
    bool const success = IResearchLinkCoordinator::toVelocyPack(builder);
    TRI_ASSERT(success);

    if (withFigures) {
      VPackBuilder figuresBuilder;

      figuresBuilder.openObject();
      toVelocyPackFigures(figuresBuilder);
      figuresBuilder.close();
      builder.add("figures", figuresBuilder.slice());
    }

    builder.close();
  }

  void unload() noexcept override {
    // NOOP
  }
}; // IResearchLinkMMFilesCoordinator

class IResearchLinkRocksDBCoordinator final
    : public arangodb::RocksDBIndex,
      public IResearchLinkCoordinator {
 public:
  IResearchLinkRocksDBCoordinator(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection
  ) : arangodb::RocksDBIndex(
        iid,
        collection,
        IResearchLinkHelper::emptyIndexSlice(),
        arangodb::RocksDBColumnFamily::invalid(),
        false
      ),
      IResearchLinkCoordinator(iid, collection) {
    _unique = false; // cannot be unique since multiple fields are indexed
    _sparse = true;  // always sparse
  }

  bool allowExpansion() const noexcept {
    // maps to multivalued
    return true;
  }

  void batchInsert(
      arangodb::transaction::Methods*,
      std::vector<std::pair<arangodb::LocalDocumentId, VPackSlice>> const&,
      std::shared_ptr<LocalTaskQueue>
  ) override {
    TRI_ASSERT(false); // should not be called
  }

  bool canBeDropped() const noexcept override {
    return true;
  }

  int drop() {
    return IResearchLinkCoordinator::drop().errorNumber();
  }

  bool hasBatchInsert() const noexcept override {
    return true;
  }

  bool hasSelectivityEstimate() const noexcept override {
    // selectivity can only be determined per query since multiple fields are indexed
    return false;
  }

  arangodb::Result insertInternal(
      arangodb::transaction::Methods*,
      arangodb::RocksDBMethods*,
      arangodb::LocalDocumentId const&,
      const VPackSlice&,
      OperationMode
  ) override {
    TRI_ASSERT(false); // should not be called
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  bool isSorted() const noexcept override {
    // IResearch does not provide a fixed default sort order
    return false;
  }

  void load() noexcept {
    // NOOP
  }

  bool matchesDefinition(VPackSlice const& slice) const override {
    IResearchLinkMeta rhs;
    std::string errorField;

    return rhs.init(slice, errorField) && _meta == rhs;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const noexcept override {
    return IResearchLinkCoordinator::memory();
  }

  arangodb::Result removeInternal(
      arangodb::transaction::Methods*,
      arangodb::RocksDBMethods*,
      arangodb::LocalDocumentId const&,
      const VPackSlice&,
      OperationMode
  ) override {
    TRI_ASSERT(false); // should not be called
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type enum value
  ////////////////////////////////////////////////////////////////////////////////
  Index::IndexType type() const noexcept override {
    return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type string value
  ////////////////////////////////////////////////////////////////////////////////
  char const* typeName() const noexcept override {
    return IResearchLinkHelper::type().c_str();
  }

  void toVelocyPack(
      arangodb::velocypack::Builder& builder,
      bool withFigures,
      bool /*forPersistence*/
  ) const {
    TRI_ASSERT(!builder.isOpenObject());
    builder.openObject();
    bool const success = IResearchLinkCoordinator::toVelocyPack(builder);
    TRI_ASSERT(success);

    if (withFigures) {
      VPackBuilder figuresBuilder;

      figuresBuilder.openObject();
      toVelocyPackFigures(figuresBuilder);
      figuresBuilder.close();
      builder.add("figures", figuresBuilder.slice());
    }

    builder.close();
  }

  void unload() noexcept override {
    // NOOP
  }
}; // IResearchLinkRocksDBCoordinator

}

namespace arangodb {
namespace iresearch {

/*static*/ std::shared_ptr<IResearchLinkCoordinator> IResearchLinkCoordinator::find(
    LogicalCollection const& collection,
    LogicalView const& view
) {
  for (auto& index : collection.getIndexes()) {
    if (!index || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
      continue; // not an iresearch Link
    }

    // TODO FIXME find a better way to retrieve an iResearch Link
    // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
    auto link = std::dynamic_pointer_cast<IResearchLinkCoordinator>(index);

    if (link && *link == view) {
      return link; // found required link
    }
  }

  return nullptr;
}

/*static*/ std::shared_ptr<Index> IResearchLinkCoordinator::createLinkMMFiles(
    arangodb::LogicalCollection* collection,
    arangodb::velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool /*isClusterConstructor*/
) noexcept {
  try {
    auto link = std::make_shared<IResearchLinkMMFilesCoordinator>(id, collection);

    return link->init(definition) ? link : nullptr;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view MMFiles link '" << id << "'" << e.what();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view MMFiles link '" << id << "'";
  }

  return nullptr;
}

/*static*/ std::shared_ptr<Index> IResearchLinkCoordinator::createLinkRocksDB(
    arangodb::LogicalCollection* collection,
    arangodb::velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool /*isClusterConstructor*/
) noexcept {
  try {
    auto link = std::make_shared<IResearchLinkRocksDBCoordinator>(id, collection);

    return link->init(definition) ? link : nullptr;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view RocksDB link '" << id << "'" << e.what();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view RocksDB link '" << id << "'";
  }

  return nullptr;
}

IResearchLinkCoordinator::IResearchLinkCoordinator(
    TRI_idx_iid_t id,
    LogicalCollection* collection
) noexcept : _collection(collection), _id(id) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  TRI_ASSERT(_collection);
}

Result IResearchLinkCoordinator::drop() {
  if (!_view) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED; // IResearchView required
  }

  // if the collection is in the process of being removed then drop it from the view
  if (_collection->deleted()) {
    // revalidate all links
    auto const result = _view->updateProperties(
      emptyObjectSlice(), true, false
    );

    if (!result.ok()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to force view link revalidation while unloading dropped IResearch link '" << _id
        << "' for IResearch view '" << _view->id() << "'";

      return result.errorNumber();
    }
  }

  // drop it from view
  return _view->drop(_collection->id());
}

bool IResearchLinkCoordinator::operator==(LogicalView const& view) const noexcept {
  return _view && _view->id() == view.id();
}

bool IResearchLinkCoordinator::init(VPackSlice definition) {
  std::string error;
  IResearchLinkMeta meta;

  if (!meta.init(definition, error)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "error parsing view link parameters from json: " << error;
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return false; // failed to parse metadata
  }

  auto const identifier = IResearchLinkHelper::getView(definition);

  if (identifier.isNumber() && uint64_t(identifier.getInt()) == identifier.getUInt()) {
    auto const viewId = identifier.getUInt();

    auto& vocbase = collection().vocbase();

    auto logicalView  = vocbase.lookupView(viewId);

    if (!logicalView
        || arangodb::iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "error looking up view '" << viewId << "': no such view";
      return false; // no such view
    }

    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto view = std::dynamic_pointer_cast<IResearchViewCoordinator>(logicalView);
    #else
      auto view = std::static_pointer_cast<IResearchViewCoordinator>(logicalView);
    #endif

    if (!view) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "error finding view: '" << viewId << "' for link '" << _id << "'";

      return false;
    }

    _view = view;

    return true;
  }

  LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "error finding view for link '" << _id << "'";
  TRI_set_errno(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  return false;
}

bool IResearchLinkCoordinator::matchesDefinition(VPackSlice const& slice) const {
  IResearchLinkMeta rhs;
  std::string errorField;

  return rhs.init(slice, errorField) && _meta == rhs;
}

bool IResearchLinkCoordinator::toVelocyPack(
    arangodb::velocypack::Builder& builder
) const {
  if (!builder.isOpenObject() || !_meta.json(builder)) {
    return false;
  }

  TRI_ASSERT(_view);

  builder.add("id", VPackValue(StringUtils::itoa(_id)));
  IResearchLinkHelper::setType(builder);
  IResearchLinkHelper::setView(builder, _view->id());

  return true;
}

} // iresearch
} // arangodb
