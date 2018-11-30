// Copyright 2013 Google Inc. All Rights Reserved.
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
// the actual algorithms is in s2closest_edge_query_test.cc.

#include "s2/s2closest_edge_query_base.h"

#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s2max_distance_targets.h"
#include "s2/s2text_format.h"

namespace {

// This is a proof-of-concept prototype of a possible S2FurthestEdgeQuery
// class.  The purpose of this test is just to make sure that the code
// compiles and does something reasonable.
using FurthestEdgeQuery = S2ClosestEdgeQueryBase<S2MaxDistance>;

class FurthestPointTarget final : public S2MaxDistancePointTarget {
 public:
  explicit FurthestPointTarget(const S2Point& point)
      : S2MaxDistancePointTarget(point) {}

  int max_brute_force_index_size() const override {
    return 10;  // Arbitrary.
  }
};

TEST(S2ClosestEdgeQueryBase, MaxDistance) {
  auto index = s2textformat::MakeIndex("0:0 | 1:0 | 2:0 | 3:0 # #");
  FurthestEdgeQuery query(index.get());
  FurthestEdgeQuery::Options options;
  options.set_max_results(1);
  FurthestPointTarget target(s2textformat::MakePoint("4:0"));
  auto results = query.FindClosestEdges(&target, options);
  ASSERT_EQ(1, results.size());
  EXPECT_EQ(0, results[0].shape_id());
  EXPECT_EQ(0, results[0].edge_id());
  EXPECT_NEAR(4, S1ChordAngle(results[0].distance()).ToAngle().degrees(),
              1e-13);
}

}  // namespace
