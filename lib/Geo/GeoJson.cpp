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

#include "Geo/GeoJson.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <span>

#include <velocypack/Iterator.h>

#include <s2/s2loop.h>
#include <s2/s2point_region.h>
#include <s2/s2polygon.h>
#include <s2/s2polyline.h>

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Geo/GeoParams.h"
#include "Geo/ShapeContainer.h"
#include "Geo/S2/S2MultiPointRegion.h"
#include "Geo/S2/S2MultiPolyline.h"
#include "Logger/Logger.h"

namespace arangodb::geo::json {
namespace {

// The main idea is check in our CI 3 compile time branches:
// 1. Validation true
// 2. Validation false kIsMaintainer true
// 3. Validation false kIsMaintainer false
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE) && defined(__linux__) && \
    !defined(__aarch64__)
constexpr bool kIsMaintainer = true;
#else
constexpr bool kIsMaintainer = false;
#endif

constexpr std::string_view kTypeStringPoint = "point";
constexpr std::string_view kTypeStringPolygon = "polygon";
constexpr std::string_view kTypeStringLineString = "linestring";
constexpr std::string_view kTypeStringMultiPoint = "multipoint";
constexpr std::string_view kTypeStringMultiPolygon = "multipolygon";
constexpr std::string_view kTypeStringMultiLineString = "multilinestring";
constexpr std::string_view kTypeStringGeometryCollection = "geometrycollection";

// TODO(MBkkt) t as parameter looks better, but compiler produce strange error
//  t isn't compile time for line toString(t) where t from template
template<Type t>
consteval std::string_view toString() noexcept {
  switch (t) {
    case Type::UNKNOWN:
      return {};
    case Type::POINT:
      return kTypeStringPoint;
    case Type::LINESTRING:
      return kTypeStringLineString;
    case Type::POLYGON:
      return kTypeStringPolygon;
    case Type::MULTI_POINT:
      return kTypeStringMultiPoint;
    case Type::MULTI_LINESTRING:
      return kTypeStringMultiLineString;
    case Type::MULTI_POLYGON:
      return kTypeStringMultiPolygon;
    case Type::GEOMETRY_COLLECTION:
      return kTypeStringGeometryCollection;
  }
}

consteval ShapeContainer::Type toShapeType(Type t) noexcept {
  switch (t) {
    case Type::UNKNOWN:
    case Type::GEOMETRY_COLLECTION:
      return ShapeContainer::Type::EMPTY;
    case Type::POINT:
      return ShapeContainer::Type::S2_POINT;
    case Type::LINESTRING:
      return ShapeContainer::Type::S2_POLYLINE;
    case Type::POLYGON:
    case Type::MULTI_POLYGON:
      return ShapeContainer::Type::S2_POLYGON;
    case Type::MULTI_POINT:
      return ShapeContainer::Type::S2_MULTIPOINT;
    case Type::MULTI_LINESTRING:
      return ShapeContainer::Type::S2_MULTIPOLYLINE;
  }
  return ShapeContainer::Type::EMPTY;
}

bool sameIgnoringCase(std::string_view s1, std::string_view s2) noexcept {
  return std::equal(s1.begin(), s1.end(), s2.begin(),
                    [](char a, char b) { return a == std::tolower(b); });
}

template<typename Vertices>
void removeAdjacentDuplicates(Vertices& vertices) noexcept {
  // TODO We don't remove antipodal vertices
  auto it = std::unique(vertices.begin(), vertices.end());
  vertices.erase(it, vertices.end());
}

bool getCoordinates(velocypack::Slice& vpack) {
  TRI_ASSERT(vpack.isObject());
  vpack = vpack.get(fields::kCoordinates);
  return vpack.isArray();
}

template<bool Validation, bool GeoJson>
bool parseImpl(velocypack::Slice vpack, S2LatLng& vertex) {
  TRI_ASSERT(vpack.isArray());
  velocypack::ArrayIterator jt{vpack};
  if (Validation && ADB_UNLIKELY(jt.size() != 2)) {
    return false;
  }
  auto const first = *jt;
  if (Validation && ADB_UNLIKELY(!first.isNumber<double>())) {
    return false;
  }
  jt.next();
  auto const second = *jt;
  if (Validation && ADB_UNLIKELY(!second.isNumber<double>())) {
    return false;
  }
  double lat, lon;
  if constexpr (GeoJson) {
    lon = first.getNumber<double>();
    lat = second.getNumber<double>();
  } else {
    lat = first.getNumber<double>();
    lon = second.getNumber<double>();
  }
  // We should Normalize all S2LatLng
  // because otherwise their converting to S2Point is invalid!
  vertex = S2LatLng::FromDegrees(lat, lon).Normalized();
  return true;
}

template<bool Validation, bool GeoJson>
Result parseImpl(velocypack::Slice vpack, std::vector<S2Point>& vertices) {
  TRI_ASSERT(vpack.isArray());
  velocypack::ArrayIterator it{vpack};
  vertices.clear();
  vertices.reserve(it.size());

  S2LatLng vertex;
  for (; it.valid(); it.next()) {
    auto slice = *it;
    if (Validation && ADB_UNLIKELY(!slice.isArray())) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Bad coordinates, should be array " + slice.toJson()};
    }
    auto ok = parseImpl<Validation, GeoJson>(slice, vertex);
    if (Validation && ADB_UNLIKELY(!ok)) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Bad coordinates values " + slice.toJson()};
    }
    vertices.emplace_back(vertex.ToPoint());
  }
  return {};
}

template<Type t>
Result validate(velocypack::Slice& vpack) {
  if (ADB_UNLIKELY(type(vpack) != t)) {
    // TODO(MBkkt) Unnecessary memcpy,
    //  this string can be constructed at compile time with compile time new
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Require type: '", toString<t>(), "'.")};
  }
  if (ADB_UNLIKELY(!getCoordinates(vpack))) {
    return {TRI_ERROR_BAD_PARAMETER, "Coordinates missing."};
  }
  return {};
}

template<bool Validation, bool GeoJson>
Result parsePointImpl(velocypack::Slice vpack, S2LatLng& region) {
  auto ok = parseImpl<Validation, GeoJson>(vpack, region);
  if (Validation && ADB_UNLIKELY(!ok)) {
    return {TRI_ERROR_BAD_PARAMETER, "Bad coordinates " + vpack.toJson()};
  }
  return {};
}

template<bool Validation>
Result parsePointsImpl(velocypack::Slice vpack,
                       std::vector<S2Point>& vertices) {
  auto r = parseImpl<Validation, true>(vpack, vertices);
  if (Validation && ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  if (Validation && ADB_UNLIKELY(vertices.empty())) {
    return {TRI_ERROR_BAD_PARAMETER,
            "Invalid MultiPoint, it must contains at least one point."};
  }
  return {};
}

template<bool Validation>
Result parseLineImpl(velocypack::Slice vpack, std::vector<S2Point>& vertices) {
  auto r = parseImpl<Validation, true>(vpack, vertices);
  if (Validation && ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  removeAdjacentDuplicates(vertices);
  if (Validation && ADB_UNLIKELY(vertices.size() < 2)) {
    return {TRI_ERROR_BAD_PARAMETER,
            "Invalid LineString,"
            " adjacent vertices must not be identical or antipodal."};
  }
  return {};
}

template<bool Validation>
Result parseLinesImpl(velocypack::Slice vpack, std::vector<S2Polyline>& lines,
                      std::vector<S2Point>& vertices) {
  TRI_ASSERT(vpack.isArray());
  velocypack::ArrayIterator it{vpack};
  lines.clear();
  lines.reserve(it.size());
  for (; it.valid(); it.next()) {
    auto slice = *it;
    if (Validation && ADB_UNLIKELY(!slice.isArray())) {
      return {TRI_ERROR_BAD_PARAMETER, "Missing coordinates."};
    }
    auto r = parseLineImpl<Validation>(slice, vertices);
    if (Validation && ADB_UNLIKELY(!r.ok())) {
      return r;
    }
    // TODO(MBkkt) single unnecessary copy here, needs to patch S2!
    lines.emplace_back(vertices, S2Debug::DISABLE);
    // TODO should be FindValidationError instead of assert?
    //  We don't remove antipodal vertices
    TRI_ASSERT(lines.back().IsValid());
  }
  if (Validation && ADB_UNLIKELY(lines.empty())) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        "Invalid MultiLinestring, it must contains at least one Linestring."};
  }
  return {};
}

template<bool Validation>
Result makeLoopValid(std::vector<S2Point>& vertices) noexcept(!Validation) {
  if (Validation && ADB_UNLIKELY(vertices.size() < 4)) {
    return {TRI_ERROR_BAD_PARAMETER,
            "Invalid GeoJson Loop, must have at least 4 vertices"};
  }
  // S2Loop doesn't like duplicates
  removeAdjacentDuplicates(vertices);
  if (Validation && ADB_UNLIKELY(vertices.front() != vertices.back())) {
    return {TRI_ERROR_BAD_PARAMETER, "Loop not closed"};
  }
  // S2Loop automatically add last edge
  if (ADB_LIKELY(vertices.size() > 1)) {
    TRI_ASSERT(vertices.size() >= 3);
    // 3 is incorrect but it will be handled by S2Loop::FindValidationError
    vertices.pop_back();
  }
  return {};
}

template<bool Validation>
Result parseRectImpl(velocypack::Slice vpack, ShapeContainer& region,
                     std::vector<S2Point>& vertices) {
  if (Validation && ADB_UNLIKELY(!vpack.isArray())) {
    return {TRI_ERROR_BAD_PARAMETER, "Missing coordinates."};
  }
  auto r = parsePointsImpl<Validation>(vpack, vertices);
  if (Validation && ADB_UNLIKELY(!r.ok())) {
    return r;
  }

  r = makeLoopValid<Validation>(vertices);
  if (Validation && ADB_UNLIKELY(!r.ok())) {
    return r;
  }

  if (vertices.size() == 4) {
    S2LatLng const v0{vertices[0]}, v1{vertices[1]}, v2{vertices[2]},
        v3{vertices[3]};
    auto const eps = S1Angle::Radians(1e-6);
    if ((v0.lat() - v1.lat()).abs() < eps &&
        (v1.lng() - v2.lng()).abs() < eps &&
        (v2.lat() - v3.lat()).abs() < eps &&
        (v3.lng() - v0.lng()).abs() < eps) {
      auto const r1 =
          R1Interval::FromPointPair(v0.lat().radians(), v2.lat().radians());
      auto const s1 =
          S1Interval::FromPointPair(v0.lng().radians(), v2.lng().radians());
      region.reset(std::make_unique<S2LatLngRect>(r1.Expanded(geo::kRadEps),
                                                  s1.Expanded(geo::kRadEps)),
                   ShapeContainer::Type::S2_LATLNGRECT);
    }
  } else if (vertices.size() == 1) {
    S2LatLng v0{vertices[0]};
    region.reset(std::make_unique<S2LatLngRect>(v0, v0),
                 ShapeContainer::Type::S2_LATLNGRECT);
  }
  return {};
}

template<bool Validation, bool Legacy>
Result parseLoopImpl(velocypack::Slice vpack,
                     std::vector<std::unique_ptr<S2Loop>>& loops,
                     std::vector<S2Point>& vertices, S2Loop*& first) {
  if (Validation && ADB_UNLIKELY(!vpack.isArray())) {
    return {TRI_ERROR_BAD_PARAMETER, "Missing coordinates."};
  }
  // Coordinates of a Polygon are an array of LinearRing coordinate arrays.
  // The first element in the array represents the exterior ring.
  // Any subsequent elements represent interior rings (or holes).
  // - A linear ring is a closed LineString with four or more positions.
  // - The first and last positions are equivalent, and they MUST contain
  //   identical values; their representation SHOULD also be identical.
  // - A linear ring is the boundary of a surface or the boundary of a
  //   hole in a surface.
  // - A linear ring MUST follow the right-hand rule with respect to the
  //   area it bounds, i.e., exterior rings are counterclockwise (CCW), and
  //   holes are clockwise (CW).
  auto r = parsePointsImpl<Validation>(vpack, vertices);
  if (Validation && ADB_UNLIKELY(!r.ok())) {
    return r;
  }

  r = makeLoopValid<Validation>(vertices);
  if (Validation && ADB_UNLIKELY(!r.ok())) {
    return r;
  }

  loops.push_back(std::make_unique<S2Loop>(vertices, S2Debug::DISABLE));
  auto& last = *loops.back();
  if constexpr (Validation) {
    S2Error error;
    if (ADB_UNLIKELY(last.FindValidationError(&error))) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("Invalid Loop in Polygon: ", error.text())};
    }
  }
  // Note that we are using InitNested for S2Polygon below.
  // Therefore, we are supposed to deliver all our loops in CCW
  // convention (aka right hand rule, interior is to the left of
  // the polyline).
  // Since we want to allow for loops, whose interior covers
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
  if constexpr (Legacy) {
    last.Normalize();
  }
  if (first == nullptr) {
    first = &last;
    return {};
  } else if constexpr (!Legacy) {
    if (ADB_LIKELY(first->Contains(last))) {
      return {};
    }
    last.Invert();
  }
  if (Validation && ADB_UNLIKELY(!first->Contains(last))) {
    return {TRI_ERROR_BAD_PARAMETER, "Subsequent loop not a hole in polygon."};
  }
  return {};
}

S2Polygon createPolygon(std::vector<std::unique_ptr<S2Loop>>&& loops) {
  if (loops.size() != 1 || !loops[0]->is_empty()) {
    return S2Polygon{std::move(loops), S2Debug::DISABLE};
  }
  // Handle creation of empty polygon otherwise will be error in validation
  S2Polygon polygon;
  polygon.set_s2debug_override(S2Debug::DISABLE);
  return polygon;
}

template<bool Validation, bool Legacy>
Result parsePolygonImpl(velocypack::ArrayIterator it, S2Polygon& region,
                        std::vector<S2Point>& vertices) {
  auto const n = it.size();
  TRI_ASSERT(n >= 1);
  std::vector<std::unique_ptr<S2Loop>> loops;
  loops.reserve(n);
  for (S2Loop* first = nullptr; it.valid(); it.next()) {
    auto r = parseLoopImpl<Validation, Legacy>(*it, loops, vertices, first);
    if (Validation && ADB_UNLIKELY(!r.ok())) {
      return r;
    }
  }
  region = createPolygon(std::move(loops));
  if constexpr (Validation) {
    S2Error error;
    if (ADB_UNLIKELY(region.FindValidationError(&error))) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("Invalid Polygon: ", error.text())};
    }
  }
  return {};
}

template<bool Validation, bool Legacy>
Result parsePolygonImpl(velocypack::Slice vpack, ShapeContainer& region,
                        std::vector<S2Point>& vertices) {
  TRI_ASSERT(vpack.isArray());
  velocypack::ArrayIterator it{vpack};
  auto const n = it.size();
  if (Validation && ADB_UNLIKELY(n == 0)) {
    return {TRI_ERROR_BAD_PARAMETER, "Invalid GeoJSON Geometry Object."};
  }
  std::unique_ptr<S2Polygon> d;
  if (Legacy && n == 1) {
    auto* old = region.region();
    auto r = parseRectImpl<Validation>(*it, region, vertices);
    if (Validation && ADB_UNLIKELY(!r.ok())) {
      return r;
    }
    if (old != region.region()) {
      return {};
    }
    // vertices already parsed, so we use it
    auto loop = std::make_unique<S2Loop>(vertices);
    S2Error error;
    if (Validation && ADB_UNLIKELY(loop->FindValidationError(&error))) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("Invalid Loop in Polygon: ", error.text())};
    }
    loop->Normalize();
    d = std::make_unique<S2Polygon>(std::move(loop), S2Debug::DISABLE);
    if (Validation && ADB_UNLIKELY(d->FindValidationError(&error))) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("Invalid Polygon: ", error.text())};
    }
  } else {
    d = std::make_unique<S2Polygon>();
    auto r = parsePolygonImpl<Validation, Legacy>(it, *d, vertices);
    if (Validation && ADB_UNLIKELY(!r.ok())) {
      return r;
    }
  }
  region.reset(std::move(d), toShapeType(Type::POLYGON));
  return {};
}

template<bool Validation, bool Legacy>
Result parseMultiPolygonImpl(velocypack::Slice vpack, S2Polygon& region,
                             std::vector<S2Point>& vertices) {
  TRI_ASSERT(vpack.isArray());
  velocypack::ArrayIterator it{vpack};
  auto const n = it.size();
  if (Validation && ADB_UNLIKELY(n == 0)) {
    return {TRI_ERROR_BAD_PARAMETER,
            "MultiPolygon should contains at least one Polygon."};
  }
  std::vector<std::unique_ptr<S2Loop>> loops;
  loops.reserve(n);
  for (S2Loop* first = nullptr; it.valid(); it.next()) {
    if (Validation && ADB_UNLIKELY(!(*it).isArray())) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Polygon should contains at least one coordinates array."};
    }
    velocypack::ArrayIterator jt{*it};
    if (Validation && ADB_UNLIKELY(jt.size() == 0)) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Polygon should contains at least one Loop."};
    }
    for (; jt.valid(); jt.next()) {
      auto r = parseLoopImpl<Validation, Legacy>(*jt, loops, vertices, first);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
    }
    first = nullptr;
  }
  region = createPolygon(std::move(loops));
  if constexpr (Validation) {
    S2Error error;
    if (ADB_UNLIKELY(region.FindValidationError(&error))) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("Invalid Loop in MultiPolygon: ", error.text())};
    }
  }
  return {};
}

template<bool Validation>
Result parseRegionImpl(velocypack::Slice vpack, ShapeContainer& region,
                       std::vector<S2Point>& cache, bool legacy) {
  auto const t = type(vpack);
  if constexpr (Validation) {
    if (ADB_UNLIKELY(t == Type::UNKNOWN)) {
      return {TRI_ERROR_BAD_PARAMETER, "Invalid GeoJSON Geometry Object."};
    }
    if (ADB_UNLIKELY(!getCoordinates(vpack))) {
      return {TRI_ERROR_BAD_PARAMETER, "Coordinates missing."};
    }
  } else {
    vpack = vpack.get(fields::kCoordinates);
  }
  switch (t) {
    case Type::POINT: {
      S2LatLng d;
      auto r = parsePointImpl<Validation, true>(vpack, d);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      region.reset(d.ToPoint());
    } break;
    case Type::LINESTRING: {
      auto r = parseLineImpl<Validation>(vpack, cache);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      auto d = std::make_unique<S2Polyline>(cache, S2Debug::DISABLE);
      // TODO should be FindValidationError instead of assert?
      //  We don't remove antipodal vertices
      TRI_ASSERT(d->IsValid());
      region.reset(std::move(d), toShapeType(Type::LINESTRING));
    } break;
    case Type::POLYGON: {
      return ADB_UNLIKELY(legacy)
                 ? parsePolygonImpl<Validation, true>(vpack, region, cache)
                 : parsePolygonImpl<Validation, false>(vpack, region, cache);
    } break;
    case Type::MULTI_POINT: {
      std::vector<S2Point> vertices;
      auto r = parsePointsImpl<Validation>(vpack, vertices);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      auto d = std::make_unique<S2MultiPointRegion>();
      d->Impl() = std::move(vertices);
      region.reset(std::move(d), toShapeType(Type::MULTI_POINT));
    } break;
    case Type::MULTI_LINESTRING: {
      std::vector<S2Polyline> lines;
      auto r = parseLinesImpl<Validation>(vpack, lines, cache);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      auto d = std::make_unique<S2MultiPolyline>();
      d->Impl() = std::move(lines);
      region.reset(std::move(d), toShapeType(Type::MULTI_LINESTRING));
    } break;
    case Type::MULTI_POLYGON: {
      auto d = std::make_unique<S2Polygon>();
      auto r = ADB_UNLIKELY(legacy)
                   ? parseMultiPolygonImpl<Validation, true>(vpack, *d, cache)
                   : parseMultiPolygonImpl<Validation, false>(vpack, *d, cache);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      region.reset(std::move(d), toShapeType(Type::MULTI_POLYGON));
    } break;
    default:
      return {TRI_ERROR_NOT_IMPLEMENTED,
              "GeoJSON type GeometryCollection is not supported"};
  }
  return {};
}

template<bool Validation>
Result parseCoordinatesImpl(velocypack::Slice vpack, ShapeContainer& region,
                            bool geoJson) {
  if (Validation && ADB_UNLIKELY(!vpack.isArray())) {
    return {TRI_ERROR_BAD_PARAMETER, "Invalid coordinate pair."};
  }
  S2LatLng d;
  auto r = geoJson ? parsePointImpl<Validation, true>(vpack, d)
                   : parsePointImpl<Validation, false>(vpack, d);
  if (Validation && ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  region.reset(d.ToPoint());
  return {};
}

}  // namespace

#define CASE(value)                                  \
  case toString<value>().size():                     \
    if (sameIgnoringCase(toString<value>(), type)) { \
      return value;                                  \
    }                                                \
    break

/// @brief parse GeoJSON Type
Type type(velocypack::Slice vpack) noexcept {
  if (ADB_UNLIKELY(!vpack.isObject())) {
    return Type::UNKNOWN;
  }
  auto const field = vpack.get(fields::kType);
  if (ADB_UNLIKELY(!field.isString())) {
    return Type::UNKNOWN;
  }
  auto const type = field.stringView();
  switch (type.size()) {
    CASE(Type::POINT);
    CASE(Type::POLYGON);
    case toString<Type::LINESTRING>().size():
      static_assert(toString<Type::LINESTRING>().size() ==
                    toString<Type::MULTI_POINT>().size());
      if (sameIgnoringCase(toString<Type::LINESTRING>(), type)) {
        return Type::LINESTRING;
      }
      if (sameIgnoringCase(toString<Type::MULTI_POINT>(), type)) {
        return Type::MULTI_POINT;
      }
      break;
      CASE(Type::MULTI_POLYGON);
      CASE(Type::MULTI_LINESTRING);
      CASE(Type::GEOMETRY_COLLECTION);
    default:
      break;
  }
  return Type::UNKNOWN;
}

#undef CASE

Result parsePoint(velocypack::Slice vpack, S2LatLng& region) {
  auto r = validate<Type::POINT>(vpack);
  if (!r.ok()) {
    return r;
  }
  return parsePointImpl<true, true>(vpack, region);
}

Result parseMultiPoint(velocypack::Slice vpack, S2MultiPointRegion& region) {
  auto r = validate<Type::MULTI_POINT>(vpack);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  auto& vertices = region.Impl();
  return parsePointsImpl<true>(vpack, vertices);
}

Result parseLinestring(velocypack::Slice vpack, S2Polyline& region) {
  auto r = validate<Type::LINESTRING>(vpack);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  std::vector<S2Point> vertices;
  r = parseLineImpl<true>(vpack, vertices);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  region = S2Polyline{vertices, S2Debug::DISABLE};
  // TODO should be FindValidationError instead of assert?
  //  We don't remove antipodal vertices
  TRI_ASSERT(region.IsValid());
  return {};
}

Result parseMultiLinestring(velocypack::Slice vpack, S2MultiPolyline& region) {
  auto r = validate<Type::MULTI_LINESTRING>(vpack);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  auto& lines = region.Impl();
  std::vector<S2Point> vertices;
  return parseLinesImpl<true>(vpack, lines, vertices);
}

Result parsePolygon(velocypack::Slice vpack, S2Polygon& region) {
  auto r = validate<Type::POLYGON>(vpack);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  velocypack::ArrayIterator it{vpack};
  if (ADB_UNLIKELY(it.size() == 0)) {
    return {TRI_ERROR_BAD_PARAMETER,
            "Polygon should contains at least one loop."};
  }
  std::vector<S2Point> vertices;
  return parsePolygonImpl<true, false>(it, region, vertices);
}

Result parseMultiPolygon(velocypack::Slice vpack, S2Polygon& region) {
  auto r = validate<Type::MULTI_POLYGON>(vpack);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  std::vector<S2Point> vertices;
  return parseMultiPolygonImpl<true, false>(vpack, region, vertices);
}

Result parseRegion(velocypack::Slice vpack, ShapeContainer& region,
                   bool legacy) {
  std::vector<S2Point> cache;
  return parseRegionImpl<true>(vpack, region, cache, legacy);
}

template<bool Validation>
Result parseRegion(velocypack::Slice vpack, ShapeContainer& region,
                   std::vector<S2Point>& cache, bool legacy) {
  auto r = parseRegionImpl<(Validation || kIsMaintainer)>(vpack, region, cache,
                                                          legacy);
  TRI_ASSERT(Validation || r.ok()) << r.errorMessage();
  return r;
}

template Result parseRegion<true>(velocypack::Slice vpack,
                                  ShapeContainer& region,
                                  std::vector<S2Point>& cache, bool legacy);
template Result parseRegion<false>(velocypack::Slice vpack,
                                   ShapeContainer& region,
                                   std::vector<S2Point>& cache, bool legacy);

template<bool Validation>
Result parseCoordinates(velocypack::Slice vpack, ShapeContainer& region,
                        bool geoJson) {
  auto r = parseCoordinatesImpl<(Validation || kIsMaintainer)>(vpack, region,
                                                               geoJson);
  TRI_ASSERT(Validation || (r.ok() && !region.empty())) << r.errorMessage();
  return r;
}

template Result parseCoordinates<true>(velocypack::Slice vpack,
                                       ShapeContainer& region, bool geoJson);
template Result parseCoordinates<false>(velocypack::Slice vpack,
                                        ShapeContainer& region, bool geoJson);

Result parseLoop(velocypack::Slice vpack, S2Loop& loop, bool geoJson) {
  static constexpr bool Validation = true;
  if (ADB_UNLIKELY(!vpack.isArray())) {
    return {TRI_ERROR_BAD_PARAMETER, "Coordinates missing."};
  }
  std::vector<S2Point> vertices;
  auto r = geoJson ? parseImpl<Validation, true>(vpack, vertices)
                   : parseImpl<Validation, false>(vpack, vertices);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  removeAdjacentDuplicates(vertices);
  switch (vertices.size()) {
    case 0:
      return {TRI_ERROR_BAD_PARAMETER,
              "Loop should be 3 different vertices or be empty or full."};
    case 1:
      break;
    default:
      if (vertices.front() == vertices.back()) {
        vertices.pop_back();
      }
      break;
  }
  // size() 2 here is incorrect but it will be handled by FindValidationError
  loop = S2Loop{vertices, S2Debug::DISABLE};
  S2Error error;
  if (ADB_UNLIKELY(loop.FindValidationError(&error))) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Invalid loop: ", error.text())};
  }
  loop.Normalize();
  return {};
}

}  // namespace arangodb::geo::json
