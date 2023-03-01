////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <vector>
#include <span>

#include <s2/s2shape.h>

class S2Polyline;
class S2Polygon;

namespace arangodb::geo {
namespace coding {
// Numbers here used for serialization, you cannot change it!

enum class Type : uint8_t {
  kPoint = 0,
  kPolyline = 1,
  kPolygon = 2,
  kMultiPoint = 3,
  kMultiPolyline = 4,
  // kGeometryCollection = 5, TODO(MBkkt) implement it?
};

enum class Options : uint8_t {
  // Use S2Point representation, it's completely lossless
  kS2Point = 0,
  kS2PointRegionCompact = 1,
  kS2PointShapeCompact = 2,
  // Use S2LatLng representation, lossy
  kS2LatLng = 3,
  kS2LatLngInt = 4,
  // To use as invalid value in template
  kInvalid = 0xFF,
};

constexpr bool isOptionsS2(Options options) noexcept {
  return static_cast<uint8_t>(options) < 3;
}

constexpr bool isSameLoss(Options lhs, Options rhs) noexcept {
  auto const l = static_cast<uint8_t>(lhs);
  auto const r = static_cast<uint8_t>(rhs);
  return (l != 4) == (r != 4);
}

constexpr auto toTag(Type t, Options o) noexcept {
  return static_cast<uint8_t>(static_cast<uint32_t>(t) << 5U |
                              static_cast<uint32_t>(o));
}

template<Type t, Options o = Options::kS2Point>
consteval auto toTag() noexcept {
  static_assert(
      static_cast<std::underlying_type_t<Type>>(t) < 8,
      "less because we want to use 0xE0 as special value to extend tag format");
  static_assert(
      static_cast<std::underlying_type_t<Options>>(o) < 31,
      "less because we want to use 0x1F as special value to extend tag format");
  return toTag(t, o);
}

constexpr auto toType(uint8_t tag) noexcept {
  return static_cast<uint8_t>(static_cast<uint32_t>(tag) & 0xE0);
}

constexpr auto toPoint(uint8_t tag) noexcept {
  return static_cast<uint32_t>(tag) & 0x1F;
}

constexpr size_t toSize(Options options) noexcept {
  switch (options) {
    case Options::kS2LatLngInt:
      return 2 * sizeof(uint32);
    case Options::kS2LatLng:
      return 2 * sizeof(double);
    case Options::kS2Point:
      return 3 * sizeof(double);
    default:
      return 0;
  }
}

}  // namespace coding

void ensureLittleEndian();

void toLatLngInt(S2LatLng& latLng) noexcept;
void encodeLatLng(Encoder& encoder, S2LatLng& latLng,
                  coding::Options options) noexcept;
void encodePoint(Encoder& encoder, S2Point const& point) noexcept;

void toLatLngInt(std::span<S2LatLng> vertices) noexcept;
void encodeVertices(Encoder& encode, std::span<S2Point const> vertices);
void encodeVertices(Encoder& encode, std::span<S2LatLng> vertices,
                    coding::Options options);
bool decodeVertices(Decoder& decoder, std::span<S2Point> vertices, uint8_t tag);

bool decodePoint(Decoder& decoder, S2Point& point, uint8_t* tag);
bool decodePoint(Decoder& decoder, S2Point& point, uint8_t tag);

void encodePolyline(Encoder& encoder, S2Polyline const& polyline,
                    coding::Options options);
bool decodePolyline(Decoder& decoder, S2Polyline& polyline, uint8_t tag,
                    std::vector<S2Point>& cache);

void encodePolygon(Encoder& encoder, S2Polygon const& polygon,
                   coding::Options options);
bool decodePolygon(Decoder& decoder, S2Polygon& polygon, uint8_t tag,
                   std::vector<S2Point>& cache);

void encodePolylines(Encoder& encoder, std::span<S2Polyline const> polylines,
                     coding::Options options);
bool decodePolylines(Decoder& decoder, std::vector<S2Polyline>& polylines,
                     uint8_t tag, std::vector<S2Point>& cache);

}  // namespace arangodb::geo
