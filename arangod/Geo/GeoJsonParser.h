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

class S2Polyline;

namespace arangodb {
namespace geo {

// should try to comply with https://tools.ietf.org/html/rfc7946
class GeoJsonParser {
 private:
  GeoJsonParser() {}

 public:
  enum class GeoJSONType : uint8_t {
    UNKNOWN = 0,
    POINT,
    LINESTRING,
    POLYGON,
    MULTI_POINT,
    MULTI_LINESTRING,
    MULTI_POLYGON,  // todo
    GEOMETRY_COLLECTION
  };

  static GeoJSONType parseGeoJSONType(velocypack::Slice const& geoJSON);
  
  /// @brief Convenience function to build a region from a geoJson type.
  static Result parseGeoJsonRegion(velocypack::Slice const& geoJSON,
                                   std::unique_ptr<S2Region>& region);
  
  /// @brief Expects an GeoJson point or an array [lon, lat]
  static Result parsePoint(velocypack::Slice const& geoJSON, S2LatLng& latLng);
    
  /// @brief parse geoJson polygon or array of loops. Each loop consists of
  /// an array of coordinates: Example [[[lon, lat], [lon, lat], ...],...]
  static Result parsePolygon(velocypack::Slice const& geoJSON, S2Polygon& poly);
    
  static Result parseLinestring(velocypack::Slice const& geoJSON, S2Polyline& ll);
  /// @brief parse geoJson multi linestring
  static Result parseMultiLinestring(velocypack::Slice const& geoJSON,
                              std::vector<S2Polyline*>& ll);
  
  // ============= Helpers ============
  
  static Result parsePoints(velocypack::Slice const& geoJSON, bool geoJson,
                            std::vector<S2Point>& vertices);

  /// @brief Parse a polygon for IS_IN_POLYGON
  /// @param loop an array of arrays with 2 elements each,
  /// representing the points of the polygon in the format [lat, lon]
  static Result parseLegacyAQLPolygon(velocypack::Slice const& loop, S2Polygon& poly);
  
  static bool isGeoJsonWithArea(arangodb::velocypack::Slice const& geoJson);
};

}  // namespace geo
}  // namespace arangodb

#endif
