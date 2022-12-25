#include "Geo/S2/S2Points.h"
#include "Basics/Exceptions.h"

#include <s2/s2cap.h>
#include <s2/s2cell.h>
#include <s2/s2latlng.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2latlng_rect_bounder.h>

namespace arangodb::geo {

S2Point S2Points::GetCentroid() const noexcept {
  // TODO(MBkkt) was comment "probably broken"
  //  It's not really broken, it's mathematically correct,
  //  but I think it can be incorrect because of limited double precision
  //  For example maybe Kahan summation algorithm for each coordinate
  //  and then divide on points.size()
  auto c = S2LatLng::FromDegrees(0.0, 0.0);
  auto const invNumPoints = 1.0 / static_cast<double>(_impl.size());
  for (auto const& point : _impl) {
    c = c + invNumPoints * S2LatLng{point};
  }
  TRI_ASSERT(c.is_valid());
  return c.ToPoint();
}

S2Region* S2Points::Clone() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

S2Cap S2Points::GetCapBound() const { return GetRectBound().GetCapBound(); }

S2LatLngRect S2Points::GetRectBound() const {
  S2LatLngRectBounder bounder;
  for (auto const& point : _impl) {
    bounder.AddPoint(point);
  }
  return bounder.GetBound();
}

bool S2Points::Contains(S2Cell const& cell) const { return false; }

bool S2Points::MayIntersect(S2Cell const& cell) const {
  for (auto const& point : _impl) {
    if (cell.Contains(point)) {
      return true;
    }
  }
  return false;
}

bool S2Points::Contains(S2Point const& p) const {
  for (auto const& point : _impl) {
    if (point == p) {
      return true;
    }
  }
  return false;
}

}  // namespace arangodb::geo
