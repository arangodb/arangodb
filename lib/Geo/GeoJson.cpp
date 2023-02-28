////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include <s2/util/coding/coder.h>

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Geo/GeoParams.h"
#include "Geo/ShapeContainer.h"
#include "Geo/S2/S2MultiPointRegion.h"
#include "Geo/S2/S2MultiPolylineRegion.h"
#include "Logger/Logger.h"

namespace arangodb::geo::json {
namespace {

template<bool Validation>
S2Point encodePointImpl(S2LatLng latLng, coding::Options options,
                        Encoder* encoder) noexcept {
  if constexpr (Validation) {
    if (encoder != nullptr) {
      TRI_ASSERT(options != coding::Options::kInvalid);
      TRI_ASSERT(encoder->avail() >= sizeof(uint8_t));
      encoder->put8(coding::toTag(coding::Type::kPoint, options));
      if (coding::isOptionsS2(options)) {
        auto point = latLng.ToPoint();
        encodePoint(*encoder, point);
        return point;
      }
      encodeLatLng(*encoder, latLng, options);
    } else if (options == coding::Options::kS2LatLngInt) {
      toLatLngInt(latLng);
    }
  } else {
    TRI_ASSERT(encoder == nullptr);
    TRI_ASSERT(options == coding::Options::kInvalid);
  }
  return latLng.ToPoint();
}

void encodeImpl(std::span<S2LatLng> cache, coding::Type type,
                coding::Options options, Encoder* encoder) {
  if (encoder != nullptr) {
    TRI_ASSERT(options != coding::Options::kInvalid);
    TRI_ASSERT(!coding::isOptionsS2(options));
    TRI_ASSERT(encoder->avail() >= sizeof(uint8_t) + Varint::kMax64);
    encoder->put8(coding::toTag(type, options));
    encoder->put_varint64(cache.size());
    encodeVertices(*encoder, cache, options);
  } else if (options == coding::Options::kS2LatLngInt) {
    toLatLngInt(cache);
  }
}

void fillPoints(std::vector<S2Point>& points, std::span<S2LatLng const> cache) {
  points.clear();
  points.reserve(cache.size());
  for (auto const& point : cache) {
    points.emplace_back(point.ToPoint());
  }
}

template<bool Validation>
size_t encodeCount(size_t count, coding::Type type, coding::Options options,
                   Encoder* encoder) {
  TRI_ASSERT(count != 0);
  if (Validation && encoder != nullptr) {
    TRI_ASSERT(options != coding::Options::kInvalid);
    TRI_ASSERT(!coding::isOptionsS2(options));
    encoder->Ensure(sizeof(uint8_t) + (1 + count) * Varint::kMax64 +
                    (2 + static_cast<size_t>(type == coding::Type::kPolygon)) *
                        coding::toSize(options));
    encoder->put8(coding::toTag(type, options));
    if (count != 1) {
      encoder->put_varint64(count * 2 + 1);
    } else {
      return 2;
    }
  }
  return 1;
}

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
Result parseImpl(velocypack::Slice vpack, std::vector<S2LatLng>& vertices) {
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
    vertices.emplace_back(vertex);
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
                       std::vector<S2LatLng>& vertices) {
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
Result parseLineImpl(velocypack::Slice vpack, std::vector<S2LatLng>& vertices) {
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
                      std::vector<S2LatLng>& vertices, coding::Options options,
                      Encoder* encoder) {
  TRI_ASSERT(vpack.isArray());
  velocypack::ArrayIterator it{vpack};
  auto const n = it.size();
  if (Validation && ADB_UNLIKELY(n == 0)) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        "Invalid MultiLinestring, it must contains at least one Linestring."};
  }
  auto multiplier = encodeCount<Validation>(n, coding::Type::kMultiPolyline,
                                            options, encoder);
  lines.clear();
  lines.reserve(n);
  for (; it.valid(); it.next()) {
    auto slice = *it;
    if (Validation && ADB_UNLIKELY(!slice.isArray())) {
      return {TRI_ERROR_BAD_PARAMETER, "Missing coordinates."};
    }
    [[maybe_unused]] auto r = parseLineImpl<Validation>(slice, vertices);
    if constexpr (Validation) {
      if (ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      if (encoder != nullptr) {
        encoder->put_varint64(n * multiplier);
        multiplier = 1;
        encodeVertices(*encoder, vertices, options);
      } else if (options == coding::Options::kS2LatLngInt) {
        toLatLngInt(vertices);
      }
      auto& back = lines.emplace_back(vertices, S2Debug::DISABLE);
      if (S2Error error; ADB_UNLIKELY(back.FindValidationError(&error))) {
        return {TRI_ERROR_BAD_PARAMETER,
                absl::StrCat("Invalid Polyline: ", error.text())};
      }
    } else {
      lines.emplace_back(vertices, S2Debug::DISABLE);
    }
  }
  return {};
}

template<bool Validation>
Result makeLoopValid(std::vector<S2LatLng>& vertices) noexcept(!Validation) {
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
                     std::vector<S2LatLng>& vertices) {
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
                     std::vector<S2LatLng>& vertices, S2Loop*& first,
                     coding::Options options, Encoder* encoder,
                     size_t multiplier) {
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
  if (Validation && options == coding::Options::kS2LatLngInt) {
    // TODO(MBkkt) remove unnecessary allocation
    auto copy = vertices;
    toLatLngInt(copy);
    loops.push_back(std::make_unique<S2Loop>(copy, S2Debug::DISABLE));
  } else {
    loops.push_back(std::make_unique<S2Loop>(vertices, S2Debug::DISABLE));
  }
  auto& last = *loops.back();
  if constexpr (Validation) {
    if (S2Error error; ADB_UNLIKELY(last.FindValidationError(&error))) {
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
  // TODO(MBkkt) encoding can be above if we remove last.Invert
  //  last.Invert() needed only for silent ignore CCW rule:
  //  in general holes of polygon should be in outer loop
  //  but if it's not true we silently invert it.
  //  I think it's not good idea, because for outer loop we respect CCW rule
  //  But change it needs break 3.10 behavior :(
  if constexpr (Legacy) {
    last.Normalize();
  }
  if (first == nullptr) {
    first = &last;
  } else if constexpr (!Legacy) {
    if (ADB_LIKELY(first->Contains(last))) {
      return {};
    }
    last.Invert();
    if (Validation && encoder != nullptr) {
      std::reverse(vertices.begin(), vertices.end());
    }
  }
  if constexpr (Validation) {
    if (ADB_UNLIKELY(first != &last && !first->Contains(last))) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Subsequent loop is not a hole in a polygon."};
    }
    if (encoder != nullptr) {
      TRI_ASSERT(encoder->avail() >= Varint::kMax64);
      encoder->put_varint64(vertices.size() * multiplier);
      encodeVertices(*encoder, vertices, options);
    }
  }
  return {};
}

void createPolygon(std::vector<std::unique_ptr<S2Loop>>&& loops,
                   coding::Options options, Encoder* encoder,
                   S2Polygon& polygon) {
  polygon.set_s2debug_override(S2Debug::DISABLE);
  if (loops.size() != 1 || !loops[0]->is_empty()) {
    polygon.InitNested(std::move(loops));
  } else {
    // Handle creation of empty polygon otherwise will be error in validation
    polygon.Init(std::move(loops[0]));
  }
}

template<bool Validation, bool Legacy>
Result parsePolygonImpl(velocypack::ArrayIterator it, S2Polygon& region,
                        std::vector<S2LatLng>& vertices,
                        coding::Options options, Encoder* encoder) {
  auto const n = it.size();
  TRI_ASSERT(n >= 1);
  std::vector<std::unique_ptr<S2Loop>> loops;
  loops.reserve(n);
  auto multiplier =
      encodeCount<Validation>(n, coding::Type::kPolygon, options, encoder);
  for (S2Loop* first = nullptr; it.valid(); it.next()) {
    auto r = parseLoopImpl<Validation, Legacy>(*it, loops, vertices, first,
                                               options, encoder, multiplier);
    if (Validation && ADB_UNLIKELY(!r.ok())) {
      return r;
    }
    multiplier = 1;
  }
  createPolygon(std::move(loops), options, encoder, region);
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
                        std::vector<S2LatLng>& vertices,
                        coding::Options options, Encoder* encoder) {
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
    auto loop = std::make_unique<S2Loop>(vertices, S2Debug::DISABLE);
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
    auto r = parsePolygonImpl<Validation, Legacy>(it, *d, vertices, options,
                                                  encoder);
    if (Validation && ADB_UNLIKELY(!r.ok())) {
      return r;
    }
  }
  region.reset(std::move(d), toShapeType(Type::POLYGON), options);
  return {};
}

template<bool Validation, bool Legacy>
Result parseMultiPolygonImpl(velocypack::Slice vpack, S2Polygon& region,
                             std::vector<S2LatLng>& vertices,
                             coding::Options options, Encoder* encoder) {
  TRI_ASSERT(vpack.isArray());
  velocypack::ArrayIterator it{vpack};
  auto const n = it.size();
  if (Validation && ADB_UNLIKELY(n == 0)) {
    return {TRI_ERROR_BAD_PARAMETER,
            "MultiPolygon should contains at least one Polygon."};
  }
  std::vector<std::unique_ptr<S2Loop>> loops;
  loops.reserve(n);
  auto multiplier =
      encodeCount<Validation>(n, coding::Type::kPolygon, options, encoder);
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
    if (encoder != nullptr) {
      encoder->Ensure(jt.size() * Varint::kMax64);
    }
    for (; jt.valid(); jt.next()) {
      auto r = parseLoopImpl<Validation, Legacy>(*jt, loops, vertices, first,
                                                 options, encoder, multiplier);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      multiplier = 1;
    }
    first = nullptr;
  }
  createPolygon(std::move(loops), options, encoder, region);
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
                       std::vector<S2LatLng>& cache, bool legacy,
                       coding::Options options, Encoder* encoder) {
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
  bool const isS2 = coding::isOptionsS2(options);
  switch (t) {
    case Type::POINT: {
      S2LatLng latLng;
      auto r = parsePointImpl<Validation, true>(vpack, latLng);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      region.reset(encodePointImpl<Validation>(latLng, options, encoder),
                   options);
      return {};
    }
    case Type::LINESTRING: {
      auto r = parseLineImpl<Validation>(vpack, cache);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      if (Validation && !isS2) {
        encodeImpl(cache, coding::Type::kPolyline, options, encoder);
      }
      auto d = std::make_unique<S2Polyline>(cache, S2Debug::DISABLE);
      if (S2Error error; Validation && d->FindValidationError(&error)) {
        return {TRI_ERROR_BAD_PARAMETER,
                absl::StrCat("Invalid Polyline: ", error.text())};
      }
      region.reset(std::move(d), toShapeType(Type::LINESTRING), options);
    } break;
    case Type::POLYGON: {
      auto r =
          ADB_UNLIKELY(legacy)
              ? parsePolygonImpl<Validation, true>(
                    vpack, region, cache, coding::Options::kInvalid, nullptr)
              : parsePolygonImpl<Validation, false>(
                    vpack, region, cache, options, isS2 ? nullptr : encoder);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
    } break;
    case Type::MULTI_POINT: {
      auto r = parsePointsImpl<Validation>(vpack, cache);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      if (Validation && !isS2) {
        encodeImpl(cache, coding::Type::kMultiPoint, options, encoder);
      }
      auto d = std::make_unique<S2MultiPointRegion>();
      fillPoints(d->Impl(), cache);
      region.reset(std::move(d), toShapeType(Type::MULTI_POINT), options);
    } break;
    case Type::MULTI_LINESTRING: {
      std::vector<S2Polyline> lines;
      auto r = parseLinesImpl<Validation>(vpack, lines, cache, options,
                                          isS2 ? nullptr : encoder);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      auto d = std::make_unique<S2MultiPolylineRegion>();
      d->Impl() = std::move(lines);
      region.reset(std::move(d), toShapeType(Type::MULTI_LINESTRING), options);
    } break;
    case Type::MULTI_POLYGON: {
      auto d = std::make_unique<S2Polygon>();
      auto r = ADB_UNLIKELY(legacy)
                   ? parseMultiPolygonImpl<Validation, true>(
                         vpack, *d, cache, coding::Options::kInvalid, nullptr)
                   : parseMultiPolygonImpl<Validation, false>(
                         vpack, *d, cache, options, isS2 ? nullptr : encoder);
      if (Validation && ADB_UNLIKELY(!r.ok())) {
        return r;
      }
      region.reset(std::move(d), toShapeType(Type::MULTI_POLYGON), options);
    } break;
    default:
      return {TRI_ERROR_NOT_IMPLEMENTED,
              "GeoJSON type GeometryCollection is not supported"};
  }
  if (Validation && encoder != nullptr && isS2) {
    TRI_ASSERT(encoder->length() == 0);
    encoder->clear();
    region.Encode(*encoder, options);
  }
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
  std::vector<S2LatLng> vertices;
  r = parsePointsImpl<true>(vpack, vertices);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  fillPoints(region.Impl(), vertices);
  return {};
}

Result parseLinestring(velocypack::Slice vpack, S2Polyline& region) {
  auto r = validate<Type::LINESTRING>(vpack);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  std::vector<S2LatLng> vertices;
  r = parseLineImpl<true>(vpack, vertices);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  region = S2Polyline{vertices, S2Debug::DISABLE};
  if (S2Error error; region.FindValidationError(&error)) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Invalid Polyline: ", error.text())};
  }
  return {};
}

Result parseMultiLinestring(velocypack::Slice vpack,
                            S2MultiPolylineRegion& region) {
  auto r = validate<Type::MULTI_LINESTRING>(vpack);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  auto& lines = region.Impl();
  std::vector<S2LatLng> vertices;
  return parseLinesImpl<true>(vpack, lines, vertices, coding::Options::kInvalid,
                              nullptr);
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
  std::vector<S2LatLng> vertices;
  return parsePolygonImpl<true, false>(it, region, vertices,
                                       coding::Options::kInvalid, nullptr);
}

Result parseMultiPolygon(velocypack::Slice vpack, S2Polygon& region) {
  auto r = validate<Type::MULTI_POLYGON>(vpack);
  if (ADB_UNLIKELY(!r.ok())) {
    return r;
  }
  std::vector<S2LatLng> vertices;
  return parseMultiPolygonImpl<true, false>(vpack, region, vertices,
                                            coding::Options::kInvalid, nullptr);
}

Result parseRegion(velocypack::Slice vpack, ShapeContainer& region,
                   bool legacy) {
  std::vector<S2LatLng> cache;
  return parseRegionImpl<true>(vpack, region, cache, legacy,
                               coding::Options::kInvalid, nullptr);
}

template<bool Valid>
Result parseRegion(velocypack::Slice vpack, ShapeContainer& region,
                   std::vector<S2LatLng>& cache, bool legacy,
                   coding::Options options, Encoder* encoder) {
  auto r = parseRegionImpl<(Valid || kIsMaintainer)>(vpack, region, cache,
                                                     legacy, options, encoder);
  TRI_ASSERT(Valid || r.ok()) << r.errorMessage();
  return r;
}

template Result parseRegion<true>(velocypack::Slice vpack,
                                  ShapeContainer& region,
                                  std::vector<S2LatLng>& cache, bool legacy,
                                  coding::Options options, Encoder* encoder);
template Result parseRegion<false>(velocypack::Slice vpack,
                                   ShapeContainer& region,
                                   std::vector<S2LatLng>& cache, bool legacy,
                                   coding::Options options, Encoder* encoder);

template<bool Valid>
Result parseCoordinates(velocypack::Slice vpack, ShapeContainer& region,
                        bool geoJson, coding::Options options,
                        Encoder* encoder) {
  auto r = [&]() -> Result {
    static constexpr bool Validation = Valid || kIsMaintainer;
    if (Validation && ADB_UNLIKELY(!vpack.isArray())) {
      return {TRI_ERROR_BAD_PARAMETER, "Invalid coordinate pair."};
    }
    S2LatLng latLng;
    auto res = geoJson ? parsePointImpl<Validation, true>(vpack, latLng)
                       : parsePointImpl<Validation, false>(vpack, latLng);
    if (Validation && ADB_UNLIKELY(!res.ok())) {
      return res;
    }
    region.reset(encodePointImpl<Validation>(latLng, options, encoder),
                 options);
    return {};
  }();
  TRI_ASSERT(Valid || r.ok()) << r.errorMessage();
  return r;
}

template Result parseCoordinates<true>(velocypack::Slice vpack,
                                       ShapeContainer& region, bool geoJson,
                                       coding::Options options,
                                       Encoder* encoder);
template Result parseCoordinates<false>(velocypack::Slice vpack,
                                        ShapeContainer& region, bool geoJson,
                                        coding::Options options,
                                        Encoder* encoder);

Result parseLoop(velocypack::Slice vpack, S2Loop& loop, bool geoJson) {
  static constexpr bool Validation = true;
  if (ADB_UNLIKELY(!vpack.isArray())) {
    return {TRI_ERROR_BAD_PARAMETER, "Coordinates missing."};
  }
  std::vector<S2LatLng> vertices;
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
  loop.set_s2debug_override(S2Debug::DISABLE);
  loop.Init(vertices);
  S2Error error;
  if (ADB_UNLIKELY(loop.FindValidationError(&error))) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Invalid loop: ", error.text())};
  }
  loop.Normalize();
  return {};
}

}  // namespace arangodb::geo::json
