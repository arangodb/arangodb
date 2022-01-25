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
//   3rdParty\s2geometry\dfefe0c\src\s2/base/logging.h(82,21): error : private
//   field 'severity_' is not used [-Werror,-Wunused-private-field]
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
#include <s2/s2cell_id.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <velocypack/Slice.h>

#include "Basics/Result.h"
#include "Geo/GeoParams.h"
#include "Geo/Shapes.h"

class S2Region;
class S2RegionCoverer;

namespace arangodb {
namespace geo {

/// interval to scan over for near / within /intersect queries.
/// Bounds are INCLUSIVE! It may hold true that min === max,
/// in that case a lookup is completely valid. Do not use these
/// bounds for any kind of arithmetics
struct Interval {
  Interval(S2CellId mn, S2CellId mx) noexcept : range_min(mn), range_max(mx) {}
  S2CellId range_min;  /// @brief inclusive minimum cell id
  S2CellId range_max;  /// @brief inclusive maximum cell id
  static bool compare(const Interval& a, const Interval& b) {
    return a.range_min < b.range_min;
  }
};

class Ellipsoid;

/// Utilitiy methods to construct S2Region objects from various definitions,
/// construct coverings for regions with S2RegionCoverer and generate
/// search intervals for use in an iterator
namespace utils {

/// Generate a cover cell from an array [lat, lng] or [lng, lat]
Result indexCellsLatLng(velocypack::Slice const& data, bool isGeoJson,
                        std::vector<S2CellId>& cells, S2Point& centroid);

/// generate intervalls of list of intervals to scan
void scanIntervals(QueryParams const& params, S2RegionCoverer* coverer,
                   S2Region const& region,
                   std::vector<geo::Interval>& sortedIntervals);

/// will return all the intervals including the cells containing them
/// in the less detailed levels. Should allow us to scan all intervals
/// which may contain intersecting geometries
void scanIntervals(QueryParams const& params,
                   std::vector<S2CellId> const& cover,
                   std::vector<geo::Interval>& sortedIntervals);

/// Returns the ellipsoidal distance between p1 and p2 on e (in meters).
/// (solves the inverse geodesic problem)
double geodesicDistance(S2LatLng const& p1, S2LatLng const& p2,
                        geo::Ellipsoid const& e);

/// Returns a point at distance `dist` (in meters) of `p` in direction `azimuth`
/// (in degrees between -180 and 180)
/// (solves the direct geodesic problem)
S2LatLng geodesicPointAtDist(S2LatLng const& p, double dist, double azimuth,
                             geo::Ellipsoid const& e);
}  // namespace utils
}  // namespace geo
}  // namespace arangodb
