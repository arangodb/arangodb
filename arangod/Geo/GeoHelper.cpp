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

#include "GeoHelper.h"
#include "Geo/GeoJsonParser.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"

#include <velocypack/velocypack-aliases.h>

#include <geometry/s2.h>
#include <geometry/s2loop.h>
#include <geometry/s2polygon.h>
#include <geometry/s2regioncoverer.h>

#include <vector>
#include <string>

using namespace arangodb;
using namespace arangodb::geo;

Result GeoHelper::generateS2CellIds(S2RegionCoverer* coverer,
                                    VPackSlice const& field,
                                    bool isGeoJson,
                                    std::vector<S2CellId>& cells) {
  
  if (field.isObject()) { // actual geojson
    if (!isGeoJson) {
      return TRI_ERROR_BAD_PARAMETER;
    }
    
    GeoJsonParser parser;
    GeoJsonParser::GeoJSONType t = parser.parseGeoJSONType(field);
    switch (t) {
      case GeoJsonParser::GEOJSON_POINT: {
        S2LatLng ll = parser.parseLatLng(field);
        cells.push_back(S2CellId::FromLatLng(ll));
      }
        
      case GeoJsonParser::GEOJSON_LINESTRING: {
        S2Polyline ll;
        Result res = parser.parseLinestring(field, ll);
        coverer->GetCovering(ll, &cells);
      }
        
      case GeoJsonParser::GEOJSON_POLYGON: {
        S2Polygon poly;
        Result res = parser.parsePolygon(field, poly);
        coverer->GetCovering(poly, &cells);
      }
      case GeoJsonParser::GEOJSON_MULTI_POINT:
      case GeoJsonParser::GEOJSON_MULTI_LINESTRING:
      case GeoJsonParser::GEOJSON_MULTI_POLYGON:
      case GeoJsonParser::GEOJSON_GEOMETRY_COLLECTION:
      case GeoJsonParser::GEOJSON_UNKNOWN:{
        return TRI_ERROR_NOT_IMPLEMENTED; // TODO
      }
    }
    
    return TRI_ERROR_NO_ERROR;
    
  } else if (field.isArray() && field.length() >= 2) {
    
    VPackSlice first = field.at(0);
    if (!first.isNumber()) {
      return TRI_ERROR_BAD_PARAMETER;
    }
    VPackSlice second = field.at(1);
    if (!second.isNumber()) {
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
    return generateS2CellIdFromLatLng(latitude, longitude, cells);
  }
  
  return TRI_ERROR_BAD_PARAMETER;
}

Result GeoHelper::generateS2CellIdFromLatLng(double lat, double lng,
                                             std::vector<S2CellId>& cells) {
  S2LatLng ll = S2LatLng::FromDegrees(lat, lng);
  cells.push_back(S2CellId::FromLatLng(ll));
  return TRI_ERROR_NO_ERROR;
}

bool GeoHelper::isGeoJsonWithArea(arangodb::velocypack::Slice const& geoJson) {
  if (geoJson.isObject()) { // no geojson
    return false;
  }
  
  GeoJsonParser parser;
  GeoJsonParser::GeoJSONType t = parser.parseGeoJSONType(geoJson);
  switch (t) {
    case GeoJsonParser::GEOJSON_POINT:
    case GeoJsonParser::GEOJSON_LINESTRING:
    case GeoJsonParser::GEOJSON_MULTI_POINT:
    case GeoJsonParser::GEOJSON_MULTI_LINESTRING:
      return false;
      
    case GeoJsonParser::GEOJSON_POLYGON:
    case GeoJsonParser::GEOJSON_MULTI_POLYGON:{
      return true; // TODO we need to perform actual checking
    }
      
    case GeoJsonParser::GEOJSON_GEOMETRY_COLLECTION:
    case GeoJsonParser::GEOJSON_UNKNOWN:{
      return false;
    }
  }
  return false;
}
