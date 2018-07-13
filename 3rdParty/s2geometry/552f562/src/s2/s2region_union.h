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

#ifndef S2_S2REGION_UNION_H_
#define S2_S2REGION_UNION_H_

#include <memory>
#include <vector>

#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/s2region.h"
#include "s2/third_party/absl/base/macros.h"

class Decoder;
class Encoder;
class S2Cap;
class S2Cell;
class S2LatLngRect;

// An S2RegionUnion represents a union of possibly overlapping regions.
// It is convenient for computing a covering of a set of regions.
class S2RegionUnion final : public S2Region {
 public:
  // Create an empty region.  Can be made non-empty by calling Init() or Add().
  S2RegionUnion() = default;

  // Create a region representing the union of the given regions.
  explicit S2RegionUnion(std::vector<std::unique_ptr<S2Region>> regions);

  // Use {} instead of = default to work around gcc bug.
  ~S2RegionUnion() override {}

  // Initialize region by taking ownership of the given regions.
  void Init(std::vector<std::unique_ptr<S2Region>> regions);

  // Releases ownership of the regions of this union and returns them,
  // leaving this region empty.
  std::vector<std::unique_ptr<S2Region>> Release();

  // Add the given region to the union.  This method can be called repeatedly
  // as an alternative to Init().
  void Add(std::unique_ptr<S2Region> region);

  // Accessor methods.
  int num_regions() const { return regions_.size(); }
  const S2Region* region(int i) const { return regions_[i].get(); }

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  S2RegionUnion* Clone() const override;
  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;
  bool Contains(const S2Point& p) const override;
  bool Contains(const S2Cell& cell) const override;
  bool MayIntersect(const S2Cell& cell) const override;

 private:
  // Internal copy constructor used only by Clone() that makes a deep copy of
  // its argument.
  S2RegionUnion(const S2RegionUnion& src);

  std::vector<std::unique_ptr<S2Region>> regions_;

  void operator=(const S2RegionUnion&) = delete;
};

#endif  // S2_S2REGION_UNION_H_
