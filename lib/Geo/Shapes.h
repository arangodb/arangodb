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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <string>

#ifdef __clang__
#pragma clang diagnostic push
// Suppress the warning
//   3rdParty\s2geometry\dfefe0c\src\s2/util/math/vector3_hash.h(32,24): error :
//   'is_pod<double>' is deprecated: warning STL4025: std::is_pod and
//   std::is_pod_v are deprecated in C++20. The std::is_trivially_copyable
//   and/or std::is_standard_layout traits likely suit your use case. You can
//   define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING or
//   _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS to acknowledge that you have
//   received this warning. [-Werror,-Wdeprecated-declarations]
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
// Suppress the warning
//   3rdParty\s2geometry\dfefe0c\src\s2/base/logging.h(82,21): error :
//   private field 'severity_' is not used [-Werror,-Wunused-private-field]
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
#include <s2/s2point.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

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
