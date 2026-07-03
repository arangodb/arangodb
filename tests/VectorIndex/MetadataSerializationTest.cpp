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

// Pins the production persist/load path: serialize VectorIndexMetadata, read it
// back via decodeMetadata. The Legacy/Future mirrors stand in for records
// written by an older/newer binary.

#include "VectorIndex/Metadata.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "gtest/gtest.h"

#include "Basics/Exceptions.h"
#include "Inspection/VPack.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::vector;

namespace {

// Older schema: still has the since-removed tunedNProbe, lacks tunedTables.
template<class Bytes>
struct LegacyRecord {
  Bytes codeData;
  std::optional<std::int64_t> tunedNProbe;
  VectorIndexFormatVersion formatVersion = VectorIndexFormatVersion::kV1;

  template<class Inspector>
  friend auto inspect(Inspector& f, LegacyRecord& x) {
    return f.object(x).fields(f.field("codeData", x.codeData),
                              f.field("tunedNProbe", x.tunedNProbe),
                              f.field("formatVersion", x.formatVersion)
                                  .fallback(VectorIndexFormatVersion::kV1));
  }
};
using OwnedLegacy = LegacyRecord<std::vector<std::uint8_t>>;

// Newer schema: carries an extra field the current binary does not know.
template<class Bytes>
struct FutureRecord {
  Bytes codeData;
  std::optional<std::int64_t> tunedNProbe;
  std::vector<OperatingPointTable> tunedTables;
  VectorIndexFormatVersion formatVersion = VectorIndexFormatVersion::kV1;
  std::int64_t addedInFutureVersion{0};

  template<class Inspector>
  friend auto inspect(Inspector& f, FutureRecord& x) {
    return f.object(x).fields(
        f.field("codeData", x.codeData), f.field("tunedNProbe", x.tunedNProbe),
        f.field("tunedTables", x.tunedTables)
            .fallback(std::vector<OperatingPointTable>{}),
        f.field("formatVersion", x.formatVersion)
            .fallback(VectorIndexFormatVersion::kV1),
        f.field("addedInFutureVersion", x.addedInFutureVersion).fallback(0));
  }
};
using OwnedFuture = FutureRecord<std::vector<std::uint8_t>>;

// The raw list of tables as it appears on the wire (and in legacy/future
// records that still model tunedTables as a plain array).
std::vector<OperatingPointTable> makeTables() {
  OperatingPointTable t10;
  t10.topK = 10;
  t10.targetRecall = 0.9;
  t10.points = {OperatingPoint{0.95, "nprobe=4", 0.2}};
  OperatingPointTable t100;
  t100.topK = 100;
  t100.targetRecall = 0.8;
  t100.points = {OperatingPoint{0.80, "nprobe=8", 0.5},
                 OperatingPoint{0.92, "nprobe=16", 1.1}};
  return {t10, t100};
}

VectorIndexMetadata makeMetadata() {
  VectorIndexMetadata md;
  md.codeData = {1, 2, 3, 4, 5};
  md.formatVersion = VectorIndexFormatVersion::kV2;
  for (auto const& table : makeTables()) {
    md.tunedTables.emplace(table.topK, table);
  }
  return md;
}

template<class T>
velocypack::Builder serializeToBuilder(T const& value) {
  velocypack::Builder b;
  velocypack::serialize(b, value);
  return b;
}

}  // namespace

// Every field survives a serialize -> decodeMetadata round trip.
TEST(VectorIndexMetadataSerialization, RoundTrip) {
  auto const original = makeMetadata();

  auto const restored = decodeMetadata(serializeToBuilder(original).slice());

  EXPECT_EQ(restored.codeData, original.codeData);
  EXPECT_EQ(restored.tunedTables, original.tunedTables);
  EXPECT_EQ(restored.formatVersion, original.formatVersion);
}

// Upgrade: a legacy record loads; the unknown tunedNProbe is dropped and the
// absent tunedTables falls back to empty.
TEST(VectorIndexMetadataSerialization, UpgradeLoadsLegacyRecord) {
  OwnedLegacy legacy;
  legacy.codeData = {9, 8, 7};
  legacy.tunedNProbe = 7;
  legacy.formatVersion = VectorIndexFormatVersion::kV2;

  VectorIndexMetadata restored;
  ASSERT_NO_THROW(
      { restored = decodeMetadata(serializeToBuilder(legacy).slice()); });

  EXPECT_EQ(restored.codeData, legacy.codeData);
  EXPECT_EQ(restored.formatVersion, VectorIndexFormatVersion::kV2);
  EXPECT_TRUE(restored.tunedTables.empty());
}

// decodeMetadata is lenient: a record with an unknown field still loads. Fails
// if ignoreUnknownFields is ever dropped from decodeMetadata.
TEST(VectorIndexMetadataSerialization, DecodeToleratesUnknownFutureField) {
  OwnedFuture future;
  future.codeData = {4, 5, 6};
  future.tunedTables = makeTables();
  future.formatVersion = VectorIndexFormatVersion::kV2;
  future.addedInFutureVersion = 999;

  VectorIndexMetadata restored;
  ASSERT_NO_THROW(
      { restored = decodeMetadata(serializeToBuilder(future).slice()); });

  EXPECT_EQ(restored.codeData, future.codeData);
  // The plain-array wire form decodes into the topK-keyed map.
  EXPECT_EQ(restored.tunedTables, makeMetadata().tunedTables);
  EXPECT_EQ(restored.formatVersion, VectorIndexFormatVersion::kV2);
}

// Why leniency is needed: a strict deserialize of an unknown field throws.
TEST(VectorIndexMetadataSerialization, StrictDecodeRejectsUnknownField) {
  OwnedFuture future;
  future.codeData = {4, 5, 6};
  future.addedInFutureVersion = 999;
  auto const builder = serializeToBuilder(future);

  VectorIndexMetadata strict;
  EXPECT_THROW(velocypack::deserialize(builder.slice(), strict),
               arangodb::basics::Exception);
}
