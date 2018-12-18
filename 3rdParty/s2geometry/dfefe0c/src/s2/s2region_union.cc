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

#include "s2/s2region_union.h"

#include "s2/s2cap.h"
#include "s2/s2latlng_rect.h"

using std::vector;

S2RegionUnion::S2RegionUnion(vector<std::unique_ptr<S2Region>> regions) {
  Init(std::move(regions));
}

void S2RegionUnion::Init(vector<std::unique_ptr<S2Region>> regions) {
  S2_DCHECK(regions_.empty());
  regions_ = std::move(regions);
}

S2RegionUnion::S2RegionUnion(const S2RegionUnion& src)
  : regions_(src.num_regions()) {
  for (int i = 0; i < num_regions(); ++i) {
    regions_[i].reset(src.region(i)->Clone());
  }
}

vector<std::unique_ptr<S2Region>> S2RegionUnion::Release() {
  vector<std::unique_ptr<S2Region>> result;
  result.swap(regions_);
  return result;
}

void S2RegionUnion::Add(std::unique_ptr<S2Region> region) {
  regions_.push_back(std::move(region));
}

S2RegionUnion* S2RegionUnion::Clone() const {
  return new S2RegionUnion(*this);
}

S2Cap S2RegionUnion::GetCapBound() const {
  // TODO(ericv): This could be optimized to return a tighter bound,
  // but doesn't seem worth it unless profiling shows otherwise.
  return GetRectBound().GetCapBound();
}

S2LatLngRect S2RegionUnion::GetRectBound() const {
  S2LatLngRect result = S2LatLngRect::Empty();
  for (int i = 0; i < num_regions(); ++i) {
    result = result.Union(region(i)->GetRectBound());
  }
  return result;
}

bool S2RegionUnion::Contains(const S2Cell& cell) const {
  // Note that this method is allowed to return false even if the cell
  // is contained by the region.
  for (int i = 0; i < num_regions(); ++i) {
    if (region(i)->Contains(cell)) return true;
  }
  return false;
}

bool S2RegionUnion::Contains(const S2Point& p) const {
  for (int i = 0; i < num_regions(); ++i) {
    if (region(i)->Contains(p)) return true;
  }
  return false;
}

bool S2RegionUnion::MayIntersect(const S2Cell& cell) const {
  for (int i = 0; i < num_regions(); ++i) {
    if (region(i)->MayIntersect(cell)) return true;
  }
  return false;
}
