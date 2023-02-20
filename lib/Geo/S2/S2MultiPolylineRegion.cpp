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
  // TODO(MBkkt) Maybe cache it?
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

using namespace coding;

void S2MultiPolylineRegion::Encode(Encoder& encoder, Options options) const {
  TRI_ASSERT(isOptionsS2(options));
  TRI_ASSERT(options != Options::kS2PointRegionCompact ||
             options != Options::kS2PointShapeCompact)
      << "In such case we need to serialize all vertices at once.";
  TRI_ASSERT(encoder.avail() >= sizeof(uint8_t) + Varint::kMax64);
  encoder.put8(toTag(Type::kMultiPolyline, options));
  auto const numPolylines = _impl.size();
  if (numPolylines == 0) {
    encoder.put_varint64(0);
    return;
  } else if (numPolylines == 1) {
    auto const vertices = _impl[0].vertices_span();
    TRI_ASSERT(!vertices.empty());
    encoder.put_varint64(vertices.size() * 2);
    encodeVertices(encoder, vertices);
    return;
  }
  encoder.Ensure((1 + numPolylines) * Varint::kMax64 + 2 * toSize(options));
  encoder.put_varint64(numPolylines * 2 + 1);
  for (size_t i = 0; i != numPolylines; ++i) {
    auto const vertices = _impl[i].vertices_span();
    encoder.put_varint64(vertices.size());
    encodeVertices(encoder, vertices);
  }
}

bool S2MultiPolylineRegion::Decode(Decoder& decoder, uint8_t tag,
                                   std::vector<S2Point>& cache) {
  _impl.clear();
  uint64 size = 0;
  if (!decoder.get_varint64(&size)) {
    return false;
  }
  if (size == 0) {
    return true;
  } else if (size % 2 == 0) {
    cache.resize(static_cast<size_t>(size / 2));
    if (!decodeVertices(decoder, cache, tag)) {
      return false;
    }
    _impl.emplace_back(cache, S2Debug::DISABLE);
    return true;
  }
  auto const numPolylines = static_cast<size_t>(size / 2);
  TRI_ASSERT(numPolylines >= 2);
  _impl.reserve(numPolylines);
  for (uint64 i = 0; i != numPolylines; ++i) {
    if (!decoder.get_varint64(&size)) {
      return false;
    }
    cache.resize(size);
    if (!decodeVertices(decoder, cache, tag)) {
      return false;
    }
    _impl.emplace_back(cache, S2Debug::DISABLE);
  }
  return true;
}

}  // namespace arangodb::geo
