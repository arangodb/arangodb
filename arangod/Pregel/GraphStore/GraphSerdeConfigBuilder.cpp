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

  auto loadableVertexShards = configBuilder->loadableVertexShards();
  auto responsibleServerMap =
      configBuilder->responsibleServerMap(loadableVertexShards);

  return errors::ErrorT<Result, GraphSerdeConfig>::ok(
      GraphSerdeConfig{.loadableVertexShards = loadableVertexShards,
                       .responsibleServerMap = responsibleServerMap});
}

}  // namespace arangodb::pregel
