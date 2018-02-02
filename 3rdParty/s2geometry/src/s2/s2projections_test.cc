// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "s2/s2projections.h"

#include <limits>
#include <gtest/gtest.h>
#include "s2/s2latlng.h"
#include "s2/s2pointutil.h"
#include "s2/s2testing.h"

namespace S2 {

TEST(PlateCarreeProjection, Interpolate) {
  PlateCarreeProjection proj(180);

  // Test that coordinates and/or arguments are not accidentally reversed.
  EXPECT_EQ(R2Point(1.5, 6),
            proj.Interpolate(0.25, R2Point(1, 5), R2Point(3, 9)));

  // Test extrapolation.
  EXPECT_EQ(R2Point(-3, 0), proj.Interpolate(-2, R2Point(1, 0), R2Point(3, 0)));

  // Check that interpolation is exact at both endpoints.
  R2Point a(1.234, -5.456e-20), b(2.1234e-20, 7.456);
  EXPECT_EQ(a, proj.Interpolate(0, a, b));
  EXPECT_EQ(b, proj.Interpolate(1, a, b));
}

void TestProjectUnproject(const Projection& projection,
                          const R2Point& px, const S2Point& x) {
  // The arguments are chosen such that projection is exact, but
  // unprojection may not be.
  EXPECT_EQ(px, projection.Project(x)) << projection.Project(x);
  EXPECT_TRUE(S2::ApproxEquals(x, projection.Unproject(px)))
      << x << " vs. " << projection.Unproject(px);
}

TEST(PlateCarreeProjection, ProjectUnproject) {
  PlateCarreeProjection proj(180);
  TestProjectUnproject(proj, R2Point(0, 0), S2Point(1, 0, 0));
  TestProjectUnproject(proj, R2Point(180, 0), S2Point(-1, 0, 0));
  TestProjectUnproject(proj, R2Point(90, 0), S2Point(0, 1, 0));
  TestProjectUnproject(proj, R2Point(-90, 0), S2Point(0, -1, 0));
  TestProjectUnproject(proj, R2Point(0, 90), S2Point(0, 0, 1));
  TestProjectUnproject(proj, R2Point(0, -90), S2Point(0, 0, -1));
}

TEST(MercatorProjection, ProjectUnproject) {
  MercatorProjection proj(180);
  double inf = std::numeric_limits<double>::infinity();
  TestProjectUnproject(proj, R2Point(0, 0), S2Point(1, 0, 0));
  TestProjectUnproject(proj, R2Point(180, 0), S2Point(-1, 0, 0));
  TestProjectUnproject(proj, R2Point(90, 0), S2Point(0, 1, 0));
  TestProjectUnproject(proj, R2Point(-90, 0), S2Point(0, -1, 0));
  TestProjectUnproject(proj, R2Point(0, inf), S2Point(0, 0, 1));
  TestProjectUnproject(proj, R2Point(0, -inf), S2Point(0, 0, -1));

  // Test one arbitrary point as a sanity check.
  TestProjectUnproject(proj, R2Point(0, 70.255578967830246),
                       S2LatLng::FromRadians(1, 0).ToPoint());
}

}  //  namespace S2
