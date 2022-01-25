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
/// @author Heiko Kernbach
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <vector>

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

#include <velocypack/Slice.h>

#include "Basics/Result.h"

class S2LatLng;
class S2Loop;
class S2Polyline;

namespace arangodb {
namespace geo {
class ShapeContainer;

namespace geojson {
/// Simple GeoJson parser should be more or less forgiving
/// and complies with most of https://tools.ietf.org/html/rfc7946

enum class Type : uint8_t {
  UNKNOWN = 0,
  POINT,
  LINESTRING,
  POLYGON,
  MULTI_POINT,
  MULTI_LINESTRING,
  MULTI_POLYGON,  // todo
  GEOMETRY_COLLECTION
};

struct Fields {
  static constexpr auto kCoordinates =
      "coordinates";                     // mandatory, value depends on type
  static constexpr auto kType = "type";  // mandatory
};

Type type(velocypack::Slice const& vpack);

/// @brief Convenience function to build a region from a GeoJson type.
Result parseRegion(velocypack::Slice const& vpack, ShapeContainer& region);

/// @brief Expects an GeoJson point or an array [lon, lat]
Result parsePoint(velocypack::Slice const& vpack, S2LatLng& latLng);

/// @brief Expects an GeoJson array of points [[lon1, lat1], [lon2, lat2],...]
Result parseMultiPoint(velocypack::Slice const& vpack, ShapeContainer& region);

Result parseLinestring(velocypack::Slice const& vpack, S2Polyline& ll);
/// @brief parse GeoJson multi linestring
Result parseMultiLinestring(velocypack::Slice const& vpack,
                            std::vector<S2Polyline>& ll);

/// @brief parse GeoJson polygon or array of loops. Each loop consists of
/// an array of coordinates: Example [[[lon, lat], [lon, lat], ...],...]
Result parsePolygon(velocypack::Slice const& vpack, ShapeContainer& region);

/// @brief parse GeoJson polygon or array of loops. Each loop consists of
/// an array of coordinates: Example [[[lon, lat], [lon, lat], ...],...].
/// The multipolygon contains an array of looops
Result parseMultiPolygon(velocypack::Slice const& vpack,
                         ShapeContainer& region);

/// @brief Parse a loop (LinearRing)
///
/// Note that at the moment we do not enforce that the final coordinate must
/// match the first, as is required of a proper LinearRing in GeoJSON format.
///
/// Note: Subject to removal when deprecated IS_IN_POLYGON function is removed.
///
/// @param coords  an array of arrays with 2 elements each, representing the
///                points of the polygon
/// @param geoJson If true, the points are assumed to be [lon, lat], otherwise
///                [lat, lon]
/// @param loop    output parameter to hold the parsed loop
Result parseLoop(velocypack::Slice const& coords, bool geoJson, S2Loop& loop);

}  // namespace geojson
}  // namespace geo
}  // namespace arangodb
