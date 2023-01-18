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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Geo/S2/S2MultiPolyline.h"
#include "Basics/Exceptions.h"

#include <s2/s2cap.h>
#include <s2/s2latlng.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2latlng_rect_bounder.h>

namespace arangodb::geo {

S2Point S2MultiPolyline::GetCentroid() const noexcept {
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

S2Region* S2MultiPolyline::Clone() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

S2Cap S2MultiPolyline::GetCapBound() const {
  return GetRectBound().GetCapBound();
}

S2LatLngRect S2MultiPolyline::GetRectBound() const {
  S2LatLngRectBounder bounder;
  for (auto const& polyline : _impl) {
    for (auto const& point : polyline.vertices_span()) {
      bounder.AddPoint(point);
    }
  }
  return bounder.GetBound();
}

bool S2MultiPolyline::Contains(S2Cell const& /*cell*/) const { return false; }

bool S2MultiPolyline::MayIntersect(S2Cell const& cell) const {
  for (auto const& polyline : _impl) {
    if (polyline.MayIntersect(cell)) {
      return true;
    }
  }
  return false;
}

bool S2MultiPolyline::Contains(S2Point const& /*p*/) const {
  // S2Polylines doesn't have a Contains(S2Point) method, because "containment"
  // is not numerically well-defined except at the polyline vertices.
  return false;
}

void S2MultiPolyline::Encode(Encoder* const encoder,
                             s2coding::CodingHint hint) const {
  encoder->Ensure(sizeof(uint64_t));
  encoder->put64(_impl.size());
  for (auto const& polyline : _impl) {
    if (hint == s2coding::CodingHint::FAST) {
      polyline.EncodeUncompressed(encoder);
    } else {
      polyline.EncodeMostCompact(encoder);
    }
  }
}

bool S2MultiPolyline::Decode(Decoder* const decoder) {
  if (decoder->avail() < sizeof(uint64_t)) {
    return false;
  }
  _impl.resize(static_cast<size_t>(decoder->get64()));
  for (auto& polyline : _impl) {
    if (!polyline.Decode(decoder)) {
      return false;
    }
  }
  return true;
}

}  // namespace arangodb::geo
