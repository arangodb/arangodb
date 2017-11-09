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

#include "Basics/Result.h"

#include <geometry/s2cellid.h>
#include <velocypack/Slice.h>
#include <cstdint>
#include <vector>

class S2Region;
class S2RegionCoverer;

namespace arangodb {
namespace geo {

/// helper class to deal with S2RegionCoverer and S2CellId
class GeoCover {
 private:
  GeoCover() {}

 public:
  
  /// interval to scan over for near / within /intersect queries.
  /// Bounds are INCLUSIVE! It may hold true that min === max,
  /// in that case a lookup is completely valid. Do not use these
  /// bounds for any kind of arithmetics
  struct Interval {
    Interval(S2CellId mn, S2CellId mx) : min(mn), max(mx) {}
    S2CellId min;
    S2CellId max;
    static bool compare(const Interval& a, const Interval& b) {
      return a.min < b.min;
    }
  };
  
  /// parses geojson or [lat, lng] pairs and turns them into
  /// a minimal set of cellIds ready for indexing
  static Result generateCover(S2RegionCoverer* coverer,
                              velocypack::Slice const& data, bool isGeoJson,
                              std::vector<S2CellId>& cells);

  /// convert lat, lng pair into cell id. Always uses max level
  static Result generateCover(double lat, double lng,
                              std::vector<S2CellId>& cells);
  
  /// parse geoJson (has to be an area) and generate list of intervals to scan
  static Result scanIntervals(S2RegionCoverer* coverer, velocypack::Slice const& data,
                              std::vector<Interval>& sortedIntervals);
  
  /// generate intervalls of list of intervals to scan
  static void scanIntervals(S2RegionCoverer* coverer, S2Region const& region,
                              std::vector<Interval>& sortedIntervals);
  
  /// will return all the intervals including the cells containing them
  /// in the less detailed levels. Should allow us to scan all intervals
  /// which may contain intersecting geometries
  static void scanIntervals(int worstIndexedLevel, std::vector<S2CellId> const& cover,
                            std::vector<Interval>& sortedIntervals);

  static bool isGeoJsonWithArea(arangodb::velocypack::Slice const& geoJson);
};

}  // namespace geo
}  // namespace arangodb

#endif
