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

#include "s2/s2loop_measures.h"

#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include "s2/s2latlng.h"
#include "s2/s2loop.h"
#include "s2/s2measures.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using s2textformat::ParsePointsOrDie;
using std::fabs;
using std::min;
using std::vector;

namespace {

// Given a string where each character "ch" represents a vertex (such as
// "abac"), returns a vector of S2Points of the form (ch, 0, 0).  Note that
// these points are not unit length and therefore are not suitable for general
// use, however they are useful for testing certain functions below.
vector<S2Point> MakeTestLoop(const string& loop_str) {
  vector<S2Point> loop;
  for (char ch : loop_str) {
    loop.push_back(S2Point(ch, 0, 0));
  }
  return loop;
}

// Given a loop whose vertices are represented as characters (such as "abcd" or
// "abccb"), verify that S2::PruneDegeneracies() yields the loop "expected".
void TestPruneDegeneracies(const string& input_str,
                           const string& expected_str) {
  vector<S2Point> input = MakeTestLoop(input_str);
  vector<S2Point> new_vertices;
  string actual_str;
  for (const S2Point& p : S2::PruneDegeneracies(input, &new_vertices)) {
    actual_str.push_back(static_cast<char>(p[0]));
  }
  EXPECT_EQ(expected_str, actual_str);
}

TEST(PruneDegeneracies, AllDegeneracies) {
  TestPruneDegeneracies("", "");
  TestPruneDegeneracies("a", "");
  TestPruneDegeneracies("aaaaa", "");
  TestPruneDegeneracies("ab", "");
  TestPruneDegeneracies("abb", "");
  TestPruneDegeneracies("aab", "");
  TestPruneDegeneracies("aba", "");
  TestPruneDegeneracies("abba", "");
  TestPruneDegeneracies("abcb", "");
  TestPruneDegeneracies("abcba", "");
  TestPruneDegeneracies("abcdcdedefedcbcdcb", "");
}

TEST(PruneDegeneracies, SomeDegeneracies) {
  TestPruneDegeneracies("abc", "abc");
  TestPruneDegeneracies("abca", "abc");
  TestPruneDegeneracies("abcc", "abc");
  TestPruneDegeneracies("abccaa", "abc");
  TestPruneDegeneracies("aabbcc", "abc");
  TestPruneDegeneracies("abcdedca", "abc");
  TestPruneDegeneracies("abcbabcbcdc", "abc");
  TestPruneDegeneracies("xyzabcazy", "abc");
  TestPruneDegeneracies("xxyyzzaabbccaazzyyxx", "abc");
}

// Given a loop whose vertices are represented as characters (such as "abcd" or
// "abccb"), verify that S2::GetCanonicalLoopOrder returns the given result.
void TestCanonicalLoopOrder(const string& input_str,
                            S2::LoopOrder expected_order) {
  EXPECT_EQ(expected_order, S2::GetCanonicalLoopOrder(MakeTestLoop(input_str)));
}

TEST(GetCanonicalLoopOrder, AllDegeneracies) {
  TestCanonicalLoopOrder("", S2::LoopOrder(0, 1));
  TestCanonicalLoopOrder("a", S2::LoopOrder(0, 1));
  TestCanonicalLoopOrder("aaaaa", S2::LoopOrder(0, 1));
  TestCanonicalLoopOrder("ba", S2::LoopOrder(1, 1));
  TestCanonicalLoopOrder("bab", S2::LoopOrder(1, 1));
  TestCanonicalLoopOrder("cbab", S2::LoopOrder(2, 1));
  TestCanonicalLoopOrder("bacbcab", S2::LoopOrder(8, -1));
}

TEST(GetPerimeter, Empty) {
  EXPECT_EQ(S1Angle::Zero(), S2::GetPerimeter(vector<S2Point>{}));
}

TEST(GetPerimeter, Octant) {
  auto loop = ParsePointsOrDie("0:0, 0:90, 90:0");
  EXPECT_DOUBLE_EQ(3 * M_PI_2, S2::GetPerimeter(loop).radians());
}

TEST(GetPerimeter, MoreThanTwoPi) {
  // Make sure that GetPerimeter doesn't use S1ChordAngle, which can only
  // represent distances up to 2*Pi.
  auto loop = ParsePointsOrDie("0:0, 0:90, 0:180, 90:0, 0:-90");
  EXPECT_DOUBLE_EQ(5 * M_PI_2, S2::GetPerimeter(loop).radians());
}

class LoopTestBase : public testing::Test {
 protected:
  // Some standard loops to use in the tests (see descriptions below).
  vector<S2Point> full_;
  vector<S2Point> v_loop_;
  vector<S2Point> north_hemi_;
  vector<S2Point> north_hemi3_;
  vector<S2Point> west_hemi_;
  vector<S2Point> east_hemi_;
  vector<S2Point> candy_cane_;
  vector<S2Point> line_triangle_;
  vector<S2Point> skinny_chevron_;
  vector<S2Point> three_leaf_clover_;
  vector<S2Point> tessellated_loop_;

 public:
  LoopTestBase() :
      // The full loop is represented as a loop with no vertices.
      full_(),

      // A degenerate loop in the shape of a "V".
      v_loop_(ParsePointsOrDie("5:1, 0:2, 5:3, 0:2")),

      // The northern hemisphere, defined using two pairs of antipodal points.
      north_hemi_(ParsePointsOrDie("0:-180, 0:-90, 0:0, 0:90")),

      // The northern hemisphere, defined using three points 120 degrees apart.
      north_hemi3_(ParsePointsOrDie("0:-180, 0:-60, 0:60")),

      // The western hemisphere, defined using two pairs of antipodal points.
      west_hemi_(ParsePointsOrDie("0:-180, -90:0, 0:0, 90:0")),

      // The eastern hemisphere, defined using two pairs of antipodal points.
      east_hemi_(ParsePointsOrDie("90:0, 0:0, -90:0, 0:-180")),

      // A spiral stripe that slightly over-wraps the equator.
      candy_cane_(ParsePointsOrDie(
          "-20:150, -20:-70, 0:70, 10:-150, 10:70, -10:-70")),

      // A completely degenerate triangle along the equator that Sign()
      // considers to be CCW.
      line_triangle_(ParsePointsOrDie("0:1, 0:2, 0:3")),

      // A nearly-degenerate CCW chevron near the equator with very long sides
      // (about 80 degrees).  Its area is less than 1e-640, which is too small
      // to represent in double precision.
      skinny_chevron_(ParsePointsOrDie("0:0, -1e-320:80, 0:1e-320, 1e-320:80")),

      // A loop where the same vertex appears three times.
      three_leaf_clover_(ParsePointsOrDie(
          "0:0, -3:3, 3:3, 0:0, 3:0, 3:-3, 0:0, -3:-3, -3:0")),

      // A loop with groups of 3 or more vertices in a straight line.
      tessellated_loop_(ParsePointsOrDie(
          "10:34, 5:34, 0:34, -10:34, -10:36, -5:36, 0:36, 10:36")) {
  }
};

static void TestAreaConsistentWithCurvature(const vector<S2Point>& loop) {
  // Check that the area computed using GetArea() is consistent with the loop
  // curvature.  According to the Gauss-Bonnet theorem, the area of the loop
  // equals 2*Pi minus its curvature.
  double area = S2::GetArea(loop);
  double gauss_area = 2 * M_PI - S2::GetCurvature(loop);
  // The error bound below is sufficient for current tests but not guaranteed.
  EXPECT_LE(fabs(area - gauss_area), 1e-14)
      << "Failed loop: " << s2textformat::ToString(loop)
      << "\nArea = " << area << ", Gauss Area = " << gauss_area;
}

TEST_F(LoopTestBase, GetAreaConsistentWithCurvature) {
  TestAreaConsistentWithCurvature(full_);
  TestAreaConsistentWithCurvature(north_hemi_);
  TestAreaConsistentWithCurvature(north_hemi3_);
  TestAreaConsistentWithCurvature(west_hemi_);
  TestAreaConsistentWithCurvature(east_hemi_);
  TestAreaConsistentWithCurvature(candy_cane_);
  TestAreaConsistentWithCurvature(line_triangle_);
  TestAreaConsistentWithCurvature(skinny_chevron_);
  TestAreaConsistentWithCurvature(three_leaf_clover_);
  TestAreaConsistentWithCurvature(tessellated_loop_);
}

TEST_F(LoopTestBase, GetAreaConsistentWithOrientation) {
  // Test that GetArea() returns an area near 0 for degenerate loops that
  // contain almost no points, and an area near 4*Pi for degenerate loops that
  // contain almost all points.

  S2Testing::Random* rnd = &S2Testing::rnd;
  static const int kMaxVertices = 6;
  for (int i = 0; i < 50; ++i) {
    int num_vertices = 3 + rnd->Uniform(kMaxVertices - 3 + 1);
    // Repeatedly choose N vertices that are exactly on the equator until we
    // find some that form a valid loop.
    vector<S2Point> loop;
    do {
      loop.clear();
      for (int i = 0; i < num_vertices; ++i) {
        // We limit longitude to the range [0, 90] to ensure that the loop is
        // degenerate (as opposed to following the entire equator).
        loop.push_back(
            S2LatLng::FromRadians(0, rnd->RandDouble() * M_PI_2).ToPoint());
      }
    } while (!S2Loop(loop, S2Debug::DISABLE).IsValid());
    bool ccw = S2::IsNormalized(loop);
    // The error bound is sufficient for current tests but not guaranteed.
    EXPECT_NEAR(ccw ? 0 : 4 * M_PI, S2::GetArea(loop), 1e-14)
        << "Failed loop " << i << ": " << s2textformat::ToString(loop);
    EXPECT_EQ(!ccw, S2Loop(loop).Contains(S2Point(0, 0, 1)));
  }
}

TEST_F(LoopTestBase, GetAreaAccuracy) {
  // TODO(ericv): Test that GetArea() has an accuracy significantly better
  // than 1e-15 on loops whose area is small.
}

TEST_F(LoopTestBase, GetAreaAndCentroid) {
  EXPECT_EQ(4 * M_PI, S2::GetArea(full_));
  EXPECT_EQ(S2Point(0, 0, 0), S2::GetCentroid(full_));

  EXPECT_DOUBLE_EQ(S2::GetArea(north_hemi_), 2 * M_PI);
  EXPECT_NEAR(2 * M_PI, S2::GetArea(east_hemi_), 1e-12);

  // Construct spherical caps of random height, and approximate their boundary
  // with closely spaces vertices.  Then check that the area and centroid are
  // correct.
  for (int iter = 0; iter < 50; ++iter) {
    // Choose a coordinate frame for the spherical cap.
    S2Point x, y, z;
    S2Testing::GetRandomFrame(&x, &y, &z);

    // Given two points at latitude phi and whose longitudes differ by dtheta,
    // the geodesic between the two points has a maximum latitude of
    // atan(tan(phi) / cos(dtheta/2)).  This can be derived by positioning
    // the two points at (-dtheta/2, phi) and (dtheta/2, phi).
    //
    // We want to position the vertices close enough together so that their
    // maximum distance from the boundary of the spherical cap is kMaxDist.
    // Thus we want fabs(atan(tan(phi) / cos(dtheta/2)) - phi) <= kMaxDist.
    static const double kMaxDist = 1e-6;
    double height = 2 * S2Testing::rnd.RandDouble();
    double phi = asin(1 - height);
    double max_dtheta = 2 * acos(tan(fabs(phi)) / tan(fabs(phi) + kMaxDist));
    max_dtheta = min(M_PI, max_dtheta);  // At least 3 vertices.

    vector<S2Point> loop;
    for (double theta = 0; theta < 2 * M_PI;
         theta += S2Testing::rnd.RandDouble() * max_dtheta) {
      loop.push_back(cos(theta) * cos(phi) * x +
                     sin(theta) * cos(phi) * y +
                     sin(phi) * z);
    }
    double area = S2::GetArea(loop);
    S2Point centroid = S2::GetCentroid(loop);
    double expected_area = 2 * M_PI * height;
    EXPECT_LE(fabs(area - expected_area), 2 * M_PI * kMaxDist);
    S2Point expected_centroid = expected_area * (1 - 0.5 * height) * z;
    EXPECT_LE((centroid - expected_centroid).Norm(), 2 * kMaxDist);
  }
}

static void ExpectSameOrder(S2PointLoopSpan loop1, S2::LoopOrder order1,
                            S2PointLoopSpan loop2, S2::LoopOrder order2) {
  S2_DCHECK_EQ(loop1.size(), loop2.size());
  int i1 = order1.first, i2 = order2.first;
  int dir1 = order1.dir, dir2 = order2.dir;
  for (int n = loop1.size(); --n >= 0; ) {
    ASSERT_EQ(loop1[i1], loop2[i2]) << ": " << order1 << " vs. " << order2;
    i1 += dir1;
    i2 += dir2;
  }
}

// Check that the curvature is *identical* when the vertex order is
// rotated, and that the sign is inverted when the vertices are reversed.
static void CheckCurvatureInvariants(const vector<S2Point>& loop_in) {
  S2::LoopOrder order_in = S2::GetCanonicalLoopOrder(loop_in);
  auto loop = loop_in;
  double expected = S2::GetCurvature(loop);
  for (int i = 0; i < loop.size(); ++i) {
    std::reverse(loop.begin(), loop.end());
    EXPECT_EQ((expected == 2 * M_PI) ? expected : -expected,
              S2::GetCurvature(loop));
    ExpectSameOrder(loop_in, order_in, loop, S2::GetCanonicalLoopOrder(loop));
    std::reverse(loop.begin(), loop.end());
    std::rotate(loop.begin(), loop.begin() + 1, loop.end());
    EXPECT_EQ(expected, S2::GetCurvature(loop));
    ExpectSameOrder(loop_in, order_in, loop, S2::GetCanonicalLoopOrder(loop));
  }
}

TEST_F(LoopTestBase, GetCurvature) {
  EXPECT_EQ(-2 * M_PI, S2::GetCurvature(full_));

  EXPECT_EQ(2 * M_PI, S2::GetCurvature(v_loop_));
  CheckCurvatureInvariants(v_loop_);

  // This curvature should be computed exactly.
  EXPECT_EQ(0, S2::GetCurvature(north_hemi3_));
  CheckCurvatureInvariants(north_hemi3_);

  EXPECT_NEAR(0, S2::GetCurvature(west_hemi_), 1e-15);
  CheckCurvatureInvariants(west_hemi_);

  // We don't have an easy way to estimate the curvature of these loops, but
  // we can still check that the expected invariants hold.
  CheckCurvatureInvariants(candy_cane_);
  CheckCurvatureInvariants(three_leaf_clover_);

  EXPECT_DOUBLE_EQ(2 * M_PI, S2::GetCurvature(line_triangle_));
  CheckCurvatureInvariants(line_triangle_);

  EXPECT_DOUBLE_EQ(2 * M_PI, S2::GetCurvature(skinny_chevron_));
  CheckCurvatureInvariants(skinny_chevron_);

  // Build a narrow spiral loop starting at the north pole.  This is designed
  // to test that the error in GetCurvature is linear in the number of
  // vertices even when the partial sum of the curvatures gets very large.
  // The spiral consists of two "arms" defining opposite sides of the loop.
  // This is a pathological loop that contains many long parallel edges.
  const int kArmPoints = 10000;    // Number of vertices in each "arm"
  const double kArmRadius = 0.01;  // Radius of spiral.
  vector<S2Point> spiral(2 * kArmPoints);
  spiral[kArmPoints] = S2Point(0, 0, 1);
  for (int i = 0; i < kArmPoints; ++i) {
    double angle = (2 * M_PI / 3) * i;
    double x = cos(angle);
    double y = sin(angle);
    double r1 = i * kArmRadius / kArmPoints;
    double r2 = (i + 1.5) * kArmRadius / kArmPoints;
    spiral[kArmPoints - i - 1] = S2Point(r1 * x, r1 * y, 1).Normalize();
    spiral[kArmPoints + i] = S2Point(r2 * x, r2 * y, 1).Normalize();
  }

  // Check that GetCurvature() is consistent with GetArea() to within the
  // error bound of the former.  We actually use a tiny fraction of the
  // worst-case error bound, since the worst case only happens when all the
  // roundoff errors happen in the same direction and this test is not
  // designed to achieve that.  The error in GetArea() can be ignored for the
  // purposes of this test since it is generally much smaller.
  EXPECT_NEAR(2 * M_PI - S2::GetArea(spiral), S2::GetCurvature(spiral),
              0.01 * S2::GetCurvatureMaxError(spiral));
}

}  // namespace
