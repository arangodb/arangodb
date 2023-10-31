////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/GraphStore/GraphSerdeConfigBuilder.h"

#include "Pregel/GraphStore/GraphSerdeConfigBuilderCluster.h"
#include "Pregel/GraphStore/GraphSerdeConfigBuilderSingleServer.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel {

[[nodiscard]] auto GraphSerdeConfigBuilderBase::construct(
    TRI_vocbase_t& vocbase, GraphByCollections const& graphByCollections)
    -> std::unique_ptr<GraphSerdeConfigBuilderBase> {
  ServerState* ss = ServerState::instance();

  switch (ss->getRole()) {
    case ServerState::ROLE_SINGLE:
      return std::make_unique<GraphSerdeConfigBuilderSingleServer>(
          vocbase, graphByCollections);
    case ServerState::ROLE_COORDINATOR:
      return std::make_unique<GraphSerdeConfigBuilderCluster>(
          vocbase, graphByCollections);
    default:
      ADB_PROD_ASSERT(false)
          << fmt::format("Cannot construct GraphSerdeConfigBuilder for role {}",
                         ss->getRole());
      return nullptr;
  }
}

auto buildGraphSerdeConfig(TRI_vocbase_t& vocbase,
                           GraphByCollections const& graphByCollections)
    -> errors::ErrorT<Result, GraphSerdeConfig> {
  auto configBuilder =
      GraphSerdeConfigBuilderBase::construct(vocbase, graphByCollections);
  ADB_PROD_ASSERT(configBuilder != nullptr);

  auto vertexCollectionsOk = configBuilder->checkVertexCollections();
  if (not vertexCollectionsOk.ok()) {
    return errors::ErrorT<Result, GraphSerdeConfig>::error(vertexCollectionsOk);
  }

  auto edgeCollectionsOk = configBuilder->checkEdgeCollections();
  if (not edgeCollectionsOk.ok()) {
    return errors::ErrorT<Result, GraphSerdeConfig>::error(edgeCollectionsOk);
  }

  return errors::ErrorT<Result, GraphSerdeConfig>::ok(GraphSerdeConfig{
      .loadableVertexShards = configBuilder->loadableVertexShards()});
}

// TODO: maybe this belongs into GraphSerdeConfigBuilderBase and should also be
// called from above?
auto checkUserPermissions(ExecContext const& execContext,
                          GraphByCollections graphByCollections,
                          bool wantToStoreResults) -> Result {
  if (!execContext.isSuperuser()) {
    for (std::string const& vc : graphByCollections.vertexCollections) {
      bool canWrite = execContext.canUseCollection(vc, auth::Level::RW);
      bool canRead = execContext.canUseCollection(vc, auth::Level::RO);
      if ((wantToStoreResults && !canWrite) || !canRead) {
        return Result{TRI_ERROR_FORBIDDEN};
      }
    }
    for (std::string const& ec : graphByCollections.edgeCollections) {
      bool canWrite = execContext.canUseCollection(ec, auth::Level::RW);
      bool canRead = execContext.canUseCollection(ec, auth::Level::RO);
      if ((wantToStoreResults && !canWrite) || !canRead) {
        return Result{TRI_ERROR_FORBIDDEN};
      }
    }
  }
  return {};
}

}  // namespace arangodb::pregel
