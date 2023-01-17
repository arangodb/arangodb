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

#include "Assertions/Assert.h"
#include "Geo/Coding.h"
#include "Logger/LogMacros.h"
#include "Basics/application-exit.h"

#include <s2/s2point.h>
#include <s2/s2latlng.h>
#include <s2/encoded_s2point_vector.h>

namespace arangodb::geo {

void ensureLittleEndian() {
  if constexpr (std::endian::native == std::endian::big) {
    LOG_TOPIC("ab9de", FATAL, Logger::FIXME)
        << "Geo serialization is not implemented on big-endian architectures";
    FATAL_ERROR_EXIT();
  }
}

void encodePointToLatLng(Encoder& encoder, S2Point const& point) {
  TRI_ASSERT(encoder.avail() >= 2 * sizeof(double));
  TRI_ASSERT(S2::IsUnitLength(point));
  S2LatLng const latLng{point};
  TRI_ASSERT(latLng.is_valid());
  encoder.putdouble(latLng.lat().radians());
  encoder.putdouble(latLng.lng().radians());
}

void decodePointFromLatLng(Decoder& decoder, S2Point& point) {
  TRI_ASSERT(decoder.avail() >= 2 * sizeof(double));
  auto const lat = decoder.getdouble();
  auto const lng = decoder.getdouble();
  auto const latLng = S2LatLng::FromRadians(lat, lng);
  TRI_ASSERT(latLng.is_valid());
  point = latLng.ToPoint();
  TRI_ASSERT(S2::IsUnitLength(point));
}

using namespace coding;

void encodePoint(Encoder& encoder, S2Point const& point,
                 Options::Point options) {
  ensureLittleEndian();
  TRI_ASSERT(S2::IsUnitLength(point));
  static_assert(sizeof(S2Point) == sizeof(double) * 3);
  if (options == Options::Point::S2Point) {
    TRI_ASSERT(encoder.avail() >= sizeof(S2Point));
    encoder.putn(&point, sizeof(S2Point));
  } else {
    TRI_ASSERT(options == Options::Point::S2LatLng);
    encodePointToLatLng(encoder, point);
  }
}

bool decodePoint(Decoder& decoder, S2Point& point) {
  if constexpr (std::endian::native == std::endian::big) {
    TRI_ASSERT(false);
    return false;
  }
  if (decoder.avail() == 3 * sizeof(double)) {
    point[0] = decoder.getdouble();
    point[1] = decoder.getdouble();
    point[2] = decoder.getdouble();
  } else if (decoder.avail() == 2 * sizeof(double)) {
    decodePointFromLatLng(decoder, point);
  } else {
    TRI_ASSERT(false);
    return false;
  }
  return true;
}

bool decodeVectorPoint(Decoder& decoder, uint8_t tag,
                       std::vector<S2Point>& points) {
  switch (tag) {
    case toTag<Type::kPoint, Options::Point::S2Point>(): {
      s2coding::EncodedS2PointVector impl;
      if (!impl.Init(&decoder)) {
        return false;
      }
      auto const size = impl.size();
      points.resize(size);
      for (size_t i = 0; i != size; ++i) {
        points[i] = impl[static_cast<int>(i)];
      }
    } break;
    case toTag<Type::kPoint, Options::Point::S2LatLng>(): {
      uint64 size = 0;
      if (!decoder.get_varint64(&size) ||
          decoder.avail() < size * 2 * sizeof(double)) {
        return false;
      }
      points.resize(size);
      for (auto& point : points) {
        decodePointFromLatLng(decoder, point);
      }
    } break;
    default:
      TRI_ASSERT(false);
      return false;
  }
  return true;
}

S2Point GetPointsCentroid(std::span<S2Point const> points) noexcept {
  // copied from s2: S2Point GetCentroid(const S2Shape& shape);
  S2Point centroid;
  for (auto const& point : points) {
    centroid += point;
  }
  return centroid;
}

}  // namespace arangodb::geo
