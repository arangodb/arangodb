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
#include "Inspection/Types.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "VocBase/Properties/InspectContexts.h"

#include <optional>
#include <cstdint>

namespace arangodb {
struct DatabaseConfiguration;
class Result;

struct ClusteringConstantProperties {
  // null must load as unset (legacy markers), and unset must omit the key --
  // std::optional without a fallback does both
  std::optional<uint64_t> numberOfShards{std::nullopt};
  inspection::NonNullOptional<std::string> distributeShardsLike{
      std::nullopt};  // For create path, this is a cid after
                      // applyDefaultsAndValidate has run; for load path, this
                      // is always a cid.
  std::optional<std::string> shardingStrategy = std::nullopt;
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
  // Written by the server only. Reject keeps the create API answering with an
  // unexpected-attribute error, which is what leaving them undeclared did.
  auto serverOwned = []() {
    return isAgencyContext<Inspector> || isInternalContext<Inspector>
               ? inspection::FieldCondition::Process
               : inspection::FieldCondition::Reject;
  };

  return f.object(props).fields(
      f.field(StaticStrings::NumberOfShards, props.numberOfShards),
      f.field(StaticStrings::DistributeShardsLike, props.distributeShardsLike)
          .fallback(f.keep()),
      f.field(StaticStrings::ShardingStrategy, props.shardingStrategy),
      f.field(StaticStrings::ShardKeys, props.shardKeys).fallback(f.keep()),
      f.field("shardsR2", props.shardsR2).fallback(f.keep()).when(serverOwned),
      f.field(StaticStrings::GroupId, props.groupId)
          .fallback(f.keep())
          .when(serverOwned));
}

}  // namespace arangodb
