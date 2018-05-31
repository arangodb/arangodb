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
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include "IResearchMMFilesLink.h"
#include "IResearchLinkHelper.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchMMFilesLink::IResearchMMFilesLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection
): Index(iid, collection, IResearchLinkHelper::emptyIndexSlice()),
   IResearchLink(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false; // cannot be unique since multiple fields are indexed
  _sparse = true;  // always sparse
}

IResearchMMFilesLink::~IResearchMMFilesLink() {
  // NOOP
}

/*static*/ IResearchMMFilesLink::ptr IResearchMMFilesLink::make(
    arangodb::LogicalCollection* collection,
    arangodb::velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
) noexcept {
  try {
    PTR_NAMED(IResearchMMFilesLink, ptr, id, collection);

    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto* link = dynamic_cast<arangodb::iresearch::IResearchMMFilesLink*>(ptr.get());
    #else
      auto* link = static_cast<arangodb::iresearch::IResearchMMFilesLink*>(ptr.get());
    #endif

    return link && link->init(definition) ? ptr : nullptr;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view MMFiles link '" << id << "'" << e.what();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view MMFiles link '" << id << "'";
  }

  return nullptr;
}

void IResearchMMFilesLink::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    bool withFigures,
    bool forPersistence
) const {
  TRI_ASSERT(!builder.isOpenObject());
  builder.openObject();
  bool success = json(builder, forPersistence);
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

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
