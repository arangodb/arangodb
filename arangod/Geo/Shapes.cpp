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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Shapes.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Geo/GeoJsonParser.h"
#include "Geo/GeoParams.h"
#include "Logger/Logger.h"

#include <geometry/s2.h>
#include <geometry/s2cap.h>
#include <geometry/s2cell.h>
#include <geometry/s2latlngrect.h>
#include <geometry/s2multipointregion.h>
#include <geometry/s2multipolyline.h>
#include <geometry/s2pointregion.h>
#include <geometry/s2polygon.h>
#include <geometry/s2region.h>
#include <geometry/s2regioncoverer.h>

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::geo;

Coordinate Coordinate::fromLatLng(S2LatLng const& ll) {
  return Coordinate(ll.lat().degrees(), ll.lng().degrees());
}

Result ShapeContainer::parseCoordinates(VPackSlice const& json, bool geoJson) {
  TRI_ASSERT(_data == nullptr);
  if (!json.isArray() || json.length() < 2) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid coordinate pair");
  }

  VPackSlice lat = json.at(geoJson ? 1 : 0);
  VPackSlice lng = json.at(geoJson ? 0 : 1);
  if (!lat.isNumber() || !lng.isNumber()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid coordinate pair");
  }
  
  resetCoordinates(lat.getNumericValue<double>(),
                   lng.getNumericValue<double>());
  return TRI_ERROR_NO_ERROR;
}

ShapeContainer::ShapeContainer(ShapeContainer&& other) noexcept
    : _data(other._data), _type(other._type) {
  other._data = nullptr;
  other._type = ShapeContainer::Type::EMPTY;
}
ShapeContainer::~ShapeContainer() { delete _data; }

void ShapeContainer::reset(std::unique_ptr<S2Region>&& ptr, Type tt) noexcept {
  delete _data;
  _type = tt;
  _data = ptr.get();
  ptr.release();
}

void ShapeContainer::reset(S2Region* ptr, Type tt) noexcept {
  delete _data;
  _type = tt;
  _data = ptr;
}

void ShapeContainer::resetCoordinates(double lat, double lon) noexcept {
  delete _data;
  _type = ShapeContainer::Type::S2_POINT;
  _data = new S2PointRegion(S2LatLng::FromDegrees(lat, lon).ToPoint());
}

bool ShapeContainer::isAreaEmpty() const noexcept {
  switch (_type) {
    case ShapeContainer::Type::S2_POLYLINE:
    case ShapeContainer::Type::S2_POINT: {
      return true;
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      return (static_cast<S2LatLngRect const*>(_data))->is_empty();
    }
    case ShapeContainer::Type::S2_CAP: {
      return (static_cast<S2Cap const*>(_data))->is_empty();
    }

    case ShapeContainer::Type::S2_POLYGON: {
      return (static_cast<S2Polygon const*>(_data))->GetArea() > 0;
    }

    default:
      LOG_TOPIC(ERR, Logger::FIXME) << "Invalid GeoShape usage";
      return true;
  }
}

geo::Coordinate ShapeContainer::centroid() const noexcept {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& c = (static_cast<S2PointRegion const*>(_data))->point();
      return Coordinate(S2LatLng::Latitude(c).degrees(),
                        S2LatLng::Longitude(c).degrees());
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLng latLng = (static_cast<S2LatLngRect const*>(_data))->GetCenter();
      return Coordinate::fromLatLng(latLng);
    }
    case ShapeContainer::Type::S2_CAP: {
      S2Point const& c = (static_cast<S2Cap const*>(_data))->axis();
      return Coordinate(S2LatLng::Latitude(c).degrees(),
                        S2LatLng::Longitude(c).degrees());
    }
    case ShapeContainer::Type::S2_POLYLINE: {
      S2Point const& c = (static_cast<S2Polyline const*>(_data))->GetCentroid();
      return Coordinate(S2LatLng::Latitude(c).degrees(),
                        S2LatLng::Longitude(c).degrees());
    }
    case ShapeContainer::Type::S2_POLYGON: {
      S2Point c = (static_cast<S2Polygon const*>(_data))->GetCentroid();
      return Coordinate(S2LatLng::Latitude(c).degrees(),
                        S2LatLng::Longitude(c).degrees());
    }
    case ShapeContainer::Type::S2_MULTIPOINT: {
      S2MultiPointRegion const* pts = (static_cast<S2MultiPointRegion const*>(_data));
      S2Point c(0,0,0);
      for (int k = 0; k < pts->num_points(); k++) {
        c += pts->point(k);
      }
      c /= pts->num_points();
      return Coordinate(S2LatLng::Latitude(c).degrees(),
                        S2LatLng::Longitude(c).degrees());
    }
    case ShapeContainer::Type::S2_MULTIPOLYLINE:{
      S2MultiPolyline const* lines = (static_cast<S2MultiPolyline const*>(_data));
      S2Point c(0,0,0);
      for (int k = 0; k < lines->num_lines(); k++) {
        c += lines->line(k).GetCentroid();
      }
      c /= lines->num_lines();
      return Coordinate(S2LatLng::Latitude(c).degrees(),
                        S2LatLng::Longitude(c).degrees());
    }
      
    case ShapeContainer::Type::EMPTY:
      LOG_TOPIC(ERR, Logger::FIXME) << "Invalid GeoShape usage";
      return geo::Coordinate::Invalid();
  }
}

std::vector<S2CellId> ShapeContainer::covering(S2RegionCoverer* coverer) const noexcept {
  TRI_ASSERT(coverer != nullptr && _data != nullptr);
  
  std::vector<S2CellId> cover;
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& c = (static_cast<S2PointRegion const*>(_data))->point();
      return {S2CellId::FromPoint(c)};
    }
    case ShapeContainer::Type::S2_LATLNGRECT:
    case ShapeContainer::Type::S2_CAP:
    case ShapeContainer::Type::S2_POLYLINE:
    case ShapeContainer::Type::S2_POLYGON: {
      coverer->GetCovering(*_data, &cover);
      break;
    }
    case ShapeContainer::Type::S2_MULTIPOINT: { // multi-optimization
      S2MultiPointRegion const* pts = (static_cast<S2MultiPointRegion const*>(_data));
      for (int k = 0; k < pts->num_points(); k++) {
        cover.emplace_back(S2CellId::FromPoint(pts->point(k)));
      }
      break;
    }
    case ShapeContainer::Type::S2_MULTIPOLYLINE: { // multi-optimization
      S2MultiPolyline const* lines = (static_cast<S2MultiPolyline const*>(_data));
      for (int k = 0; k < lines->num_lines(); k++) {
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
      LOG_TOPIC(ERR, Logger::FIXME) << "Invalid GeoShape usage";
      TRI_ASSERT(false);
  }
  
  return cover;
}

double ShapeContainer::distanceFrom(geo::Coordinate const& other) const noexcept {
  geo::Coordinate centroid = this->centroid();
  double p1 = centroid.latitude * (M_PI / 180.0);
  double p2 = other.latitude * (M_PI / 180.0);
  double d1 = (other.latitude - centroid.latitude) * (M_PI / 180.0);
  double d2 = (other.longitude - centroid.longitude) * (M_PI / 180.0);
  double a =
      std::sin(d1 / 2.0) * std::sin(d1 / 2.0) +
      std::cos(p1) * std::cos(p2) * std::sin(d2 / 2.0) * std::sin(d2 / 2.0);
  double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  return c * geo::kEarthRadiusInMeters;
}

/// @brief may intersect cell
bool ShapeContainer::mayIntersect(S2CellId cell) const noexcept  {
  TRI_ASSERT(_data != nullptr);
  return _data->MayIntersect(S2Cell(cell));
}

void ShapeContainer::updateBounds(QueryParams& qp) const noexcept {
  TRI_ASSERT(_data != nullptr);
  if (_data == nullptr) {
    return;
  }
  S2LatLngRect rect = _data->GetRectBound();
  geo::Coordinate orig = centroid();
  S2LatLng ll = S2LatLng::FromDegrees(orig.latitude, orig.longitude);
  S1Angle a1(ll, rect.lo());
  S1Angle a2(ll, S2LatLng(rect.lat_lo(), rect.lng_hi()));
  S1Angle a3(ll, S2LatLng(rect.lat_hi(), rect.lng_lo()));
  S1Angle a4(ll, rect.hi());

  qp.origin = orig;
  qp.maxDistance = std::max(std::max(a1.radians(), a2.radians()),
                            std::max(a3.radians(), a4.radians())) * kEarthRadiusInMeters;
}

bool ShapeContainer::contains(Coordinate const* cc) const {
  S2Point pp = S2LatLng::FromDegrees(cc->latitude, cc->longitude).ToPoint();
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      return (static_cast<S2PointRegion const*>(_data))->Contains(pp);
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      return (static_cast<S2LatLngRect const*>(_data))->Contains(pp);
    }
    case ShapeContainer::Type::S2_CAP: {
      return (static_cast<S2Cap const*>(_data))->Contains(pp);
    }
      
    case ShapeContainer::Type::S2_POLYLINE: {
      // containment is only numerically defined on the endpoints
      S2Polyline const* ll = static_cast<S2Polyline const*>(_data);
      for (int k = 0; k < ll->num_vertices(); k++) {
        if (ll->vertex(k) == pp) {
          return true;
        }
      }
      return false;
    }
      
    case ShapeContainer::Type::S2_POLYGON: {
      return (static_cast<S2Polygon const*>(_data))->Contains(pp);
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}

bool ShapeContainer::contains(S2Polyline const* otherLine) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      return false;  // rect.is_point() && rect.lo() == S2LatLng
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLngRect const* rect = static_cast<S2LatLngRect const*>(_data);
      return rect->Contains(*otherLine);
    }
    case ShapeContainer::Type::S2_CAP: {
      // FIXME: why do we need the complement here at all ?
      S2Cap cmp = (static_cast<S2Cap const*>(_data))->Complement();
      // calculate angle between complement cap center and closest point on
      // polygon
      int ignored;
      S1Angle angle(cmp.axis(), otherLine->Project(cmp.axis(), &ignored));
      // if angle is bigger than complement cap angle, than the polyline is
      // contained
      // we need to use the complement otherwise we would test for intersection
      return angle.radians() >= cmp.angle().radians();
    }

    case ShapeContainer::Type::S2_POLYLINE: {
      S2Polyline const* ll = static_cast<S2Polyline const*>(_data);
      return ll->ApproxEquals(otherLine, 1e-8);
    }

    case ShapeContainer::Type::S2_POLYGON: {
      // FIXME this seems to be incomplete
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      std::vector<S2Polyline*> cut;
      TRI_DEFER(for (S2Polyline* pp : cut) { delete pp; });
      poly->IntersectWithPolyline(otherLine, &cut);
      if (cut.size() != 1) {
        return false;  // is clipping holes or no edg
      }

      // The line may be in the polygon
      return cut[0]->NearlyCoversPolyline(*otherLine, S1Angle::Degrees(1e-10));
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}

bool ShapeContainer::contains(S2Polygon const* poly) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      return false;  // rect.is_point() && rect.lo() == S2LatLng
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      // works for exact bounds, which GetRectBound() guarantees
      return (static_cast<S2LatLngRect const*>(_data))
          ->Contains(poly->GetRectBound());
    }
    case ShapeContainer::Type::S2_CAP: {
      // FIXME: why do we need the complement here at all ?
      S2Cap cmp = (static_cast<S2Cap const*>(_data))->Complement();
      // calculate angle between complement cap center and closest point on
      // polygon
      S1Angle angle(cmp.axis(), poly->Project(cmp.axis()));
      // if angle is bigger than complement cap angle, than the polyline is
      // contained
      // we need to use the complement otherwise we would test for intersection
      return angle.radians() >= cmp.angle().radians();
    }

    case ShapeContainer::Type::S2_POLYLINE: {
      return false;  // numerically not well defined
    }

    case ShapeContainer::Type::S2_POLYGON: {
      return (static_cast<S2Polygon const*>(_data))->Contains(poly);
    }

    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}

bool ShapeContainer::contains(ShapeContainer const* cc) const {
  switch (cc->_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& p = static_cast<S2PointRegion*>(cc->_data)->point();
      return _data->VirtualContainsPoint(p);
    }
    case ShapeContainer::Type::S2_LATLNGRECT:
    case ShapeContainer::Type::S2_CAP: {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }

    case ShapeContainer::Type::S2_POLYLINE: {
      return contains(static_cast<S2Polyline const*>(cc->_data));
    }

    case ShapeContainer::Type::S2_POLYGON: {
      return contains(static_cast<S2Polygon const*>(cc->_data));
    }
      
    case ShapeContainer::Type::S2_MULTIPOINT: {
      auto pts = static_cast<S2MultiPointRegion const*>(cc->_data);
      for (int k = 0; k < pts->num_points(); k++) {
        if (!_data->VirtualContainsPoint(pts->point(k))) {
          return false;
        }
      }
      return true;
    }
      
    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      auto lines = static_cast<S2MultiPolyline const*>(cc->_data);
      for (int k = 0; k < lines->num_lines(); k++) {
        if (!this->contains(&(lines->line(k)))) {
          return false;
        }
      }
      return true;
    }
      
    case ShapeContainer::Type::EMPTY:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid ShapeContainer");
  }
}

bool ShapeContainer::intersects(geo::Coordinate const* cc) const {
  return contains(cc);  // same
}

bool ShapeContainer::intersects(S2Polyline const* otherLine) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      return false;  // rect.is_point() && rect.lo() == S2LatLng
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLngRect const* rect = static_cast<S2LatLngRect const*>(_data);
      return rect->Intersects(*otherLine);
    }
    case ShapeContainer::Type::S2_CAP: {
      S2Cap const* cap = static_cast<S2Cap const*>(_data);
      // calculate angle between cap center and closest point on polygon
      int ignored;
      S1Angle angle(cap->axis(), otherLine->Project(cap->axis(), &ignored));
      return angle.radians() <= cap->angle().radians();
    }

    case ShapeContainer::Type::S2_POLYLINE: {
      S2Polyline const* ll = static_cast<S2Polyline const*>(_data);
      return ll->Intersects(otherLine);
    }

    case ShapeContainer::Type::S2_POLYGON: {
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      std::vector<S2Polyline*> cut;
      TRI_DEFER(for (S2Polyline* pp : cut) { delete pp; });
      poly->IntersectWithPolyline(otherLine, &cut);
      return cut.size() > 0;
    }
      
    case ShapeContainer::Type::EMPTY:
    case ShapeContainer::Type::S2_MULTIPOINT:
    case ShapeContainer::Type::S2_MULTIPOLYLINE:
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}

bool ShapeContainer::intersects(S2Polygon const* otherPoly) const {
  switch (_type) {
    case ShapeContainer::Type::S2_POINT: {
      return false;  // rect.is_point() && rect.lo() == S2LatLng
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      S2LatLngRect const* rect = static_cast<S2LatLngRect const*>(_data);
      for (int k = 0; k < otherPoly->num_loops(); k++) {
        S2Loop* loop = otherPoly->loop(k);
        for (int v = 0; v < loop->num_vertices(); v++) {
          if (rect->Contains(loop->vertex(v))) {
            return true;
          }
        }
      }
      return false;
    }
    case ShapeContainer::Type::S2_CAP: {
      S2Cap const* cap = static_cast<S2Cap const*>(_data);
      // calculate angle between cap center and closest point on polygon
      S1Angle angle(cap->axis(), otherPoly->Project(cap->axis()));
      return angle.radians() <= cap->angle().radians();
    }

    case ShapeContainer::Type::S2_POLYLINE: {
      LOG_TOPIC(ERR, Logger::FIXME) << "intersection with polyline is not well defined";
      return false;  // numerically not well defined
    }

    case ShapeContainer::Type::S2_POLYGON: {
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      return poly->Intersects(otherPoly);
    }

    case ShapeContainer::Type::EMPTY:
    case ShapeContainer::Type::S2_MULTIPOINT:
    case ShapeContainer::Type::S2_MULTIPOLYLINE:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}

bool ShapeContainer::intersects(ShapeContainer const* cc) const {
  switch (cc->_type) {
    case ShapeContainer::Type::S2_POINT: {
      S2Point const& p = static_cast<S2PointRegion*>(cc->_data)->point();
      return _data->VirtualContainsPoint(p); // same
    }
    case ShapeContainer::Type::S2_LATLNGRECT: {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
    case ShapeContainer::Type::S2_CAP: {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }

    case ShapeContainer::Type::S2_POLYLINE: {
      return intersects(static_cast<S2Polyline const*>(cc->_data));
    }

    case ShapeContainer::Type::S2_POLYGON: {
      return intersects(static_cast<S2Polygon const*>(cc->_data));
    }

    case ShapeContainer::Type::S2_MULTIPOINT: {
      auto pts = static_cast<S2MultiPointRegion const*>(cc->_data);
      for (int k = 0; k < pts->num_points(); k++) {
        if (_data->VirtualContainsPoint(pts->point(k))) {
          return true;
        }
      }
      return false;
    }
      
    case ShapeContainer::Type::S2_MULTIPOLYLINE: {
      auto lines = static_cast<S2MultiPolyline const*>(cc->_data);
      for (int k = 0; k < lines->num_lines(); k++) {
        if (this->intersects(&(lines->line(k)))) {
          return true;
        }
      }
      return false;
    }
      
    case ShapeContainer::Type::EMPTY:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid container");
      break;
  }
}
