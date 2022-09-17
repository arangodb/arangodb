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
#include "VocBase/Properties/UtilityInvariants.h"

#include <velocypack/Builder.h>

namespace arangodb {

namespace velocypack {
class Builder;
}

struct CollectionMutableProperties {
  std::string name = StaticStrings::Empty;
  bool waitForSync = false;

  uint64_t replicationFactor = 1;
  uint64_t writeConcern = 1;
  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder computedValues =
      VPackBuilder{VPackSlice::emptyArraySlice()};

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder schema =
      VPackBuilder{VPackSlice::emptyObjectSlice()};

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
};

template<class Inspector>
auto inspect(Inspector& f, CollectionMutableProperties& props) {
  return f.object(props).fields(
      f.field("name", props.name)
          .fallback(f.keep())
          .invariant(UtilityInvariants::isNonEmpty),
      f.field("waitForSync", props.waitForSync).fallback(f.keep()),
      // Deprecated, and not documented anymore
      // The ordering is important here, minReplicationFactor
      // has to be before writeConcern, this way we ensure that writeConcern
      // will overwrite the minReplicationFactor value if present
      f.field("minReplicationFactor", props.writeConcern).fallback(f.keep()),
      // Now check the new attribute, if it is not there,
      // fallback to minReplicationFactor / default, whatever
      // is set already.
      // Then do the invariant check, this should now cover both
      // values.
      f.field("writeConcern", props.writeConcern)
          .fallback(f.keep())
          .invariant(UtilityInvariants::isGreaterZero),
      f.field("replicationFactor", props.replicationFactor)
          .fallback(f.keep())
          .transformWith(CollectionMutableProperties::Transformers::
                             ReplicationSatellite{}),
      f.field("schema", props.schema).fallback(f.keep()),
      f.field("computedValues", props.computedValues).fallback(f.keep()));
}

}  // namespace arangodb
