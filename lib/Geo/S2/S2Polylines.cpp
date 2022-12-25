#include "Geo/S2/S2Polylines.h"
#include "Basics/Exceptions.h"

#include <s2/s2cap.h>
#include <s2/s2latlng.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2latlng_rect_bounder.h>

namespace arangodb::geo {

S2Point S2Polylines::GetCentroid() const noexcept {
  // TODO(MBkkt) probably same issues as in S2Points
  auto c = S2LatLng::FromDegrees(0.0, 0.0);
  double totalWeight = 0.0;
  for (auto const& line : _impl) {
    totalWeight += line.GetLength().radians();
  }
  for (auto const& line : _impl) {
    auto const weight = line.GetLength().radians() / totalWeight;
    c = c + weight * S2LatLng{line.GetCentroid()};
  }
  TRI_ASSERT(c.is_valid());
  return c.ToPoint();
}

S2Region* S2Polylines::Clone() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

S2Cap S2Polylines::GetCapBound() const { return GetRectBound().GetCapBound(); }

S2LatLngRect S2Polylines::GetRectBound() const {
  S2LatLngRectBounder bounder;
  for (auto const& polyline : _impl) {
    for (auto const& point : polyline.vertices_span()) {
      bounder.AddPoint(point);
    }
  }
  return bounder.GetBound();
}

bool S2Polylines::Contains(S2Cell const& /*cell*/) const { return false; }

bool S2Polylines::MayIntersect(S2Cell const& cell) const {
  for (auto const& polyline : _impl) {
    if (polyline.MayIntersect(cell)) {
      return true;
    }
  }
  return false;
}

bool S2Polylines::Contains(S2Point const& /*p*/) const {
  // S2Polylines doesn't have a Contains(S2Point) method, because "containment"
  // is not numerically well-defined except at the polyline vertices.
  return false;
}

}  // namespace arangodb::geo
