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
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

#include <string>

namespace arangodb {
template<typename T>
class ResultT;

namespace inspection {
struct Status;
}
namespace velocypack {
class Slice;
}

struct PlanCollection {
  PlanCollection();

  static ResultT<PlanCollection> fromCreateAPIBody(
      arangodb::velocypack::Slice input);

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

  // TODO: Maybe this is better off with a transformator Uint -> col_type_e
  [[nodiscard]] TRI_col_type_e getType() const noexcept {
    return TRI_col_type_e(type);
  }
};

template<class Inspector>
auto inspect(Inspector& f, PlanCollection& planCollection) {
  return f.object(planCollection)
      .fields(
          f.field("name", planCollection.name)
              .invariant([](auto const& n) -> inspection::Status {
                if (n.empty()) {
                  return {"Name cannot be empty."};
                }
                return inspection::Status::Success{};
              }),
          f.field("waitForSync", planCollection.waitForSync).fallback(false),
          f.field("isSystem", planCollection.isSystem).fallback(false),
          f.field("doCompact", planCollection.doCompact).fallback(false),
          f.field("cacheEnabled", planCollection.cacheEnabled).fallback(false),
          f.field("isVolatile", planCollection.isVolatile).fallback(false),
          f.field("numberOfShards", planCollection.numberOfShards)
              .fallback(1ULL)
              .invariant([](auto const& value) -> inspection::Status {
                if (value > 0) {
                  return inspection::Status::Success{};
                }
                return {"Value has to be > 0"};
              }),
          f.field("replicationFactor", planCollection.replicationFactor)
              .fallback(1ULL)
              .invariant([](auto const& value) -> inspection::Status {
                if (value > 0) {
                  return inspection::Status::Success{};
                }
                return {"Value has to be > 0"};
              }),
          f.field("writeConcern", planCollection.writeConcern)
              .fallback(1ULL)
              .invariant([](auto const& value) -> inspection::Status {
                if (value > 0) {
                  return inspection::Status::Success{};
                }
                return {"Value has to be > 0"};
              }),
          f.field("distributeShardsLike", planCollection.distributeShardsLike)
              .fallback(""),
          f.field("smartJoinAttribute", planCollection.smartJoinAttribute)
              .fallback(""),
          f.field("globallyUniqueId", planCollection.globallyUniqueId)
              .fallback(""),
          f.field("shardingStrategy", planCollection.shardingStrategy)
              .fallback("hash")
              .invariant([](auto const& strat) -> inspection::Status {
                // Note we may be better off with a lookup list here
                // Hash is first on purpose (default)
                if (strat == "hash" || strat == "enterprise-hash-smart-edge" ||
                    strat == "community-compat" ||
                    strat == "enterprise-compat" ||
                    strat == "enterprise-smart-edge-compat") {
                  return inspection::Status::Success{};
                }
                return {
                    "Please use 'hash' or remove, advanced users please "
                    "pick a strategy from the documentation, " +
                    strat + " is not allowed."};
              }),
          f.field("shardKeys", planCollection.shardKeys)
              .fallback(std::vector<std::string>{StaticStrings::KeyString}),
          f.field("type", planCollection.type)
              .fallback(TRI_col_type_e::TRI_COL_TYPE_DOCUMENT)
              .invariant([](auto t) -> inspection::Status {
                if (t == TRI_col_type_e::TRI_COL_TYPE_DOCUMENT ||
                    t == TRI_col_type_e::TRI_COL_TYPE_EDGE) {
                  return inspection::Status::Success{};
                }
                return {"Only 2 (document) and 3 (edge) are allowed."};
              }),
          f.field("schema", planCollection.schema)
              .fallback(VPackSlice::emptyObjectSlice()),
          f.field("keyOptions", planCollection.keyOptions)
              .fallback(VPackSlice::emptyObjectSlice()),
          f.field("computedValues", planCollection.computedValues)
              .fallback(VPackSlice::emptyArraySlice()));
}

}  // namespace arangodb
