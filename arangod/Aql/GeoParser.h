////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_GEOPARSER_H
#define ARANGOD_AQL_GEOPARSER_H 1

#include "Basics/Common.h"
#include "Aql/AqlValue.h"

#include <geometry/s2.h>
#include <geometry/s2loop.h>
#include <geometry/s2polygon.h>
#include <geometry/strings/split.h>
#include <geometry/strings/strutil.h>

namespace arangodb {

namespace velocypack {
  class Builder;
  class Slice;
}

namespace aql {

  class GeoParser {
    public:
      virtual ~GeoParser() {}

    public:
      enum GeoJSONType {
        GEOJSON_UNKNOWN = 0,
        GEOJSON_POINT,
        GEOJSON_LINESTRING,
        GEOJSON_POLYGON,
        GEOJSON_MULTI_POINT,
        GEOJSON_MULTI_LINESTRING,
        GEOJSON_MULTI_POLYGON,
        GEOJSON_GEOMETRY_COLLECTION
      };

      bool parseGeoJSONType(const AqlValue geoJSON);
      bool parseGeoJSONTypePoint(const AqlValue geoJSON);
      bool parseGeoJSONTypePolygon(const AqlValue geoJSON);
      bool parseGeoJSONTypePolyline(const AqlValue geoJSON);
      S2Point parseGeoJSONPoint(const AqlValue geoJSON);
      S2LatLng parseGeoJSONLatLng(const AqlValue geoJSON);
      S2Polygon* parseGeoJSONPolygon(const AqlValue geoJSON);
      S2Polyline* parseGeoJSONPolyline(const AqlValue geoJSON);
      vector<S2Point> parseGeoJSONMultiPoint(const AqlValue geoJSON);
    private:

  };

}  // namespace aql
}  // namespace arangodb

#endif
