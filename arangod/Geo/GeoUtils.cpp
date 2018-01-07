////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/VelocyPackHelper.h"
#include "Geo/GeoJsonParser.h"
#include "Geo/Shapes.h"
#include "Logger/Logger.h"

#include <geometry/s2.h>
#include <geometry/s2cap.h>
#include <geometry/s2loop.h>
#include <geometry/s2polygon.h>
#include <geometry/s2regioncoverer.h>

#include <string>
#include <vector>

using namespace arangodb;
using namespace arangodb::geo;

Result GeoUtils::indexCellsGeoJson(S2RegionCoverer* coverer,
                                   VPackSlice const& data,
                                   std::vector<S2CellId>& cells,
                                   geo::Coordinate& centroid) {
  if (!data.isObject()) {  // actual geojson
    return TRI_ERROR_BAD_PARAMETER;
  }

  GeoJsonParser::GeoJSONType t = GeoJsonParser::parseGeoJSONType(data);
  switch (t) {
    case GeoJsonParser::GeoJSONType::POINT: {
      S2LatLng ll;
      Result res = GeoJsonParser::parsePoint(data, ll);
      if (res.ok()) {
        cells.push_back(S2CellId::FromLatLng(ll));
        centroid.latitude = ll.lat().degrees();
        centroid.longitude = ll.lng().degrees();
      }
      return res;
    }

    case GeoJsonParser::GeoJSONType::LINESTRING: {
      S2Polyline line;
      Result res = GeoJsonParser::parseLinestring(data, line);
      if (res.ok()) {
        coverer->GetCovering(line, &cells);
        S2LatLng ll(line.GetCentroid());
        centroid.latitude = ll.lat().degrees();
        centroid.longitude = ll.lng().degrees();
      }
      return res;
    }

    case GeoJsonParser::GeoJSONType::POLYGON: {
      S2Polygon poly;
      Result res = GeoJsonParser::parsePolygon(data, poly);
      if (res.ok()) {
        coverer->GetCovering(poly, &cells);
        S2LatLng ll(poly.GetCentroid());
        centroid.latitude = ll.lat().degrees();
        centroid.longitude = ll.lng().degrees();
      }
      return res;
    }
    case GeoJsonParser::GeoJSONType::MULTI_POINT:
    case GeoJsonParser::GeoJSONType::MULTI_LINESTRING:
    case GeoJsonParser::GeoJSONType::MULTI_POLYGON:
    case GeoJsonParser::GeoJSONType::GEOMETRY_COLLECTION:
    case GeoJsonParser::GeoJSONType::UNKNOWN: {
      return TRI_ERROR_NOT_IMPLEMENTED;  // TODO
    }
  }
  return TRI_ERROR_NOT_IMPLEMENTED;
}

Result GeoUtils::indexCellsLatLng(VPackSlice const& data, bool isGeoJson,
                                  std::vector<S2CellId>& cells,
                                  geo::Coordinate& centroid) {
  if (!data.isArray() || data.length() < 2) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  VPackSlice lat = data.at(isGeoJson ? 1 : 0);
  VPackSlice lon = data.at(isGeoJson ? 0 : 1);
  if (!lat.isNumber() || !lat.isNumber()) {
    return TRI_ERROR_BAD_PARAMETER;
  }
  centroid.latitude = lat.getNumericValue<double>();
  centroid.longitude = lon.getNumericValue<double>();
  S2LatLng ll = S2LatLng::FromDegrees(centroid.latitude, centroid.longitude);
  cells.push_back(S2CellId::FromLatLng(ll));
  return TRI_ERROR_NO_ERROR;
}

/// convert lat, lng pair into cell id. Always uses max level
Result GeoUtils::indexCells(geo::Coordinate const& c,
                            std::vector<S2CellId>& cells) {
  S2LatLng ll = S2LatLng::FromDegrees(c.latitude, c.longitude);
  cells.push_back(S2CellId::FromLatLng(ll));
  return TRI_ERROR_NO_ERROR;
}

/// parse geoJson (has to be an area) and generate maxium cover
/*Result GeoCover::scanIntervals(S2RegionCoverer* coverer, VPackSlice const&
data,
                               std::vector<Interval>& sortedIntervals) {
  TRI_ASSERT(isGeoJsonWithArea(data));
  std::vector<S2CellId> cover;
  Result res = generateCover(coverer, data, true, cover);
  if (res.fail()) {
    return res;
  }
  TRI_ASSERT(!cover.empty());
  scanIntervals(coverer->min_level(), cover, sortedIntervals);
  return TRI_ERROR_NO_ERROR;
}*/

/// generate intervalls of list of intervals to scan
void GeoUtils::scanIntervals(S2RegionCoverer* coverer, S2Region const& region,
                             std::vector<Interval>& sortedIntervals) {
  std::vector<S2CellId> cover;
  coverer->GetCovering(region, &cover);
  TRI_ASSERT(!cover.empty());
  scanIntervals(coverer->min_level(), cover, sortedIntervals);
}

/// will return all the intervals including the cells containing them
/// in the less detailed levels. Should allow us to scan all intervals
/// which may contain intersecting geometries
void GeoUtils::scanIntervals(int worstIndexedLevel,
                             std::vector<S2CellId> const& cover,
                             std::vector<Interval>& sortedIntervals) {
  TRI_ASSERT(worstIndexedLevel > 0);
  if (cover.empty()) {
    return;
  }

  // prefix matches
  for (S2CellId const& prefix : cover) {
    if (prefix.is_leaf()) {
      sortedIntervals.emplace_back(prefix, prefix);
    } else {
      sortedIntervals.emplace_back(prefix.range_min(), prefix.range_max());
    }
  }

  // we need to find larger cells that may still contain (parts of) the cover,
  // these are parent cells, up to the minimum allowed cell level allowed in
  // the index. In that case we do not need to look at all sub-cells only
  // at the exact parent cell id. E.g. we got cover cell id [47|11|50]; we do
  // not need
  // to look at [47|1|40] or [47|11|60] because these cells don't intersect,
  // but polygons indexed with exact cell id [47|11] still might.
  std::unordered_set<S2CellId> parentSet;
  for (const S2CellId& interval : cover) {
    S2CellId cell = interval;

    // add all parent cells of our "exact" cover
    while (worstIndexedLevel < cell.level()) {  // don't use level < 0
      cell = cell.parent();
      parentSet.insert(cell);
    }
  }
  // just add them, sort them later
  for (S2CellId const& exact : parentSet) {
    sortedIntervals.emplace_back(exact, exact);
  }
  // sort these disjunct intervals
  std::sort(sortedIntervals.begin(), sortedIntervals.end(), Interval::compare);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  //  constexpr size_t diff = 64;
  for (size_t i = 0; i < sortedIntervals.size() - 1; i++) {
    TRI_ASSERT(sortedIntervals[i].min <= sortedIntervals[i].max);
    TRI_ASSERT(sortedIntervals[i].max < sortedIntervals[i + 1].min);
    /*
    if (std::abs((sortedIntervals[i].max.id() -
                  sortedIntervals[i + 1].min.id())) < diff) {
      sortedIntervals[i].max = sortedIntervals.min
    }*/
  }
  TRI_ASSERT(!sortedIntervals.empty());
  TRI_ASSERT(sortedIntervals[0].min < sortedIntervals.back().max);
#endif
}
