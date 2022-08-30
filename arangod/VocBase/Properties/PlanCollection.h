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
#include "Inspection/Status.h"
#include "Inspection/VPackLoadInspector.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

#include <string>

namespace arangodb {
template<typename T>
class ResultT;

struct ServerDefaults;

namespace inspection {
struct Status;
}
namespace velocypack {
class Slice;
}

struct PlanCollection {
  struct Invariants {
    [[nodiscard]] static auto isNonEmpty(std::string const& value)
        -> inspection::Status;

    [[nodiscard]] static auto isGreaterZero(uint64_t const& value)
        -> inspection::Status;

    [[nodiscard]] static auto isValidShardingStrategy(std::string const& value)
        -> inspection::Status;

    [[nodiscard]] static auto isValidCollectionType(
        std::underlying_type_t<TRI_col_type_e> const& value)
        -> inspection::Status;
  };

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

  PlanCollection();

  static ResultT<PlanCollection> fromCreateAPIBody(
      arangodb::velocypack::Slice input,
      arangodb::ServerDefaults defaultValues);

  // Temporary method to handOver information from
  arangodb::velocypack::Builder toCollectionsCreate();

  std::string name;
  std::underlying_type_t<TRI_col_type_e> type;
  bool waitForSync;
  bool isSystem;
  bool doCompact;
  bool isVolatile;
  bool cacheEnabled;

  uint64_t numberOfShards;
  uint64_t replicationFactor;
  uint64_t writeConcern;
  std::string distributeShardsLike;
  std::string smartJoinAttribute;
  std::string shardingStrategy;
  std::string globallyUniqueId;

  std::vector<std::string> shardKeys;

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder computedValues;

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder schema;

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder keyOptions;

  // NOTE: These attributes are not documented
  bool syncByRevision;
  bool usesRevisionsAsDocumentIds;
  bool isSmart;
  bool isDisjoint;
  std::string smartGraphAttribute;
  // Deprecated, and not documented anymore

  std::string id;

  // TODO: Maybe this is better off with a transformator Uint -> col_type_e
  [[nodiscard]] TRI_col_type_e getType() const noexcept {
    return TRI_col_type_e(type);
  }
};

// Please note in the following inspect, there are some `f.keep()` calls
// This is used for parameters that have configurable defaults. The defaults
// are set on planCollection before calling the inspect.
template<class Inspector>
auto inspect(Inspector& f, PlanCollection& planCollection) {
  return f.object(planCollection)
      .fields(
          f.field("name", planCollection.name)
              .invariant(PlanCollection::Invariants::isNonEmpty),
          f.field("id", planCollection.id).fallback(""),
          f.field("waitForSync", planCollection.waitForSync).fallback(false),
          f.field("isSystem", planCollection.isSystem).fallback(false),
          f.field("doCompact", planCollection.doCompact).fallback(false),
          f.field("cacheEnabled", planCollection.cacheEnabled).fallback(false),
          f.field("isVolatile", planCollection.isVolatile).fallback(false),
          f.field("syncByRevision", planCollection.syncByRevision)
              .fallback(true),
          f.field("usesRevisionsAsDocumentIds",
                  planCollection.usesRevisionsAsDocumentIds)
              .fallback(true),
          f.field("isSmart", planCollection.isSmart).fallback(false),
          f.field("isDisjoint", planCollection.isDisjoint).fallback(false),
          f.field("smartGraphAttribute", planCollection.smartGraphAttribute)
              .fallback(""),
          f.field("numberOfShards", planCollection.numberOfShards)
              .fallback(f.keep())
              .invariant(PlanCollection::Invariants::isGreaterZero),
          // Deprecated, and not documented anymore
          // The ordering is important here, minReplicationFactor
          // has to be before writeConcern, this way we ensure that writeConcern
          // will overwrite the minReplicationFactor value if present
          f.field("minReplicationFactor", planCollection.writeConcern)
              .fallback(f.keep()),
          // Now check the new attribute, if it is not there,
          // fallback to minReplicationFactor / default, whatever
          // is set already.
          // Then do the invariant check, this should now cover both
          // values.
          f.field("writeConcern", planCollection.writeConcern)
              .fallback(f.keep())
              .invariant(PlanCollection::Invariants::isGreaterZero),
          f.field("replicationFactor", planCollection.replicationFactor)
              .fallback(f.keep())
              .transformWith(
                  PlanCollection::Transformers::ReplicationSatellite{}),
          f.field("distributeShardsLike", planCollection.distributeShardsLike)
              .fallback(""),
          f.field("smartJoinAttribute", planCollection.smartJoinAttribute)
              .fallback(""),
          f.field("globallyUniqueId", planCollection.globallyUniqueId)
              .fallback(""),
          f.field("shardingStrategy", planCollection.shardingStrategy)
              .fallback("")
              .invariant(PlanCollection::Invariants::isValidShardingStrategy),
          f.field("shardKeys", planCollection.shardKeys)
              .fallback(std::vector<std::string>{StaticStrings::KeyString}),
          f.field("type", planCollection.type)
              .fallback(TRI_col_type_e::TRI_COL_TYPE_DOCUMENT)
              .invariant(PlanCollection::Invariants::isValidCollectionType),
          f.field("schema", planCollection.schema)
              .fallback(VPackSlice::emptyObjectSlice()),
          f.field("keyOptions", planCollection.keyOptions)
              .fallback(VPackSlice::emptyObjectSlice()),
          f.field("computedValues", planCollection.computedValues)
              .fallback(VPackSlice::emptyArraySlice()));
}

}  // namespace arangodb
