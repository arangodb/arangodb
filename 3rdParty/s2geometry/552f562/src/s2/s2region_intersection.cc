// Copyright 2006 Google Inc. All Rights Reserved.
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


#include "s2/s2region_intersection.h"

#include "s2/s2cap.h"
#include "s2/s2latlng_rect.h"

using std::vector;

S2RegionIntersection::S2RegionIntersection(
    vector<std::unique_ptr<S2Region>> regions) {
  Init(std::move(regions));
}

void S2RegionIntersection::Init(vector<std::unique_ptr<S2Region>> regions) {
  S2_DCHECK(regions_.empty());
  regions_ = std::move(regions);
}

S2RegionIntersection::S2RegionIntersection(const S2RegionIntersection& src)
  : regions_(src.num_regions()) {
  for (int i = 0; i < num_regions(); ++i) {
    regions_[i].reset(src.region(i)->Clone());
  }
}

vector<std::unique_ptr<S2Region>> S2RegionIntersection::Release() {
  vector<std::unique_ptr<S2Region>> result;
  result.swap(regions_);
  return result;
}

S2RegionIntersection* S2RegionIntersection::Clone() const {
  return new S2RegionIntersection(*this);
}

S2Cap S2RegionIntersection::GetCapBound() const {
  // TODO(ericv): This could be optimized to return a tighter bound, but
  // doesn't seem worth it unless profiling shows otherwise.
  return GetRectBound().GetCapBound();
}

S2LatLngRect S2RegionIntersection::GetRectBound() const {
  S2LatLngRect result = S2LatLngRect::Full();
  for (int i = 0; i < num_regions(); ++i) {
    result = result.Intersection(region(i)->GetRectBound());
  }
  return result;
}

bool S2RegionIntersection::Contains(const S2Cell& cell) const {
  for (int i = 0; i < num_regions(); ++i) {
    if (!region(i)->Contains(cell)) return false;
  }
  return true;
}

bool S2RegionIntersection::Contains(const S2Point& p) const {
  for (int i = 0; i < num_regions(); ++i) {
    if (!region(i)->Contains(p)) return false;
  }
  return true;
}

bool S2RegionIntersection::MayIntersect(const S2Cell& cell) const {
  for (int i = 0; i < num_regions(); ++i) {
    if (!region(i)->MayIntersect(cell)) return false;
  }
  return true;
}
