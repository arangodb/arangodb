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

#include "Pregel/GraphStore/GraphSerdeConfigBuilderSingleServer.h"

#include "Cluster/ServerState.h"
#include "Containers/Enumerate.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::pregel {

GraphSerdeConfigBuilderSingleServer::GraphSerdeConfigBuilderSingleServer(
    TRI_vocbase_t& vocbase, GraphByCollections const& graphByCollections)
    : vocbase(vocbase), graphByCollections(graphByCollections) {}

[[nodiscard]] auto GraphSerdeConfigBuilderSingleServer::checkVertexCollections()
    const -> Result {
  for (std::string const& name : graphByCollections.vertexCollections) {
    auto coll = vocbase.lookupCollection(name);

    if (coll == nullptr || coll->deleted()) {
      return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name};
    }
  }
  return Result{};
}

[[nodiscard]] auto GraphSerdeConfigBuilderSingleServer::checkEdgeCollections()
    const -> Result {
  for (std::string const& name : graphByCollections.edgeCollections) {
    auto coll = vocbase.lookupCollection(name);

    if (coll == nullptr || coll->deleted()) {
      return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name};
    }
  }
  return Result{};
}

[[nodiscard]] auto GraphSerdeConfigBuilderSingleServer::loadableVertexShards()
    const -> std::vector<LoadableVertexShard> {
  auto result = std::vector<LoadableVertexShard>{};
  auto const server = ServerState::instance()->getId();

  for (auto const [idx, vertexCollection] :
       enumerate(graphByCollections.vertexCollections)) {
    auto loadableVertexShard =
        LoadableVertexShard{.pregelShard = PregelShard(idx),
                            .vertexShard = vertexCollection,
                            .responsibleServer = server,
                            .collectionName = vertexCollection,
                            .edgeShards = {}};
    for (auto const& edgeCollection : graphByCollections.edgeCollections) {
      if (not graphByCollections.isRestricted(vertexCollection,
                                              edgeCollection)) {
        loadableVertexShard.edgeShards.emplace_back(edgeCollection);
      }
    }
    result.emplace_back(loadableVertexShard);
  }
  return result;
}

}  // namespace arangodb::pregel
