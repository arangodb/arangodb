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

#include "CollectionProperties.h"

#include "Basics/Result.h"
#include "Utilities/NameValidator.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

using namespace arangodb;

[[nodiscard]] arangodb::Result
CollectionProperties::applyDefaultsAndValidateDatabaseConfiguration(
    DatabaseConfiguration const& config) {
  //  Check name is allowed
  if (!CollectionNameValidator::isAllowedName(
          isSystem, config.allowExtendedNames, name)) {
    return {TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }

  auto res = CollectionInternalProperties::
      applyDefaultsAndValidateDatabaseConfiguration(config);
  if (res.fail()) {
    return res;
  }
  res = ClusteringProperties::applyDefaultsAndValidateDatabaseConfiguration(
      config);
  if (res.fail()) {
    return res;
  }

  if (isSatellite()) {
    // We are a satellite, we cannot be smart at the same time
    if (isSmart) {
      return {TRI_ERROR_BAD_PARAMETER,
              "'isSmart' and replicationFactor 'satellite' cannot be combined"};
    }
    if (isSmartChild) {
      return {TRI_ERROR_BAD_PARAMETER,
              "'isSmartChild' and replicationFactor 'satellite' cannot be "
              "combined"};
    }
    if (shardKeys.size() != 1 || shardKeys[0] != StaticStrings::KeyString) {
      return {TRI_ERROR_BAD_PARAMETER, "'satellite' cannot use shardKeys"};
    }
  }
  return {TRI_ERROR_NO_ERROR};
}
