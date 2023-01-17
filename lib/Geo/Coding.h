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

class S2LaxPolylineShape;
class S2LaxPolygonShape;

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

struct Options {
  enum class Container : uint8_t {
    S2Shape = 0,
    S2Region = 1,
  };
  enum class Point : uint8_t {
    S2Point = 0,   // Use S2Point representation, it's completely lossless
    S2LatLng = 1,  // Use S2LatLng representation, lossy
    // TODO(MBkkt) implement they?
    // S2PointFloat = 2,   // Same as S2Point, but use float instead of double
    // S2LatLngFloat = 3,  // Same as S2LatLng, but use float instead of double
  };
  static constexpr auto kDefaultContainer = Container::S2Shape;
  static constexpr auto kDefaultPoint = Point::S2Point;
  static constexpr auto kDefaultHint = s2coding::CodingHint::COMPACT;

  Container container = kDefaultContainer;
  Point multiPoint = kDefaultPoint;
  Point point = kDefaultPoint;
  s2coding::CodingHint hint = kDefaultHint;
};

template<Type t>
consteval auto toTagRegion() noexcept {
  static_assert(
      static_cast<std::underlying_type_t<Type>>(t) < 15,
      "less because we want to use 0xF0 as special value to extend tag format");
  return static_cast<uint8_t>(static_cast<uint32_t>(t) << 4U |
                              static_cast<uint32_t>(0x0F));
}

template<Type t, Options::Point p = Options::Point::S2Point>
consteval auto toTag() noexcept {
  static_assert(
      static_cast<std::underlying_type_t<Type>>(t) < 15,
      "less because we want to use 0xF0 as special value to extend tag format");
  static_assert(
      static_cast<std::underlying_type_t<Options::Point>>(p) < 15,
      "less because we want to use 0x0F as tag for Container::S2Region");
  return static_cast<uint8_t>(static_cast<uint32_t>(t) << 4U |
                              static_cast<uint32_t>(p));
}
/*

constexpr auto toType(uint8_t byte) {
  return static_cast<uint8_t>(static_cast<uint32_t>(byte) & 0xF0);
}

constexpr auto toPoint(uint8_t byte) {
  return static_cast<uint8_t>(static_cast<uint32_t>(byte) & 0x0F);
}
*/

}  // namespace coding

void ensureLittleEndian();

void encodePointToLatLng(Encoder& encoder, S2Point const& point);
void decodePointFromLatLng(Decoder& decoder, S2Point& point);

void encodePoint(Encoder& encoder, S2Point const& point,
                 coding::Options::Point options);

bool decodePoint(Decoder& decoder, S2Point& point);

bool decodeVectorPoint(Decoder& decoder, uint8_t tag,
                       std::vector<S2Point>& points);

S2Point getPointsCentroid(std::span<S2Point const> points) noexcept;

/*

void encodePolyline(Encoder& encoder, S2LaxPolylineShape const& polyline,
                    coding::Options::Point options);

bool decodePolyline(Decoder& decoder, S2LaxPolylineShape const& polyline);
*/

}  // namespace arangodb::geo
