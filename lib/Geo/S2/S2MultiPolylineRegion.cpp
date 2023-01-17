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

#include "Geo/S2/S2MultiPolylineRegion.h"
#include "Basics/Exceptions.h"

#include <s2/s2cap.h>
#include <s2/s2latlng.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2latlng_rect_bounder.h>
#include <s2/s2polyline_measures.h>

namespace arangodb::geo {

S2Point S2MultiPolylineRegion::GetCentroid() const noexcept {
  // copied from s2: S2Point GetCentroid(const S2Shape& shape);
  S2Point centroid;
  for (auto const& polyline : _impl) {
    centroid += S2::GetCentroid(polyline.vertices_span());
  }
  return centroid;
}

S2Region* S2MultiPolylineRegion::Clone() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

S2Cap S2MultiPolylineRegion::GetCapBound() const {
  return GetRectBound().GetCapBound();
}

S2LatLngRect S2MultiPolylineRegion::GetRectBound() const {
  S2LatLngRectBounder bounder;
  for (auto const& polyline : _impl) {
    for (auto const& point : polyline.vertices_span()) {
      bounder.AddPoint(point);
    }
  }
  return bounder.GetBound();
}

bool S2MultiPolylineRegion::Contains(S2Cell const& /*cell*/) const {
  return false;
}

bool S2MultiPolylineRegion::MayIntersect(S2Cell const& cell) const {
  for (auto const& polyline : _impl) {
    if (polyline.MayIntersect(cell)) {
      return true;
    }
  }
  return false;
}

bool S2MultiPolylineRegion::Contains(S2Point const& /*p*/) const {
  // S2MultiPolylineRegion doesn't have a Contains(S2Point) method, because
  // "containment" isn't numerically well-defined except at the polyline
  // vertices.
  return false;
}

void S2MultiPolylineRegion::Encode(Encoder* const encoder,
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

bool S2MultiPolylineRegion::Decode(Decoder* const decoder) {
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
