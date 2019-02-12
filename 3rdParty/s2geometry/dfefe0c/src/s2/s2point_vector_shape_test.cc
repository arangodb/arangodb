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

#include "s2/s2point_vector_shape.h"

#include <vector>

#include <gtest/gtest.h>
#include "s2/s2testing.h"

TEST(S2PointVectorShape, Empty) {
  std::vector<S2Point> points;
  S2PointVectorShape shape(std::move(points));
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(0, shape.num_chains());
  EXPECT_EQ(0, shape.dimension());
  EXPECT_TRUE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2PointVectorShape, ConstructionAndAccess) {
  std::vector<S2Point> points;
  S2Testing::rnd.Reset(FLAGS_s2_random_seed);
  const int kNumPoints = 100;
  for (int i = 0; i < kNumPoints; ++i) {
    points.push_back(S2Testing::RandomPoint());
  }
  S2PointVectorShape shape(std::move(points));

  EXPECT_EQ(kNumPoints, shape.num_edges());
  EXPECT_EQ(kNumPoints, shape.num_chains());
  EXPECT_EQ(0, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  S2Testing::rnd.Reset(FLAGS_s2_random_seed);
  for (int i = 0; i < kNumPoints; ++i) {
    EXPECT_EQ(i, shape.chain(i).start);
    EXPECT_EQ(1, shape.chain(i).length);
    auto edge = shape.edge(i);
    S2Point pt = S2Testing::RandomPoint();
    EXPECT_EQ(pt, edge.v0);
    EXPECT_EQ(pt, edge.v1);
  }
}
