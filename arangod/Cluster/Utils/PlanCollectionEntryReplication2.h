////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "ShardDistribution.h"
#include "Cluster/Utils/AgencyIsBuildingFlags.h"
#include "VocBase/Properties/CollectionIndexesProperties.h"
#include "VocBase/Properties/UserInputCollectionProperties.h"
#include "Cluster/Utils/PlanShardToServerMappping.h"
#include "Cluster/Utils/IShardDistributionFactory.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "Replication2/AgencyCollectionSpecificationInspectors.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace replication2::agency {
struct LogTarget;
}

struct PlanCollectionEntryReplication2 {
  explicit PlanCollectionEntryReplication2(
      UserInputCollectionProperties collection);

  [[nodiscard]] std::string getCID() const;

  [[nodiscard]] std::string const& getName() const;

  replication2::agency::CollectionTargetSpecification _properties{};
};

template<class Inspector>
auto inspect(Inspector& f, PlanCollectionEntryReplication2& planCollection) {
  return f.apply(planCollection._properties);
}

}  // namespace arangodb
