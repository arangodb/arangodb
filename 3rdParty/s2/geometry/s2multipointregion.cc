// Copyright 2005 Google Inc. All Rights Reserved.

#include "s2multipointregion.h"
#include "base/logging.h"
#include "util/coding/coder.h"
#include "s2cap.h"
#include "s2cell.h"
#include "s2latlngrect.h"
#include "s2edgeutil.h"

static const unsigned char kCurrentEncodingVersionNumber = 1;

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
  S2EdgeUtil::RectBounder bounder;
  for (int i = 0; i < num_points_; ++i) {
    bounder.AddPoint(&points_[i]);
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

void S2MultiPointRegion::Encode(Encoder* encoder) const {
  encoder->Ensure(10 + 30 * num_points_);  // sufficient

  encoder->put8(kCurrentEncodingVersionNumber);
  encoder->put32(num_points_);
  for (int k = 0; k < num_points_; k++) {
    for (int i = 0; i < 3; ++i) {
      encoder->putdouble(points_[k][i]);
    }
  }
  DCHECK_GE(encoder->avail(), 0);
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
  
  DCHECK(S2::IsUnitLength(point_));

  return decoder->avail() >= 0;
}
