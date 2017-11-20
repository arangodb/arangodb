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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GEO_GEOJSONPARSER_H
#define ARANGOD_GEO_GEOJSONPARSER_H 1

#include "Basics/Result.h"

#include <velocypack/Slice.h>
#include <cstdint>
#include <vector>

#include <geometry/s2latlng.h>
#include <geometry/s2polygon.h>
#include <geometry/s2polyline.h>

namespace arangodb {
namespace geo {

// should try to comply with https://tools.ietf.org/html/rfc7946
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

  GeoJSONType parseGeoJSONType(velocypack::Slice const& geoJSON) const;
  
  /// @brief Convenience function to build
  Result parseGeoJsonRegion(velocypack::Slice const& geoJSON,
                            std::unique_ptr<S2Region>& region) const;
  
  /// @brief Expects an GeoJson point or an array [lon, lat]
  Result parsePoint(velocypack::Slice const& geoJSON, S2LatLng& latLng) const;
    
  /// @brief parse geoJson polygon or array of loops.
  /// Each loop consists of array of coordinates. [[[lon, lat], [lon, lat], ...],...]
  Result parsePolygon(velocypack::Slice const& geoJSON, S2Polygon& poly) const;
    
  Result parseLinestring(velocypack::Slice const& geoJSON, S2Polyline& ll) const;
  /// @brief parse geoJson multi linestring
  Result parseMultiLinestring(velocypack::Slice const& geoJSON,
                              std::vector<S2Polyline>& ll) const;

  /// @brief Parse a polygon for IS_IN_POLYGON
  /// @param loop an array of arrays with 2 elements each,
  /// representing the points of the polygon in the format [lat, lon]
  Result parseLegacyAQLPolygon(velocypack::Slice const& loop, S2Polygon& poly) const;
  
  bool isGeoJsonWithArea(arangodb::velocypack::Slice const& geoJson);
};

}  // namespace geo
}  // namespace arangodb

#endif
