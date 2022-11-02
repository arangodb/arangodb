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

namespace {

TEST(TriangleTrueCentroid, SmallTriangles) {
  // Test TrueCentroid() with very small triangles.  This test assumes that
  // the triangle is small enough so that it is nearly planar.
  for (int iter = 0; iter < 100; ++iter) {
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

TEST(EdgeTrueCentroid, SemiEquator) {
  // Test the centroid of polyline ABC that follows the equator and consists
  // of two 90 degree edges (i.e., C = -A).  The centroid (multiplied by
  // length) should point toward B and have a norm of 2.0.  (The centroid
  // itself has a norm of 2/Pi, and the total edge length is Pi.)
  S2Point a(0, -1, 0), b(1, 0, 0), c(0, 1, 0);
  S2Point centroid = S2::TrueCentroid(a, b) + S2::TrueCentroid(b, c);
  EXPECT_TRUE(S2::ApproxEquals(b, centroid.Normalize()));
  EXPECT_DOUBLE_EQ(2.0, centroid.Norm());
}

TEST(EdgeTrueCentroid, GreatCircles) {
  // Construct random great circles and divide them randomly into segments.
  // Then make sure that the centroid is approximately at the center of the
  // sphere.  Note that because of the way the centroid is computed, it does
  // not matter how we split the great circle into segments.
  //
  // Note that this is a direct test of the properties that the centroid
  // should have, rather than a test that it matches a particular formula.

  for (int iter = 0; iter < 100; ++iter) {
    // Choose a coordinate frame for the great circle.
    S2Point x, y, z, centroid;
    S2Testing::GetRandomFrame(&x, &y, &z);

    S2Point v0 = x;
    for (double theta = 0; theta < 2 * M_PI;
         theta += pow(S2Testing::rnd.RandDouble(), 10)) {
      S2Point v1 = cos(theta) * x + sin(theta) * y;
      centroid += S2::TrueCentroid(v0, v1);
      v0 = v1;
    }
    // Close the circle.
    centroid += S2::TrueCentroid(v0, x);
    EXPECT_LE(centroid.Norm(), 2e-14);
  }
}

}  // namespace
