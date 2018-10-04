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

IResearchLinkCoordinator::IResearchLinkCoordinator(
    TRI_idx_iid_t id,
    LogicalCollection& collection
): arangodb::Index(id, collection, IResearchLinkHelper::emptyIndexSlice()) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  _unique = false; // cannot be unique since multiple fields are indexed
  _sparse = true;  // always sparse
}

bool IResearchLinkCoordinator::operator==(LogicalView const& view) const noexcept {
  return _view && _view->id() == view.id();
}

bool IResearchLinkCoordinator::init(VPackSlice definition) {
  std::string error;

  if (!_meta.init(definition, error)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "error parsing view link parameters from json: " << error;
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return false; // failed to parse metadata
  }

  if (!definition.isObject()
      || !(definition.get(StaticStrings::ViewIdField).isString() ||
           definition.get(StaticStrings::ViewIdField).isNumber())) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "error finding view for link '" << id() << "'";
    TRI_set_errno(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

    return false;
  }

  auto idSlice = definition.get(StaticStrings::ViewIdField);
  std::string viewId = idSlice.isString() ? idSlice.copyString() : std::to_string(idSlice.getUInt());
  auto& vocbase = _collection.vocbase();

  TRI_ASSERT(ClusterInfo::instance());
  auto logicalView  = ClusterInfo::instance()->getView(vocbase.name(), viewId);

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
        << "error finding view '" << viewId << "' for link '" << id() << "'";

    return false;
  }

  if (!view->emplace(_collection.id(), _collection.name(), definition)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "error emplacing link to collection '" << _collection.name() << "' into IResearch view '" << viewId << "' link '" << id() << "'";

    return false;
  }

  _view = view;

  return true;
}

/*static*/ IResearchLinkCoordinator::ptr IResearchLinkCoordinator::make(
  arangodb::LogicalCollection& collection,
  arangodb::velocypack::Slice const& definition,
  TRI_idx_iid_t id,
  bool // isClusterConstructor
) noexcept {
  try {
    PTR_NAMED(IResearchLinkCoordinator, ptr, id, collection);

    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto* link = dynamic_cast<IResearchLinkCoordinator*>(ptr.get());
    #else
      auto* link = static_cast<IResearchLinkCoordinator*>(ptr.get());
    #endif

    return link && link->init(definition) ? ptr : nullptr;
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while creating IResearch view Coordinator link '" << id << "': " << e.code() << " "  << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while creating IResearch view Coordinator link '" << id << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while creating IResearch view Coordinator link '" << id << "'";
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

bool IResearchLinkCoordinator::matchesDefinition(VPackSlice const& slice) const {
  IResearchLinkMeta rhs;
  std::string errorField;

  return rhs.init(slice, errorField) && _meta == rhs;
}

void IResearchLinkCoordinator::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type flags
) const {
  TRI_ASSERT(_view);
  TRI_ASSERT(!builder.isOpenObject());
  builder.openObject();

  bool success = _meta.json(builder);

  TRI_ASSERT(success);
  builder.add(
    arangodb::StaticStrings::IndexId,
    arangodb::velocypack::Value(std::to_string(id()))
  );
  builder.add(
    arangodb::StaticStrings::IndexType,
    arangodb::velocypack::Value(IResearchLinkHelper::type())
  );
  builder.add(
    StaticStrings::ViewIdField,
    arangodb::velocypack::Value(_view->guid())
  );

  if (arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Figures)) {
    builder.add(
      "figures",
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
    );
    toVelocyPackFigures(builder);
    builder.close();
  }

  builder.close();
}

char const* IResearchLinkCoordinator::typeName() const {
  return IResearchLinkHelper::type().c_str();
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
