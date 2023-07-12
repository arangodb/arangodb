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

#include "ClusteringMutableProperties.h"

#include "Basics/StaticStrings.h"
#include "Basics/Result.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

#include "Inspection/VPack.h"
#include <velocypack/Builder.h>

using namespace arangodb;

auto ClusteringMutableProperties::Transformers::ReplicationSatellite::
    toSerialized(MemoryType v, SerializedType& result)
        -> arangodb::inspection::Status {
  if (v == 0) {
    result.add(VPackValue(StaticStrings::Satellite));
  } else {
    result.add(VPackValue(v));
  }
  return {};
}

auto ClusteringMutableProperties::Transformers::ReplicationSatellite::
    fromSerialized(SerializedType const& b, MemoryType& result)
        -> arangodb::inspection::Status {
  auto v = b.slice();
  if (v.isString() && v.isEqualString(StaticStrings::Satellite)) {
    result = 0;
    return {};
  } else if (v.isNumber()) {
    try {
      result = v.getNumber<MemoryType>();
      if (result != 0) {
        return {};
      }
    } catch (...) {
      // intentionally fall through. We got disallowed number type (e.g. negative value)
    }
  }
  return {"Only an integer number or 'satellite' is allowed"};
}

[[nodiscard]] bool ClusteringMutableProperties::isSatellite() const noexcept {
  TRI_ASSERT(replicationFactor.has_value());
  return replicationFactor.has_value() && replicationFactor.value() == 0;
}

void ClusteringMutableProperties::applyDatabaseDefaults(
    DatabaseConfiguration const& config) {
  if (!replicationFactor.has_value()) {
    replicationFactor = config.defaultReplicationFactor;
  }
  if (!writeConcern.has_value()) {
    if (isSatellite()) {
      // Satellites can only have writeConcern 1
      writeConcern = 1;
    } else {
      writeConcern = config.defaultWriteConcern;
    }
  }
}

[[nodiscard]] arangodb::Result
ClusteringMutableProperties::validateDatabaseConfiguration(
    DatabaseConfiguration const& config) const {
  TRI_ASSERT(replicationFactor.has_value());
  // Check Replication factor
  if (replicationFactor.has_value()) {
    if (config.enforceReplicationFactor) {
      if (config.maxReplicationFactor > 0 &&
          replicationFactor.value() > config.maxReplicationFactor) {
        return {TRI_ERROR_BAD_PARAMETER,
                std::string("replicationFactor must not be higher than "
                            "maximum allowed replicationFactor (") +
                    std::to_string(config.maxReplicationFactor) + ")"};
      }

      if (!isSatellite() &&
          replicationFactor.value() < config.minReplicationFactor) {
        return {TRI_ERROR_BAD_PARAMETER,
                std::string("replicationFactor must not be lower than "
                            "minimum allowed replicationFactor (") +
                    std::to_string(config.minReplicationFactor) + ")"};
      }
    }

    if (writeConcern.has_value()) {
      if (!isSatellite() && replicationFactor.value() < writeConcern.value()) {
        return {TRI_ERROR_BAD_PARAMETER,
                "writeConcern must not be higher than replicationFactor"};
      }
      if (isSatellite()) {
        // NOTE: Some APIS set writeConcern 1
        // Some set writeConcern 0
        // In order to support backwards compatibility
        // we allow both here. IMO 1 is correct.
        if (writeConcern.value() > 1ull) {
          return {TRI_ERROR_BAD_PARAMETER,
                  "For a satellite collection writeConcern must not be set"};
        }
      }
    }
  }

  if (config.isOneShardDB && isSatellite()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "Collection in a 'oneShardDatabase' cannot be a "
            "'satellite'"};
  }
#ifndef USE_ENTERPRISE
  if (isSatellite()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "'satellite' collections only allowed in enterprise edition"};
  }
#endif
  return {TRI_ERROR_NO_ERROR};
}
