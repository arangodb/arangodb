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

#include "IResearchCommon.h"
#include "IResearchLinkHelper.h"
#include "IResearchView.h"
#include "Cluster/ServerState.h"
#include "Indexes/IndexFactory.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "MMFiles/MMFilesCollection.h"
#include "VocBase/LogicalCollection.h"

#include "IResearchMMFilesLink.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchLinkCoordinator-specific implementation of an
///        IndexTypeFactory
////////////////////////////////////////////////////////////////////////////////
struct IResearchMMFilesLink::IndexFactory: public arangodb::IndexTypeFactory {
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
      // ensure loaded so that we have valid data in next check
      if (TRI_VOC_COL_STATUS_LOADED != collection.status()) {
        collection.load();
      }

      // try casting underlying collection to an MMFilesCollection
      // this may not succeed because we may have to deal with a PhysicalCollectionMock here
      auto mmfilesCollection =
        dynamic_cast<arangodb::MMFilesCollection*>(collection.getPhysical());

      if (mmfilesCollection && !mmfilesCollection->hasAllPersistentLocalIds()) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          "mmfiles collection uses pre-3.4 format and cannot be linked to an arangosearch view; try recreating collection and moving the contents to the new collection"
        );
      }

      auto link = std::shared_ptr<IResearchMMFilesLink>(
        new IResearchMMFilesLink(id, collection)
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
        std::string("caught exception while creating arangosearch view MMFiles link '") + std::to_string(id) + "': " + e.what()
      );
    } catch (std::exception const& e) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception while creating arangosearch view MMFiles link '") + std::to_string(id) + "': " + e.what()
      );
    } catch (...) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception while creating arangosearch view MMFiles link '") + std::to_string(id) + "'"
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

IResearchMMFilesLink::IResearchMMFilesLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection
): Index(iid, collection, IResearchLinkHelper::emptyIndexSlice()),
   IResearchLink(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false; // cannot be unique since multiple fields are indexed
  _sparse = true;  // always sparse
}

IResearchMMFilesLink::~IResearchMMFilesLink() {
  // NOOP
}

/*static*/ arangodb::IndexTypeFactory const& IResearchMMFilesLink::factory() {
  static const IndexFactory factory;

  return factory;
}

void IResearchMMFilesLink::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type flags
) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failed to generate link definition for arangosearch view MMFiles link '") + std::to_string(arangodb::Index::id()) + "'"
    ));
  }

  builder.openObject();

  if (!json(builder)) {
    THROW_ARANGO_EXCEPTION(arangodb::Result(
      TRI_ERROR_INTERNAL,
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

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------