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

// These tests pin how a vector index's metadata is persisted and loaded:
// persistMetadata serializes VectorIndexMetadata directly, loadStoredMetadata
// reads it back through decodeMetadata (which owns the ignoreUnknownFields
// choice). Both production paths are exercised here. The LegacyRecord /
// FutureRecord mirrors stand in for records written by an older or newer
// binary to pin the upgrade/downgrade behaviour.

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

// Pre-tunedTables on-disk schema: simulates a record written by an older
// binary that does not know the tunedTables field.
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

// Hypothetical newer on-disk schema: carries an extra field the current binary
// does not know about, to simulate reading a record from a newer binary.
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

VectorIndexMetadata makeMetadata() {
  VectorIndexMetadata md;
  md.codeData = {1, 2, 3, 4, 5};
  md.tunedNProbe = 12;
  md.formatVersion = VectorIndexFormatVersion::kV2;
  OperatingPointTable t10;
  t10.topK = 10;
  t10.minRecall = 0.9;
  t10.points = {OperatingPoint{0.95, "nprobe=4", 0.2}};
  OperatingPointTable t100;
  t100.topK = 100;
  t100.minRecall = 0.8;
  t100.points = {OperatingPoint{0.80, "nprobe=8", 0.5},
                 OperatingPoint{0.92, "nprobe=16", 1.1}};
  md.tunedTables = {t10, t100};
  return md;
}

template<class T>
velocypack::Builder serializeToBuilder(T const& value) {
  velocypack::Builder b;
  velocypack::serialize(b, value);
  return b;
}

}  // namespace

// The persist -> load round trip: persistMetadata serializes the metadata and
// loadStoredMetadata reads it back via decodeMetadata. Every field, including
// the tuning tables and the format version, survives.
TEST(VectorIndexMetadataSerialization, RoundTrip) {
  auto const original = makeMetadata();

  auto const restored = decodeMetadata(serializeToBuilder(original).slice());

  EXPECT_EQ(restored.codeData, original.codeData);
  EXPECT_EQ(restored.tunedNProbe, original.tunedNProbe);
  EXPECT_EQ(restored.tunedTables, original.tunedTables);
  EXPECT_EQ(restored.formatVersion, original.formatVersion);
}

// Upgrade: a record from an older binary (codeData + tunedNProbe, no
// tunedTables) loads through the production decode. The missing tunedTables
// falls back to empty; the known fields are preserved.
TEST(VectorIndexMetadataSerialization, UpgradeLoadsLegacyRecord) {
  OwnedLegacy legacy;
  legacy.codeData = {9, 8, 7};
  legacy.tunedNProbe = 7;
  legacy.formatVersion = VectorIndexFormatVersion::kV2;

  VectorIndexMetadata restored;
  ASSERT_NO_THROW(
      { restored = decodeMetadata(serializeToBuilder(legacy).slice()); });

  EXPECT_EQ(restored.codeData, legacy.codeData);
  EXPECT_EQ(restored.tunedNProbe, legacy.tunedNProbe);
  EXPECT_EQ(restored.formatVersion, VectorIndexFormatVersion::kV2);
  EXPECT_TRUE(restored.tunedTables.empty());
}

// Downgrade: a record from a newer binary carries a field the current schema
// does not know. The production decode is lenient (ignoreUnknownFields), so it
// drops the unknown field and still loads the known ones. This fails if the
// ignoreUnknownFields option is ever removed from decodeMetadata.
TEST(VectorIndexMetadataSerialization, DecodeToleratesUnknownFutureField) {
  OwnedFuture future;
  future.codeData = {4, 5, 6};
  future.tunedTables = makeMetadata().tunedTables;
  future.formatVersion = VectorIndexFormatVersion::kV2;
  future.addedInFutureVersion = 999;

  VectorIndexMetadata restored;
  ASSERT_NO_THROW(
      { restored = decodeMetadata(serializeToBuilder(future).slice()); });

  EXPECT_EQ(restored.codeData, future.codeData);
  EXPECT_EQ(restored.tunedTables, future.tunedTables);
  EXPECT_EQ(restored.formatVersion, VectorIndexFormatVersion::kV2);
}

// Guard rail documenting why decodeMetadata must be lenient: a strict
// deserialize of that same future record (unknown field present) throws.
TEST(VectorIndexMetadataSerialization, StrictDecodeRejectsUnknownField) {
  OwnedFuture future;
  future.codeData = {4, 5, 6};
  future.addedInFutureVersion = 999;
  auto const builder = serializeToBuilder(future);

  VectorIndexMetadata strict;
  EXPECT_THROW(velocypack::deserialize(builder.slice(), strict),
               arangodb::basics::Exception);
}
