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
#include "VocBase/Properties/UtilityInvariants.h"
#include "VocBase/Properties/InspectContexts.h"

#include <cstdint>
#include <optional>

namespace arangodb {

namespace velocypack {
class Builder;
}

struct DatabaseConfiguration;
class Result;

struct ClusteringMutableProperties {
  struct Transformers {
    // Serialized form is a number, or the string "satellite" for 0. Every
    // writer spells 0 as "satellite", so a numeric 0 is rejected everywhere.
    struct ReplicationSatellite {
      using MemoryType = uint64_t;
      using SerializedType = arangodb::velocypack::Builder;
      arangodb::inspection::Status toSerialized(MemoryType v,
                                                SerializedType& result) const;
      arangodb::inspection::Status fromSerialized(SerializedType const& v,
                                                  MemoryType& result) const;
    };
  };

  struct Invariants {
    [[nodiscard]] static auto writeConcernAllowedToBeZeroForSatellite(
        ClusteringMutableProperties const& props) -> inspection::Status;
  };

  inspection::NonNullOptional<uint64_t> replicationFactor{std::nullopt};
  inspection::NonNullOptional<uint64_t> writeConcern{std::nullopt};

  bool waitForSync = false;

  bool operator==(ClusteringMutableProperties const& other) const = default;

  [[nodiscard]] bool isSatellite() const noexcept;

  void applyDatabaseDefaults(DatabaseConfiguration const& config);

  [[nodiscard]] arangodb::Result validateDatabaseConfiguration(
      DatabaseConfiguration const& config) const;
};

template<class Inspector>
auto inspect(Inspector& f, ClusteringMutableProperties& props) {
  auto result = f.object(props).fields(
      f.field(StaticStrings::WaitForSyncString, props.waitForSync)
          .fallback(f.keep()),
      // minReplicationFactor is deprecated, and not documented anymore
      // The ordering is important here, minReplicationFactor
      // has to be before writeConcern, this way we ensure that writeConcern
      // will overwrite the minReplicationFactor value if present
      f.field(StaticStrings::MinReplicationFactor, props.writeConcern)
          .fallback(f.keep()),
      // Now check the new attribute, if it is not there,
      // fallback to minReplicationFactor / default, whatever
      // is set already.
      f.field(StaticStrings::WriteConcern, props.writeConcern)
          .fallback(f.keep()),
      f.field(StaticStrings::ReplicationFactor, props.replicationFactor)
          .transformWith(
              ClusteringMutableProperties::Transformers::
                  ReplicationSatellite{}));

  if constexpr (isInternalContext<Inspector>) {
    // Not an invariant of the type: EE SmartGraph edge collections are
    // persisted with writeConcern == 0 and a non-satellite replicationFactor.
    // The rule only constrains what a user may ask for.
    return inspection::Status{std::move(result)};
  } else {
    return result.invariant(ClusteringMutableProperties::Invariants::
                                writeConcernAllowedToBeZeroForSatellite);
  }
}

}  // namespace arangodb
