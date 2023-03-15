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

#include "Basics/debugging.h"
#include "Basics/ResultT.h"
#include "VocBase/Properties/UserInputCollectionProperties.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

using namespace arangodb;

[[nodiscard]] arangodb::Result
ClusteringProperties::applyDefaultsAndValidateDatabaseConfiguration(
    DatabaseConfiguration const& config) {
  if (!distributeShardsLike.has_value()) {
    // DistributeShardsLike has been handled in the caller.
    // See UserInputCollectionProperties.cpp

    // Apply database defaults
    ClusteringMutableProperties::applyDatabaseDefaults(config);
    ClusteringConstantProperties::applyDatabaseDefaults(config);
  }

  // Validate that necessary values are now filled
  TRI_ASSERT(replicationFactor.has_value());
  TRI_ASSERT(writeConcern.has_value());
  TRI_ASSERT(numberOfShards.has_value());

  // Make sure we do not violate conditions anymore
  auto validationResult =
      ClusteringMutableProperties::validateDatabaseConfiguration(config);
  if (!validationResult.ok()) {
    return validationResult;
  }
  validationResult =
      ClusteringConstantProperties::validateDatabaseConfiguration(config);
  if (!validationResult.ok()) {
    return validationResult;
  }

  if (isSatellite()) {
    if (numberOfShards.value() != 1) {
      return {TRI_ERROR_BAD_PARAMETER,
              "A satellite collection can only have a single shard"};
    }
  }
  return TRI_ERROR_NO_ERROR;
}
