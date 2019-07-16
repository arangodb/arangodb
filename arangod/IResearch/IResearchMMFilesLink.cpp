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

#include "Cluster/ServerState.h"
#include "IResearchCommon.h"
#include "IResearchLinkHelper.h"
#include "IResearchView.h"
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollection.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/LogicalCollection.h"

#include "IResearchMMFilesLink.h"

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchMMFilesLink-specific implementation of an IndexTypeFactory
////////////////////////////////////////////////////////////////////////////////
struct IResearchMMFilesLink::IndexFactory : public arangodb::IndexTypeFactory {
  bool equal(arangodb::velocypack::Slice const& lhs,
             arangodb::velocypack::Slice const& rhs) const override {
    return arangodb::iresearch::IResearchLinkHelper::equal(lhs, rhs);
  }

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
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
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
          "mmfiles collection uses pre-3.4 format and cannot be linked to an "
          "arangosearch view; try recreating collection and moving the "
          "contents to the new collection");
    }

    auto link =
        std::shared_ptr<IResearchMMFilesLink>(new IResearchMMFilesLink(id, collection));
    auto res = link->init(definition);

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    return link;
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    return IResearchLinkHelper::normalize( // normalize
      normalized, definition, isCreation, vocbase // args
    );
  }
};

IResearchMMFilesLink::IResearchMMFilesLink(TRI_idx_iid_t iid,
                                           arangodb::LogicalCollection& collection)
    : MMFilesIndex(iid, collection, IResearchLinkHelper::emptyIndexSlice()),
      IResearchLink(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

/*static*/ arangodb::IndexTypeFactory const& IResearchMMFilesLink::factory() {
  static const IndexFactory factory;

  return factory;
}

void IResearchMMFilesLink::toVelocyPack( // generate definition
    arangodb::velocypack::Builder& builder, // destination buffer
    std::underlying_type<arangodb::Index::Serialize>::type flags // definition flags
) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("failed to generate link definition for arangosearch view MMFiles link '") + std::to_string(arangodb::Index::id()) + "'"
    ));
  }

  auto forPersistence = // definition for persistence
    arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Internals);

  builder.openObject();

  if (!properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("failed to generate link definition for arangosearch view MMFiles link '") + std::to_string(arangodb::Index::id()) + "'"
    ));
  }

  if (arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Figures)) {
    VPackBuilder figuresBuilder;

    figuresBuilder.openObject();
    toVelocyPackFigures(figuresBuilder);
    figuresBuilder.close();
    builder.add("figures", figuresBuilder.slice());
  }

  builder.close();
}

bool IResearchMMFilesLink::isPersistent() const {
  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  // FIXME TODO remove once MMFilesEngine will fillIndex(...) during recovery
  // currently the index is created but fill is deferred until the end of
  // recovery at the end of recovery only non-persistent indexes are filled
  if (engine && engine->inRecovery()) {
    return false;
  }

  return true;  // records persisted into the iResearch view
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
