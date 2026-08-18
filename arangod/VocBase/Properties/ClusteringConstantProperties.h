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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/StaticStrings.h"
#include "Inspection/Access.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "VocBase/Properties/InspectContexts.h"
#include "VocBase/Properties/UtilityInvariants.h"

#include <optional>
#include <cstdint>

namespace arangodb {
struct DatabaseConfiguration;
class Result;

struct ClusteringConstantProperties {
  inspection::NonNullOptional<uint64_t> numberOfShards{std::nullopt};
  inspection::NonNullOptional<std::string> distributeShardsLike{std::nullopt};
  inspection::NonNullOptional<std::string> distributeShardsLikeCid{
      std::nullopt};
  std::optional<std::string> shardingStrategy =
      std::nullopt;  // defaultShardingStrategy
  inspection::NonNullOptional<std::vector<std::string>> shardKeys{std::nullopt};
  inspection::NonNullOptional<std::vector<ShardID>> shardsR2{std::nullopt};
  inspection::NonNullOptional<replication2::agency::CollectionGroupId> groupId{
      std::nullopt};

  bool operator==(ClusteringConstantProperties const& other) const = default;

  void applyDatabaseDefaults(DatabaseConfiguration const& config);

  [[nodiscard]] arangodb::Result validateDatabaseConfiguration(
      DatabaseConfiguration const& config) const;
};

template<class Inspector>
auto inspect(Inspector& f, ClusteringConstantProperties& props) {
  auto distShardsLikeField = std::invoke([&]() {
    if constexpr (isAgencyContext<Inspector> || isInternalContext<Inspector>) {
      // The agency requires the CollectionID
      return userInvariant(f,
                           f.field(StaticStrings::DistributeShardsLike,
                                   props.distributeShardsLikeCid),
                           UtilityInvariants::isNonEmptyIfPresent);
    } else {
      // The user gives the CollectionName
      return userInvariant(f,
                           f.field(StaticStrings::DistributeShardsLike,
                                   props.distributeShardsLike)
                               .fallback(f.keep()),
                           UtilityInvariants::isNonEmptyIfPresent);
    }
  });

  auto numberOfShardsField = userInvariant(
      f, f.field(StaticStrings::NumberOfShards, props.numberOfShards),
      UtilityInvariants::isGreaterZeroIfPresent);

  auto shardingStrategyField = userInvariant(
      f, f.field(StaticStrings::ShardingStrategy, props.shardingStrategy),
      UtilityInvariants::isValidShardingStrategyIfPresent);

  if constexpr (isAgencyContext<Inspector> || isInternalContext<Inspector>) {
    return f.object(props).fields(
        std::move(numberOfShardsField), std::move(distShardsLikeField),
        std::move(shardingStrategyField),
        f.field(StaticStrings::ShardKeys, props.shardKeys).fallback(f.keep()),
        f.field("shardsR2", props.shardsR2).fallback(f.keep()),
        f.field(StaticStrings::GroupId, props.groupId).fallback(f.keep()));
  } else {
    // If the user specifies the shards list and groupId, we reject it.
    return f.object(props).fields(
        std::move(numberOfShardsField), std::move(distShardsLikeField),
        std::move(shardingStrategyField),
        f.field(StaticStrings::ShardKeys, props.shardKeys).fallback(f.keep()));
  }
}

}  // namespace arangodb
