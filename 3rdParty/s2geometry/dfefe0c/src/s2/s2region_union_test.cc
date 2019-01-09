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

#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2point_region.h"
#include "s2/s2region_coverer.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using std::unique_ptr;
using std::vector;

namespace {

TEST(S2RegionUnionTest, Basic) {
  S2RegionUnion ru_empty((vector<unique_ptr<S2Region>>()));
  EXPECT_EQ(0, ru_empty.num_regions());
  EXPECT_EQ(S2Cap::Empty(), ru_empty.GetCapBound());
  EXPECT_EQ(S2LatLngRect::Empty(), ru_empty.GetRectBound());
  unique_ptr<S2Region> empty_clone(ru_empty.Clone());

  vector<unique_ptr<S2Region>> two_point_region;
  two_point_region.emplace_back(
      new S2PointRegion(S2LatLng::FromDegrees(35, 40).ToPoint()));
  two_point_region.emplace_back(
      new S2PointRegion(S2LatLng::FromDegrees(-35, -40).ToPoint()));

  auto two_points_orig =
      make_unique<S2RegionUnion>(std::move(two_point_region));
  // two_point_region is in a valid, but unspecified, state.

  // Check that Clone() returns a deep copy.
  unique_ptr<S2RegionUnion> two_points(two_points_orig->Clone());
  two_points_orig.reset();
  // The bounds below may not be exactly equal because the S2PointRegion
  // version converts each S2LatLng value to an S2Point and back.
  EXPECT_TRUE(s2textformat::MakeLatLngRect("-35:-40,35:40").ApproxEquals(
      two_points->GetRectBound()))
      << two_points->GetRectBound();

  S2Cell face0 = S2Cell::FromFace(0);
  EXPECT_TRUE(two_points->MayIntersect(face0));
  EXPECT_FALSE(two_points->Contains(face0));

  EXPECT_TRUE(two_points->Contains(S2LatLng::FromDegrees(35, 40).ToPoint()));
  EXPECT_TRUE(two_points->Contains(S2LatLng::FromDegrees(-35, -40).ToPoint()));
  EXPECT_FALSE(two_points->Contains(S2LatLng::FromDegrees(0, 0).ToPoint()));

  // Check that we can Add() another region.
  unique_ptr<S2RegionUnion> three_points(two_points->Clone());
  EXPECT_FALSE(three_points->Contains(S2LatLng::FromDegrees(10, 10).ToPoint()));
  three_points->Add(
      make_unique<S2PointRegion>(S2LatLng::FromDegrees(10, 10).ToPoint()));
  EXPECT_TRUE(three_points->Contains(S2LatLng::FromDegrees(10, 10).ToPoint()));

  S2RegionCoverer coverer;
  coverer.mutable_options()->set_max_cells(1);
  vector<S2CellId> covering;
  coverer.GetCovering(*two_points, &covering);
  EXPECT_EQ(1, covering.size());
  EXPECT_EQ(face0.id(), covering[0]);
}

}  // namespace
