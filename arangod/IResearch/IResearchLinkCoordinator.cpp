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

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "ClusterEngine/ClusterEngine.h"
#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchLinkHelper.h"
#include "IResearchViewCoordinator.h"
#include "Indexes/IndexFactory.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VelocyPackHelper.h"
#include "VocBase/LogicalCollection.h"

namespace {

arangodb::ClusterEngineType getEngineType() {
#ifdef ARANGODB_USE_GOOGLE_TESTS
  // during the unit tests there is a mock storage engine which cannot be casted
  // to a ClusterEngine at all. the only sensible way to find out the engine type is 
  // to try a dynamic_cast here and assume the MockEngine if the cast goes wrong
  auto engine = dynamic_cast<arangodb::ClusterEngine*>(arangodb::EngineSelectorFeature::ENGINE);
  if (engine != nullptr) {
    return engine->engineType();
  }
  return arangodb::ClusterEngineType::MockEngine;
#else
  return static_cast<arangodb::ClusterEngine*>(arangodb::EngineSelectorFeature::ENGINE)->engineType();
#endif
}

} // namespace

namespace arangodb {
namespace iresearch {

IResearchLinkCoordinator::IResearchLinkCoordinator(IndexId id, LogicalCollection& collection)
    : arangodb::ClusterIndex(id, collection, ::getEngineType(),
                             arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK,
                             IResearchLinkHelper::emptyIndexSlice()),
      IResearchLink(id, collection) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

void IResearchLinkCoordinator::toVelocyPack( // generate definition
    arangodb::velocypack::Builder& builder, // destination buffer
    std::underlying_type<arangodb::Index::Serialize>::type flags // definition flags
) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result(  // result
        TRI_ERROR_BAD_PARAMETER,              // code
        std::string("failed to generate link definition for arangosearch view "
                    "Cluster link '") +
            std::to_string(arangodb::Index::id().id()) + "'"));
  }

  auto forPersistence = // definition for persistence
    arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Internals);

  builder.openObject();

  if (!properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result(  // result
        TRI_ERROR_INTERNAL,                   // code
        std::string("failed to generate link definition for arangosearch view "
                    "Cluster link '") +
            std::to_string(arangodb::Index::id().id()) + "'"));
  }

  if (arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }

  builder.close();
}

IResearchLinkCoordinator::IndexFactory::IndexFactory(arangodb::application_features::ApplicationServer& server)
    : IndexTypeFactory(server) {}

bool IResearchLinkCoordinator::IndexFactory::equal(arangodb::velocypack::Slice const& lhs,
                                                   arangodb::velocypack::Slice const& rhs,
                                                   std::string const& dbname) const {
  return arangodb::iresearch::IResearchLinkHelper::equal(_server, lhs, rhs, dbname);
}

std::shared_ptr<arangodb::Index> IResearchLinkCoordinator::IndexFactory::instantiate(
    arangodb::LogicalCollection& collection, arangodb::velocypack::Slice const& definition,
    IndexId id, bool isClusterConstructor) const {
  auto link = std::shared_ptr<arangodb::iresearch::IResearchLinkCoordinator>(
      new arangodb::iresearch::IResearchLinkCoordinator(id, collection));
  auto res = link->init(definition);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return link;
}

arangodb::Result IResearchLinkCoordinator::IndexFactory::normalize(  // normalize definition
    arangodb::velocypack::Builder& normalized,  // normalized definition (out-param)
    arangodb::velocypack::Slice definition,  // source definition
    bool isCreation,                         // definition for index creation
    TRI_vocbase_t const& vocbase             // index vocbase
    ) const {
  return arangodb::iresearch::IResearchLinkHelper::normalize(  // normalize
      normalized, definition, isCreation, vocbase              // args
  );
}

std::shared_ptr<IResearchLinkCoordinator::IndexFactory> IResearchLinkCoordinator::createFactory(
    application_features::ApplicationServer& server) {
  return std::shared_ptr<IResearchLinkCoordinator::IndexFactory>(
      new IResearchLinkCoordinator::IndexFactory(server));
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
