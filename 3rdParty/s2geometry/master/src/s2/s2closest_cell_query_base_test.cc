// Copyright 2018 Google Inc. All Rights Reserved.
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
//
// This file contains some basic tests of the templating support.  Testing of
// the actual algorithms is in s2closest_cell_query_test.cc.

#include "s2/s2closest_cell_query_base.h"

#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s2max_distance_targets.h"
#include "s2/s2text_format.h"

using s2textformat::MakeCellIdOrDie;
using s2textformat::MakeCellUnionOrDie;

namespace {

// This is a proof-of-concept prototype of a possible S2FurthestCellQuery
// class.  The purpose of this test is just to make sure that the code
// compiles and does something reasonable.
using FurthestCellQuery = S2ClosestCellQueryBase<S2MaxDistance>;

class FurthestPointTarget final : public S2MaxDistancePointTarget {
 public:
  explicit FurthestPointTarget(const S2Point& point)
      : S2MaxDistancePointTarget(point) {}

  int max_brute_force_index_size() const override {
    return 10;  // Arbitrary.
  }
};

TEST(S2ClosestCellQueryBase, MaxDistance) {
  S2CellIndex index;
  index.Add(MakeCellUnionOrDie("0/123, 0/22, 0/3"), 1 /*label*/);
  index.Build();
  FurthestCellQuery query(&index);
  FurthestCellQuery::Options options;
  options.set_max_results(1);
  FurthestPointTarget target(MakeCellIdOrDie("3/123").ToPoint());
  auto results = query.FindClosestCells(&target, options);
  ASSERT_EQ(1, results.size());
  EXPECT_EQ("0/123", results[0].cell_id().ToString());
  EXPECT_EQ(1, results[0].label());
  EXPECT_EQ(4.0, S1ChordAngle(results[0].distance()).length2());
}

}  // namespace
