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

#include "ClusteringConstantProperties.h"

#include "Basics/debugging.h"
#include "Basics/Result.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

using namespace arangodb;

void ClusteringConstantProperties::applyDatabaseDefaults(
    DatabaseConfiguration const& config) {
  // This will make sure the default configuration for Sharding attributes are
  // applied
  if (!numberOfShards.has_value()) {
    numberOfShards = config.defaultNumberOfShards;
  }

  if (!shardKeys.has_value()) {
    shardKeys = std::vector<std::string>{StaticStrings::KeyString};
  }
}

[[nodiscard]] arangodb::Result
ClusteringConstantProperties::validateDatabaseConfiguration(
    DatabaseConfiguration const& config) const {
  // When we call validate, all default values have been applied
  TRI_ASSERT(numberOfShards.has_value());
  if (config.shouldValidateClusterSettings) {
    if (config.maxNumberOfShards > 0 &&
        numberOfShards.value() > config.maxNumberOfShards) {
      return {TRI_ERROR_CLUSTER_TOO_MANY_SHARDS,
              std::string("too many shards. maximum number of shards is ") +
                  std::to_string(config.maxNumberOfShards)};
    }
  }

  if (config.isOneShardDB) {
    if (numberOfShards.value() != 1) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Collection in a 'oneShardDatabase' must have 1 shard"};
    }
  }

  return {TRI_ERROR_NO_ERROR};
}
