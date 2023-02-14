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

#include "Geo/S2/S2MultiPointRegion.h"
#include "Basics/Exceptions.h"

#include <s2/s2cap.h>
#include <s2/s2cell.h>
#include <s2/s2latlng.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2latlng_rect_bounder.h>
#include <s2/encoded_s2point_vector.h>

namespace arangodb::geo {

S2Point S2MultiPointRegion::GetCentroid() const noexcept {
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

S2Region* S2MultiPointRegion::Clone() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

S2Cap S2MultiPointRegion::GetCapBound() const {
  return GetRectBound().GetCapBound();
}

S2LatLngRect S2MultiPointRegion::GetRectBound() const {
  S2LatLngRectBounder bounder;
  for (auto const& point : _impl) {
    bounder.AddPoint(point);
  }
  return bounder.GetBound();
}

bool S2MultiPointRegion::Contains(S2Cell const& cell) const { return false; }

bool S2MultiPointRegion::MayIntersect(S2Cell const& cell) const {
  for (auto const& point : _impl) {
    if (cell.Contains(point)) {
      return true;
    }
  }
  return false;
}

bool S2MultiPointRegion::Contains(S2Point const& p) const {
  for (auto const& point : _impl) {
    if (point == p) {
      return true;
    }
  }
  return false;
}

void S2MultiPointRegion::Encode(Encoder* const encoder,
                                s2coding::CodingHint hint) const {
  s2coding::EncodeS2PointVector(_impl, hint, encoder);
}

bool S2MultiPointRegion::Decode(Decoder* const decoder) {
  s2coding::EncodedS2PointVector impl;
  if (!impl.Init(decoder)) {
    return false;
  }
  _impl = impl.Decode();
  return true;
}

}  // namespace arangodb::geo
