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
  inspection::NonNullOptional<std::vector<std::string>> shardsR2{std::nullopt};
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
    if constexpr (std::is_same_v<typename Inspector::Context,
                                 InspectAgencyContext>) {
      // The agency requires the CollectionID
      auto field = f.field(StaticStrings::DistributeShardsLike,
                           props.distributeShardsLikeCid)
                       .invariant(UtilityInvariants::isNonEmptyIfPresent);
      return field;
    } else {
      // The user gives the CollectionName
      return f
          .field(StaticStrings::DistributeShardsLike,
                 props.distributeShardsLike)
          .fallback(f.keep())
          .invariant(UtilityInvariants::isNonEmptyIfPresent);
    }
  });

  if constexpr (std::is_same_v<typename Inspector::Context,
                               InspectAgencyContext>) {
    return f.object(props).fields(
        f.field(StaticStrings::NumberOfShards, props.numberOfShards)
            .invariant(UtilityInvariants::isGreaterZeroIfPresent),
        std::move(distShardsLikeField),
        f.field(StaticStrings::ShardingStrategy, props.shardingStrategy)
            .invariant(UtilityInvariants::isValidShardingStrategyIfPresent),
        f.field(StaticStrings::ShardKeys, props.shardKeys).fallback(f.keep()),
        f.field("shardsR2", props.shardsR2).fallback(f.keep()),
        f.field(StaticStrings::GroupId, props.groupId).fallback(f.keep()));
  } else {
    // If the user specifies the shards list and groupId, we reject it.
    return f.object(props).fields(
        f.field(StaticStrings::NumberOfShards, props.numberOfShards)
            .invariant(UtilityInvariants::isGreaterZeroIfPresent),
        std::move(distShardsLikeField),
        f.field(StaticStrings::ShardingStrategy, props.shardingStrategy)
            .invariant(UtilityInvariants::isValidShardingStrategyIfPresent),
        f.field(StaticStrings::ShardKeys, props.shardKeys).fallback(f.keep()));
  }
}

}  // namespace arangodb
