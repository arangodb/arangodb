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
  std::shared_ptr<arangodb::velocypack::Builder> toCollectionsCreate();

  std::string name;
  bool waitForSync;
  std::underlying_type_t<TRI_col_type_e> type;
  bool isSystem;
  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder schema;

  // TODO: This can be optimized into it's own struct.
  // Did a short_cut here to avoid concatenated changes
  arangodb::velocypack::Builder keyOptions;
};

template<class Inspector>
auto inspect(Inspector& f, PlanCollection& planCollection) {
  return f.object(planCollection)
      .fields(
          f.field("name", planCollection.name),
          f.field("waitForSync", planCollection.waitForSync).fallback(false),
          f.field("isSystem", planCollection.isSystem).fallback(false),
          f.field("type", planCollection.type)
              .fallback(TRI_col_type_e::TRI_COL_TYPE_DOCUMENT)
              .invariant([](auto t) {
                return t == TRI_col_type_e::TRI_COL_TYPE_DOCUMENT ||
                       t == TRI_col_type_e::TRI_COL_TYPE_EDGE;
              }),
          f.field("schema", planCollection.schema)
              .fallback(VPackSlice::emptyObjectSlice()),
          f.field("keyOptions", planCollection.keyOptions)
              .fallback(VPackSlice::emptyObjectSlice()));
}

}  // namespace arangodb
