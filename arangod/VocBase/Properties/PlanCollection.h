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
namespace inspection {
struct Status;
}
namespace velocypack {
class Slice;
}

struct PlanCollection {
  PlanCollection();

  static PlanCollection fromCreateAPIBody(arangodb::velocypack::Slice input);

  // Temporary method to handOver information from
  arangodb::velocypack::Builder toCollectionsCreate();

  std::string name;
  bool waitForSync;
  std::underlying_type_t<TRI_col_type_e> type;
  bool isSystem;
  bool allowSystem;
  bool doCompact;
  bool isVolatile;
  bool cacheEnabled;
  uint64_t numberOfShards;
  uint64_t replicationFactor;
  uint64_t writeConcern;

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
          f.field("allowSystem", planCollection.allowSystem).fallback(false),
          f.field("doCompact", planCollection.doCompact).fallback(false),
          f.field("cacheEnabled", planCollection.cacheEnabled).fallback(false),
          f.field("isVolatile", planCollection.isVolatile).fallback(false),
          f.field("numberOfShards", planCollection.numberOfShards)
              .fallback(1ULL),
          f.field("replicationFactor", planCollection.replicationFactor)
              .fallback(1ULL),
          f.field("writeConcern", planCollection.writeConcern).fallback(1ULL),
          f.field("type", planCollection.type)
              .fallback(TRI_col_type_e::TRI_COL_TYPE_DOCUMENT)
              .invariant([](auto t) -> inspection::Status {
                if (t == TRI_col_type_e::TRI_COL_TYPE_DOCUMENT ||
                    t == TRI_col_type_e::TRI_COL_TYPE_EDGE) {
                  return inspection::Status::Success{};
                }
                return {"Only 2 (document) and 3 (edge) are allowed."};
              }),
          f.field(StaticStrings::DataSourceSystem, planCollection.allowSystem)
              .fallback(false),
          f.field("schema", planCollection.schema)
              .fallback(VPackSlice::emptyObjectSlice()),
          f.field("keyOptions", planCollection.keyOptions)
              .fallback(VPackSlice::emptyObjectSlice()),
          f.field("computedValues", planCollection.computedValues)
              .fallback(VPackSlice::emptyArraySlice()));
}

}  // namespace arangodb
