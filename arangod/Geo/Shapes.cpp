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

#include <geometry/s2.h>
#include <geometry/s2cap.h>
#include <geometry/s2pointregion.h>
#include <geometry/s2region.h>
#include <geometry/s2polygon.h>
#include <geometry/s2latlngrect.h>

using namespace arangodb::geo;

GeometryContainer::~GeometryContainer() {
  delete _data;
}
    
bool GeometryContainer::contains(Coordinate const* cc) const {
  S2Point pp = S2LatLng::FromDegrees(cc->latitude, cc->longitude).ToPoint();
  switch (_type) {
    case GeometryContainer::Type::S2_POINT:{
      return (static_cast<S2PointRegion const*>(_data))->Contains(pp);
    }
    case GeometryContainer::Type::S2_LATLNGRECT:{
      return (static_cast<S2LatLngRect const*>(_data))->Contains(pp);
    }
    case GeometryContainer::Type::S2_CAP:{
      return (static_cast<S2Cap const*>(_data))->Contains(pp);
    }
      
    case GeometryContainer::Type::S2_POLYLINE:{
      S2Polyline const* ll = static_cast<S2Polyline const*>(_data);
      for (int k = 0; k < ll->num_vertices(); k++) {
        if (ll->vertex(k) == pp) {
          return true;
        }
      }
      return false;
    }
      
    case GeometryContainer::Type::S2_POLYGON:{
      return (static_cast<S2Polygon const*>(_data))->Contains(pp);
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
  
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
/*bool GeometryContainer::contains(S2LatLngRect const& rect) const {
  switch (_type) {
    case GeometryContainer::Type::S2_POINT:{
      return rect.is_point() && _data->VirtualContainsPoint(rect.lo().ToPoint());
    }
    case GeometryContainer::Type::S2_LATLNGRECT:{
      return (static_cast<S2LatLngRect const*>(_data))->Contains(rect);
    }
      
    case GeometryContainer::Type::S2_CAP:{
      S2Cap const* cap = (static_cast<S2Cap const*>(_data));
      return cap->Contains(rect.lo().ToPoint()) &&
             cap->Contains(rect.hi().ToPoint());
    }
      
    case GeometryContainer::Type::S2_POLYLINE:
      return false; // numerically not well defined
      
    case GeometryContainer::Type::S2_POLYGON:{
      // this works as long there is only one shell and n holes i.e.
      // all geoJson polygons, but not multipolygons
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      int num = poly->num_loops();
      TRI_ASSERT(num >= 1);
      if (!poly->loop(0)->Contains(&rect)) { // shell needs to contain
        return false;
      }
      for (int k = 1; k < num; k++) {
        if (poly->loop(k)->Intersects(&rect)) {
          return false;
        }
      }
      return true;
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
  
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}*/
/*bool GeometryContainer::contains(S2Cap const& cap) const {
  switch (_type) {
    case GeometryContainer::Type::S2_POINT:{
      return false;
    }
    case GeometryContainer::Type::S2_LATLNGRECT:{
      // works for exact bounds, which GetRectBound() guarantees
      return (static_cast<S2LatLngRect const*>(_data))->Contains(cap.GetRectBound());
    }
      
    case GeometryContainer::Type::S2_CAP:{
      return (static_cast<S2Cap const*>(_data))->Contains(cap);
    }
      
    case GeometryContainer::Type::S2_POLYLINE:
      return false; // numerically not well defined
      
    case GeometryContainer::Type::S2_POLYGON:{
      // this works as long there is only one shell and n holes i.e.
      // all geoJson polygons, but not multipolygons
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      int num = poly->num_loops();
      TRI_ASSERT(num >= 1);
      if (!poly->loop(0)->Contains(&rect)) { // shell needs to contain
        return false;
      }
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}*/
bool GeometryContainer::contains(S2Polyline const* otherLine) const {
  switch (_type) {
    case GeometryContainer::Type::S2_POINT:{
      return false;// rect.is_point() && rect.lo() == S2LatLng
    }
    case GeometryContainer::Type::S2_LATLNGRECT:{
      S2LatLngRect const* rect = static_cast<S2LatLngRect const*>(_data);
      return rect->Contains(*otherLine);
    }
    case GeometryContainer::Type::S2_CAP:{
      // FIXME: why do we need the complement here at all ?
      S2Cap cmp = (static_cast<S2Cap const*>(_data))->Complement();
      // calculate angle between complement cap center and closest point on polygon
      int ignored;
      S1Angle angle(cmp.axis(), otherLine->Project(cmp.axis(), &ignored));
      // if angle is bigger than complement cap angle, than the polyline is contained
      // we need to use the complement otherwise we would test for intersection
      return angle.radians() >= cmp.angle().radians();
    }
      
    case GeometryContainer::Type::S2_POLYLINE:{
      S2Polyline const* ll = static_cast<S2Polyline const*>(_data);
      return ll->ApproxEquals(otherLine, 1e-8);
    }
      
    case GeometryContainer::Type::S2_POLYGON:{
      // FIXME this seems to be incomplete
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      std::vector<S2Polyline*> cut;
      TRI_DEFER(for (S2Polyline* pp : cut) { delete pp; });
      poly->IntersectWithPolyline(otherLine, &cut);
      if (cut.size() != 1) {
        return false; // is clipping holes or no edg
      }
      
      // The line may be in the polygon
      return cut[0]->NearlyCoversPolyline(*otherLine, S1Angle::Degrees(1e-10));
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}
bool GeometryContainer::contains(S2Polygon const* poly) const {
  switch (_type) {
    case GeometryContainer::Type::S2_POINT:{
      return false;// rect.is_point() && rect.lo() == S2LatLng
    }
    case GeometryContainer::Type::S2_LATLNGRECT:{
      // works for exact bounds, which GetRectBound() guarantees
      return (static_cast<S2LatLngRect const*>(_data))->Contains(poly->GetRectBound());
    }
    case GeometryContainer::Type::S2_CAP:{
      // FIXME: why do we need the complement here at all ?
      S2Cap cmp = (static_cast<S2Cap const*>(_data))->Complement();
      // calculate angle between complement cap center and closest point on polygon
      S1Angle angle(cmp.axis(), poly->Project(cmp.axis()));
      // if angle is bigger than complement cap angle, than the polyline is contained
      // we need to use the complement otherwise we would test for intersection
      return angle.radians() >= cmp.angle().radians();
    }
      
    case GeometryContainer::Type::S2_POLYLINE:{
      return false; // numerically not well defined
    }
      
    case GeometryContainer::Type::S2_POLYGON:{
      return (static_cast<S2Polygon const*>(_data))->Contains(poly);
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}
bool GeometryContainer::contains(GeometryContainer const* cc) const {
  switch (cc->_type) {
    case GeometryContainer::Type::S2_POINT:{
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
    case GeometryContainer::Type::S2_LATLNGRECT:{
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
    case GeometryContainer::Type::S2_CAP:{
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
      
    case GeometryContainer::Type::S2_POLYLINE:{
      return contains(static_cast<S2Polyline const*>(cc->_data));
    }
      
    case GeometryContainer::Type::S2_POLYGON:{
      return contains(static_cast<S2Polygon const*>(cc->_data));
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}


bool GeometryContainer::intersects(Coordinate const* cc) const {
  return contains(cc); // same
}

bool GeometryContainer::intersects(S2Polyline const* otherLine) const {
  switch (_type) {
    case GeometryContainer::Type::S2_POINT:{
      return false;// rect.is_point() && rect.lo() == S2LatLng
    }
    case GeometryContainer::Type::S2_LATLNGRECT:{
      S2LatLngRect const* rect = static_cast<S2LatLngRect const*>(_data);
      return rect->Intersects(*otherLine);
    }
    case GeometryContainer::Type::S2_CAP:{
      S2Cap const* cap = static_cast<S2Cap const*>(_data);
      // calculate angle between cap center and closest point on polygon
      int ignored;
      S1Angle angle(cap->axis(), otherLine->Project(cap->axis(), &ignored));
      return angle.radians() <= cap->angle().radians();
    }
      
    case GeometryContainer::Type::S2_POLYLINE:{
      S2Polyline const* ll = static_cast<S2Polyline const*>(_data);
      return ll->Intersects(otherLine);
    }
      
    case GeometryContainer::Type::S2_POLYGON:{
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      std::vector<S2Polyline*> cut;
      TRI_DEFER(for (S2Polyline* pp : cut) { delete pp; });
      poly->IntersectWithPolyline(otherLine, &cut);
      return cut.size() > 0;
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}
bool GeometryContainer::intersects(S2Polygon const* otherPoly) const {
  switch (_type) {
    case GeometryContainer::Type::S2_POINT:{
      return false;// rect.is_point() && rect.lo() == S2LatLng
    }
    case GeometryContainer::Type::S2_LATLNGRECT:{
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
    case GeometryContainer::Type::S2_CAP:{
      S2Cap const* cap = static_cast<S2Cap const*>(_data);
      // calculate angle between cap center and closest point on polygon
      S1Angle angle(cap->axis(), otherPoly->Project(cap->axis()));
      return angle.radians() <= cap->angle().radians();
    }
      
    case GeometryContainer::Type::S2_POLYLINE:{
      return false; // numerically not well defined
    }
      
    case GeometryContainer::Type::S2_POLYGON:{
      S2Polygon const* poly = static_cast<S2Polygon const*>(_data);
      return poly->Intersects(otherPoly);
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}
bool GeometryContainer::intersects(GeometryContainer const* cc) const {
  switch (cc->_type) {
    case GeometryContainer::Type::S2_POINT:{
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
    case GeometryContainer::Type::S2_LATLNGRECT:{
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
    case GeometryContainer::Type::S2_CAP:{
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
      
    case GeometryContainer::Type::S2_POLYLINE:{
      return intersects(static_cast<S2Polyline const*>(cc->_data));
    }
      
    case GeometryContainer::Type::S2_POLYGON:{
      return intersects(static_cast<S2Polygon const*>(cc->_data));
    }
      
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
  }
}

