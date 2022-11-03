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
  std::vector<std::string> shardKeys =
      std::vector<std::string>{StaticStrings::KeyString};

  bool operator==(ClusteringConstantProperties const& other) const = default;

  void applyDatabaseDefaults(DatabaseConfiguration const& config);

  [[nodiscard]] arangodb::Result validateDatabaseConfiguration(
      DatabaseConfiguration const& config) const;
};

template<class Inspector>
auto inspect(Inspector& f, ClusteringConstantProperties& props) {

  auto distShardsLikeField = std::invoke([&]() {
    // Make the Inspector ignore distributeShardsLike on write
    // if there is no value.

    if constexpr (!Inspector::isLoading) {
      // The agency requires the CollectionID
      auto field =
          f.field("distributeShardsLike", props.distributeShardsLikeCid)
              .invariant(UtilityInvariants::isNonEmptyIfPresent);
      return field;
    } else {
      // The user gives the CollectionName
      auto field = f.field("distributeShardsLike", props.distributeShardsLike)
                       .invariant(UtilityInvariants::isNonEmptyIfPresent);
      return std::move(field).fallback(f.keep());
    }
  });

  return f.object(props).fields(
      f.field("numberOfShards", props.numberOfShards)
          .invariant(UtilityInvariants::isGreaterZeroIfPresent),
      std::move(distShardsLikeField),
      f.field("shardingStrategy", props.shardingStrategy)
          .invariant(UtilityInvariants::isValidShardingStrategyIfPresent),
      f.field("shardKeys", props.shardKeys)
          .fallback(f.keep())
          .invariant(UtilityInvariants::areShardKeysValid)

  );
}

}
