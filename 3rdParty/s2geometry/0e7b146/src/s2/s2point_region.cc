// Copyright 2005 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/s2point_region.h"

#include "s2/base/logging.h"
#include "s2/util/coding/coder.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2pointutil.h"

static const unsigned char kCurrentLosslessEncodingVersionNumber = 1;

S2PointRegion::~S2PointRegion() {
}

S2PointRegion* S2PointRegion::Clone() const {
  return new S2PointRegion(point_);
}

S2Cap S2PointRegion::GetCapBound() const {
  return S2Cap::FromPoint(point_);
}

S2LatLngRect S2PointRegion::GetRectBound() const {
  S2LatLng ll(point_);
  return S2LatLngRect(ll, ll);
}

bool S2PointRegion::MayIntersect(const S2Cell& cell) const {
  return cell.Contains(point_);
}

void S2PointRegion::Encode(Encoder* encoder) const {
  encoder->Ensure(30);  // sufficient

  encoder->put8(kCurrentLosslessEncodingVersionNumber);
  for (int i = 0; i < 3; ++i) {
    encoder->putdouble(point_[i]);
  }
  S2_DCHECK_GE(encoder->avail(), 0);
}

bool S2PointRegion::Decode(Decoder* decoder) {
  if (decoder->avail() < sizeof(unsigned char) + 3 * sizeof(double))
    return false;
  unsigned char version = decoder->get8();
  if (version > kCurrentLosslessEncodingVersionNumber) return false;

  for (int i = 0; i < 3; ++i) {
    point_[i] = decoder->getdouble();
  }
  if (!S2::IsUnitLength(point_)) return false;

  return true;
}
