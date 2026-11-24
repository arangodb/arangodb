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

#include "RocksDBEngine/InvertedIndex/Format.h"

#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "RocksDBEngine/RocksDBFormat.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace arangodb::inverted_index {

namespace {
enum TermType : std::uint8_t {
  kNull = 0x00,
  kFalse = 0x01,
  kTrue = 0x02,
  kNumber = 0x03,
  kShortString = 0x04,
  kLongString = 0x7f,
};
inline constexpr std::size_t kMaxShortStringLength =
    kLongString - kShortString - 1;

std::uint64_t canonicalBits(double d) noexcept {
  if (d == 0.0) {
    d = 0.0;
  } else if (std::isnan(d)) {
    d = std::numeric_limits<double>::quiet_NaN();
  }
  return std::bit_cast<std::uint64_t>(d);
}

void appendBigEndian(std::string& out, std::uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    out.push_back(static_cast<char>((value >> shift) & 0xff));
  }
}

void appendLeb128(std::string& out, std::uint64_t value) {
  while (value >= 0x80) {
    out.push_back(static_cast<char>((value & 0x7f) | 0x80));
    value >>= 7;
  }
  out.push_back(static_cast<char>(value));
}

std::string numberTerm(double d) {
  std::string out;
  out.reserve(1 + sizeof(std::uint64_t));
  out.push_back(static_cast<char>(kNumber));
  appendBigEndian(out, canonicalBits(d));
  return out;
}

std::string stringTerm(std::string_view s) {
  std::string out;
  if (s.size() <= kMaxShortStringLength) {
    out.reserve(1 + s.size());
    out.push_back(static_cast<char>(kShortString + s.size()));
  } else {
    out.reserve(1 + 10 + s.size());
    out.push_back(static_cast<char>(kLongString));
    appendLeb128(out, s.size());
  }
  out.append(s);
  return out;
}
}  // namespace

std::optional<std::string> normalizedTerm(velocypack::Slice value) {
  if (value.isNumber()) {
    return numberTerm(value.getNumber<double>());
  }
  if (value.isString()) {
    return stringTerm(value.stringView());
  }
  if (value.isNull()) {
    return std::string(1, static_cast<char>(kNull));
  }
  if (value.isBoolean()) {
    return std::string(1,
                       static_cast<char>(value.getBoolean() ? kTrue : kFalse));
  }
  return std::nullopt;
}

std::string termKey(std::uint64_t objectId, std::string_view term) {
  std::string key;
  key.reserve(sizeof(objectId) + term.size() + kShardBytes);
  rocksutils::uint64ToPersistent(key, objectId);
  key.append(term);
  return key;
}

std::string bitmapKey(std::string_view termKey, std::uint64_t shard) {
  std::string key(termKey);
  key.resize(termKey.size() + kShardBytes);
  setShardOfBitmapKey(key, shard);
  return key;
}

void setShardOfBitmapKey(std::string& bitmapKey, std::uint64_t shard) {
  TRI_ASSERT(bitmapKey.size() >= kShardBytes);
  char* p = bitmapKey.data() + bitmapKey.size() - kShardBytes;
  for (std::size_t i = kShardBytes; i > 0; --i) {
    *p++ = static_cast<char>((shard >> (8 * (i - 1))) & 0xff);
  }
}

std::optional<std::uint64_t> shardOfBitmapKey(std::string_view key,
                                              std::string_view termKey) {
  if (key.size() != termKey.size() + kShardBytes || !key.starts_with(termKey)) {
    return std::nullopt;
  }
  std::uint64_t shard = 0;
  for (char c : key.substr(termKey.size())) {
    shard = (shard << 8) | static_cast<std::uint8_t>(c);
  }
  return shard;
}

std::optional<ValueTag> tagOf(std::string_view value) noexcept {
  if (value.empty()) {
    return std::nullopt;
  }
  switch (value[0]) {
    case static_cast<char>(ValueTag::Bitmap):
    case static_cast<char>(ValueTag::Delta):
    case static_cast<char>(ValueTag::Count):
    case static_cast<char>(ValueTag::CountDelta):
      return static_cast<ValueTag>(value[0]);
    default:
      return std::nullopt;
  }
}

namespace {
void appendBitmap(std::string& out, roaring::Roaring const& bitmap) {
  std::size_t size = bitmap.getSizeInBytes(/*portable*/ true);
  std::size_t offset = out.size();
  out.resize(offset + size);
  bitmap.write(out.data() + offset, /*portable*/ true);
}

roaring::Roaring readBitmap(char const* p, std::size_t size,
                            std::size_t* consumed) {
  std::size_t n = roaring::Roaring::serializedSizeInBytesSafe(p, size);
  if (n == 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid RocksDB inverted index value");
  }
  if (consumed != nullptr) {
    *consumed = n;
  }
  return roaring::Roaring::readSafe(p, n);
}
}  // namespace

std::string encodeBitmap(roaring::Roaring& bitmap) {
  bitmap.runOptimize();
  bitmap.shrinkToFit();
  std::string out;
  out.push_back(static_cast<char>(ValueTag::Bitmap));
  appendBitmap(out, bitmap);
  return out;
}

std::string encodeDelta(roaring::Roaring const& adds,
                        roaring::Roaring const& removes) {
  std::string out;
  out.push_back(static_cast<char>(ValueTag::Delta));
  appendBitmap(out, adds);
  appendBitmap(out, removes);
  return out;
}

std::string encodeCount(ValueTag tag, std::int64_t count) {
  std::string out;
  out.push_back(static_cast<char>(tag));
  out.append(reinterpret_cast<char const*>(&count), sizeof(count));
  return out;
}

void decodeDelta(std::string_view value, roaring::Roaring& adds,
                 roaring::Roaring& removes) {
  TRI_ASSERT(tagOf(value) == ValueTag::Delta);
  char const* p = value.data() + 1;
  std::size_t left = value.size() - 1;
  std::size_t consumed = 0;
  adds = readBitmap(p, left, &consumed);
  removes = readBitmap(p + consumed, left - consumed, nullptr);
}

roaring::Roaring decodeBitmap(std::string_view value) {
  auto tag = tagOf(value);
  if (tag == ValueTag::Bitmap) {
    return readBitmap(value.data() + 1, value.size() - 1, nullptr);
  }
  if (tag == ValueTag::Delta) {
    roaring::Roaring adds;
    roaring::Roaring removes;
    decodeDelta(value, adds, removes);
    adds -= removes;
    return adds;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid RocksDB inverted index value");
}

std::int64_t decodeCount(std::string_view value) {
  auto tag = tagOf(value);
  if ((tag != ValueTag::Count && tag != ValueTag::CountDelta) ||
      value.size() != 1 + sizeof(std::int64_t)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "invalid RocksDB inverted index count value");
  }
  std::int64_t count;
  std::memcpy(&count, value.data() + 1, sizeof(count));
  return count;
}

}  // namespace arangodb::inverted_index
