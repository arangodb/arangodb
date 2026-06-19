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
#include <utility>
#include <vector>

#include "Inspection/VPack.h"
#include "VectorIndex/VectorIndexDefinition.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb::vector {

/// @brief On-disk metadata record for a vector index.
/// Template exists only so we can seralize/deserialize from a view without
/// copying the code data.
template<class Bytes>
struct StoredMetadata {
  Bytes codeData;
  // Legacy single tuned nprobe; superseded by `tunedTables`. Retained for
  // backwards compatibility until the search/apply path migrates.
  std::optional<std::int64_t> tunedNProbe;
  // Autotuned operating-point tables, one per tuned topK.
  std::vector<OperatingPointTable> tunedTables;
  VectorIndexFormatVersion formatVersion = VectorIndexFormatVersion::kV1;

  template<class Inspector>
  friend auto inspect(Inspector& f, StoredMetadata& x) {
    return f.object(x).fields(f.field("codeData", x.codeData),
                              f.field("tunedNProbe", x.tunedNProbe),
                              f.field("tunedTables", x.tunedTables)
                                  .fallback(std::vector<OperatingPointTable>{}),
                              f.field("formatVersion", x.formatVersion)
                                  .fallback(VectorIndexFormatVersion::kV1));
  }
};

using OwnedMetadata = StoredMetadata<std::vector<std::uint8_t>>;
using MetadataView = StoredMetadata<std::vector<std::uint8_t> const&>;

/// @brief Serialize the live trained data into a VPack record for persistence.
/// codeData is borrowed (not copied) via MetadataView. This is the single
/// definition of how a metadata record is written; persistMetadata and the
/// tests both go through here.
inline velocypack::Builder encodeStoredMetadata(
    TrainedData const& data, VectorIndexFormatVersion version) {
  MetadataView const view{data.codeData, data.tunedNProbe, data.tunedTables,
                          version};
  velocypack::Builder builder;
  velocypack::serialize(builder, view);
  return builder;
}

/// @brief Parse a stored metadata record. Reads leniently
/// (ignoreUnknownFields) so a record carrying a field this binary no longer
/// knows (e.g. the legacy tunedNProbe once removed) still loads instead of
/// failing deserialization. This is the single definition of how a record is
/// read.
inline OwnedMetadata decodeStoredMetadata(velocypack::Slice slice) {
  OwnedMetadata result;
  velocypack::deserialize(
      slice, result, inspection::ParseOptions{.ignoreUnknownFields = true});
  return result;
}

/// @brief Copy a parsed record into the index's live state. The single
/// definition of the stored-record -> in-memory-field mapping; a field added
/// to the record schema must be wired through here too.
inline void assignStoredMetadata(OwnedMetadata&& stored, TrainedData& data,
                                 VectorIndexFormatVersion& version) {
  data.codeData = std::move(stored.codeData);
  data.tunedNProbe = stored.tunedNProbe;
  data.tunedTables = std::move(stored.tunedTables);
  version = stored.formatVersion;
}

}  // namespace arangodb::vector
