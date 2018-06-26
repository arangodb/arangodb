////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_GEO_GEO_COVER_H
#define ARANGOD_GEO_GEO_COVER_H 1

#include <cstdint>
#include <vector>

#include <s2/s2cell_id.h>

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
  S2CellId range_min;
  S2CellId range_max;
  static bool compare(const Interval& a, const Interval& b) {
    return a.range_min < b.range_min;
  }
};

/// Utilitiy methods to construct S2Region objects from various definitions,
/// construct coverings for regions with S2RegionCoverer and generate
/// search intervals for use in an iterator
class GeoUtils {
 private:
  GeoUtils() {}

 public:
  /// Generate a cover cell from an array [lat, lng] or [lng, lat]
  static Result indexCellsLatLng(velocypack::Slice const& data, bool isGeoJson,
                                 std::vector<S2CellId>& cells,
                                 S2Point& centroid);

  /// generate intervalls of list of intervals to scan
  static void scanIntervals(QueryParams const& params,
                            S2RegionCoverer* coverer, S2Region const& region,
                            std::vector<geo::Interval>& sortedIntervals);

  /// will return all the intervals including the cells containing them
  /// in the less detailed levels. Should allow us to scan all intervals
  /// which may contain intersecting geometries
  static void scanIntervals(QueryParams const& params,
                            std::vector<S2CellId> const& cover,
                            std::vector<geo::Interval>& sortedIntervals);
};

}  // namespace geo
}  // namespace arangodb

#endif
