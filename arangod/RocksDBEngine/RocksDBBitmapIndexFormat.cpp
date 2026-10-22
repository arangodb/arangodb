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

#include "RocksDBBitmapIndexFormat.h"

#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>

#include <cstring>

namespace arangodb::bitmap_index {

void appendShard(std::string& out, std::uint64_t shard) {
  for (std::size_t i = kShardBytes; i > 0; --i) {
    out.push_back(static_cast<char>((shard >> (8 * (i - 1))) & 0xff));
  }
}

std::uint64_t readShard(char const* p) noexcept {
  std::uint64_t shard = 0;
  for (std::size_t i = 0; i < kShardBytes; ++i) {
    shard = (shard << 8) | static_cast<std::uint8_t>(p[i]);
  }
  return shard;
}

bool appendNormalized(std::string& out, velocypack::Slice value) {
  velocypack::Builder builder;
  if (value.isNumber()) {
    double d = value.getNumber<double>();
    if (d == 0.0) {
      // -0.0 and 0.0 compare equal
      d = 0.0;
    }
    builder.add(velocypack::Value(d));
  } else if (value.isString()) {
    builder.add(velocypack::Value(value.stringView()));
  } else if (value.isBoolean()) {
    builder.add(velocypack::Value(value.getBoolean()));
  } else if (value.isNull()) {
    builder.add(velocypack::Value(velocypack::ValueType::Null));
  } else {
    return false;
  }
  out.append(reinterpret_cast<char const*>(builder.data()), builder.size());
  return true;
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
                                   "invalid bitmap index value");
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
                                 "invalid bitmap index value");
}

std::int64_t decodeCount(std::string_view value) {
  auto tag = tagOf(value);
  if ((tag != ValueTag::Count && tag != ValueTag::CountDelta) ||
      value.size() != 1 + sizeof(std::int64_t)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid bitmap index count value");
  }
  std::int64_t count;
  std::memcpy(&count, value.data() + 1, sizeof(count));
  return count;
}

}  // namespace arangodb::bitmap_index
