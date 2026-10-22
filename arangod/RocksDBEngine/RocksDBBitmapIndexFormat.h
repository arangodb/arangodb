////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <velocypack/Slice.h>

#include "roaring.hh"

namespace arangodb::bitmap_index {

/// Number of low LocalDocumentId bits stored inside one bitmap. Everything
/// above goes into the key. Changing this changes the on-disk format; the
/// value an index was built with is persisted in its definition.
inline constexpr unsigned kShardBits = 19;
static_assert(kShardBits >= 1 && kShardBits <= 32,
              "offsets must fit a 32-bit Roaring bitmap");

inline constexpr std::uint64_t kOffsetMask =
    (std::uint64_t{1} << kShardBits) - 1;

/// number of bytes used to encode the shard number in a key
inline constexpr std::size_t kShardBytes = (64 - kShardBits + 7) / 8;

constexpr std::uint64_t shardOf(std::uint64_t id) noexcept {
  return id >> kShardBits;
}
constexpr std::uint32_t offsetOf(std::uint64_t id) noexcept {
  return static_cast<std::uint32_t>(id & kOffsetMask);
}
constexpr std::uint64_t makeId(std::uint64_t shard,
                               std::uint32_t offset) noexcept {
  return (shard << kShardBits) | offset;
}

/// first byte of every value and merge operand
enum class ValueTag : char {
  Bitmap = 'B',      // full bitmap
  Delta = 'D',       // bitmap delta: offsets to add and offsets to remove
  Count = 'C',       // full document count of a term
  CountDelta = 'c',  // signed change of a document count
};

/// append the big-endian shard number to a key body
void appendShard(std::string& out, std::uint64_t shard);
/// read a shard number written by appendShard
std::uint64_t readShard(char const* p) noexcept;

/// Append the normalized VelocyPack encoding of a scalar value. Numbers are
/// stored as doubles so that values AQL considers equal share an encoding.
/// Returns false and appends nothing for arrays, objects and other
/// non-scalar values.
bool appendNormalized(std::string& out, velocypack::Slice value);

std::optional<ValueTag> tagOf(std::string_view value) noexcept;

std::string encodeBitmap(roaring::Roaring& bitmap);
std::string encodeDelta(roaring::Roaring const& adds,
                        roaring::Roaring const& removes);
std::string encodeCount(ValueTag tag, std::int64_t count);

/// decode a full bitmap value; a delta value is applied to an empty bitmap
roaring::Roaring decodeBitmap(std::string_view value);
void decodeDelta(std::string_view value, roaring::Roaring& adds,
                 roaring::Roaring& removes);
std::int64_t decodeCount(std::string_view value);

}  // namespace arangodb::bitmap_index
