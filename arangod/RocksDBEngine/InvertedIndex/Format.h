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

#include "roaring.hh"

#include <velocypack/Slice.h>

namespace arangodb::inverted_index {

// clang-format off
/// On-disk layout of the inverted index in the RocksDBInvertedIndex column
/// family.
///
/// A document is identified by its LocalDocumentId. The id is split into a
/// shard and an offset, see IdLayout. The shard goes into the key and the
/// offset goes into a Roaring bitmap. One term therefore has one bitmap entry
/// for each shard that holds at least one matching document.
///
/// Each term has two kinds of entries:
///
/// Legend:
/// OI - index objectId (8 bytes, in the key format endianness of the database,
///      which is big-endian for all newly created databases)
/// TM - term, see below
/// SH - shard number (kShardBytes bytes, always big-endian)
///
/// | OI | TM |      count key  (termKey)
/// | OI | TM | SH | bitmap key (bitmapKey)
///
/// The shard is big-endian so that the bitmap keys of one term sort by shard.
/// An iterator can then Seek() to the bitmap that holds a target document.
/// Because TM is self-delimiting, the count key sorts directly before the
/// bitmap keys of the same term, and no key of one term is a prefix of a key
/// of another term.
///
/// A term is the stored form of a scalar value, made by normalizedTerm(). Its
/// first byte is the type. Strings also use the first byte for their length:
///
/// | 0x00 |                                         null
/// | 0x01 |                                         false
/// | 0x02 |                                         true
/// | 0x03 | IEEE 754 double (8 bytes, big-endian) | number
/// | 0x04 + n | n bytes UTF-8 |                      string, 0 <= n <= 122
/// | 0x7f | LEB128 length n | n bytes UTF-8 |        string, n > 122
/// | 0x80 - 0xff |                                   reserved for extensions
///
/// Every number is converted to a double. -0 becomes 0 and every NaN becomes
/// one quiet NaN, so that values AQL considers equal have equal bytes. The
/// index only looks up terms by equality, so the byte order of terms does not
/// need to match the AQL order of the values.
///
/// This encoding does not depend on velocypack, because a change of the
/// velocypack encoding must not change the keys of existing indexes.
///
/// The first byte of each value is a ValueTag:
///
/// | 'C' | int64 count (8 bytes, machine endianness) |   full document count
/// | 'c' | int64 delta (8 bytes, machine endianness) |   merge operand for a count
/// | 'B' | Roaring bitmap (portable format)          |   full bitmap of offsets
/// | 'D' | Roaring adds | Roaring removes            |   merge operand for a bitmap
///
/// Writers never put a value directly. They only merge 'c' and 'D' operands,
/// and the merge operator folds them into 'C' and 'B'.
// clang-format on

/// How a LocalDocumentId is split into a shard, which goes into the key, and
/// a 32-bit offset, which goes into the Roaring bitmap of that shard.
///
/// LocalDocumentIds are hybrid logical clock values, ms << 20 | count. The
/// count restarts at 0 every millisecond and stays small. A Roaring bitmap
/// keeps the low 16 bits of an offset in a container that the high 16 bits
/// select. The plain split (windowBits 0) puts the count into the low 16
/// bits, so every millisecond with a document becomes its own container. The
/// window layout (windowBits W, 16 <= W <= 32) reorders the bits instead,
/// with c = 32 - W:
///
///   shard  = | ms >> W | count >> c |                        (32 bits)
///   offset = | ms bits 16 .. W-1 | count low c bits | ms low 16 bits |
///
/// One shard then covers 2^W milliseconds, and one container holds the
/// documents of 65.5 seconds that share a count. The reordering is a
/// bijection, so every id has exactly one place. Only the density depends on
/// the ids being HLC values.
struct IdLayout {
  unsigned windowBits = 0;
  bool operator==(IdLayout const&) const = default;
};

inline constexpr unsigned kDefaultWindowBits = 20;

constexpr bool isValid(IdLayout layout) noexcept {
  return layout.windowBits == 0 ||
         (layout.windowBits >= 16 && layout.windowBits <= 32);
}

/// number of bytes used to encode the shard number in a key; the plain split
/// has 33-bit shards
inline constexpr std::size_t kShardBytes = 5;

namespace detail {
inline constexpr unsigned kPlainShardBits = 31;
inline constexpr unsigned kHlcCountBits = 20;
constexpr std::uint64_t lowBits(unsigned n) noexcept {
  return n >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << n) - 1;
}
}  // namespace detail

constexpr std::uint64_t shardOf(IdLayout layout, std::uint64_t id) noexcept {
  using namespace detail;
  if (layout.windowBits == 0) {
    return id >> kPlainShardBits;
  }
  unsigned const w = layout.windowBits;
  unsigned const c = 32 - w;
  std::uint64_t const ms = id >> kHlcCountBits;
  std::uint64_t const count = id & lowBits(kHlcCountBits);
  return ((ms >> w) << (kHlcCountBits - c)) | (count >> c);
}

constexpr std::uint32_t offsetOf(IdLayout layout, std::uint64_t id) noexcept {
  using namespace detail;
  if (layout.windowBits == 0) {
    return static_cast<std::uint32_t>(id & lowBits(kPlainShardBits));
  }
  unsigned const w = layout.windowBits;
  unsigned const c = 32 - w;
  std::uint64_t const ms = id >> kHlcCountBits;
  std::uint64_t const count = id & lowBits(kHlcCountBits);
  return static_cast<std::uint32_t>((((ms & lowBits(w)) >> 16) << (16 + c)) |
                                    ((count & lowBits(c)) << 16) |
                                    (ms & lowBits(16)));
}

constexpr std::uint64_t makeId(IdLayout layout, std::uint64_t shard,
                               std::uint32_t offset) noexcept {
  using namespace detail;
  if (layout.windowBits == 0) {
    return (shard << kPlainShardBits) | offset;
  }
  unsigned const w = layout.windowBits;
  unsigned const c = 32 - w;
  std::uint64_t const msLow = offset & lowBits(16);
  std::uint64_t const countLow = (offset >> 16) & lowBits(c);
  std::uint64_t const msMiddle = std::uint64_t{offset} >> (16 + c);
  std::uint64_t const ms =
      ((shard >> (kHlcCountBits - c)) << w) | (msMiddle << 16) | msLow;
  std::uint64_t const count =
      ((shard & lowBits(kHlcCountBits - c)) << c) | countLow;
  return (ms << kHlcCountBits) | count;
}

/// first byte of every value and merge operand
enum class ValueTag : char {
  Bitmap = 'B',      // full bitmap
  Delta = 'D',       // bitmap delta: offsets to add and offsets to remove
  Count = 'C',       // full document count of a term
  CountDelta = 'c',  // signed change of a document count
};

/// Stored form of an indexed or searched value, see the layout above. Arrays,
/// objects and the other non-scalar types have no stored form.
std::optional<std::string> normalizedTerm(velocypack::Slice value);

/// Key of the document count of a term: objectId | term. A term is
/// self-delimiting, so no term key is a prefix of another term's keys, and the
/// count key sorts directly before the term's bitmap keys.
std::string termKey(std::uint64_t objectId, std::string_view term);
/// key of the bitmap of a term for one shard: termKey | big-endian shard
std::string bitmapKey(std::string_view termKey, std::uint64_t shard);
/// Overwrites the shard of a bitmap key in place. Readers that seek one term
/// through many shards use it to avoid building a new key for every seek.
void setShardOfBitmapKey(std::string& bitmapKey, std::uint64_t shard);
/// shard of a bitmap key of the given term, none for other keys
std::optional<std::uint64_t> shardOfBitmapKey(std::string_view key,
                                              std::string_view termKey);

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

}  // namespace arangodb::inverted_index
