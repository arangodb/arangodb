////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <stddef.h>
#include <algorithm>

#include <s2/s1angle.h>
#include <s2/s2cell.h>
#include <s2/s2cell_id.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2loop.h>
#include <s2/s2point_region.h>
#include <s2/s2polygon.h>
#include <s2/s2polyline.h>
#include <s2/s2region.h>
#include <s2/s2region_coverer.h>
#include <s2/util/math/vector.h>

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "ShapeContainer.h"

#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Geo/Ellipsoid.h"
#include "Geo/GeoParams.h"
#include "Geo/S2/S2MultiPointRegion.h"
#include "Geo/S2/S2MultiPolyline.h"
#include "Geo/Shapes.h"
#include "Geo/Utils.h"
#include "Geo/karney/geodesic.h"
#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::geo;

Result ShapeContainer::parseCoordinates(VPackSlice const& json, bool geoJson) {
  if (!json.isArray() || json.length() < 2) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid coordinate pair");
  }

  VPackSlice lat = json.at(size_t(geoJson));
  VPackSlice lng = json.at(1 - size_t(geoJson));
  if (!lat.isNumber() || !lng.isNumber()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid coordinate pair");
  }

  resetCoordinates(lat.getNumber<double>(), lng.getNumber<double>());
  return TRI_ERROR_NO_ERROR;
}

ShapeContainer::ShapeContainer(ShapeContainer&& other) noexcept
    : _data(other._data), _type(other._type) {
  other._data = nullptr;
  other._type = ShapeContainer::Type::EMPTY;
}
ShapeContainer::~ShapeContainer() { delete _data; }

ShapeContainer& ShapeContainer::operator=(ShapeContainer&& rhs) noexcept {
  if (this != &rhs) {
    this->reset(rhs._data, rhs._type);
    rhs._data = nullptr;
    rhs._type = Type::EMPTY;
  }
  return *this;
}

void ShapeContainer::reset(std::unique_ptr<S2Region> ptr, Type tt) noexcept {
  delete _data;
  _type = tt;
  _data = ptr.release();
}

void ShapeContainer::reset(S2Region* ptr, Type tt) noexcept {
  delete _data;
  _type = tt;
  _data = ptr;
}

void ShapeContainer::resetCoordinates(S2LatLng ll) {
  if (_type != Type::S2_POINT || !_data) {
    delete _data;
    _type = ShapeContainer::Type::S2_POINT;
    _data = new S2PointRegion(ll.ToPoint());
  } else {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto& region = dynamic_cast<S2PointRegion&>(*_data);
#else
    auto& region = static_cast<S2PointRegion&>(*_data);
#endif
    region = S2PointRegion(ll.ToPoint());
  }
}

S2Point ShapeContainer::centroid() const noexcept {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      return (static_cast<S2PointRegion const*>(_data))->point();
    }
    case ShapeContainer::Type::S2_POLYLINE: {
      return (static_cast<S2Polyline const*>(_data))->GetCentroid().Normalize();
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      return static_cast<S2LatLngRect const*>(_data)->GetCenter().ToPoint();
    }
    case ShapeContainer::Type::S2_POLYGON: {
      // not unit length by default
      return (static_cast<S2Polygon const*>(_data))->GetCentroid().Normalize();
    }
    case ShapeContainer::Type::S2_MULTIPOINT: {
      S2MultiPointRegion const* pts = (static_cast<S2MultiPointRegion const*>(_data));
      S2LatLng c = S2LatLng::FromDegrees(0.0, 0.0);
      for (int k = 0; k < pts->num_points(); k++) {
        c = c + ((1 / static_cast<double>(pts->num_points())) * S2LatLng(pts->point(k)));
      }
      return c.ToPoint().Normalize();  // FIXME probably broken
    }
    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      S2MultiPolyline const* lines = (static_cast<S2MultiPolyline const*>(_data));
      S2LatLng c = S2LatLng::FromDegrees(0.0, 0.0);
      double totalWeight = 0.0;
      for (size_t k = 0; k < lines->num_lines(); k++) {
        totalWeight += lines->line(k).GetLength().radians();
      }
      for (size_t k = 0; k < lines->num_lines(); k++) {
        double weight = (lines->line(k).GetLength().radians() / totalWeight);
        S2LatLng point(lines->line(k).GetCentroid());
        S2LatLng weightedPoint = weight * point;
        c = c + weightedPoint;
      }
      return c.ToPoint().Normalize();
    }

    case ShapeContainer::Type::EMPTY:
      TRI_ASSERT(false);
  }
  return S2Point();
}

std::vector<S2CellId> ShapeContainer::covering(S2RegionCoverer* coverer) const noexcept {
  TRI_ASSERT(coverer != nullptr && _data != nullptr);

  std::vector<S2CellId> cover;
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& c = (static_cast<S2PointRegion const*>(_data))->point();
      return {S2CellId(c)};
    }
    case ShapeContainer::Type::S2_POLYLINE:
    case ShapeContainer::Type::S2_LATLNGRECT:
    case ShapeContainer::Type::S2_POLYGON: {
      coverer->GetCovering(*_data, &cover);
      break;
    }
    case ShapeContainer::Type::S2_MULTIPOINT: {  // multi-optimization
      S2MultiPointRegion const* pts = (static_cast<S2MultiPointRegion const*>(_data));
      for (int k = 0; k < pts->num_points(); k++) {
        cover.emplace_back(S2CellId(pts->point(k)));
      }
      break;
    }
    case ShapeContainer::Type::S2_MULTIPOLYLINE: {  // multi-optimization
      S2MultiPolyline const* lines = (static_cast<S2MultiPolyline const*>(_data));
      for (size_t k = 0; k < lines->num_lines(); k++) {
        std::vector<S2CellId> tmp;
        coverer->GetCovering(*lines, &tmp);
        if (!tmp.empty()) {
          cover.reserve(cover.size() + tmp.size());
          cover.insert(cover.end(), tmp.begin(), tmp.end());
        }
      }
      break;
    }

    case ShapeContainer::Type::EMPTY:
      LOG_TOPIC("8f601", ERR, Logger::FIXME) << "Invalid GeoShape usage";
      TRI_ASSERT(false);
  }

  return cover;
}

double ShapeContainer::distanceFromCentroid(S2Point const& other) const noexcept {
  return centroid().Angle(other) * geo::kEarthRadiusInMeters;
}

double ShapeContainer::distanceFromCentroid(S2Point const& other,
                                            Ellipsoid const& e) const noexcept {
  return geo::utils::geodesicDistance(S2LatLng(centroid()), S2LatLng(other), e);
}

/// @brief may intersect cell
bool ShapeContainer::mayIntersect(S2CellId cell) const noexcept {
  TRI_ASSERT(_data != nullptr);
  return _data->MayIntersect(S2Cell(cell));
}

/// @brief adjust query parameters (specifically max distance)
void ShapeContainer::updateBounds(QueryParams& qp) const noexcept {
  TRI_ASSERT(_data != nullptr);
  if (_data == nullptr) {
    return;
  }

  S2LatLng ll(this->centroid());
  qp.origin = ll;

  S2LatLngRect rect = _data->GetRectBound();
  if (!rect.is_empty() && !rect.is_point()) {
    S1Angle a1(ll, rect.lo());
    S1Angle a2(ll, S2LatLng(rect.lat_lo(), rect.lng_hi()));
    S1Angle a3(ll, S2LatLng(rect.lat_hi(), rect.lng_lo()));
    S1Angle a4(ll, rect.hi());

    double rad = geo::kRadEps + std::max(std::max(a1.radians(), a2.radians()),
                                         std::max(a3.radians(), a4.radians()));
    qp.maxDistance = rad * kEarthRadiusInMeters;
  } else {
    qp.maxDistance = 0;
  }
}

bool ShapeContainer::contains(S2Point const& pp) const {
  if (_type == ShapeContainer::Type::EMPTY) {
    return false;
  }
  return _data->Contains(pp);
}

bool ShapeContainer::contains(S2Polyline const* other) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT:
    case ShapeContainer::Type::S2_MULTIPOINT: {
      return false;  // rect.is_point() && rect.lo() == S2LatLng
    }

    case ShapeContainer::Type::S2_POLYLINE: {
      S2Polyline const* ll = static_cast<S2Polyline const*>(_data);
      return ll->ApproxEquals(*other, S1Angle::Radians(1e-6));
    }

    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLngRect const* rect = static_cast<S2LatLngRect const*>(_data);
      for (int k = 0; k < other->num_vertices(); k++) {
        if (!rect->Contains(other->vertex(k))) {
          return false;
        }
      }
      return true;
    }

    case ShapeContainer::Type::S2_POLYGON: {
      // FIXME this seems to be incomplete
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      auto cuts = poly->IntersectWithPolyline(*other);
      if (cuts.size() != 1) {
        return false;  // is clipping holes or no edge
      }
      // The line may be in the polygon
      return cuts[0]->NearlyCovers(*other, S1Angle::Degrees(1e-10));
    }

    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      S2MultiPolyline const* mpl = static_cast<S2MultiPolyline const*>(_data);
      for (size_t k = 0; k < mpl->num_lines(); k++) {
        if (mpl->line(k).ApproxEquals(*other, S1Angle::Degrees(1e-6))) {
          return true;
        }
      }
      return false;
    }

    case ShapeContainer::Type::EMPTY:
      TRI_ASSERT(false);
  }
  return false;
}

bool ShapeContainer::contains(S2LatLngRect const* other) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT:
      if (other->is_point()) {
        return static_cast<S2PointRegion*>(_data)->point() == other->lo().ToPoint();
      }
      return false;

    case ShapeContainer::Type::S2_POLYLINE: {
      if (other->is_point()) {
        S2Point pp = other->lo().ToPoint();
        S2Polyline* pl = static_cast<S2Polyline*>(_data);
        for (int k = 0; k < pl->num_vertices(); k++) {
          if (pl->vertex(k) == pp) {
            return true;
          }
        }
      }
      return false;
    }

    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLngRect const* self = static_cast<S2LatLngRect const*>(_data);
      return self->Contains(*other);
    }

    case ShapeContainer::Type::S2_POLYGON: {
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      for (int k = 0; k < 4; k++) {
        if (!poly->Contains(other->GetVertex(k).ToPoint())) {
          return false;
        }
      }
      return true;
    }

    case ShapeContainer::Type::S2_MULTIPOINT: {
      if (other->is_point()) {
        S2Point pp = other->lo().ToPoint();
        S2MultiPointRegion* mpr = static_cast<S2MultiPointRegion*>(_data);
        for (int k = 0; mpr->num_points() < k; k++) {
          if (mpr->point(k) == pp) {
            return true;
          }
        }
      }
      return false;
    }

    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      if (other->is_point()) {
        S2Point pp = other->lo().ToPoint();
        S2MultiPolyline* mpl = static_cast<S2MultiPolyline*>(_data);
        for (size_t k = 0; k < mpl->num_lines(); k++) {
          S2Polyline const& pl = mpl->line(k);
          for (int n = 0; n < pl.num_vertices(); n++) {
            if (pl.vertex(n) == pp) {
              return true;
            }
          }
        }
      }
      return false;
    }

    case ShapeContainer::Type::EMPTY:
      TRI_ASSERT(false);
  }
  return false;
}

bool ShapeContainer::contains(S2Polygon const* poly) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT:
    case ShapeContainer::Type::S2_MULTIPOINT: {
      return false;  // rect.is_point() && rect.lo() == S2LatLng
    }
    case ShapeContainer::Type::S2_POLYLINE:
    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      return false;  // numerically not well defined
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLngRect const* rect = static_cast<S2LatLngRect const*>(_data);
      for (int k = 0; k < poly->num_loops(); k++) {
        S2Loop const* loop = poly->loop(k);
        for (int n = 0; n < loop->num_vertices(); n++) {
          if (!rect->Contains(loop->vertex(n))) {
            return false;
          }
        }
      }
      return true;
    }
    case ShapeContainer::Type::S2_POLYGON: {
      return (static_cast<S2Polygon const*>(_data))->Contains(poly);
    }
    case ShapeContainer::Type::EMPTY:
      TRI_ASSERT(false);
  }
  return false;
}

bool ShapeContainer::contains(ShapeContainer const* cc) const {
  switch (cc->_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& p = static_cast<S2PointRegion*>(cc->_data)->point();
      return _data->Contains(p);
    }
    case ShapeContainer::Type::S2_POLYLINE: {
      return contains(static_cast<S2Polyline const*>(cc->_data));
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      return contains(static_cast<S2LatLngRect const*>(cc->_data));
    }
    case ShapeContainer::Type::S2_POLYGON: {
      return contains(static_cast<S2Polygon const*>(cc->_data));
    }
    case ShapeContainer::Type::S2_MULTIPOINT: {
      auto pts = static_cast<S2MultiPointRegion const*>(cc->_data);
      for (int k = 0; k < pts->num_points(); k++) {
        if (!_data->Contains(pts->point(k))) {
          return false;
        }
      }
      return true;
    }
    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      auto lines = static_cast<S2MultiPolyline const*>(cc->_data);
      for (size_t k = 0; k < lines->num_lines(); k++) {
        if (!this->contains(&(lines->line(k)))) {
          return false;
        }
      }
      return true;
    }

    case ShapeContainer::Type::EMPTY:
      TRI_ASSERT(false);
  }
  return false;
}

bool ShapeContainer::equals(Coordinate const* cc) const {
  S2Point const& p = static_cast<S2PointRegion*>(_data)->point();
  double lat1 = S2LatLng::Latitude(p).degrees();
  double lon1 = S2LatLng::Longitude(p).degrees();

  double lat2 = cc->latitude;
  double lon2 = cc->longitude;

  if (lat1 == lat2 && lon1 == lon2) {
    return true;
  }

  return false;
}

bool ShapeContainer::equals(Coordinate const& point, Coordinate const& other) const {
  if (point.latitude == other.latitude && point.longitude == other.longitude) {
    return true;
  }

  return false;
}

bool ShapeContainer::equals(double lat2, double lon2) const {
  S2Point const& p = static_cast<S2PointRegion*>(_data)->point();
  double lat1 = S2LatLng::Latitude(p).degrees();
  double lon1 = S2LatLng::Longitude(p).degrees();

  if (lat1 == lat2 && lon1 == lon2) {
    return true;
  }

  return false;
}

bool ShapeContainer::equals(S2Polyline const* other) const {
  S2Polyline const* ll = static_cast<S2Polyline const*>(_data);
  return ll->Equals(other);
}

bool ShapeContainer::equals(S2Polyline const* poly, S2Polyline const* other) const {
  return poly->Equals(other);
}

bool ShapeContainer::equals(S2LatLngRect const* other) const {
  S2LatLngRect const* llrect = static_cast<S2LatLngRect const*>(_data);
  return llrect->ApproxEquals(*other);
}

bool ShapeContainer::equals(S2Polygon const* other) const {
  S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
  return poly->Equals(other);
}

bool ShapeContainer::equals(ShapeContainer const* cc) const {
  // equals function will only trigger for equal types
  // if types are not equal, result is false
  if (cc->_type != _type) {
    return false;
  }

  switch (cc->_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& p = static_cast<S2PointRegion*>(cc->_data)->point();
      return equals(S2LatLng::Latitude(p).degrees(), S2LatLng::Longitude(p).degrees());
    }
    case ShapeContainer::Type::S2_POLYLINE: {
      return equals(static_cast<S2Polyline const*>(cc->_data));
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      return equals(static_cast<S2LatLngRect const*>(cc->_data));
    }
    case ShapeContainer::Type::S2_POLYGON: {
      return equals(static_cast<S2Polygon const*>(cc->_data));
    }
    case ShapeContainer::Type::S2_MULTIPOINT: {
      auto pts1 = static_cast<S2MultiPointRegion const*>(_data);
      auto pts2 = static_cast<S2MultiPointRegion const*>(cc->_data);

      if (pts1->num_points() != pts2->num_points()) {
        return false;
      }

      for (int k = 0; k < pts1->num_points(); k++) {
        if (!equals(Coordinate(S2LatLng::Latitude(pts1->point(k)).degrees(),
                               S2LatLng::Longitude(pts1->point(k)).degrees()),
                    Coordinate(S2LatLng::Latitude(pts2->point(k)).degrees(),
                               S2LatLng::Longitude(pts2->point(k)).degrees()))) {
          return false;
        }
      }
      return true;
    }
    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      auto lines1 = static_cast<S2MultiPolyline const*>(_data);
      auto lines2 = static_cast<S2MultiPolyline const*>(cc->_data);

      if (lines1->num_lines() != lines2->num_lines()) {
        return false;
      }

      for (size_t k = 0; k < lines2->num_lines(); k++) {
        if (!this->equals(&(lines1->line(k)), &(lines2->line(k)))) {
          return false;
        }
      }
      return true;
    }

    case ShapeContainer::Type::EMPTY:
      TRI_ASSERT(false);
  }
  return false;
}

bool ShapeContainer::intersects(S2Polyline const* other) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& p = static_cast<S2PointRegion*>(_data)->point();
      // containment is only numerically defined on the endpoints
      for (int k = 0; k < other->num_vertices(); k++) {
        if (other->vertex(k) == p) {
          return true;
        }
      }
      return false;
    }
    case ShapeContainer::Type::S2_POLYLINE: {
      S2Polyline const* ll = static_cast<S2Polyline const*>(_data);
      return ll->Intersects(other);
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLngRect const* rect = static_cast<S2LatLngRect const*>(_data);
      for (int k = 0; k < other->num_vertices(); k++) {
        if (rect->Contains(other->vertex(k))) {
          return true;
        }
      }
      return false;
    }
    case ShapeContainer::Type::S2_POLYGON: {
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      auto cuts = poly->IntersectWithPolyline(*other);
      return !cuts.empty();
    }
    case ShapeContainer::Type::S2_MULTIPOINT:
    case ShapeContainer::Type::S2_MULTIPOLYLINE:
    case ShapeContainer::Type::EMPTY:
      TRI_ASSERT(false);
  }
  return false;
}

bool ShapeContainer::intersects(S2LatLngRect const* other) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT:
    case ShapeContainer::Type::S2_POLYLINE:
      return contains(other);  // same

    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLngRect const* self = static_cast<S2LatLngRect const*>(_data);
      return self->Intersects(*other);
    }

    case ShapeContainer::Type::S2_POLYGON: {
      S2Polygon const* self = static_cast<S2Polygon const*>(_data);
      if (other->is_full()) {
        return true;  // rectangle spans entire sphere
      } else if (other->is_point()) {
        return self->Contains(other->lo().ToPoint());  // easy case
      } else if (!other->Intersects(self->GetRectBound())) {
        return false;  // cheap rejection
      }
      // construct bounding polyline of rect
      S2Polyline rectBound({other->GetVertex(0), other->GetVertex(1),
                            other->GetVertex(2), other->GetVertex(3)});
      return self->Intersects(rectBound);
    }

    case ShapeContainer::Type::S2_MULTIPOINT:
    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      return contains(other);  // same
    }

    case ShapeContainer::Type::EMPTY:
      TRI_ASSERT(false);
  }
  return false;
}

bool ShapeContainer::intersects(S2Polygon const* other) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& p = static_cast<S2PointRegion*>(_data)->point();
      return other->Contains(p);
    }
    case ShapeContainer::Type::S2_POLYLINE: {
      LOG_TOPIC("2cb3c", ERR, Logger::FIXME)
          << "intersection with polyline is not well defined";
      return false;  // numerically not well defined
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLngRect const* self = static_cast<S2LatLngRect const*>(_data);
      if (self->is_full()) {
        return true;  // rectangle spans entire sphere
      } else if (self->is_point()) {
        return other->Contains(self->lo().ToPoint());  // easy case
      } else if (!self->Intersects(other->GetRectBound())) {
        return false;  // cheap rejection
      }
      // construct bounding polyline of rect
      S2Polyline rectBound({self->GetVertex(0), self->GetVertex(1),
                            self->GetVertex(2), self->GetVertex(3)});
      return other->Intersects(rectBound);
    }
    case ShapeContainer::Type::S2_POLYGON: {
      S2Polygon const* self = static_cast<S2Polygon const*>(_data);
      return self->Intersects(other);
    }
    case ShapeContainer::Type::EMPTY:
    case ShapeContainer::Type::S2_MULTIPOINT:
    case ShapeContainer::Type::S2_MULTIPOLYLINE:
      TRI_ASSERT(false);
  }
  return false;
}

bool ShapeContainer::intersects(ShapeContainer const* cc) const {
  switch (cc->_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& p = static_cast<S2PointRegion*>(cc->_data)->point();
      return _data->Contains(p);  // same
    }
    case ShapeContainer::Type::S2_POLYLINE: {
      return intersects(static_cast<S2Polyline const*>(cc->_data));
    }
    case ShapeContainer::Type::S2_POLYGON: {
      return intersects(static_cast<S2Polygon const*>(cc->_data));
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      return intersects(static_cast<S2LatLngRect const*>(cc->_data));
    }
    case ShapeContainer::Type::S2_MULTIPOINT: {
      auto pts = static_cast<S2MultiPointRegion const*>(cc->_data);
      for (int k = 0; k < pts->num_points(); k++) {
        if (_data->Contains(pts->point(k))) {
          return true;
        }
      }
      return false;
    }
    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      auto lines = static_cast<S2MultiPolyline const*>(cc->_data);
      for (size_t k = 0; k < lines->num_lines(); k++) {
        if (this->intersects(&(lines->line(k)))) {
          return true;
        }
      }
      return false;
    }
    case ShapeContainer::Type::EMPTY:
      TRI_ASSERT(false);
  }
  return false;
}

/// @brief calculate area of polygon or multipolygon
double ShapeContainer::area(geo::Ellipsoid const& e) {
  if (!isAreaType()) {
    return 0.0;
  }

  // TODO: perhaps remove in favor of one code-path below ?
  if (e.flattening() == 0.0) {
    switch (_type) {
      case Type::S2_LATLNGRECT:
        return static_cast<S2LatLngRect*>(_data)->Area() * kEarthRadiusInMeters * kEarthRadiusInMeters;
      case Type::S2_POLYGON:
        return static_cast<S2Polygon*>(_data)->GetArea() * kEarthRadiusInMeters * kEarthRadiusInMeters;
      default:
        TRI_ASSERT(false);
        return 0.0;
    }
  }

  double area = 0.0;
  struct geod_geodesic g;
  geod_init(&g, e.equator_radius(), e.flattening());
  double A = 0.0;
  double P = 0.0;

  switch (_type) {
    case Type::S2_LATLNGRECT: {
      struct geod_polygon p;
      geod_polygon_init(&p, 0);

      S2LatLngRect const* rect = static_cast<S2LatLngRect*>(_data);
      geod_polygon_addpoint(&g, &p, rect->lat_lo().degrees(), rect->lng_lo().degrees());
      geod_polygon_addpoint(&g, &p, rect->lat_lo().degrees(), rect->lng_hi().degrees());
      geod_polygon_addpoint(&g, &p, rect->lat_hi().degrees(), rect->lng_hi().degrees());
      geod_polygon_addpoint(&g, &p, rect->lat_hi().degrees(), rect->lng_lo().degrees());

      geod_polygon_compute(&g, &p, 0, 1, &A, &P);
      area = A;
    } break;

    case Type::S2_POLYGON: {
      struct geod_polygon p;

      S2Polygon const* poly = static_cast<S2Polygon*>(_data);
      for (int k = 0; k < poly->num_loops(); k++) {
        geod_polygon_init(&p, 0);

        S2Loop const* loop = poly->loop(k);
        for (int n = 0; n < loop->num_vertices(); n++) {
          S2LatLng latLng(loop->vertex(n));
          geod_polygon_addpoint(&g, &p, latLng.lat().degrees(), latLng.lng().degrees());
        }

        geod_polygon_compute(&g, &p, /*reverse*/ false, /*sign*/ true, &A, &P);
        area += A;
      }
    } break;

    default:
      TRI_ASSERT(false);
      return 0;
  }

  return area;
}
