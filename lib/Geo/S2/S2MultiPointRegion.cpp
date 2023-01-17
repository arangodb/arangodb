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
  return getPointsCentroid(_impl);
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

using namespace coding;

void S2MultiPointRegion::Encode(Encoder& encoder, Options options) const {
  if (options.multiPoint == Options::Point::S2Point) {
    TRI_ASSERT(encoder.avail() >= 8);
    encoder.put8(toTag<Type::kMultiPoint, Options::Point::S2Point>());
    s2coding::EncodeS2PointVector(_impl, options.hint, &encoder);
  } else {
    TRI_ASSERT(options.multiPoint == Options::Point::S2LatLng);
    auto const size = _impl.size();
    encoder.Ensure(8 + Varint::kMax64 + size * 2 * sizeof(double));
    encoder.put8(toTag<Type::kMultiPoint, Options::Point::S2LatLng>());
    encoder.put_varint64(size);
    for (auto const& point : _impl) {
      encodePointToLatLng(encoder, point);
    }
  }
}

bool S2MultiPointRegion::Decode(Decoder& decoder, uint8_t tag) {
  return decodeVectorPoint(decoder, tag, _impl);
}

}  // namespace arangodb::geo
