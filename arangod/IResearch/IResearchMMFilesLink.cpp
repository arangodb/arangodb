////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "IResearchMMFilesLink.h"

#include "Cluster/ServerState.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollection.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
namespace iresearch {

IResearchMMFilesLink::IResearchMMFilesLink(IndexId iid, arangodb::LogicalCollection& collection)
    : MMFilesIndex(iid, collection, IResearchLinkHelper::emptyIndexSlice()),
      IResearchLink(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

void IResearchMMFilesLink::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type flags) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result(  // result
        TRI_ERROR_BAD_PARAMETER,              // code
        std::string("failed to generate link definition for arangosearch view "
                    "MMFiles link '") +
            std::to_string(arangodb::Index::id().id()) + "'"));
  }

  auto forPersistence = // definition for persistence
    arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Internals);

  builder.openObject();

  if (!properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result(  // result
        TRI_ERROR_INTERNAL,                   // code
        std::string("failed to generate link definition for arangosearch view "
                    "MMFiles link '") +
            std::to_string(arangodb::Index::id().id()) + "'"));
  }

  if (arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }

  builder.close();
}

bool IResearchMMFilesLink::isPersistent() const {
  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  if (engine && engine->inRecovery()) {
    // FIXME
    // Remove once MMFilesEngine will fillIndex(...) during recovery.
    // Currently the index is created but fill is deferred until the end of
    // recovery. At the end of recovery only non-persistent indexes are filled,
    // that's why we pretend link to be non-persistent if it was created during
    // recovery.
    return !IResearchLink::createdInRecovery();
  }

  return true;
}

IResearchMMFilesLink::IndexFactory::IndexFactory(arangodb::application_features::ApplicationServer& server)
    : IndexTypeFactory(server) {}

bool IResearchMMFilesLink::IndexFactory::equal(arangodb::velocypack::Slice const& lhs,
                                               arangodb::velocypack::Slice const& rhs) const {
  return arangodb::iresearch::IResearchLinkHelper::equal(_server, lhs, rhs);
}

std::shared_ptr<arangodb::Index> IResearchMMFilesLink::IndexFactory::instantiate(
    arangodb::LogicalCollection& collection, arangodb::velocypack::Slice const& definition,
    IndexId id, bool isClusterConstructor) const {
  // ensure loaded so that we have valid data in next check
  if (TRI_VOC_COL_STATUS_LOADED != collection.status()) {
    collection.load();
  }

  // try casting underlying collection to an MMFilesCollection
  // this may not succeed because we may have to deal with a
  // PhysicalCollectionMock here
  auto mmfilesCollection =
      dynamic_cast<arangodb::MMFilesCollection*>(collection.getPhysical());

  if (mmfilesCollection && !mmfilesCollection->hasAllPersistentLocalIds()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "mmfiles collection uses pre-3.4 format and cannot be linked to an "
        "arangosearch view; try recreating collection and moving the "
        "contents to the new collection");
  }

  auto link = std::shared_ptr<arangodb::iresearch::IResearchMMFilesLink>(
      new arangodb::iresearch::IResearchMMFilesLink(id, collection));
  auto res = link->init(definition);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return link;
}

arangodb::Result IResearchMMFilesLink::IndexFactory::normalize(  // normalize definition
    arangodb::velocypack::Builder& normalized,  // normalized definition (out-param)
    arangodb::velocypack::Slice definition,  // source definition
    bool isCreation,                         // definition for index creation
    TRI_vocbase_t const& vocbase             // index vocbase
    ) const {
  return arangodb::iresearch::IResearchLinkHelper::normalize(  // normalize
      normalized, definition, isCreation, vocbase              // args
  );
}

std::shared_ptr<IResearchMMFilesLink::IndexFactory> IResearchMMFilesLink::createFactory(
    application_features::ApplicationServer& server) {
  return std::shared_ptr<IResearchMMFilesLink::IndexFactory>(
      new IResearchMMFilesLink::IndexFactory(server));
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
