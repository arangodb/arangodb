////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GEO_SHAPES_H
#define ARANGOD_GEO_SHAPES_H 1

#include <cmath>
#include <string>

#include <s2/s2point.h>

class S2LatLng;

namespace arangodb {
namespace geo {
struct QueryParams;

/// coordinate point on the sphere in DEGREES
struct Coordinate {
 public:
  Coordinate(double lat, double lon) noexcept : latitude(lat), longitude(lon) {}
  Coordinate(Coordinate&& c) noexcept
      : latitude(c.latitude), longitude(c.longitude) {}
  Coordinate(Coordinate const& c) noexcept
      : latitude(c.latitude), longitude(c.longitude) {}

  static Coordinate fromLatLng(S2LatLng const&) noexcept;
  static inline Coordinate Invalid() noexcept { return Coordinate(91, 181); }

 public:
  Coordinate& operator=(Coordinate const& other) noexcept {
    latitude = other.latitude;
    longitude = other.longitude;
    return *this;
  }

  bool operator==(Coordinate const& other) const {
    return latitude == other.latitude && longitude == other.longitude;
  }

  bool operator!=(Coordinate const& other) const {
    return latitude != other.latitude || longitude != other.longitude;
  }

  bool isValid() const {
    return std::abs(latitude) <= 90.0 && std::abs(longitude) <= 180.0;
  }

  std::string toString() const {
    return "(lat: " + std::to_string(latitude) +
           ", lon: " + std::to_string(longitude) + ")";
  }

  S2Point toPoint() const noexcept;

 public:
  double latitude;   // in degrees
  double longitude;  // in degrees
};

}  // namespace geo
}  // namespace arangodb

#endif
