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

#include "ClusteringProperties.h"

#include "Basics/Result.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

using namespace arangodb;

[[nodiscard]] arangodb::Result
ClusteringProperties::applyDefaultsAndValidateDatabaseConfiguration(
    DatabaseConfiguration const& config) {
  // DistributeShardsLike has highest binding. We have to handle this first
  if (!config.defaultDistributeShardsLike.empty()) {
    if (!distributeShardsLike.has_value()) {
      distributeShardsLike = config.defaultDistributeShardsLike;
    } else {
      return {TRI_ERROR_NOT_IMPLEMENTED,
              "Have not yet implemented error messages if "
              "default-distShardLike mismatches user-value"};
    }
  }
  if (distributeShardsLike.has_value()) {
    return {TRI_ERROR_NOT_IMPLEMENTED,
            "Need to apply sharding values of distributeShardsLike partner"};
  } else {
    // Apply database defaults
    ClusteringMutableProperties::applyDatabaseDefaults(config);
    ClusteringConstantProperties::applyDatabaseDefaults(config);
  }

  // Make sure we do not violate conditions anymore
  auto mutableRes =
      ClusteringMutableProperties::validateDatabaseConfiguration(config);
  if (!mutableRes.ok()) {
    return mutableRes;
  }
  return ClusteringConstantProperties::validateDatabaseConfiguration(config);
}
