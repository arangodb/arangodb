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
/// @author Heiko Kernbach
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "GeoJsonParser.h"
#include "Basics/VelocyPackHelper.h"
#include "Geo/Shapes.h"
#include "Logger/Logger.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <geometry/s2.h>
#include <geometry/s2loop.h>
#include <geometry/s2multipointregion.h>
#include <geometry/s2multipolyline.h>
#include <geometry/s2pointregion.h>
#include <geometry/s2polygon.h>
#include <geometry/s2polyline.h>
#include <geometry/strings/split.h>
#include <geometry/strings/strutil.h>

#include <string>
#include <vector>

using namespace arangodb;
using namespace arangodb::geo;

// This field must be present, and...
static const std::string GEOJSON_TYPE = "type";
// Have one of these values:
static const std::string GEOJSON_TYPE_POINT = "Point";
static const std::string GEOJSON_TYPE_LINESTRING = "PolyLine";
static const std::string GEOJSON_TYPE_POLYGON = "Polygon";
static const std::string GEOJSON_TYPE_MULTI_POINT = "MultiPoint";
static const std::string GEOJSON_TYPE_MULTI_LINESTRING = "MultiLineString";
static const std::string GEOJSON_TYPE_MULTI_POLYGON = "MultiPolygon";
static const std::string GEOJSON_TYPE_GEOMETRY_COLLECTION =
    "GeometryCollection";
// This field must also be present.  The value depends on the type.
static const std::string FIELD_COORDINATES = "coordinates";


static inline bool cmpICase(char a, char b) {
  return std::toupper(a) == std::toupper(b);
}

static bool icompare(std::string const& s1, std::string const& s2) {
       return s1.size() == s2.size() &&
              std::equal(s1.begin(), s1.end(), s2.begin(), cmpICase);
}

/// @brief parse GeoJSON Type
GeoJsonParser::GeoJSONType GeoJsonParser::parseGeoJSONType(
    VPackSlice const& geoJSON) {
  if (!geoJSON.isObject()) {
    return GeoJSONType::UNKNOWN;
  }

  VPackSlice type = geoJSON.get("type");
  //VPackSlice coordinates = geoJSON.get("coordinates");

  if (!type.isString()) {
    return GeoJSONType::UNKNOWN;
  } /*else if (!coordinates.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Invalid GeoJSON coordinates format.");
  }*/
  
  std::string typeStr = type.copyString();
  if (icompare(typeStr, GEOJSON_TYPE_POINT)) {
    return GeoJSONType::POINT;
  } else if (icompare(typeStr, GEOJSON_TYPE_LINESTRING) ) {
    return GeoJSONType::LINESTRING;
  } else if (icompare(typeStr, GEOJSON_TYPE_POLYGON)) {
    return GeoJSONType::POLYGON;
  } else if (icompare(typeStr, GEOJSON_TYPE_MULTI_POINT)) {
    return GeoJSONType::MULTI_POINT;
  } else if (icompare(typeStr, GEOJSON_TYPE_MULTI_LINESTRING)) {
    return GeoJSONType::MULTI_LINESTRING;
  } else if (icompare(typeStr, GEOJSON_TYPE_MULTI_POLYGON)) {
    return GeoJSONType::MULTI_POLYGON;
  } else if (icompare(typeStr, GEOJSON_TYPE_GEOMETRY_COLLECTION)) {
    return GeoJSONType::GEOMETRY_COLLECTION;
  }
  return GeoJSONType::UNKNOWN;
};

Result GeoJsonParser::parseGeoJson(VPackSlice const& geoJSON,
                                   ShapeContainer& region) {
  Result badParam(TRI_ERROR_BAD_PARAMETER, "Invalid GeoJSON Geometry Object.");
  if (!geoJSON.isObject()) {
    return badParam;
  }

  GeoJsonParser::GeoJSONType t = parseGeoJSONType(geoJSON);
  switch (t) {
    case GeoJSONType::POINT: {
      S2LatLng ll;
      Result res = parsePoint(geoJSON, ll);
      if (res.ok()) {
        region.reset(new S2PointRegion(ll.ToPoint()),
                     ShapeContainer::Type::S2_POINT);
      }
      return res;
    }
    case GeoJSONType::MULTI_POINT: {
      std::vector<S2Point> vertices;
      Result res = parsePoints(geoJSON, true, vertices);
      if (res.ok()) {
        region.reset(new S2MultiPointRegion(&vertices),
                     ShapeContainer::Type::S2_MULTIPOINT);
      }
      return res;
    }
    case GeoJSONType::LINESTRING: {
      auto line = std::make_unique<S2Polyline>();
      Result res = parseLinestring(geoJSON, *line.get());
      if (res.ok()) {
        region.reset(std::move(line), ShapeContainer::Type::S2_POLYLINE);
      }
      return res;
    }
    case GeoJSONType::MULTI_LINESTRING: {
      std::vector<S2Polyline> polylines;
      Result res = parseMultiLinestring(geoJSON, polylines);
      if (res.ok()) {
        region.reset(new S2MultiPolyline(std::move(polylines)),
                     ShapeContainer::Type::S2_MULTIPOLYLINE);
      }
      return res;
    }
    case GeoJSONType::POLYGON: {
      auto poly = std::make_unique<S2Polygon>();
      Result res = parsePolygon(geoJSON, *poly.get());
      if (res.ok()) {
        region.reset(std::move(poly), ShapeContainer::Type::S2_POLYGON);
      }
      return res;
    }
    case GeoJSONType::MULTI_POLYGON:
    case GeoJSONType::GEOMETRY_COLLECTION:
      return Result(TRI_ERROR_NOT_IMPLEMENTED, "GeoJSON type is not supported");
    case GeoJSONType::UNKNOWN: {
      return badParam;
    }
  }
}

// parse geojson coordinates into s2 points
Result GeoJsonParser::parsePoints(VPackSlice const& geoJSON, bool geoJson,
                                  std::vector<S2Point>& vertices) {
  vertices.clear();
  VPackSlice coordinates = geoJSON;
  if (geoJSON.isObject()) {
    coordinates = geoJSON.get("coordinates");
  }
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "coordinates missing");
  }

  for (VPackSlice pt : VPackArrayIterator(coordinates)) {
    if (!pt.isArray() || pt.length() < 2) {
      return Result(TRI_ERROR_BAD_PARAMETER, "bad coordinate " + pt.toJson());
    }

    VPackSlice lat = pt.at(geoJson ? 1 : 0);
    VPackSlice lon = pt.at(geoJson ? 0 : 1);
    if (!lat.isNumber() || !lon.isNumber()) {
      return Result(TRI_ERROR_BAD_PARAMETER, "bad coordinate " + pt.toJson());
    }
    vertices.push_back(
        S2LatLng::FromDegrees(lat.getNumber<double>(), lon.getNumber<double>())
            .ToPoint());
  }
  return TRI_ERROR_NO_ERROR;
}

/*
static Result MakeLoop(VPackSlice const& geoJSON, S2Loop& loop) {
  std::vector<S2Point> vertices;
  Result res = parsePoints(geoJSON, vertices);
  if (res.ok()) {
    loop.Init(vertices);
    loop.Normalize();
  }
  return res;
}*/

static Result isClosedLoop(std::vector<S2Point> const& vertices) {
  if (vertices.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Empty loop");
  } else if (vertices.front() != vertices.back()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Loop not closed");
  }
  return TRI_ERROR_NO_ERROR;
}

static void removeAdjacentDuplicates(std::vector<S2Point>& vertices) {
  for (size_t i = 0; i < vertices.size() - 1; i++) {
    if (vertices[i] == vertices[i + 1]) {
      vertices.erase(vertices.begin() + i + 1);
      i--;
    }
  }
}

/*std::vector<S2Polygon*> MakeMultiPolygon(VPackSlice const& geoJSON) {
  std::vector<S2Polygon*> polygonsArr;

  VPackSlice coordinates = geoJSON.get("coordinates");

  VPackBuilder b;

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
  }

  return polygonsArr;
}*/

// create a std vector filled with points (multipoint)
/*std::vector<S2LatLng> MakeMultiPoint(VPackSlice const& geoJSON) {
  std::vector<S2LatLng> multiPoint;
  ParsePoints(geoJSON, &multiPoint);

  return multiPoint;
}

/// @brief create and return MultiPolygon
std::vector<S2Polygon*> GeoParser::parseMultiPolygon(const AqlValue geoJSON) {
  return MakeMultiPolygon(geoJSON);
};*/

/// @brief create s2 latlng
Result GeoJsonParser::parsePoint(VPackSlice const& geoJSON, S2LatLng& latLng) {
  VPackSlice coordinates = geoJSON.get(FIELD_COORDINATES);

  if (coordinates.isArray() && coordinates.length() == 2) {
    latLng = S2LatLng::FromDegrees(coordinates.at(1).getDouble(),
                                   coordinates.at(0).getDouble())
                 .Normalized();
    return TRI_ERROR_NO_ERROR;
  }
  return TRI_ERROR_BAD_PARAMETER;
}

/// https://tools.ietf.org/html/rfc7946#section-3.1.6
/// First Loop represent the outer bound, subsequent loops must be holes
/// { "type": "Polygon",
///   "coordinates": [
///    [ [100.0, 0.0], [101.0, 0.0], [101.0, 1.0], [100.0, 1.0], [100.0, 0.0] ],
///    [ [100.2, 0.2], [100.8, 0.2], [100.8, 0.8], [100.2, 0.8], [100.2, 0.2] ]
///   ]
/// }
Result GeoJsonParser::parsePolygon(VPackSlice const& geoJSON, S2Polygon& poly) {
  VPackSlice coordinates = geoJSON;
  if (geoJSON.isObject()) {
    coordinates = geoJSON.get(FIELD_COORDINATES);
    TRI_ASSERT(parseGeoJSONType(geoJSON) == GeoJSONType::POLYGON);
  }
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "coordinates missing");
  }

  // Coordinates of a Polygon are an array of LinearRing coordinate arrays.
  // The first element in the array represents the exterior ring. Any subsequent
  // elements
  // represent interior rings (or holes).
  // - A linear ring is a closed LineString with four or more positions.
  // -  The first and last positions are equivalent, and they MUST contain
  //    identical values; their representation SHOULD also be identical.
  std::vector<std::unique_ptr<S2Loop>> loops;
  for (VPackSlice loopPts : VPackArrayIterator(coordinates)) {
    std::vector<S2Point> vertices;
    Result res = parsePoints(loopPts, /*geoJson*/ true, vertices);
    if (res.fail()) {
      return res;
    }
    res = isClosedLoop(vertices); // check last vertices are same
    if (res.fail()) {
      return res;
    }

    removeAdjacentDuplicates(vertices);  // s2loop doesn't like duplicates
    vertices.resize(vertices.size() - 1); // remove redundant last vertex
    loops.push_back(std::make_unique<S2Loop>(vertices));
    if (!loops.back()->IsValid()) {  // will check first and last for us
      return Result(TRI_ERROR_BAD_PARAMETER, "Invalid loop in polygon");
    }
    S2Loop* loop = loops.back().get();
    loop->Normalize();

    // Any subsequent loops must be holes within first loop
    if (loops.size() > 1 && !loops.front()->Contains(loop)) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "Subsequent loop not a hole in polygon");
    }
  }

  std::vector<S2Loop*> ptrs;
  for (auto const& ll : loops) {
    ptrs.emplace_back(ll.get());
  }
  if (!S2Polygon::IsValid(ptrs)) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid polygon");
  }
  poly.Init(&ptrs);
  TRI_ASSERT(poly.IsValid());

  for (std::unique_ptr<S2Loop>& ll : loops) {
    ll.release();
  }
  return TRI_ERROR_NO_ERROR;
}

// https://tools.ietf.org/html/rfc7946#section-3.1.4
/// from the rfc {"type":"LineString","coordinates":[[100.0, 0.0],[101.0,1.0]]}
Result GeoJsonParser::parseLinestring(VPackSlice const& geoJson,
                                      S2Polyline& linestring) {
  // verify polygon values
  if (!geoJson.isObject() && !geoJson.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid linestring inpuz");
  }
  TRI_ASSERT(!geoJson.isObject() ||
             parseGeoJSONType(geoJson) == GeoJSONType::LINESTRING);

  std::vector<S2Point> vertices;
  Result res = parsePoints(geoJson, /*geoJson*/ true, vertices);
  if (res.ok()) {
    removeAdjacentDuplicates(vertices);
    if (vertices.size() < 2 || !S2Polyline::IsValid(vertices)) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "Invalid linestring, adjacent "
                    "vertices must not be identical or antipodal.");
    }
    linestring.Init(vertices);
    TRI_ASSERT(vertices.empty());
  }
  return res;
};

/// Coordinates of a MultiLineString are an array of LineString coordinate
/// arrays:
Result GeoJsonParser::parseMultiLinestring(VPackSlice const& geoJSON,
                                           std::vector<S2Polyline>& ll) {
  TRI_ASSERT(parseGeoJSONType(geoJSON) == GeoJSONType::LINESTRING);
  VPackSlice coordinates = geoJSON.get(FIELD_COORDINATES);
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "coordinates missing");
  }

  for (VPackSlice linestring : VPackArrayIterator(coordinates)) {
    S2Polyline polyline;
    // can handle lineatring array
    Result res = parseLinestring(linestring, polyline);
    if (res.fail()) {
      return res;
    }
    ll.emplace_back(std::move(polyline));
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief Parse a polygon for IS_IN_POLYGON
/// @param loop an array of arrays with 2 elements each,
/// representing the points of the polygon in the format [lat, lon]
Result GeoJsonParser::parseLoop(VPackSlice const& coords,
                                S2Loop& loop) {
  if (!coords.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "coordinates missing");
  }

  std::vector<S2Point> vertices;
  Result res = parsePoints(coords, /*geoJson*/ false, vertices);
  if (res.fail()) {
    return res;
  }
  res = isClosedLoop(vertices); // check last vertices are same
  if (res.fail()) {
    return res;
  }

  removeAdjacentDuplicates(vertices);  // s2loop doesn't like duplicates
  vertices.resize(vertices.size() - 1); // remove redundant last vertex
  loop.Init(vertices);
  if (!loop.IsValid()) {  // will check first and last for us
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid GeoJson loop");
  }
  loop.Normalize();

  return TRI_ERROR_NO_ERROR;
}
