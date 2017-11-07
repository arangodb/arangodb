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

#include "GeoCover.h"
#include "Basics/VelocyPackHelper.h"
#include "Geo/GeoJsonParser.h"
#include "Logger/Logger.h"

#include <velocypack/velocypack-aliases.h>

#include <geometry/s2.h>
#include <geometry/s2loop.h>
#include <geometry/s2polygon.h>
#include <geometry/s2regioncoverer.h>

#include <string>
#include <vector>

using namespace arangodb;
using namespace arangodb::geo;

Result GeoCover::generateCover(S2RegionCoverer* coverer,
                                VPackSlice const& data, bool isGeoJson,
                                std::vector<S2CellId>& cells) {
  if (data.isObject()) {  // actual geojson
    if (!isGeoJson) {
      return TRI_ERROR_BAD_PARAMETER;
    }

    GeoJsonParser parser;
    GeoJsonParser::GeoJSONType t = parser.parseGeoJSONType(data);
    switch (t) {
      case GeoJsonParser::GEOJSON_POINT: {
        S2LatLng ll = parser.parseLatLng(data);
        cells.push_back(S2CellId::FromLatLng(ll));
      }

      case GeoJsonParser::GEOJSON_LINESTRING: {
        S2Polyline ll;
        Result res = parser.parseLinestring(data, ll);
        coverer->GetCovering(ll, &cells);
      }

      case GeoJsonParser::GEOJSON_POLYGON: {
        S2Polygon poly;
        Result res = parser.parsePolygon(data, poly);
        coverer->GetCovering(poly, &cells);
      }
      case GeoJsonParser::GEOJSON_MULTI_POINT:
      case GeoJsonParser::GEOJSON_MULTI_LINESTRING:
      case GeoJsonParser::GEOJSON_MULTI_POLYGON:
      case GeoJsonParser::GEOJSON_GEOMETRY_COLLECTION:
      case GeoJsonParser::GEOJSON_UNKNOWN: {
        return TRI_ERROR_NOT_IMPLEMENTED;  // TODO
      }
    }

    return TRI_ERROR_NO_ERROR;

  } else if (data.isArray() && data.length() >= 2) {
    VPackSlice first = data.at(0);
    VPackSlice second = data.at(1);
    if (!first.isNumber() || !second.isNumber()) {
      return TRI_ERROR_BAD_PARAMETER;
    }
    double longitude, latitude;
    if (isGeoJson) {
      longitude = first.getNumericValue<double>();
      latitude = second.getNumericValue<double>();
    } else {
      latitude = first.getNumericValue<double>();
      longitude = second.getNumericValue<double>();
    }
    return generateCover(latitude, longitude, cells);
  }

  return TRI_ERROR_BAD_PARAMETER;
}

/// convert lat, lng pair into cell id. Always uses max level
Result GeoCover::generateCover(double lat, double lng,
                                std::vector<S2CellId>& cells) {
  S2LatLng ll = S2LatLng::FromDegrees(lat, lng);
  cells.push_back(S2CellId::FromLatLng(ll));
  return TRI_ERROR_NO_ERROR;
}

/// parse geoJson (has to be an area) and generate maxium cover
Result GeoCover::scanIntervals(S2RegionCoverer* coverer, VPackSlice const& data,
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
}

/// generate intervalls of list of intervals to scan
Result GeoCover::scanIntervals(S2RegionCoverer* coverer, S2Region const& region,
                               std::vector<Interval>& sortedIntervals) {
  
  std::vector<S2CellId> cover;
  coverer->GetCovering(region, &cover);
  TRI_ASSERT(!cover.empty());
  scanIntervals(coverer->min_level(), cover, sortedIntervals);
  return TRI_ERROR_NO_ERROR;
}

/// will return all the intervals including the cells containing them
/// in the less detailed levels. Should allow us to scan all intervals
/// which may contain intersecting geometries
void GeoCover::scanIntervals(int worstIndexedLevel, std::vector<S2CellId> const& cover,
                             std::vector<Interval>& sortedIntervals) {
  TRI_ASSERT(worstIndexedLevel > 0);
  
  // prefix matches
  for (S2CellId const& prefix : cover) {
    /*if (prefix.is_leaf()) {
      sortedIntervals.emplace_back(prefix, prefix);
    } else {*/
      sortedIntervals.emplace_back(prefix.range_min(), prefix.range_max());
    //}
  }
  
  // we need to find larger cells that may still contain (parts of) the cover,
  // these are parent cells, up to the minimum allowed cell level allowed in
  // the index. In that case we do not need to look at all sub-cells only
  // at the exact parent cell id. E.g. we got cover cell id [47|11|50]; we do not need
  // to look at [47|1|40] or [47|11|60] because these cells don't intersect,
  // but polygons indexed with exact cell id [47|11] still might.
  std::unordered_set<S2CellId> parentSet;
  for (const S2CellId& interval : cover) {
    S2CellId cell = interval;

    // add all parent cells of our "exact" cover
    while (cell.level() > worstIndexedLevel) { // don't use level < 0
      cell = cell.parent();
      parentSet.insert(cell);
    }
  }
  // just add them sorting them later
  for (S2CellId const& exact : parentSet) {
    sortedIntervals.emplace_back(exact, exact);
  }
  
  std::sort(sortedIntervals.begin(), sortedIntervals.end(), Interval::compare);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // intervals must not overlap
  for (size_t i = 0; i < sortedIntervals.size() - 1; i++) {
    TRI_ASSERT(sortedIntervals[i].min <= sortedIntervals[i].max);
    TRI_ASSERT(sortedIntervals[i].max < sortedIntervals[i+1].min);
  }
#endif
}


bool GeoCover::isGeoJsonWithArea(VPackSlice const& data) {
  if (data.isObject()) {  // no geojson
    return false;
  }

  GeoJsonParser parser;
  GeoJsonParser::GeoJSONType t = parser.parseGeoJSONType(data);
  switch (t) {
    case GeoJsonParser::GEOJSON_POINT:
    case GeoJsonParser::GEOJSON_LINESTRING:
    case GeoJsonParser::GEOJSON_MULTI_POINT:
    case GeoJsonParser::GEOJSON_MULTI_LINESTRING:
      return false;

    case GeoJsonParser::GEOJSON_POLYGON:
    case GeoJsonParser::GEOJSON_MULTI_POLYGON: {
      return true;  // TODO we need to perform actual checking
    }

    case GeoJsonParser::GEOJSON_GEOMETRY_COLLECTION:
    case GeoJsonParser::GEOJSON_UNKNOWN: {
      return false;
    }
  }
  return false;
}
