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
#include "VocBase/Properties/UtilityInvariants.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

#include <string>

namespace arangodb {
class Result;

template<typename T>
class ResultT;

namespace inspection {
struct Status;
}
namespace velocypack {
class Slice;
}

struct PlanCollection {
  struct DatabaseConfiguration {
#if ARANGODB_USE_GOOGLE_TESTS
    // Default constructor for testability.
    // In production, we need to use vocbase
    // constructor.
    DatabaseConfiguration() = default;
#endif
    explicit DatabaseConfiguration(TRI_vocbase_t const& database);

    bool allowExtendedNames = false;
    bool shouldValidateClusterSettings = false;
    uint32_t maxNumberOfShards = 0;

    uint32_t minReplicationFactor = 0;
    uint32_t maxReplicationFactor = 0;
    bool enforceReplicationFactor = true;

    uint64_t defaultNumberOfShards = 1;
    uint64_t defaultReplicationFactor = 1;
    uint64_t defaultWriteConcern = 1;
    std::string defaultDistributeShardsLike = "";
    bool isOneShardDB = false;
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
      arangodb::velocypack::Slice input, DatabaseConfiguration config);

  static ResultT<PlanCollection> fromCreateAPIV8(
      arangodb::velocypack::Slice properties, std::string const& name,
      TRI_col_type_e type, DatabaseConfiguration config);

  static arangodb::velocypack::Builder toCreateCollectionProperties(
      std::vector<PlanCollection> const& collections);

  // Temporary method to handOver information from
  [[nodiscard]] arangodb::velocypack::Builder toCollectionsCreate() const;

  [[nodiscard]] arangodb::Result validateDatabaseConfiguration(
      DatabaseConfiguration config) const;

  std::string name = StaticStrings::Empty;
  std::underlying_type_t<TRI_col_type_e> type =
      TRI_col_type_e::TRI_COL_TYPE_DOCUMENT;
  bool waitForSync = false;
  bool isSystem = false;
  bool doCompact = false;
  bool isVolatile = false;
  bool cacheEnabled = false;

  uint64_t numberOfShards = 1;
  uint64_t replicationFactor = 1;
  uint64_t writeConcern = 1;
  std::string distributeShardsLike = StaticStrings::Empty;
  std::optional<std::string> smartJoinAttribute = std::nullopt;
  std::string shardingStrategy = StaticStrings::Empty;
  std::string globallyUniqueId = StaticStrings::Empty;

  std::vector<std::string> shardKeys =
      std::vector<std::string>{StaticStrings::KeyString};

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder computedValues =
      VPackBuilder{VPackSlice::emptyArraySlice()};

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder schema =
      VPackBuilder{VPackSlice::emptyObjectSlice()};

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder keyOptions =
      VPackBuilder{VPackSlice::emptyObjectSlice()};

  // NOTE: These attributes are not documented
  bool syncByRevision = true;
  bool usesRevisionsAsDocumentIds = true;
  bool isSmart = false;
  bool isDisjoint = false;
  bool isSmartChild = false;
  std::string smartGraphAttribute = StaticStrings::Empty;

  // Deprecated, and not documented anymore

  std::string id = StaticStrings::Empty;

  // Not documented, actually this is an option, not a configuration parameter
  std::vector<std::string> avoidServers = {};

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
              .fallback(f.keep())
              .invariant(UtilityInvariants::isNonEmpty),
          f.field("id", planCollection.id).fallback(f.keep()),
          f.field("waitForSync", planCollection.waitForSync).fallback(f.keep()),
          f.field("isSystem", planCollection.isSystem).fallback(f.keep()),
          f.field("doCompact", planCollection.doCompact).fallback(f.keep()),
          f.field("cacheEnabled", planCollection.cacheEnabled)
              .fallback(f.keep()),
          f.field("isVolatile", planCollection.isVolatile).fallback(f.keep()),
          f.field("syncByRevision", planCollection.syncByRevision)
              .fallback(f.keep()),
          f.field("usesRevisionsAsDocumentIds",
                  planCollection.usesRevisionsAsDocumentIds)
              .fallback(f.keep()),
          f.field("isSmart", planCollection.isSmart).fallback(f.keep()),
          f.field("isDisjoint", planCollection.isDisjoint).fallback(f.keep()),
          f.field("smartGraphAttribute", planCollection.smartGraphAttribute)
              .fallback(f.keep()),
          f.field("numberOfShards", planCollection.numberOfShards)
              .fallback(f.keep())
              .invariant(UtilityInvariants::isGreaterZero),
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
              .invariant(UtilityInvariants::isGreaterZero),
          f.field("replicationFactor", planCollection.replicationFactor)
              .fallback(f.keep())
              .transformWith(
                  PlanCollection::Transformers::ReplicationSatellite{}),
          f.field("distributeShardsLike", planCollection.distributeShardsLike)
              .fallback(f.keep()),
          f.field(StaticStrings::SmartJoinAttribute,
                  planCollection.smartJoinAttribute)
              .invariant(UtilityInvariants::isNonEmptyIfPresent),
          f.field("globallyUniqueId", planCollection.globallyUniqueId)
              .fallback(f.keep()),
          f.field("shardingStrategy", planCollection.shardingStrategy)
              .fallback(f.keep())
              .invariant(UtilityInvariants::isValidShardingStrategy),
          f.field("shardKeys", planCollection.shardKeys)
              .fallback(f.keep())
              .invariant(UtilityInvariants::areShardKeysValid),
          f.field("type", planCollection.type)
              .fallback(f.keep())
              .invariant(UtilityInvariants::isValidCollectionType),
          f.field("schema", planCollection.schema).fallback(f.keep()),
          f.field("keyOptions", planCollection.keyOptions).fallback(f.keep()),
          f.field("computedValues", planCollection.computedValues)
              .fallback(f.keep()),
          f.field("avoidServers", planCollection.avoidServers)
              .fallback(f.keep()),
          f.field("isSmartChild", planCollection.isSmartChild)
              .fallback(f.keep()));
}

}  // namespace arangodb
