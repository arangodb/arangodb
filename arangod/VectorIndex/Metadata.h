////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "Inspection/VPack.h"
#include "VectorIndex/VectorIndexDefinition.h"

#include <velocypack/Slice.h>

namespace arangodb::vector {

/// @brief A vector index metadata
struct VectorIndexMetadata {
  std::vector<std::uint8_t> codeData;
  // Autotuned operating-point tables, one per tuned topK.
  std::vector<OperatingPointTable> tunedTables;
  VectorIndexFormatVersion formatVersion = VectorIndexFormatVersion::kV1;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, VectorIndexMetadata& x) {
    return f.object(x).fields(f.field("codeData", x.codeData),
                              f.field("tunedTables", x.tunedTables)
                                  .fallback(std::vector<OperatingPointTable>{}),
                              f.field("formatVersion", x.formatVersion)
                                  .fallback(VectorIndexFormatVersion::kV1));
  }
};

inline VectorIndexMetadata decodeMetadata(velocypack::Slice slice) {
  VectorIndexMetadata result;
  velocypack::deserialize(
      slice, result, inspection::ParseOptions{.ignoreUnknownFields = true});
  return result;
}

}  // namespace arangodb::vector
