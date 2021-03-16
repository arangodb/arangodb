////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "IResearchLinkMock.h"

#include "Cluster/ServerState.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
namespace iresearch {

IResearchLinkMock::IResearchLinkMock(IndexId iid, arangodb::LogicalCollection& collection)
    : Index(iid, collection, IResearchLinkHelper::emptyIndexSlice(0).slice()),
      IResearchLink(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

void IResearchLinkMock::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type flags) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result(  // result
        TRI_ERROR_BAD_PARAMETER,              // code
        std::string("failed to generate link definition for arangosearch view "
                    "link '") +
            std::to_string(arangodb::Index::id().id()) + "'"));
  }

  auto forPersistence = // definition for persistence
    arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Internals);

  builder.openObject();

  if (!properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result(  // result
        TRI_ERROR_INTERNAL,                   // code
        std::string("failed to generate link definition for arangosearch view "
                    "link '") +
            std::to_string(arangodb::Index::id().id()) + "'"));
  }

  if (arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }

  builder.close();
}

std::function<void(irs::directory&)> IResearchLinkMock::InitCallback;
}  // namespace iresearch
}  // namespace arangodb
