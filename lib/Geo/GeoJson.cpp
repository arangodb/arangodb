////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "GeoJson.h"

#include <string>
#include <string_view>
#include <vector>

#include <velocypack/Iterator.h>

#include <s2/s2loop.h>
#include <s2/s2point_region.h>
#include <s2/s2polygon.h>
#include <s2/s2polyline.h>

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Geo/GeoParams.h"
#include "Geo/S2/S2MultiPointRegion.h"
#include "Geo/S2/S2MultiPolyline.h"
#include "Geo/ShapeContainer.h"
#include "Logger/Logger.h"

namespace {
// Have one of these values:
std::string const kTypeStringPoint = "Point";
std::string const kTypeStringLineString = "LineString";
std::string const kTypeStringPolygon = "Polygon";
std::string const kTypeStringMultiPoint = "MultiPoint";
std::string const kTypeStringMultiLineString = "MultiLineString";
std::string const kTypeStringMultiPolygon = "MultiPolygon";
std::string const kTypeStringGeometryCollection = "GeometryCollection";
}  // namespace

namespace {
inline bool sameCharIgnoringCase(char a, char b) noexcept {
  return arangodb::basics::StringUtils::toupper(a) ==
         arangodb::basics::StringUtils::toupper(b);
}
}  // namespace

namespace {
bool sameIgnoringCase(std::string_view s1, std::string const& s2) {
  return s1.size() == s2.size() &&
         std::equal(s1.begin(), s1.end(), s2.begin(), ::sameCharIgnoringCase);
}

arangodb::Result verifyClosedLoop(std::vector<S2Point> const& vertices) {
  using arangodb::Result;

  if (vertices.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Loop with no vertices");
  } else if (vertices.front() != vertices.back()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Loop not closed");
  }
  return TRI_ERROR_NO_ERROR;
}

void removeAdjacentDuplicates(std::vector<S2Point>& vertices) {
  if (vertices.size() < 2) {
    return;
  }
  TRI_ASSERT(!vertices.empty());
  for (size_t i = 0; i < vertices.size() - 1; i++) {
    if (vertices[i] == vertices[i + 1]) {
      vertices.erase(vertices.begin() + i + 1);
      i--;
    }
  }
  TRI_ASSERT(!vertices.empty());
}

// parse geojson coordinates into s2 points
arangodb::Result parsePoints(VPackSlice const& vpack, bool geoJson,
                             std::vector<S2Point>& vertices) {
  vertices.clear();
  VPackSlice coordinates = vpack;
  if (vpack.isObject()) {
    coordinates = vpack.get(arangodb::geo::geojson::Fields::kCoordinates);
  }

  if (!coordinates.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER, "Coordinates missing"};
  }

  VPackArrayIterator it(coordinates);
  vertices.reserve(it.size());

  for (VPackSlice pt : it) {
    if (!pt.isArray() || pt.length() != 2) {
      return {TRI_ERROR_BAD_PARAMETER, "Bad coordinate " + pt.toJson()};
    }

    VPackSlice lat = pt.at(geoJson ? 1 : 0);
    VPackSlice lon = pt.at(geoJson ? 0 : 1);
    if (!lat.isNumber() || !lon.isNumber()) {
      return {TRI_ERROR_BAD_PARAMETER, "Bad coordinate " + pt.toJson()};
    }
    vertices.emplace_back(
        S2LatLng::FromDegrees(lat.getNumber<double>(), lon.getNumber<double>())
            .ToPoint());
  }
  return {TRI_ERROR_NO_ERROR};
}
}  // namespace

namespace arangodb {
namespace geo {
namespace geojson {

/// @brief parse GeoJSON Type
Type type(VPackSlice const& vpack) {
  if (!vpack.isObject()) {
    return Type::UNKNOWN;
  }

  VPackSlice type = vpack.get(Fields::kType);
  if (!type.isString()) {
    return Type::UNKNOWN;
  }

  std::string_view typeStr = type.stringView();
  if (::sameIgnoringCase(typeStr, ::kTypeStringPoint)) {
    return Type::POINT;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringLineString)) {
    return Type::LINESTRING;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringPolygon)) {
    return Type::POLYGON;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringMultiPoint)) {
    return Type::MULTI_POINT;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringMultiLineString)) {
    return Type::MULTI_LINESTRING;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringMultiPolygon)) {
    return Type::MULTI_POLYGON;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringGeometryCollection)) {
    return Type::GEOMETRY_COLLECTION;
  }
  return Type::UNKNOWN;
};

Result parseRegion(VPackSlice const& vpack, ShapeContainer& region,
                   bool legacy) {
  if (vpack.isObject()) {
    Type t = type(vpack);
    switch (t) {
      case Type::POINT: {
        S2LatLng ll;
        Result res = parsePoint(vpack, ll);
        if (res.ok()) {
          region.resetCoordinates(ll);
        }
        return res;
      }
      case Type::MULTI_POINT: {
        return parseMultiPoint(vpack, region);
      }
      case Type::LINESTRING: {
        auto line = std::make_unique<S2Polyline>();
        Result res = parseLinestring(vpack, *line.get());
        if (res.ok()) {
          region.reset(std::move(line), ShapeContainer::Type::S2_POLYLINE);
        }
        return res;
      }
      case Type::MULTI_LINESTRING: {
        std::vector<S2Polyline> polylines;
        Result res = parseMultiLinestring(vpack, polylines);
        if (res.ok()) {
          region.reset(new S2MultiPolyline(std::move(polylines)),
                       ShapeContainer::Type::S2_MULTIPOLYLINE);
        }
        return res;
      }
      case Type::POLYGON: {
        return parsePolygon(vpack, region, legacy);
      }
      case Type::MULTI_POLYGON: {
        return parseMultiPolygon(vpack, region, legacy);
      }
      case Type::GEOMETRY_COLLECTION: {
        return Result(TRI_ERROR_NOT_IMPLEMENTED,
                      "GeoJSON type is not supported");
      }
      case Type::UNKNOWN: {
        // will return TRI_ERROR_BAD_PARAMETER
        break;
      }
    }
  }

  return Result(TRI_ERROR_BAD_PARAMETER, "Invalid GeoJSON Geometry Object.");
}

/// @brief create s2 latlng
Result parsePoint(VPackSlice const& vpack, S2LatLng& latLng) {
  if (Type::POINT != type(vpack)) {
    return {TRI_ERROR_BAD_PARAMETER, "require type: 'Point'"};
  }
  TRI_ASSERT(Type::POINT == type(vpack));

  VPackSlice coordinates = vpack.get(Fields::kCoordinates);
  if (coordinates.isArray() && coordinates.length() == 2 &&
      coordinates.at(0).isNumber() && coordinates.at(1).isNumber()) {
    latLng = S2LatLng::FromDegrees(coordinates.at(1).getNumber<double>(),
                                   coordinates.at(0).getNumber<double>())
                 .Normalized();
    return TRI_ERROR_NO_ERROR;
  }
  return TRI_ERROR_BAD_PARAMETER;
}

/// https://tools.ietf.org/html/rfc7946#section-3.1.3
Result parseMultiPoint(VPackSlice const& vpack, ShapeContainer& region) {
  if (!vpack.isObject()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid GeoJSON Geometry Object.");
  }

  if (Type::MULTI_POINT != type(vpack)) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  std::vector<S2Point> vertices;
  Result res = ::parsePoints(vpack, true, vertices);
  if (res.ok()) {
    if (vertices.empty()) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "Invalid MultiPoint, must contain at least one point.");
    }
    region.reset(new S2MultiPointRegion(&vertices),
                 ShapeContainer::Type::S2_MULTIPOINT);
  }
  return res;
}

/// https://tools.ietf.org/html/rfc7946#section-3.1.6
/// First Loop represent the outer bound, subsequent loops must be holes
/// { "type": "Polygon",
///   "coordinates": [
///    [ [100.0, 0.0], [101.0, 0.0], [101.0, 1.0], [100.0, 1.0], [100.0, 0.0] ],
///    [ [100.2, 0.2], [100.8, 0.2], [100.8, 0.8], [100.2, 0.8], [100.2, 0.2] ]
///   ]
/// }
Result parsePolygon(VPackSlice const& vpack, ShapeContainer& region,
                    bool legacy) {
  if (Type::POLYGON != type(vpack)) {
    return {TRI_ERROR_BAD_PARAMETER, "requires type: 'Polygon'"};
  }
  TRI_ASSERT(Type::POLYGON == type(vpack));

  VPackSlice coordinates = vpack;
  if (vpack.isObject()) {
    coordinates = vpack.get(Fields::kCoordinates);
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
  // - A linear ring is the boundary of a surface or the boundary of a
  //   hole in a surface.
  // - A linear ring MUST follow the right-hand rule with respect to the
  //   area it bounds, i.e., exterior rings are counterclockwise (CCW), and
  //   holes are clockwise (CW).
  VPackArrayIterator it(coordinates);
  size_t const n = it.size();

  std::vector<std::unique_ptr<S2Loop>> loops;
  loops.reserve(n);

  for (VPackSlice loopVertices : it) {
    std::vector<S2Point> vtx;
    Result res = ::parsePoints(loopVertices, /*geoJson*/ true, vtx);
    if (res.fail()) {
      return res;
    }

    // A linear ring is a closed LineString with four or more positions.
    if (vtx.size() < 4) {  // last vertex must be same as first
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "Invalid loop in polygon, must have at least 4 vertices");
    }

    res = ::verifyClosedLoop(vtx);  // check last vertices are same
    if (res.fail()) {
      return res;
    }
    ::removeAdjacentDuplicates(vtx);  // s2loop doesn't like duplicates
    if (vtx.size() >= 2 && vtx.front() == vtx.back()) {
      vtx.resize(vtx.size() - 1);  // remove redundant last vertex
    } else if (vtx.empty()) {
      return Result(TRI_ERROR_BAD_PARAMETER, "Loop with no vertices");
    }

    if (legacy && n == 1) {   // cheap rectangle detection
      if (vtx.size() == 1) {  // empty rectangle
        S2LatLng v0(vtx[0]);
        region.reset(std::make_unique<S2LatLngRect>(v0, v0),
                     ShapeContainer::Type::S2_LATLNGRECT);
        return TRI_ERROR_NO_ERROR;
      } else if (vtx.size() == 4) {
        S2LatLng v0(vtx[0]), v1(vtx[1]), v2(vtx[2]), v3(vtx[3]);
        S1Angle eps = S1Angle::Radians(1e-6);
        if ((v0.lat() - v1.lat()).abs() < eps &&
            (v1.lng() - v2.lng()).abs() < eps &&
            (v2.lat() - v3.lat()).abs() < eps &&
            (v3.lng() - v0.lng()).abs() < eps) {
          R1Interval r1 =
              R1Interval::FromPointPair(v0.lat().radians(), v2.lat().radians());
          S1Interval s1 =
              S1Interval::FromPointPair(v0.lng().radians(), v2.lng().radians());
          region.reset(
              std::make_unique<S2LatLngRect>(r1.Expanded(geo::kRadEps),
                                             s1.Expanded(geo::kRadEps)),
              ShapeContainer::Type::S2_LATLNGRECT);
          return TRI_ERROR_NO_ERROR;
        }
      }
    }
    loops.push_back(std::make_unique<S2Loop>(std::move(vtx), S2Debug::DISABLE));

    S2Error error;
    if (loops.back()->FindValidationError(&error)) {
      return Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("Invalid loop in polygon: ").append(error.text()));
    }

    // Note that we are using InitNested for S2Polygon below.
    // Therefore, we are supposed to deliver all our loops in CCW
    // convention (aka right hand rule, interiour is to the left of
    // the polyline).
    // Since we want to allow for loops, whose interiour covers
    // more than half of the earth, we must not blindly "Normalize"
    // the loops, as we did in earlier versions, although RFC7946
    // says "parsers SHOULD NOT reject Polygons that do not follow
    // the right-hand rule". Since we cannot detect if the outer loop
    // respects the rule, we cannot reject it. However, for the other
    // loops, we can be a bit more tolerant: If a subsequent loop is
    // not contained in the first one (following the right hand rule),
    // then we can invert it silently and if it is then contained
    // in the first, we have proper nesting and leave the rest to
    // InitNested. This is, why we proceed like this:
    S2Loop* loop = loops.back().get();
    if (legacy) {
      loop->Normalize();
    }

    // subsequent loops must be holes within first loop:
    if (loops.size() > 1 && !loops.front()->Contains(loop)) {
      bool bad = true;
      if (!legacy) {
        loop->Invert();
        if (loops.front()->Contains(loop)) {
          bad = false;
        }
      }
      if (bad) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "Subsequent loop not a hole in polygon");
      }
    }
  }

  std::unique_ptr<S2Polygon> poly;
  if (loops.size() == 1) {
    poly = std::make_unique<S2Polygon>(std::move(loops[0]));
  } else if (loops.size() > 1) {
    poly = std::make_unique<S2Polygon>();
    poly->InitNested(std::move(loops));
  } else {
    return Result(TRI_ERROR_BAD_PARAMETER, "Empty polygons are not allowed");
  }
  S2Error err;
  poly->FindValidationError(&err);
  if (!err.ok()) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  "S2 polygon verification error: " + err.text());
  }
  region.reset(std::move(poly), ShapeContainer::Type::S2_POLYGON);
  return TRI_ERROR_NO_ERROR;
}

/// @brief parse GeoJson polygon or array of loops. Each loop consists of
/// an array of coordinates: Example [[[lon, lat], [lon, lat], ...],...].
/// The multipolygon contains an array of looops
Result parseMultiPolygon(velocypack::Slice const& vpack, ShapeContainer& region,
                         bool legacy) {
  if (Type::MULTI_POLYGON != type(vpack)) {
    return {TRI_ERROR_BAD_PARAMETER, "requires type: 'MultiPolygon'"};
  }
  TRI_ASSERT(Type::MULTI_POLYGON == type(vpack));

  VPackSlice coordinates = vpack;
  if (vpack.isObject()) {
    coordinates = vpack.get(Fields::kCoordinates);
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
  // - A linear ring is the boundary of a surface or the boundary of a
  //   hole in a surface.
  // - A linear ring MUST follow the right-hand rule with respect to the
  //   area it bounds, i.e., exterior rings are counterclockwise (CCW), and
  //   holes are clockwise (CW).
  std::vector<std::unique_ptr<S2Loop>> loops;
  for (VPackSlice polygons : VPackArrayIterator(coordinates)) {
    if (!polygons.isArray()) {
      return Result(
          TRI_ERROR_BAD_PARAMETER,
          "Multi-Polygon must consist of an array of Polygons coordinates");
    }

    // the loop of the
    size_t outerLoop = loops.size();
    for (VPackSlice loopVertices : VPackArrayIterator(polygons)) {
      std::vector<S2Point> vtx;
      Result res = ::parsePoints(loopVertices, /*geoJson*/ true, vtx);
      if (res.fail()) {
        return res;
      }
      // A linear ring is a closed LineString with four or more positions.
      if (vtx.size() < 4) {  // last vertex must be same as first
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "Invalid loop in polygon, must have at least 4 vertices");
      }

      res = ::verifyClosedLoop(vtx);  // check last vertices are same
      if (res.fail()) {
        return res;
      }
      ::removeAdjacentDuplicates(vtx);  // s2loop doesn't like duplicates
      if (vtx.size() >= 2 && vtx.front() == vtx.back()) {
        vtx.resize(vtx.size() - 1);  // remove redundant last vertex
      } else if (vtx.empty()) {
        return Result(TRI_ERROR_BAD_PARAMETER, "Loop with no vertices");
      }

      loops.push_back(std::make_unique<S2Loop>(vtx, S2Debug::DISABLE));
      S2Error error;
      if (loops.back()->FindValidationError(&error)) {
        return Result(
            TRI_ERROR_BAD_PARAMETER,
            std::string("Invalid loop in polygon: ").append(error.text()));
      }
      // Note that we are using InitNested for S2Polygon below.
      // Therefore, we are supposed to deliver all our loops in CCW
      // convention (aka right hand rule, interiour is to the left of
      // the polyline).
      // Since we want to allow for loops, whose interiour covers
      // more than half of the earth, we must not blindly "Normalize"
      // the loops, as we did in earlier versions, although RFC7946
      // says "parsers SHOULD NOT reject Polygons that do not follow
      // the right-hand rule". Since we cannot detect if the outer loop
      // respects the rule, we cannot reject it. However, for the other
      // loops, we can be a bit more tolerant: If a subsequent loop is
      // not contained in the first one (following the right hand rule),
      // then we can invert it silently and if it is then contained
      // in the first, we have proper nesting and leave the rest to
      // InitNested. This is, why we proceed like this:
      S2Loop* loop = loops.back().get();
      if (legacy) {
        loop->Normalize();
      }

      // Any subsequent loop must be a hole within first loop
      if (outerLoop + 1 < loops.size() &&
          !loops[outerLoop]->Contains(loops.back().get())) {
        bool bad = true;
        if (!legacy) {
          loop->Invert();
          if (loops[outerLoop]->Contains(loop)) {
            bad = false;
          }
        }
        if (bad) {
          return Result(TRI_ERROR_BAD_PARAMETER,
                        "Subsequent loop not a hole in polygon");
        }
      }
    }
  }

  std::unique_ptr<S2Polygon> poly;
  if (loops.size() == 1) {
    poly = std::make_unique<S2Polygon>(std::move(loops[0]));
  } else if (loops.size() > 1) {
    poly = std::make_unique<S2Polygon>();
    poly->InitNested(std::move(loops));
  } else {
    return Result(TRI_ERROR_BAD_PARAMETER, "Empty polygons are not allowed");
  }
  S2Error err;
  poly->FindValidationError(&err);
  if (!err.ok()) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  "S2 multipolygon verification error: " + err.text());
  }
  region.reset(std::move(poly), ShapeContainer::Type::S2_POLYGON);
  return TRI_ERROR_NO_ERROR;
}

/// https://tools.ietf.org/html/rfc7946#section-3.1.4
/// from the rfc {"type":"LineString","coordinates":[[100.0, 0.0],[101.0,1.0]]}
Result parseLinestring(VPackSlice const& vpack, S2Polyline& linestring) {
  if (Type::LINESTRING != type(vpack)) {
    return {TRI_ERROR_BAD_PARAMETER, "require type: 'Linestring'"};
  }
  TRI_ASSERT(Type::LINESTRING == type(vpack));

  VPackSlice coordinates = vpack;
  if (vpack.isObject()) {
    coordinates = vpack.get(Fields::kCoordinates);
  }
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  std::vector<S2Point> vertices;
  Result res = ::parsePoints(vpack, /*geoJson*/ true, vertices);
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
Result parseMultiLinestring(VPackSlice const& vpack,
                            std::vector<S2Polyline>& ll) {
  if (Type::MULTI_LINESTRING != type(vpack)) {
    return {TRI_ERROR_BAD_PARAMETER, "require type: 'MultiLinestring'"};
  }
  TRI_ASSERT(Type::MULTI_LINESTRING == type(vpack));

  if (!vpack.isObject()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid MultiLinestring");
  }
  VPackSlice coordinates = vpack.get(Fields::kCoordinates);
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  for (VPackSlice linestring : VPackArrayIterator(coordinates)) {
    if (!linestring.isArray()) {
      return Result(TRI_ERROR_BAD_PARAMETER, "Invalid MultiLinestring");
    }
    ll.emplace_back(S2Polyline());
    // can handle linestring array
    std::vector<S2Point> vertices;
    Result res = ::parsePoints(linestring, /*geoJson*/ true, vertices);
    if (res.ok()) {
      ::removeAdjacentDuplicates(vertices);  // no need for duplicates
      if (vertices.size() < 2) {
        return Result(TRI_ERROR_BAD_PARAMETER,
                      "Invalid embedded LineString, adjacent "
                      "vertices must not be identical or antipodal.");
      }
      ll.back().Init(vertices);
      TRI_ASSERT(ll.back().IsValid());
    } else {
      return Result(TRI_ERROR_BAD_PARAMETER, "Invalid MultiLinestring");
    }
  }
  if (ll.empty()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "Invalid MultiLinestring, must contain at least one Linestring."};
  }
  return TRI_ERROR_NO_ERROR;
}

Result parseLoop(velocypack::Slice const& coords, bool geoJson, S2Loop& loop) {
  if (!coords.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  std::vector<S2Point> vertices;
  Result res = ::parsePoints(coords, geoJson, vertices);
  if (res.fail()) {
    return res;
  }

  /* TODO enable this check after removing deprecated IS_IN_POLYGON fn
      res = ::verifyClosedLoop(vertices);  // check last vertices are same
      if (res.fail()) {
        return res;
      }*/

  ::removeAdjacentDuplicates(vertices);  // s2loop doesn't like duplicates
  if (vertices.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "No adjacent duplicates allowed");
  }
  if (vertices.front() == vertices.back()) {
    vertices.resize(vertices.size() - 1);  // remove redundant last vertex
  }
  loop.Init(vertices);
  S2Error error;
  if (loop.FindValidationError(&error)) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  std::string("Invalid loop: ").append(error.text()));
  }
  loop.Normalize();

  return TRI_ERROR_NO_ERROR;
}

}  // namespace geojson
}  // namespace geo
}  // namespace arangodb
