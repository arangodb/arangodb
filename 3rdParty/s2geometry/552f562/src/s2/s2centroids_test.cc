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

#include "s2/s2centroids.h"

#include <gtest/gtest.h>
#include "s2/s2testing.h"

TEST(S2, TrueCentroid) {
  // Test TrueCentroid() with very small triangles.  This test assumes that
  // the triangle is small enough so that it is nearly planar.
  for (int i = 0; i < 100; ++i) {
    Vector3_d p, x, y;
    S2Testing::GetRandomFrame(&p, &x, &y);
    double d = 1e-4 * pow(1e-4, S2Testing::rnd.RandDouble());
    S2Point p0 = (p - d * x).Normalize();
    S2Point p1 = (p + d * x).Normalize();
    S2Point p2 = (p + 3 * d * y).Normalize();
    S2Point centroid = S2::TrueCentroid(p0, p1, p2).Normalize();

    // The centroid of a planar triangle is at the intersection of its
    // medians, which is two-thirds of the way along each median.
    S2Point expected_centroid = (p + d * y).Normalize();
    EXPECT_LE(centroid.Angle(expected_centroid), 2e-8);
  }
}
