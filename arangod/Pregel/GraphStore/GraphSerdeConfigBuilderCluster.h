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
#pragma once

#include "Pregel/GraphStore/GraphSerdeConfigBuilder.h"

namespace arangodb {
class ClusterInfo;
}

namespace arangodb::pregel {

struct GraphSerdeConfigBuilderCluster : GraphSerdeConfigBuilderBase {
  GraphSerdeConfigBuilderCluster(TRI_vocbase_t& vocbase,
                                 GraphByCollections const& graphByCollections);

  [[nodiscard]] virtual auto edgeCollectionRestrictionsByShard() const
      -> ShardMap override;

  [[nodiscard]] virtual auto checkVertexCollections() const -> Result override;

  [[nodiscard]] virtual auto checkEdgeCollections() const -> Result override;

  [[nodiscard]] virtual auto loadableVertexShards() const
      -> LoadableVertexShards override;

  [[nodiscard]] virtual auto responsibleServerMap(
      LoadableVertexShards const& loadableVertexShards) const
      -> ResponsibleServerMap override;

  [[nodiscard]] auto getShardIds(ShardID collection) const
      -> std::vector<ShardID>;
  [[nodiscard]] auto resolveCollectionNameToIds(CollectionName name) const
      -> std::vector<DataSourceId>;

  TRI_vocbase_t& vocbase;
  ClusterInfo& clusterInfo;
  GraphByCollections const& graphByCollections;
};

}  // namespace arangodb::pregel
