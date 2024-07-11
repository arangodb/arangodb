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

#include "ClusteringMutableProperties.h"

#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Inspection/VPack.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

#include <absl/strings/str_cat.h>
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
      // intentionally fall through. We got disallowed number type (e.g.
      // negative value)
    }
  }
  return {"Only an integer number or 'satellite' is allowed"};
}

[[nodiscard]] auto ClusteringMutableProperties::Invariants::
    writeConcernAllowedToBeZeroForSatellite(
        ClusteringMutableProperties const& props) -> inspection::Status {
  if (props.writeConcern.has_value() && props.writeConcern.value() == 0) {
    // Special case: We are allowed to give writeConcern 0 for satellites
    if (props.replicationFactor.has_value() && props.isSatellite()) {
      return inspection::Status::Success{};
    }
    return {"writeConcern has to be > 0"};
  }
  return inspection::Status::Success{};
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
                absl::StrCat("replicationFactor must not be higher than "
                             "maximum allowed replicationFactor (",
                             config.maxReplicationFactor, ")")};
      }

      if (!isSatellite() &&
          replicationFactor.value() < config.minReplicationFactor) {
        return {TRI_ERROR_BAD_PARAMETER,
                absl::StrCat("replicationFactor must not be lower than "
                             "minimum allowed replicationFactor (",
                             config.minReplicationFactor, ")")};
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
            "Collection in a OneShard database cannot have replicationFactor "
            "'satellite'"};
  }
#ifndef USE_ENTERPRISE
  if (isSatellite()) {
    return {TRI_ERROR_ONLY_ENTERPRISE,
            "'satellite' collections only allowed in Enterprise Edition"};
  }
#endif
  return {};
}
