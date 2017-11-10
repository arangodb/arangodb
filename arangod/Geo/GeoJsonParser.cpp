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

#include "GeoJsonParser.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <geometry/s2.h>
#include <geometry/s2loop.h>
#include <geometry/s2polygon.h>
#include <geometry/strings/split.h>
#include <geometry/strings/strutil.h>

#include <string>
#include <vector>

using namespace arangodb;
using namespace arangodb::geo;

// This field must be present, and...
static const string GEOJSON_TYPE = "type";
// Have one of these values:
static const string GEOJSON_TYPE_POINT = "Point";
static const string GEOJSON_TYPE_LINESTRING = "PolyLine";
static const string GEOJSON_TYPE_POLYGON = "Polygon";
static const string GEOJSON_TYPE_MULTI_POINT = "MultiPoint";
static const string GEOJSON_TYPE_MULTI_LINESTRING = "MultiLineString";
static const string GEOJSON_TYPE_MULTI_POLYGON = "MultiPolygon";
static const string GEOJSON_TYPE_GEOMETRY_COLLECTION = "GeometryCollection";
// This field must also be present.  The value depends on the type.
static const string GEOJSON_COORDINATES = "coordinates";

/// @brief parse GeoJSON Type
GeoJsonParser::GeoJSONType GeoJsonParser::parseGeoJSONType(
    VPackSlice const& geoJSON) {
  if (!geoJSON.isObject()) {
    return GeoJsonParser::GEOJSON_UNKNOWN;
  }

  VPackSlice type = geoJSON.get("type");
  VPackSlice coordinates = geoJSON.get("coordinates");

  if (!type.isString()) {
    return GeoJsonParser::GEOJSON_UNKNOWN;
  }

  if (!coordinates.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Invalid GeoJSON coordinates format.");
  }

  const string& typeString = type.copyString();
  if (GEOJSON_TYPE_POINT == typeString) {
    return GeoJsonParser::GEOJSON_POINT;
  } else if (GEOJSON_TYPE_LINESTRING == typeString) {
    return GeoJsonParser::GEOJSON_LINESTRING;
  } else if (GEOJSON_TYPE_POLYGON == typeString) {
    return GeoJsonParser::GEOJSON_POLYGON;
  } else if (GEOJSON_TYPE_MULTI_POINT == typeString) {
    return GeoJsonParser::GEOJSON_MULTI_POINT;
  } else if (GEOJSON_TYPE_MULTI_LINESTRING == typeString) {
    return GeoJsonParser::GEOJSON_MULTI_LINESTRING;
  } else if (GEOJSON_TYPE_MULTI_POLYGON == typeString) {
    return GeoJsonParser::GEOJSON_MULTI_POLYGON;
  } else if (GEOJSON_TYPE_GEOMETRY_COLLECTION == typeString) {
    return GeoJsonParser::GEOJSON_GEOMETRY_COLLECTION;
  }
  return GeoJsonParser::GEOJSON_UNKNOWN;
};

/// @brief parse GeoJSON Polygon Type
/*bool GeoJsonParser::parseGeoJSONTypePolygon(VPackSlice const& geoJSON) {
  VPackSlice type = geoJSON.get("type");
  VPackSlice coordinates = geoJSON.get("coordinates");

  if (!type.isString()) {
    return GeoJsonParser::GEOJSON_UNKNOWN;
  }

  const string& typeString = type.copyString();

  // verify type
  if (GEOJSON_TYPE_POLYGON != typeString) {
    return false;
  }

  if (coordinates.length() < 4) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "Too less
coordinate values to build a polygon.");
  }

  return true;
};

/// @brief parse GeoJSON Polyline/Linetring Type
bool GeoJsonParser::parseGeoJSONTypePolyline(VPackSlice const& geoJSON) {
  VPackSlice type = geoJSON.get("type");

  if (!type.isString()) {
    return GeoJsonParser::GEOJSON_UNKNOWN;
  }

  const string& typeString = type.copyString();

  // verify type
  if (GEOJSON_TYPE_LINESTRING != typeString) {
    return 0;
  }
  return 1;
};

/// @brief parse GeoJSON Point Type
bool GeoJsonParser::parseGeoJSONTypePoint(VPackSlice const& geoJSON) {
  if (!geoJSON.isObject()) {
    return 0;
  }

  VPackSlice slice = geoJSON.slice();
  VPackSlice type = slice.get("type");

  if (!type.isString()) {
    return GeoJsonParser::GEOJSON_UNKNOWN;
  }

  const string& typeString = type.copyString();

  // verify type
  if (GEOJSON_TYPE_POINT != typeString) {
    return 0;
  }
  return 1;
};*/

// parse geojson coordinates into s2 points
void ParsePoints(VPackSlice const& geoJSON, std::vector<S2Point>* vertices) {
  std::vector<S2LatLng> latlngs;
  vertices->clear();

  VPackSlice coordinates = geoJSON.get("coordinates");

  if (coordinates.isArray()) {
    for (auto const& coordinate : VPackArrayIterator(coordinates)) {
      vertices->push_back(
          S2LatLng::FromDegrees(coordinate.at(1).getNumber<double>(),
                                coordinate.at(0).getNumber<double>())
              .ToPoint());
    }
  }
}

static S2Loop* MakeLoop(VPackSlice const& geoJSON) {
  std::vector<S2Point> vertices;
  ParsePoints(geoJSON, &vertices);
  return new S2Loop(vertices);
}

// create a s2 polygon function
Result MakePolygon(VPackSlice const& geoJSON, S2Polygon& poly) {
  std::vector<S2Loop*> loops;

  S2Loop* loop = MakeLoop(geoJSON);

  loop->Normalize();
  loops.push_back(loop);
  poly.Init(&loops);  // Takes ownership.
  TRI_ASSERT(loops.empty());

  return TRI_ERROR_NO_ERROR;
}

std::vector<S2Polygon*> MakeMultiPolygon(VPackSlice const& geoJSON) {
  std::vector<S2Polygon*> polygonsArr;

  VPackSlice coordinates = geoJSON.get("coordinates");

  /*VPackBuilder b;

  b.add(Value(VPackValueType::Object));
  b.add("type", VPackValue("Polygon"));
  b.add("coordinates", VPackValue(VPackValueType::Array));

  if (coordinates.isArray()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(I)";
    for (auto const& polygons : VPackArrayIterator(coordinates)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(2)";
      if (polygons.isArray()) {
        for (auto const& polygon : VPackArrayIterator(polygons)) {
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(3)";
          b.openArray();
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(4)";
          for (auto const& coord : VPackArrayIterator(polygon)) {
            LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(5)";
            if (coord.isNumber()) {
              LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(6)";
              b.add(Value(coord.getNumber<double>()));
            } else {
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH,
  "Invalid type in coordinates array.");
            }
          }
          b.close();
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(7)";
          b.close();
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(8)";
          b.close();
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(9)";

          polygonsArr.push_back(MakePolygon(AqlValue(b)));
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "(10)";
        }
      }
    }
  }*/

  return polygonsArr;
}

// create a s2 point function
S2Point MakePoint(VPackSlice const& geoJSON) {
  VPackSlice coordinates = geoJSON.get("coordinates");

  if (coordinates.isArray() && coordinates.length() == 2) {
    // Note that it's (lat, lng) for S2
    return S2LatLng::FromDegrees(coordinates.at(0).getDouble(),
                                 coordinates.at(1).getDouble())
        .Normalized()
        .ToPoint();
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
}

// create a s2 latlng function
S2LatLng MakeLatLng(VPackSlice const& geoJSON) {
  VPackSlice coordinates = geoJSON.get("coordinates");

  if (coordinates.isArray() && coordinates.length() == 2) {
    return S2LatLng::FromDegrees(coordinates.at(1).getDouble(),
                                 coordinates.at(0).getDouble())
        .Normalized();
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
}

// create a std vector filled with points (multipoint)
std::vector<S2Point> MakeMultiPoint(VPackSlice const& geoJSON) {
  std::vector<S2Point> multiPoint;
  ParsePoints(geoJSON, &multiPoint);

  return multiPoint;
}

/// @brief create and return MultiPolygon
/*std::vector<S2Polygon*> GeoParser::parseMultiPolygon(const AqlValue geoJSON) {
  return MakeMultiPolygon(geoJSON);
};*/

/// @brief create s2 point
S2Point GeoJsonParser::parsePoint(VPackSlice const& geoJSON) {
  return MakePoint(geoJSON);
};

/// @brief create s2 latlng
S2LatLng GeoJsonParser::parseLatLng(VPackSlice const& geoJSON) {
  return MakeLatLng(geoJSON);
};

/// @brief create and return polygon
Result GeoJsonParser::parsePolygon(VPackSlice const& geoJSON, S2Polygon& poly) {
  return MakePolygon(geoJSON, poly);
};

/// @brief create and return linestring
Result GeoJsonParser::parseLinestring(VPackSlice const& geoJSON,
                                      S2Polyline& poly) {
  // TODO #1: verify polygon values
  // VerifyPolygon(geoJSON);

  // TODO #2: build polygon
  // create a s2 polyline function
  std::vector<S2Point> vertices;
  ParsePoints(geoJSON, &vertices);
  poly.Init(vertices);
  TRI_ASSERT(vertices.empty());

  return TRI_ERROR_NO_ERROR;
};

/// @brief create multipoint vector
std::vector<S2Point> GeoJsonParser::parseMultiPoint(VPackSlice const& geoJSON) {
  // TODO #1: verify points values
  // each points -> VerifyMultiPoint(geoJSON);

  // TODO #2: build vector containing points
  return MakeMultiPoint(geoJSON);
};
