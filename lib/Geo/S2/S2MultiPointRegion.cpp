////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "S2MultiPointRegion.h"
#include <s2/s2cap.h>
#include <s2/s2cell.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2latlng_rect_bounder.h>
#include <s2/util/coding/coder.h>

S2MultiPointRegion::S2MultiPointRegion(std::vector<S2Point>* points)
    : num_points_(points->size()), points_(nullptr) {
  if (num_points_ > 0) {
    points_ = new S2Point[num_points_];
    memcpy(points_, points->data(), num_points_ * sizeof(S2Point));
  }
}

S2MultiPointRegion::S2MultiPointRegion(S2MultiPointRegion const* other)
    : num_points_(other->num_points_), points_(nullptr) {
  if (num_points_ > 0) {
    points_ = new S2Point[num_points_];
    memcpy(points_, other->points_, num_points_ * sizeof(S2Point));
  }
}

S2MultiPointRegion* S2MultiPointRegion::Clone() const {
  return new S2MultiPointRegion(this);
}

S2Cap S2MultiPointRegion::GetCapBound() const {
  return GetRectBound().GetCapBound();
}

S2LatLngRect S2MultiPointRegion::GetRectBound() const {
  S2LatLngRectBounder bounder;
  for (int i = 0; i < num_points_; ++i) {
    bounder.AddPoint(points_[i]);
  }
  return bounder.GetBound();
}

bool S2MultiPointRegion::Contains(S2Point const& p) const {
  for (int k = 0; k < num_points_; k++) {
    if (points_[k] == p) {
      return true;
    }
  }
  return false;
}

bool S2MultiPointRegion::MayIntersect(S2Cell const& cell) const {
  for (int k = 0; k < num_points_; k++) {
    if (cell.Contains(points_[k])) {
      return true;
    }
  }
  return false;
}
/*
static const unsigned char kCurrentEncodingVersionNumber = 1;

void S2MultiPointRegion::Encode(Encoder* encoder) const {
  encoder->Ensure(10 + 30 * num_points_);  // sufficient

  encoder->put8(kCurrentEncodingVersionNumber);
  encoder->put32(num_points_);
  for (int k = 0; k < num_points_; k++) {
    for (int i = 0; i < 3; ++i) {
      encoder->putdouble(points_[k][i]);
    }
  }
  assert(encoder->avail() >= 0);
}

bool S2MultiPointRegion::Decode(Decoder* decoder) {
  unsigned char version = decoder->get8();
  if (version > kCurrentEncodingVersionNumber) return false;

  num_points_ = decoder->get32();
  delete[] points_;
  points_ = new S2Point[num_points_];
  for (int k = 0; k < num_points_; k++) {
    for (int i = 0; i < 3; ++i) {
      points_[k][i] = decoder->getdouble();
    }
  }

  assert(S2::IsUnitLength(point_));

  return decoder->avail() >= 0;
}
*/
