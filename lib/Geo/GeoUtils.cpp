////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "GeoUtils.h"

#include <string>
#include <vector>
#include <set>

#include <s2/s2latlng.h>
#include <s2/s2region_coverer.h>

#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Containers/HashSet.h"
#include "Geo/ShapeContainer.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::geo;

Result GeoUtils::indexCellsLatLng(VPackSlice const& data, bool isGeoJson,
                                  std::vector<S2CellId>& cells, S2Point& centroid) {
  if (!data.isArray() || data.length() < 2) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  VPackSlice lat = data.at(isGeoJson ? 1 : 0);
  VPackSlice lon = data.at(isGeoJson ? 0 : 1);
  if (!lat.isNumber() || !lon.isNumber()) {
    return TRI_ERROR_BAD_PARAMETER;
  }
  S2LatLng ll = S2LatLng::FromDegrees(lat.getNumericValue<double>(),
                                      lon.getNumericValue<double>());
  if (!ll.is_valid()) {
    return TRI_ERROR_BAD_PARAMETER;
  }
  centroid = ll.ToPoint();
  cells.emplace_back(centroid);
  return TRI_ERROR_NO_ERROR;
}

/// generate intervalls of list of intervals to scan
void GeoUtils::scanIntervals(QueryParams const& params, S2RegionCoverer* coverer,
                             S2Region const& region, std::vector<Interval>& sortedIntervals) {
  std::vector<S2CellId> cover;
  coverer->GetCovering(region, &cover);
  TRI_ASSERT(!cover.empty());
  TRI_ASSERT(params.cover.worstIndexedLevel == coverer->options().min_level());
  scanIntervals(params, cover, sortedIntervals);
}

/// will return all the intervals including the cells containing them
/// in the less detailed levels. Should allow us to scan all intervals
/// which may contain intersecting geometries
void GeoUtils::scanIntervals(QueryParams const& params, std::vector<S2CellId> const& cover,
                             std::vector<Interval>& sortedIntervals) {
  TRI_ASSERT(params.cover.worstIndexedLevel > 0);
  if (cover.empty()) {
    return;
  }
  // reserve some space
  int pl = std::max(cover[0].level() - params.cover.worstIndexedLevel, 0);
  sortedIntervals.reserve(cover.size() + pl * cover.size());

  // prefix matches
  for (S2CellId const& prefix : cover) {
    if (prefix.is_leaf()) {
      sortedIntervals.emplace_back(prefix, prefix);
    } else {
      sortedIntervals.emplace_back(prefix.range_min(), prefix.range_max());
    }
  }

  if (!params.pointsOnly) {
    // we need to find larger cells that may still contain (parts of) the cover,
    // these are parent cells, up to the minimum allowed cell level allowed in
    // the index. In that case we do not need to look at all sub-cells only
    // at the exact parent cell id. E.g. we got cover cell id [47|11|50]; we do
    // not need
    // to look at [47|1|40] or [47|11|60] because these cells don't intersect,
    // but polygons indexed with exact cell id [47|11] still might.
    ::arangodb::containers::HashSet<uint64_t> parentSet;
    for (const S2CellId& interval : cover) {
      S2CellId cell = interval;

      // add all parent cells of our "exact" cover
      while (params.cover.worstIndexedLevel < cell.level()) {  // don't use
                                                               // level < 0
        cell = cell.parent();
        parentSet.insert(cell.id());
      }
    }
    // just add them, sort them later
    for (uint64_t exact : parentSet) {
      sortedIntervals.emplace_back(S2CellId(exact), S2CellId(exact));
    }
  }

  // sort these disjunct intervals
  std::sort(sortedIntervals.begin(), sortedIntervals.end(), Interval::compare);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  //  constexpr size_t diff = 64;
  for (size_t i = 0; i < sortedIntervals.size() - 1; i++) {
    TRI_ASSERT(sortedIntervals[i].range_min <= sortedIntervals[i].range_max);
    TRI_ASSERT(sortedIntervals[i].range_max < sortedIntervals[i + 1].range_min);
    /*
    if (std::abs((sortedIntervals[i].max.id() -
                  sortedIntervals[i + 1].min.id())) < diff) {
      sortedIntervals[i].max = sortedIntervals.min
    }*/
  }
  TRI_ASSERT(!sortedIntervals.empty());
  TRI_ASSERT(sortedIntervals[0].range_min < sortedIntervals.back().range_max);
#endif
}
