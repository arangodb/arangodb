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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Geo/ShapeContainer.h"
#include "Geo/GeoParams.h"
#include "Geo/Ellipsoid.h"
#include "Geo/Utils.h"
#include "Geo/S2/S2MultiPointRegion.h"
#include "Geo/S2/S2MultiPolylineRegion.h"
#include "Geo/karney/geodesic.h"
#include "Basics/application-exit.h"
#include "Basics/DownCast.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"

#include <s2/s1angle.h>
#include <s2/s2latlng.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2point_region.h>
#include <s2/s2polygon.h>
#include <s2/s2polyline.h>

#include <bit>
#include <cmath>

namespace arangodb::geo {
namespace rect {
// only used in legacy situations

static S2Polygon toPolygon(S2LatLngRect const& rect) {
  std::array<S2Point, 4> v{
      rect.GetVertex(0).ToPoint(), rect.GetVertex(1).ToPoint(),
      rect.GetVertex(2).ToPoint(), rect.GetVertex(3).ToPoint()};
  auto loop = std::make_unique<S2Loop>(v, S2Debug::DISABLE);
  return S2Polygon{std::move(loop), S2Debug::DISABLE};
}

static bool contains(S2LatLngRect const& rect, S2Polyline const& polyline) {
  for (auto const& point : polyline.vertices_span()) {
    if (!rect.Contains(point)) {
      return false;
    }
  }
  return true;
}

static bool contains(S2Polygon const& polygon, S2LatLngRect const& rect) {
  if (ADB_UNLIKELY(rect.is_point())) {
    TRI_ASSERT(rect.lo().is_valid());
    return polygon.Contains(rect.lo().ToPoint());
  }
  auto subregionBound = polygon.GetSubRegionBound();
  if (ADB_LIKELY(!subregionBound.Contains(rect))) {
    return false;
  }
  // TODO(MBkkt) ask and check is it really necessary
  //  The main question will be full rect is full polygon or not?
  if (ADB_UNLIKELY(polygon.is_empty() && rect.is_full())) {
    return false;
  }
  auto rectPolygon = toPolygon(rect);
  return S2BooleanOperation::Contains(polygon.index(), rectPolygon.index());
}

bool intersects(S2LatLngRect const& rect, S2Polyline const& polyline) {
  if (ADB_UNLIKELY(rect.is_point())) {
    // is numerically unstable and thus always false
    return false;
  }
  auto rectPolygon = toPolygon(rect);
  return rectPolygon.Intersects(polyline);
}

static bool intersects(S2LatLngRect const& rect, S2Polygon const& polygon) {
  if (ADB_UNLIKELY(rect.is_point())) {
    return polygon.Contains(rect.lo().ToPoint());
  }
  auto bound = polygon.GetRectBound();
  if (ADB_LIKELY(!rect.Intersects(bound))) {
    return false;
  } else if (ADB_LIKELY(rect.Contains(bound))) {
    return true;
  }
  // TODO(MBkkt) ask and check is it really necessary
  if (ADB_UNLIKELY(rect.is_full() && polygon.is_full())) {
    return true;
  }
  auto rectPolygon = toPolygon(rect);
  return S2BooleanOperation::Intersects(rectPolygon.index(), polygon.index());
}

}  // namespace rect
namespace {

bool isExcessiveLngLat(S1Angle lngsmall, S1Angle lngbig, S1Angle latsmall,
                       S1Angle latbig) {
  return std::fabs(lngbig.radians() - lngsmall.radians()) +
             std::fabs(latbig.radians() - latsmall.radians()) >=
         M_PI;
}

constexpr uint8_t binOpCase(ShapeContainer::Type lhs,
                            ShapeContainer::Type rhs) noexcept {
  // any >= 7 && <= 41 is ok, but compiler use 8, so why not?
  return (static_cast<uint8_t>(lhs) * 8) + static_cast<uint8_t>(rhs);
}

template<typename T>
bool containsPoint(S2Region const& region, S2Region const& point) {
  auto const& lhs = basics::downCast<T>(region);
  auto const& rhs = basics::downCast<S2PointRegion>(point);
  return lhs.Contains(rhs.point());
}

template<typename T>
bool containsPoints(S2Region const& region, S2Region const& points) {
  auto const& lhs = basics::downCast<T>(region);
  auto const& rhs = basics::downCast<S2MultiPointRegion>(points);
  for (auto const& point : rhs.Impl()) {
    if (!lhs.Contains(point)) {
      return false;
    }
  }
  return true;
}

template<typename T>
bool containsPolyline(T const& lhs, S2Polyline const& rhs) {
  static constexpr auto kMaxError = S1Angle::Radians(1e-6);
  if constexpr (std::is_same_v<T, S2Polyline>) {
    return lhs.ApproxEquals(rhs, kMaxError);
  } else if constexpr (std::is_same_v<T, S2LatLngRect>) {
    return rect::contains(lhs, rhs);
  } else if constexpr (std::is_same_v<T, S2Polygon>) {
    return lhs.Contains(rhs);
  } else {
    static_assert(std::is_same_v<T, S2MultiPolylineRegion>);
    for (auto const& polyline : lhs.Impl()) {
      if (polyline.ApproxEquals(rhs, kMaxError)) {
        return true;
      }
    }
    return false;
  }
}

template<typename T>
bool containsPolyline(S2Region const& region, S2Region const& polyline) {
  auto const& lhs = basics::downCast<T>(region);
  auto const& rhs = basics::downCast<S2Polyline>(polyline);
  return containsPolyline<T>(lhs, rhs);
}

template<typename T>
bool containsPolylines(S2Region const& region, S2Region const& polylines) {
  auto const& lhs = basics::downCast<T>(region);
  auto const& rhs = basics::downCast<S2MultiPolylineRegion>(polylines);
  for (auto const& polyline : rhs.Impl()) {
    if (!containsPolyline<T>(lhs, polyline)) {
      return false;
    }
  }
  return true;
}

template<typename T>
bool containsRect(S2Region const& region, S2Region const& rect) {
  auto const& lhs = basics::downCast<T>(region);
  auto const& rhs = basics::downCast<S2LatLngRect>(rect);
  if constexpr (std::is_same_v<T, S2LatLngRect>) {
    return lhs.Contains(rhs);
  } else if constexpr (std::is_same_v<T, S2Polygon>) {
    return rect::contains(lhs, rhs);
  } else {
    if (ADB_LIKELY(!rhs.is_point())) {
      return false;
    }
    return lhs.Contains(rhs.lo().ToPoint());
  }
}

template<typename R1, typename R2>
bool intersectsHelper(S2Region const& r1, S2Region const& r2) {
  auto const& lhs = basics::downCast<R1>(r1);
  auto const& rhs = basics::downCast<R2>(r2);
  if constexpr (std::is_same_v<R1, S2PointRegion>) {
    return rhs.Contains(lhs.point());
  } else {
    return rhs.Intersects(lhs);
  }
}

template<typename T>
bool intersectsRect(S2Region const& r1, S2Region const& r2) {
  auto const& lhs = basics::downCast<T>(r1);
  auto const& rhs = basics::downCast<S2LatLngRect>(r2);
  return rect::intersects(rhs, lhs);
}

}  // namespace

void ShapeContainer::updateBounds(QueryParams& qp) const noexcept {
  TRI_ASSERT(!empty());

  S2LatLng ll{centroid()};
  qp.origin = ll;

  auto rect = _data->GetRectBound();
  if (rect.is_empty() || rect.is_point()) {
    qp.maxDistance = 0.0;
    return;
  }
  double radMax = 0.0;
  // The following computation deserves an explanation:
  // We want to derive from the bounding LatLng box an upper bound for
  // the maximal distance. The centroid of the shape is contained in the
  // bounding box and the main idea is to take the maximum distance to
  // any of the corners and take this as upper bound for the distance.
  // The hope is then that the complete bounding box is contained in the
  // circle with radius this maximal distance.
  // However, this is not correct in all cases. A prominent counterexample
  // is the bounding box {lat:[-90, 90], lng:[-180, 180]} which is used
  // for very large polygons. Its "four" corners are twice the north pole
  // and twice the south pole. Most points on earth have a maximal distance
  // to north and south pole of less than half the diameter of the earth,
  // and yet, the smallest circle to contain the whole bounding box has
  // radius half of the diameter of the earth.
  // So we need to adjust our bound here. What we do is the following:
  // If the sum of the added difference in latitude and longitude
  // is less than 180 degrees, then the actual shortest geodesic to a
  // corner runs as expected (for example, with increasing lat and lng
  // towards the upper right corner of the bounding box). In this case
  // the estimate of the maximal distance is correct, otherwise we simply
  // take M_PI or 180 degrees or half the diameter of the earth as estimate.
  if (isExcessiveLngLat(rect.lng_lo(), ll.lng(), rect.lat_lo(), ll.lat())) {
    radMax = M_PI;
  } else {
    S1Angle a1{ll, rect.lo()};
    radMax = a1.radians();
  }
  if (isExcessiveLngLat(ll.lng(), rect.lng_hi(), rect.lat_lo(), ll.lat())) {
    radMax = M_PI;
  } else {
    S1Angle a2{ll, S2LatLng{rect.lat_lo(), rect.lng_hi()}};
    radMax = std::max(radMax, a2.radians());
  }
  if (isExcessiveLngLat(rect.lng_lo(), ll.lng(), ll.lat(), rect.lat_hi())) {
    radMax = M_PI;
  } else {
    S1Angle a3{ll, S2LatLng{rect.lat_hi(), rect.lng_lo()}};
    radMax = std::max(radMax, a3.radians());
  }
  if (isExcessiveLngLat(ll.lng(), rect.lng_hi(), ll.lat(), rect.lat_hi())) {
    radMax = M_PI;
  } else {
    S1Angle a4{ll, rect.hi()};
    radMax = std::max(radMax, a4.radians());
  }
  auto const rad = kRadEps + radMax;
  qp.maxDistance = rad * kEarthRadiusInMeters;
}

S2Point ShapeContainer::centroid() const noexcept {
  switch (_type) {
    case Type::S2_POINT:
      // S2PointRegion should be constructed from unit length Point
      return basics::downCast<S2PointRegion>(*_data).point();
    case Type::S2_POLYLINE:
      // S2Polyline::GetCentroid() result isn't unit length
      return basics::downCast<S2Polyline>(*_data).GetCentroid().Normalize();
    case Type::S2_LATLNGRECT:  // only used in legacy situations
      // S2LatLngRect should be constructed from Normalized S2LatLng,
      // so it's ok to don't Normalize here?
      return basics::downCast<S2LatLngRect>(*_data).GetCentroid().Normalize();
    case Type::S2_POLYGON:
      // S2Polygon::GetCentroid() result isn't unit length
      return basics::downCast<S2Polygon>(*_data).GetCentroid().Normalize();
    case Type::S2_MULTIPOINT:
      // S2MultiPointRegion::GetCentroid() result isn't unit length
      return basics::downCast<S2MultiPointRegion>(*_data)
          .GetCentroid()
          .Normalize();
    case Type::S2_MULTIPOLYLINE:
      // S2MultiPolylineRegion::GetCentroid() result isn't unit length
      return basics::downCast<S2MultiPolylineRegion>(*_data)
          .GetCentroid()
          .Normalize();
    case Type::EMPTY:
      TRI_ASSERT(false);
  }
  return {};
}

bool ShapeContainer::contains(S2Point const& other) const {
  TRI_ASSERT(!empty());
  return _data->Contains(other);
}

bool ShapeContainer::contains(ShapeContainer const& other) const {
  TRI_ASSERT(coding::isSameLoss(_options, other._options));
  // In this implementation we force compiler to generate single jmp table.
  auto const sum = binOpCase(_type, other._type);
  switch (sum) {
    case binOpCase(Type::S2_POINT, Type::S2_POINT):
      return containsPoint<S2PointRegion>(*_data, *other._data);
    case binOpCase(Type::S2_LATLNGRECT, Type::S2_POINT):
      return containsPoint<S2LatLngRect>(*_data, *other._data);
    case binOpCase(Type::S2_POLYGON, Type::S2_POINT):
      return containsPoint<S2Polygon>(*_data, *other._data);
    case binOpCase(Type::S2_MULTIPOINT, Type::S2_POINT):
      return containsPoint<S2MultiPointRegion>(*_data, *other._data);

    case binOpCase(Type::S2_POINT, Type::S2_MULTIPOINT):
      return containsPoints<S2PointRegion>(*_data, *other._data);
    case binOpCase(Type::S2_LATLNGRECT, Type::S2_MULTIPOINT):
      return containsPoints<S2LatLngRect>(*_data, *other._data);
    case binOpCase(Type::S2_POLYGON, Type::S2_MULTIPOINT):
      return containsPoints<S2Polygon>(*_data, *other._data);
    case binOpCase(Type::S2_MULTIPOINT, Type::S2_MULTIPOINT):
      return containsPoints<S2MultiPointRegion>(*_data, *other._data);

    case binOpCase(Type::S2_POLYLINE, Type::S2_POLYLINE):
      return containsPolyline<S2Polyline>(*_data, *other._data);
    case binOpCase(Type::S2_LATLNGRECT, Type::S2_POLYLINE):
      return containsPolyline<S2LatLngRect>(*_data, *other._data);
    case binOpCase(Type::S2_POLYGON, Type::S2_POLYLINE):
      return containsPolyline<S2Polygon>(*_data, *other._data);
    case binOpCase(Type::S2_MULTIPOLYLINE, Type::S2_POLYLINE):
      return containsPolyline<S2MultiPolylineRegion>(*_data, *other._data);

    case binOpCase(Type::S2_POLYLINE, Type::S2_MULTIPOLYLINE):
      return containsPolylines<S2Polyline>(*_data, *other._data);
    case binOpCase(Type::S2_LATLNGRECT, Type::S2_MULTIPOLYLINE):
      return containsPolylines<S2LatLngRect>(*_data, *other._data);
    case binOpCase(Type::S2_POLYGON, Type::S2_MULTIPOLYLINE):
      return containsPolylines<S2Polygon>(*_data, *other._data);
    case binOpCase(Type::S2_MULTIPOLYLINE, Type::S2_MULTIPOLYLINE):
      return containsPolylines<S2MultiPolylineRegion>(*_data, *other._data);

      // only used in legacy situations
    case binOpCase(Type::S2_POINT, Type::S2_LATLNGRECT):
      return containsRect<S2PointRegion>(*_data, *other._data);
    case binOpCase(Type::S2_LATLNGRECT, Type::S2_LATLNGRECT):
      return containsRect<S2LatLngRect>(*_data, *other._data);
    case binOpCase(Type::S2_POLYGON, Type::S2_LATLNGRECT):
      return containsRect<S2Polygon>(*_data, *other._data);
    case binOpCase(Type::S2_MULTIPOINT, Type::S2_LATLNGRECT):
      return containsRect<S2MultiPointRegion>(*_data, *other._data);

    case binOpCase(Type::S2_LATLNGRECT, Type::S2_POLYGON): {
      auto const& lhs = basics::downCast<S2LatLngRect>(*_data);
      auto const& rhs = basics::downCast<S2Polygon>(*other._data);
      return lhs.Contains(rhs.GetRectBound());
    }
    case binOpCase(Type::S2_POLYGON, Type::S2_POLYGON): {
      auto const& lhs = basics::downCast<S2Polygon>(*_data);
      auto const& rhs = basics::downCast<S2Polygon>(*other._data);
      return lhs.Contains(rhs);
    }

    case binOpCase(Type::S2_POLYLINE, Type::S2_POINT):
    case binOpCase(Type::S2_MULTIPOLYLINE, Type::S2_POINT):
    case binOpCase(Type::S2_POINT, Type::S2_POLYLINE):
    case binOpCase(Type::S2_MULTIPOINT, Type::S2_POLYLINE):
    case binOpCase(Type::S2_POLYLINE, Type::S2_LATLNGRECT):
    case binOpCase(Type::S2_MULTIPOLYLINE, Type::S2_LATLNGRECT):
    case binOpCase(Type::S2_POINT, Type::S2_POLYGON):
    case binOpCase(Type::S2_POLYLINE, Type::S2_POLYGON):
    case binOpCase(Type::S2_MULTIPOINT, Type::S2_POLYGON):
    case binOpCase(Type::S2_MULTIPOLYLINE, Type::S2_POLYGON):
    case binOpCase(Type::S2_POLYLINE, Type::S2_MULTIPOINT):
    case binOpCase(Type::S2_MULTIPOLYLINE, Type::S2_MULTIPOINT):
    case binOpCase(Type::S2_POINT, Type::S2_MULTIPOLYLINE):
    case binOpCase(Type::S2_MULTIPOINT, Type::S2_MULTIPOLYLINE):
      // is numerically unstable and thus always false
      return false;
    default:
      TRI_ASSERT(false);
  }
  return false;
}

bool ShapeContainer::intersects(ShapeContainer const& other) const {
  TRI_ASSERT(coding::isSameLoss(_options, other._options));
  // In this implementation we manually decrease count of switch cases,
  // and force compiler to generate single jmp table.
  // We can because user expect intersects(a, b) == intersects(b, a)
  auto const* d1 = _data.get();
  auto const* d2 = other._data.get();
  auto t1 = _type;
  auto t2 = other._type;
  if (t1 > t2) {
    std::swap(d1, d2);
    std::swap(t1, t2);
  }
  auto const sum = binOpCase(t1, t2);
  switch (sum) {
    case binOpCase(Type::S2_POINT, Type::S2_POINT):
      return intersectsHelper<S2PointRegion, S2PointRegion>(*d1, *d2);
    case binOpCase(Type::S2_POINT, Type::S2_LATLNGRECT):
      return intersectsHelper<S2PointRegion, S2LatLngRect>(*d1, *d2);
    case binOpCase(Type::S2_POINT, Type::S2_POLYGON):
      return intersectsHelper<S2PointRegion, S2Polygon>(*d1, *d2);
    case binOpCase(Type::S2_POINT, Type::S2_MULTIPOINT):
      return intersectsHelper<S2PointRegion, S2MultiPointRegion>(*d1, *d2);

    case binOpCase(Type::S2_POLYLINE, Type::S2_POLYLINE):
      return intersectsHelper<S2Polyline, S2Polyline>(*d1, *d2);
    case binOpCase(Type::S2_POLYLINE, Type::S2_LATLNGRECT):
      return intersectsRect<S2Polyline>(*d1, *d2);
    case binOpCase(Type::S2_POLYLINE, Type::S2_POLYGON):
      return intersectsHelper<S2Polyline, S2Polygon>(*d1, *d2);
    case binOpCase(Type::S2_POLYLINE, Type::S2_MULTIPOLYLINE):
      return intersectsHelper<S2Polyline, S2MultiPolylineRegion>(*d1, *d2);

    case binOpCase(Type::S2_LATLNGRECT, Type::S2_LATLNGRECT):
      return intersectsHelper<S2LatLngRect, S2LatLngRect>(*d1, *d2);
    case binOpCase(Type::S2_LATLNGRECT, Type::S2_POLYGON):
      return intersectsRect<S2Polygon>(*d2, *d1);
    case binOpCase(Type::S2_LATLNGRECT, Type::S2_MULTIPOINT):
      return intersectsHelper<S2LatLngRect, S2MultiPointRegion>(*d1, *d2);
    case binOpCase(Type::S2_LATLNGRECT, Type::S2_MULTIPOLYLINE):
      return intersectsHelper<S2LatLngRect, S2MultiPolylineRegion>(*d1, *d2);

    case binOpCase(Type::S2_POLYGON, Type::S2_POLYGON):
      return intersectsHelper<S2Polygon, S2Polygon>(*d1, *d2);
    case binOpCase(Type::S2_POLYGON, Type::S2_MULTIPOINT):
      return intersectsHelper<S2Polygon, S2MultiPointRegion>(*d1, *d2);
    case binOpCase(Type::S2_POLYGON, Type::S2_MULTIPOLYLINE):
      return intersectsHelper<S2Polygon, S2MultiPolylineRegion>(*d1, *d2);

    case binOpCase(Type::S2_MULTIPOINT, Type::S2_MULTIPOINT):
      return intersectsHelper<S2MultiPointRegion, S2MultiPointRegion>(*d1, *d2);

    case binOpCase(Type::S2_MULTIPOLYLINE, Type::S2_MULTIPOLYLINE):
      return intersectsHelper<S2MultiPolylineRegion, S2MultiPolylineRegion>(
          *d1, *d2);

    case binOpCase(Type::S2_POINT, Type::S2_POLYLINE):
    case binOpCase(Type::S2_POINT, Type::S2_MULTIPOLYLINE):
    case binOpCase(Type::S2_POLYLINE, Type::S2_MULTIPOINT):
    case binOpCase(Type::S2_MULTIPOINT, Type::S2_MULTIPOLYLINE): {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "The case GEO_INTERSECTS(<some points>, <some polylines>)"
          " is numerically unstable and thus not supported.");
    }
    default:
      TRI_ASSERT(false);
  }
  return false;
}

void ShapeContainer::reset(std::unique_ptr<S2Region> data, Type type,
                           coding::Options options) noexcept {
  TRI_ASSERT((data == nullptr) == (type == Type::EMPTY));
  TRI_ASSERT(data == nullptr || type != Type::S2_POINT ||
             S2::IsUnitLength(basics::downCast<S2PointRegion>(*data).point()));
  _data = std::move(data);
  _type = type;
  _options = options;
}

void ShapeContainer::reset(S2Point point, coding::Options options) {
  // TODO(MBkkt) enable s2 checks in maintainer mode
  // assert from S2PointRegion ctor
  TRI_ASSERT(S2::IsUnitLength(point));
  if (ADB_LIKELY(_type == Type::S2_POINT)) {
    TRI_ASSERT(_data);
    auto& region = basics::downCast<S2PointRegion>(*_data);
    region = S2PointRegion{point};
  } else {
    _data = std::make_unique<S2PointRegion>(point);
    _type = Type::S2_POINT;
  }
  _options = options;
}

bool ShapeContainer::equals(ShapeContainer const& other) const {
  if (_type != other._type || !coding::isSameLoss(_options, other._options)) {
    return false;
  }
  switch (_type) {
    case Type::EMPTY:
      return true;
    case Type::S2_POINT: {
      auto const& lhs = basics::downCast<S2PointRegion>(*_data);
      auto const& rhs = basics::downCast<S2PointRegion>(*other._data);
      return lhs.Contains(rhs.point());
    }
    case Type::S2_POLYLINE: {
      auto const& lhs = basics::downCast<S2Polyline>(*_data);
      auto const& rhs = basics::downCast<S2Polyline>(*other._data);
      return lhs.Equals(rhs);
    }
    case Type::S2_LATLNGRECT: {  // only used in legacy situations
      auto const& lhs = basics::downCast<S2LatLngRect>(*_data);
      auto const& rhs = basics::downCast<S2LatLngRect>(*other._data);
      return lhs.ApproxEquals(rhs);
    }
    case Type::S2_POLYGON: {
      auto const& lhs = basics::downCast<S2Polygon>(*_data);
      auto const& rhs = basics::downCast<S2Polygon>(*other._data);
      return lhs.Equals(rhs);
    }
    case Type::S2_MULTIPOINT: {
      auto const& lhs = basics::downCast<S2MultiPointRegion>(*_data);
      auto const& rhs = basics::downCast<S2MultiPointRegion>(*other._data);
      auto const& lhsPoints = lhs.Impl();
      auto const& rhsPoints = rhs.Impl();
      auto const size = lhsPoints.size();
      if (size != rhsPoints.size()) {
        return false;
      }
      for (size_t i = 0; i != size; ++i) {
        if (lhsPoints[i] != rhsPoints[i]) {
          return false;
        }
      }
      return true;
    }
    case Type::S2_MULTIPOLYLINE: {
      auto const& lhs = basics::downCast<S2MultiPolylineRegion>(*_data);
      auto const& rhs = basics::downCast<S2MultiPolylineRegion>(*other._data);
      auto const& lhsLines = lhs.Impl();
      auto const& rhsLines = rhs.Impl();
      auto const size = lhsLines.size();
      if (size != rhsLines.size()) {
        return false;
      }
      for (size_t i = 0; i != size; ++i) {
        if (!lhsLines[i].ApproxEquals(rhsLines[i])) {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

double ShapeContainer::distanceFromCentroid(S2Point const& other,
                                            Ellipsoid const& e) const noexcept {
  return utils::geodesicDistance(S2LatLng{centroid()}, S2LatLng{other}, e);
}

double ShapeContainer::distanceFromCentroid(
    S2Point const& other) const noexcept {
  return centroid().Angle(other) * geo::kEarthRadiusInMeters;
}

double ShapeContainer::area(Ellipsoid const& e) const {
  if (!isAreaType()) {
    return 0.0;
  }

  // TODO: perhaps remove in favor of one code-path below ?
  if (e.flattening() == 0.0) {
    switch (_type) {
      case Type::S2_LATLNGRECT: {  // only used in legacy situations
        auto& data = basics::downCast<S2LatLngRect>(*_data);
        return data.Area() * kEarthRadiusInMeters * kEarthRadiusInMeters;
      }
      case Type::S2_POLYGON: {
        auto& data = basics::downCast<S2Polygon>(*_data);
        return data.GetArea() * kEarthRadiusInMeters * kEarthRadiusInMeters;
      }
      default:
        TRI_ASSERT(false);
        return 0.0;
    }
  }

  double area = 0.0;
  geod_geodesic g{};
  geod_init(&g, e.equator_radius(), e.flattening());
  double A = 0.0;
  double P = 0.0;

  switch (_type) {
    case Type::S2_LATLNGRECT: {  // only used in legacy situations
      geod_polygon p{};
      geod_polygon_init(&p, 0);

      auto& data = basics::downCast<S2LatLngRect>(*_data);
      geod_polygon_addpoint(&g, &p, data.lat_lo().degrees(),
                            data.lng_lo().degrees());
      geod_polygon_addpoint(&g, &p, data.lat_lo().degrees(),
                            data.lng_hi().degrees());
      geod_polygon_addpoint(&g, &p, data.lat_hi().degrees(),
                            data.lng_hi().degrees());
      geod_polygon_addpoint(&g, &p, data.lat_hi().degrees(),
                            data.lng_lo().degrees());

      geod_polygon_compute(&g, &p, 0, 1, &A, &P);
      area = A;
    } break;
    case Type::S2_POLYGON: {
      geod_polygon p{};
      auto& data = basics::downCast<S2Polygon>(*_data);
      for (int k = 0; k < data.num_loops(); ++k) {
        geod_polygon_init(&p, 0);

        auto const* loop = data.loop(k);
        for (auto const& vertex : loop->vertices_span()) {
          S2LatLng latLng{vertex};
          geod_polygon_addpoint(&g, &p, latLng.lat().degrees(),
                                latLng.lng().degrees());
        }

        geod_polygon_compute(&g, &p, /*reverse=*/false, /*sign=*/true, &A, &P);
        area += A;
      }
    } break;
    default:
      TRI_ASSERT(false);
      return 0.0;
  }
  return area;
}

std::vector<S2CellId> ShapeContainer::covering(S2RegionCoverer& coverer) const {
  std::vector<S2CellId> cover;
  switch (_type) {
    case Type::S2_POINT: {
      auto const& data = basics::downCast<S2PointRegion>(*_data);
      cover = {S2CellId(data.point())};
    } break;
    case Type::S2_POLYLINE:
    case Type::S2_LATLNGRECT:
    case Type::S2_POLYGON: {
      coverer.GetCovering(*_data, &cover);
    } break;
    case Type::S2_MULTIPOINT: {
      auto const& data = basics::downCast<S2MultiPointRegion>(*_data);
      auto const& points = data.Impl();
      cover.reserve(points.size());
      for (auto const& point : points) {
        cover.emplace_back(S2CellId{point});
      }
    } break;
    case Type::S2_MULTIPOLYLINE: {
      auto const& data = basics::downCast<S2MultiPolylineRegion>(*_data);
      auto const& lines = data.Impl();
      std::vector<S2CellId> lineCover;
      for (auto const& line : lines) {
        coverer.GetCovering(line, &lineCover);
        if (!lineCover.empty()) {
          cover.insert(cover.end(), lineCover.begin(), lineCover.end());
        }
      }
    } break;
    case Type::EMPTY:
      TRI_ASSERT(false);
  }
  return cover;
}

void ShapeContainer::Encode(Encoder& encoder, coding::Options options) const {
  ensureLittleEndian();
  TRI_ASSERT(coding::isOptionsS2(options));
  TRI_ASSERT(encoder.avail() >= sizeof(uint8_t));
  switch (_type) {
    case Type::S2_POLYLINE: {
      auto const& data = basics::downCast<S2Polyline>(*_data);
      encodePolyline(encoder, data, options);
    } break;
    case Type::S2_POLYGON: {
      auto const& data = basics::downCast<S2Polygon>(*_data);
      encodePolygon(encoder, data, options);
    } break;
    case Type::S2_MULTIPOINT: {
      auto const& data = basics::downCast<S2MultiPointRegion>(*_data);
      data.Encode(encoder, options);
    } break;
    case Type::S2_MULTIPOLYLINE: {
      auto const& data = basics::downCast<S2MultiPolylineRegion>(*_data);
      data.Encode(encoder, options);
    } break;
    case Type::S2_POINT:       // encoded in other path
    case Type::S2_LATLNGRECT:  // not exist in non-legacy geo
    case Type::EMPTY:
      TRI_ASSERT(false);
  }
}

template<ShapeContainer::Type TypeT, typename T>
void ShapeContainer::decodeImpl(Decoder& decoder) {
  if (ADB_UNLIKELY(_type != TypeT)) {
    if constexpr (std::is_same_v<T, S2PointRegion>) {
      // Any S2Point, but should be unit length
      _data = std::make_unique<T>(S2Point{1, 0, 0});
    } else {
      auto data = std::make_unique<T>();
      if constexpr (std::is_same_v<T, S2Polyline> ||
                    std::is_same_v<T, S2Polygon>) {
        data->set_s2debug_override(S2Debug::DISABLE);
      }
      _data = std::move(data);
    }
    _type = TypeT;
  }
}

bool ShapeContainer::Decode(Decoder& decoder, std::vector<S2Point>& cache) {
  if constexpr (std::endian::native == std::endian::big) {
    return false;
  }
  if (decoder.avail() < sizeof(uint8_t)) {
    return false;
  }
  auto const tag = decoder.get8();
  switch (coding::toType(tag)) {
    case coding::toTag<coding::Type::kPoint>(): {
      decodeImpl<Type::S2_POINT, S2PointRegion>(decoder);
      auto& data = basics::downCast<S2PointRegion>(*_data);
      if (S2Point point; decodePoint(decoder, point, tag)) {
        data = S2PointRegion{point};
        _options = static_cast<coding::Options>(coding::toPoint(tag));
        return true;
      }
    } break;
    case coding::toTag<coding::Type::kPolyline>(): {
      decodeImpl<Type::S2_POLYLINE, S2Polyline>(decoder);
      auto& polyline = basics::downCast<S2Polyline>(*_data);
      if (decodePolyline(decoder, polyline, tag, cache)) {
        _options = static_cast<coding::Options>(coding::toPoint(tag));
        return true;
      }
    } break;
    case coding::toTag<coding::Type::kPolygon>(): {
      decodeImpl<Type::S2_POLYGON, S2Polygon>(decoder);
      auto& polygon = basics::downCast<S2Polygon>(*_data);
      if (decodePolygon(decoder, polygon, tag, cache)) {
        _options = static_cast<coding::Options>(coding::toPoint(tag));
        return true;
      }
    } break;
    case coding::toTag<coding::Type::kMultiPoint>(): {
      decodeImpl<Type::S2_MULTIPOINT, S2MultiPointRegion>(decoder);
      auto& points = basics::downCast<S2MultiPointRegion>(*_data);
      if (points.Decode(decoder, tag)) {
        _options = static_cast<coding::Options>(coding::toPoint(tag));
        return true;
      }
    } break;
    case coding::toTag<coding::Type::kMultiPolyline>(): {
      decodeImpl<Type::S2_MULTIPOLYLINE, S2MultiPolylineRegion>(decoder);
      auto& polylines = basics::downCast<S2MultiPolylineRegion>(*_data);
      if (polylines.Decode(decoder, tag, cache)) {
        _options = static_cast<coding::Options>(coding::toPoint(tag));
        return true;
      }
    } break;
    default:
      TRI_ASSERT(false);
  }
  return false;
}

}  // namespace arangodb::geo
