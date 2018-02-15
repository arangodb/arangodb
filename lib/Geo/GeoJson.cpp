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

#include "GeoJson.h"

#include <string>
#include <vector>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <s2/s2loop.h>
#include <s2/s2multipoint_region.h>
#include <s2/s2multipolyline.h>
#include <s2/s2point_region.h>
#include <s2/s2polygon.h>
#include <s2/s2polyline.h>

#include "Basics/VelocyPackHelper.h"
#include "Geo/ShapeContainer.h"
#include "Logger/Logger.h"

namespace {
// This field must be present, and...
const std::string FieldType = "type";
// Have one of these values:
const std::string TypeStringPoint = "Point";
const std::string TypeStringLinestring = "LineString";
const std::string TypeStringPolygon = "Polygon";
const std::string TypeStringMultiPoint = "MultiPoint";
const std::string TypeStringMultiLinestring = "MultiLineString";
const std::string TypeStringMultiPolygon = "MultiPolygon";
const std::string TypeStringGeometryCollection = "GeometryCollection";
// This field must also be present.  The value depends on the type.
const std::string FieldCoordinates = "coordinates";
}  // namespace

namespace {
inline bool sameCharIgnoringCase(char a, char b) {
  return std::toupper(a) == std::toupper(b);
}
}  // namespace

namespace {
bool sameIgnoringCase(std::string const& s1, std::string const& s2) {
  return s1.size() == s2.size() &&
         std::equal(s1.begin(), s1.end(), s2.begin(), ::sameCharIgnoringCase);
}
}  // namespace

namespace {
arangodb::Result verifyClosedLoop(std::vector<S2Point> const& vertices) {
  using arangodb::Result;

  if (vertices.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Empty loop");
  } else if (vertices.front() != vertices.back()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Loop not closed");
  }
  return TRI_ERROR_NO_ERROR;
}
}  // namespace

namespace {
void removeAdjacentDuplicates(std::vector<S2Point>& vertices) {
  for (size_t i = 0; i < vertices.size() - 1; i++) {
    if (vertices[i] == vertices[i + 1]) {
      vertices.erase(vertices.begin() + i + 1);
      i--;
    }
  }
}
}  // namespace

namespace arangodb {
namespace geo {
namespace geojson {

/// @brief parse GeoJSON Type
Type type(VPackSlice const& geoJSON) {
  if (!geoJSON.isObject()) {
    return Type::UNKNOWN;
  }

  VPackSlice type = geoJSON.get("type");
  if (!type.isString()) {
    return Type::UNKNOWN;
  }

  std::string typeStr = type.copyString();
  if (::sameIgnoringCase(typeStr, ::TypeStringPoint)) {
    return Type::POINT;
  } else if (::sameIgnoringCase(typeStr, ::TypeStringLinestring)) {
    return Type::LINESTRING;
  } else if (::sameIgnoringCase(typeStr, ::TypeStringPolygon)) {
    return Type::POLYGON;
  } else if (::sameIgnoringCase(typeStr, ::TypeStringMultiPoint)) {
    return Type::MULTI_POINT;
  } else if (::sameIgnoringCase(typeStr, ::TypeStringMultiLinestring)) {
    return Type::MULTI_LINESTRING;
  } else if (::sameIgnoringCase(typeStr, ::TypeStringMultiPolygon)) {
    return Type::MULTI_POLYGON;
  } else if (::sameIgnoringCase(typeStr, ::TypeStringGeometryCollection)) {
    return Type::GEOMETRY_COLLECTION;
  }
  return Type::UNKNOWN;
};

Result parseRegion(VPackSlice const& geoJSON, ShapeContainer& region) {
  Result badParam(TRI_ERROR_BAD_PARAMETER, "Invalid GeoJSON Geometry Object.");
  if (!geoJSON.isObject()) {
    return badParam;
  }

  Type t = type(geoJSON);
  switch (t) {
    case Type::POINT: {
      S2LatLng ll;
      Result res = parsePoint(geoJSON, ll);
      if (res.ok()) {
        region.reset(new S2PointRegion(ll.ToPoint()),
                     ShapeContainer::Type::S2_POINT);
      }
      return res;
    }
    case Type::MULTI_POINT: {
      std::vector<S2Point> vertices;
      Result res = parsePoints(geoJSON, true, vertices);
      if (res.ok()) {
        region.reset(new S2MultiPointRegion(&vertices),
                     ShapeContainer::Type::S2_MULTIPOINT);
      }
      return res;
    }
    case Type::LINESTRING: {
      auto line = std::make_unique<S2Polyline>();
      Result res = parseLinestring(geoJSON, *line.get());
      if (res.ok()) {
        region.reset(std::move(line), ShapeContainer::Type::S2_POLYLINE);
      }
      return res;
    }
    case Type::MULTI_LINESTRING: {
      std::vector<S2Polyline> polylines;
      Result res = parseMultiLinestring(geoJSON, polylines);
      if (res.ok()) {
        region.reset(new S2MultiPolyline(std::move(polylines)),
                     ShapeContainer::Type::S2_MULTIPOLYLINE);
      }
      return res;
    }
    case Type::POLYGON: {
      auto poly = std::make_unique<S2Polygon>();
      Result res = parsePolygon(geoJSON, *poly.get());
      if (res.ok()) {
        region.reset(std::move(poly), ShapeContainer::Type::S2_POLYGON);
      }
      return res;
    }
    case Type::MULTI_POLYGON:
    case Type::GEOMETRY_COLLECTION:
      return Result(TRI_ERROR_NOT_IMPLEMENTED, "GeoJSON type is not supported");
    case Type::UNKNOWN: {
      return badParam;
    }
  }

  return badParam;
}

/// @brief create s2 latlng
Result parsePoint(VPackSlice const& geoJSON, S2LatLng& latLng) {
  VPackSlice coordinates = geoJSON.get(::FieldCoordinates);

  if (coordinates.isArray() && coordinates.length() == 2) {
    latLng = S2LatLng::FromDegrees(coordinates.at(1).getNumber<double>(),
                                   coordinates.at(0).getNumber<double>())
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
Result parsePolygon(VPackSlice const& geoJSON, S2Polygon& poly) {
  VPackSlice coordinates = geoJSON;
  if (geoJSON.isObject()) {
    coordinates = geoJSON.get(::FieldCoordinates);
    TRI_ASSERT(type(geoJSON) == Type::POLYGON);
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
    res = ::verifyClosedLoop(vertices);  // check last vertices are same
    if (res.fail()) {
      return res;
    }

    ::removeAdjacentDuplicates(vertices);  // s2loop doesn't like duplicates
    vertices.resize(vertices.size() - 1);  // remove redundant last vertex
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

  /*if (!S2Polygon::IsValid(ptrs)) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid polygon");
  }*/
  if (loops.size() == 1) {
    poly.Init(std::move(loops[0]));
  } else if (loops.size() > 1) {
    poly.InitNested(std::move(loops));
  }
  TRI_ASSERT(poly.IsValid());
  return TRI_ERROR_NO_ERROR;
}

/// https://tools.ietf.org/html/rfc7946#section-3.1.4
/// from the rfc {"type":"LineString","coordinates":[[100.0, 0.0],[101.0,1.0]]}
Result parseLinestring(VPackSlice const& geoJson, S2Polyline& linestring) {
  VPackSlice coordinates = geoJson;
  if (geoJson.isObject()) {
    TRI_ASSERT(type(geoJson) == Type::LINESTRING);
    coordinates = geoJson.get(::FieldCoordinates);
  }
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  std::vector<S2Point> vertices;
  Result res = parsePoints(geoJson, /*geoJson*/ true, vertices);
  if (res.ok()) {
    ::removeAdjacentDuplicates(vertices);  // no need for duplicates
    if (vertices.size() < 2) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "Invalid LineString, adjacent "
                    "vertices must not be identical or antipodal.");
    }
    linestring.Init(vertices);
    TRI_ASSERT(linestring.IsValid());
  }
  return res;
};

/// MultiLineString contain an array of LineString coordinate arrays:
///  {"type": "MultiLineString",
///   "coordinates": [[[170.0, 45.0], [180.0, 45.0]],
///                   [[-180.0, 45.0], [-170.0, 45.0]]] }
Result parseMultiLinestring(VPackSlice const& geoJson,
                            std::vector<S2Polyline>& ll) {
  if (!geoJson.isObject()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid MultiLineString");
  }
  TRI_ASSERT(type(geoJson) == Type::MULTI_LINESTRING);
  VPackSlice coordinates = geoJson.get(::FieldCoordinates);
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  for (VPackSlice linestring : VPackArrayIterator(coordinates)) {
    if (!linestring.isArray()) {
      return Result(TRI_ERROR_BAD_PARAMETER, "Invalid MultiLineString");
    }
    ll.emplace_back(S2Polyline());
    // can handle linestring array
    Result res = parseLinestring(linestring, ll.back());
    if (res.fail()) {
      return res;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

// parse geojson coordinates into s2 points
Result parsePoints(VPackSlice const& geoJSON, bool geoJson,
                   std::vector<S2Point>& vertices) {
  vertices.clear();
  VPackSlice coordinates = geoJSON;
  if (geoJSON.isObject()) {
    coordinates = geoJSON.get("coordinates");
  } else if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  for (VPackSlice pt : VPackArrayIterator(coordinates)) {
    if (!pt.isArray() || pt.length() < 2) {
      return Result(TRI_ERROR_BAD_PARAMETER, "Bad coordinate " + pt.toJson());
    }

    VPackSlice lat = pt.at(geoJson ? 1 : 0);
    VPackSlice lon = pt.at(geoJson ? 0 : 1);
    if (!lat.isNumber() || !lon.isNumber()) {
      return Result(TRI_ERROR_BAD_PARAMETER, "Bad coordinate " + pt.toJson());
    }
    vertices.push_back(
        S2LatLng::FromDegrees(lat.getNumber<double>(), lon.getNumber<double>())
            .ToPoint());
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief Parse a polygon for IS_IN_POLYGON
/// @param loop an array of arrays with 2 elements each,
/// representing the points of the polygon in the format [lat, lon]
Result parseLoop(VPackSlice const& coords, S2Loop& loop) {
  if (!coords.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  std::vector<S2Point> vertices;
  Result res = parsePoints(coords, /*geoJson*/ false, vertices);
  if (res.fail()) {
    return res;
  }
  res = ::verifyClosedLoop(vertices);  // check last vertices are same
  if (res.fail()) {
    return res;
  }

  ::removeAdjacentDuplicates(vertices);  // s2loop doesn't like duplicates
  vertices.resize(vertices.size() - 1);  // remove redundant last vertex
  loop.Init(vertices);
  if (!loop.IsValid()) {  // will check first and last for us
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid GeoJSON loop");
  }
  loop.Normalize();

  return TRI_ERROR_NO_ERROR;
}

}  // namespace geojson
}  // namespace geo
}  // namespace arangodb
