////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Exceptions.h"

#include "Cluster/Utils/AgencyIsBuildingFlags.h"
#include "VocBase/Properties/CollectionIndexesProperties.h"
#include "VocBase/Properties/UserInputCollectionProperties.h"
#include "ShardDistribution.h"
#include "Cluster/Utils/PlanShardToServerMappping.h"
#include "Cluster/Utils/IShardDistributionFactory.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

struct PlanCollectionEntry {
  PlanCollectionEntry(UserInputCollectionProperties collection,
                      ShardDistribution shardDistribution,
                      AgencyIsBuildingFlags isBuildingFlags);

  [[nodiscard]] std::string getCID() const;

  [[nodiscard]] std::string const& getName() const;

  [[nodiscard]] bool requiresCurrentWatcher() const;

  // To be replaced by Inspect below, as soon as same-level fields are merged.
  [[nodiscard]] velocypack::Builder toVPackDeprecated() const;

  [[nodiscard]] PlanShardToServerMapping getShardMapping() const;

  // Remove the isBuilding flags, call it if we are completed
  void removeBuildingFlags();

 private:
  UserInputCollectionProperties _properties{};
  std::optional<AgencyIsBuildingFlags> _buildingFlags{AgencyIsBuildingFlags{}};
  CollectionIndexesProperties _indexProperties{};
  ShardDistribution _shardDistribution;
};

template<class Inspector>
auto inspect(Inspector& f, PlanCollectionEntry& planCollection) {
  // TODO This is waiting on inspector with fields on same toplevel object
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  return f.object(planCollection).fields();
}

}  // namespace arangodb
