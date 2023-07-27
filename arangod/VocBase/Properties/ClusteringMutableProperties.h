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

#include <cstdint>
#include <optional>

#include "Basics/StaticStrings.h"
#include "Inspection/Access.h"
#include "VocBase/Properties/UtilityInvariants.h"

namespace arangodb {

namespace velocypack {
class Builder;
}

struct DatabaseConfiguration;
class Result;

struct ClusteringMutableProperties {
  struct Transformers {
    struct ReplicationSatellite {
      using MemoryType = uint64_t;
      using SerializedType = arangodb::velocypack::Builder;

      static arangodb::inspection::Status toSerialized(MemoryType v,
                                                       SerializedType& result);

      static arangodb::inspection::Status fromSerialized(
          SerializedType const& v, MemoryType& result);
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
  return f.object(props)
      .fields(
          f.field(StaticStrings::WaitForSyncString, props.waitForSync)
              .fallback(f.keep()),
          // Deprecated, and not documented anymore
          // The ordering is important here, minReplicationFactor
          // has to be before writeConcern, this way we ensure that writeConcern
          // will overwrite the minReplicationFactor value if present
          f.field(StaticStrings::MinReplicationFactor, props.writeConcern)
              .fallback(f.keep()),
          // Now check the new attribute, if it is not there,
          // fallback to minReplicationFactor / default, whatever
          // is set already.
          // Then do the invariant check, this should now cover both
          // values.
          f.field(StaticStrings::WriteConcern, props.writeConcern)
              .fallback(f.keep())
              .invariant(UtilityInvariants::isGreaterOrEqualZeroIfPresent),
          f.field(StaticStrings::ReplicationFactor, props.replicationFactor)
              .transformWith(ClusteringMutableProperties::Transformers::
                                 ReplicationSatellite{}))
      .invariant(ClusteringMutableProperties::Invariants::
                     writeConcernAllowedToBeZeroForSatellite);
}
}  // namespace arangodb
