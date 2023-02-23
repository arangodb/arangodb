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

namespace arangodb::geo {

S2Point S2MultiPointRegion::GetCentroid() const noexcept {
  // copied from s2: S2Point GetCentroid(const S2Shape& shape);
  S2Point centroid;
  for (auto const& point : _impl) {
    centroid += point;
  }
  return centroid;
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
  // TODO(MBkkt) Maybe cache it?
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
  TRI_ASSERT(isOptionsS2(options));
  TRI_ASSERT(options != Options::kS2PointRegionCompact ||
             options != Options::kS2PointShapeCompact)
      << "Not implemented yet.";
  TRI_ASSERT(encoder.avail() >= sizeof(uint8_t) + Varint::kMax64);
  encoder.put8(toTag(Type::kMultiPoint, options));
  encoder.put_varint64(_impl.size());
  encodeVertices(encoder, _impl);
}

bool S2MultiPointRegion::Decode(Decoder& decoder, uint8_t tag) {
  uint64 size = 0;
  if (!decoder.get_varint64(&size)) {
    return false;
  }
  _impl.resize(size);
  if (!decodeVertices(decoder, _impl, tag)) {
    return false;
  }
  return true;
}

}  // namespace arangodb::geo
