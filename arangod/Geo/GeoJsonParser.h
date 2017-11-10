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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GEO_GEOPARSER_H
#define ARANGOD_GEO_GEOPARSER_H 1

#include "Basics/Result.h"

#include <velocypack/Slice.h>
#include <cstdint>
#include <vector>

#include <geometry/s2latlng.h>
#include <geometry/s2polygon.h>
#include <geometry/s2polyline.h>

namespace arangodb {
namespace geo {

class GeoJsonParser {
 public:
  GeoJsonParser() {}

 public:
  enum GeoJSONType : uint8_t {
    GEOJSON_UNKNOWN = 0,
    GEOJSON_POINT,
    GEOJSON_LINESTRING,
    GEOJSON_POLYGON,
    GEOJSON_MULTI_POINT,
    GEOJSON_MULTI_LINESTRING,
    GEOJSON_MULTI_POLYGON,  // todo
    GEOJSON_GEOMETRY_COLLECTION
  };

  GeoJSONType parseGeoJSONType(velocypack::Slice const& geoJSON);
  S2Point parsePoint(velocypack::Slice const& geoJSON);
  S2LatLng parseLatLng(velocypack::Slice const& geoJSON);
  Result parsePolygon(velocypack::Slice const& geoJSON, S2Polygon& poly);
  Result parseLinestring(velocypack::Slice const& geoJSON, S2Polyline& ll);
  std::vector<S2Point> parseMultiPoint(velocypack::Slice const& geoJSON);
};

}  // namespace geo
}  // namespace arangodb

#endif
