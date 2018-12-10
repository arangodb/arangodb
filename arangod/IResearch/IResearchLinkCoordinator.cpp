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
#include "Basics/StringUtils.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Cluster/ClusterInfo.h"
#include "Indexes/IndexFactory.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchLinkCoordinator-specific implementation of an
///        IndexTypeFactory
////////////////////////////////////////////////////////////////////////////////
struct IResearchLinkCoordinator::IndexFactory: public arangodb::IndexTypeFactory {
  virtual bool equal(
      arangodb::velocypack::Slice const& lhs,
      arangodb::velocypack::Slice const& rhs
  ) const override {
    return arangodb::iresearch::IResearchLinkHelper::equal(lhs, rhs);
  }

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    try {
      auto link = std::shared_ptr<IResearchLinkCoordinator>(
        new IResearchLinkCoordinator(id, collection)
      );
      auto res = link->init(definition);

      if (!res.ok()) {
        return res;
      }

      index = link;
    } catch (arangodb::basics::Exception const& e) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(
        e.code(),
        std::string("caught exception while creating arangosearch view Coordinator link '") + std::to_string(id) + "': " + e.what()
      );
    } catch (std::exception const& e) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception while creating arangosearch view Coordinator link '") + std::to_string(id) + "': " + e.what()
      );
    } catch (...) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception while creating arangosearch view Coordinator link '") + std::to_string(id) + "'"
      );
    }

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    return IResearchLinkHelper::normalize(normalized, definition, isCreation);
  }
};

/*static*/ arangodb::IndexTypeFactory const& IResearchLinkCoordinator::factory() {
  static const IndexFactory factory;

  return factory;
}

IResearchLinkCoordinator::IResearchLinkCoordinator(
    TRI_idx_iid_t id,
    LogicalCollection& collection
): arangodb::ClusterIndex(
     id,
     collection,
     static_cast<arangodb::ClusterEngine*>(arangodb::EngineSelectorFeature::ENGINE)->engineType(),
     arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK,
     IResearchLinkHelper::emptyIndexSlice()
   ),
   IResearchLink(id, collection) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  _unique = false; // cannot be unique since multiple fields are indexed
  _sparse = true;  // always sparse
}

bool IResearchLinkCoordinator::operator==(LogicalView const& view) const noexcept {
  return _view && _view->id() == view.id();
}

arangodb::Result IResearchLinkCoordinator::init(
    arangodb::velocypack::Slice const& definition
) {
  std::string error;

  if (!_meta.init(definition, error)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error parsing view link parameters from json: ") + error
    );
  }

  if (!definition.isObject()
      || !definition.get(StaticStrings::ViewIdField).isString()) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      std::string("error finding view for link '") + std::to_string(Index::id()) + "'"
    );
  }

  auto idSlice = definition.get(StaticStrings::ViewIdField);
  auto viewId = idSlice.copyString();
  auto& vocbase = Index::collection().vocbase();

  TRI_ASSERT(ClusterInfo::instance());
  auto logicalView  = ClusterInfo::instance()->getView(vocbase.name(), viewId);

  if (!logicalView
      || arangodb::iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      std::string("error looking up view '") + viewId + "': no such view"
    );
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto view = std::dynamic_pointer_cast<IResearchViewCoordinator>(logicalView);
#else
  auto view = std::static_pointer_cast<IResearchViewCoordinator>(logicalView);
#endif

  if (!view) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      std::string("error finding view '") + viewId + "' for link '" + std::to_string(Index::id()) + "'"
    );
  }

  if (!view->emplace(Index::collection().id(), Index::collection().name(), definition)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("error emplacing link to collection '") + Index::collection().name() + "' into arangosearch view '" + viewId + "' link '" + std::to_string(Index::id()) + "'"
    );
  }

  _view = view;

  IResearchLink::init(definition);

  return arangodb::Result();
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
    arangodb::velocypack::Value(std::to_string(Index::id()))
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