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

#include "s2/s2loop.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "s2/base/commandlineflags.h"
#include "s2/base/logging.h"
#include <gtest/gtest.h>

#include "s2/util/coding/coder.h"
#include "s2/r1interval.h"
#include "s2/s1angle.h"
#include "s2/s1interval.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2debug.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2edge_distances.h"
#include "s2/s2error.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect_bounder.h"
#include "s2/s2measures.h"
#include "s2/s2point_compression.h"
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"
#include "s2/third_party/absl/container/fixed_array.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/util/math/matrix3x3.h"
#include "s2/util/math/vector.h"

using absl::make_unique;
using std::fabs;
using std::map;
using std::max;
using std::min;
using std::set;
using std::unique_ptr;
using std::vector;

class S2LoopTestBase : public testing::Test {
 protected:
  // The set of all loops declared below.
  vector<const S2Loop*> all_loops;

  // Some standard loops to use in the tests (see descriptions below).
  const unique_ptr<const S2Loop> empty_;
  const unique_ptr<const S2Loop> full_;
  const unique_ptr<const S2Loop> north_hemi_;
  const unique_ptr<const S2Loop> north_hemi3_;
  const unique_ptr<const S2Loop> south_hemi_;
  const unique_ptr<const S2Loop> west_hemi_;
  const unique_ptr<const S2Loop> east_hemi_;
  const unique_ptr<const S2Loop> near_hemi_;
  const unique_ptr<const S2Loop> far_hemi_;
  const unique_ptr<const S2Loop> candy_cane_;
  const unique_ptr<const S2Loop> small_ne_cw_;
  const unique_ptr<const S2Loop> arctic_80_;
  const unique_ptr<const S2Loop> antarctic_80_;
  const unique_ptr<const S2Loop> line_triangle_;
  const unique_ptr<const S2Loop> skinny_chevron_;
  const unique_ptr<const S2Loop> loop_a_;
  const unique_ptr<const S2Loop> loop_b_;
  const unique_ptr<const S2Loop> a_intersect_b_;
  const unique_ptr<const S2Loop> a_union_b_;
  const unique_ptr<const S2Loop> a_minus_b_;
  const unique_ptr<const S2Loop> b_minus_a_;
  const unique_ptr<const S2Loop> loop_c_;
  const unique_ptr<const S2Loop> loop_d_;
  const unique_ptr<const S2Loop> loop_e_;
  const unique_ptr<const S2Loop> loop_f_;
  const unique_ptr<const S2Loop> loop_g_;
  const unique_ptr<const S2Loop> loop_h_;
  const unique_ptr<const S2Loop> loop_i_;
  unique_ptr<const S2Loop> snapped_loop_a_;

 private:
  unique_ptr<const S2Loop> AddLoop(const string& str) {
    return AddLoop(s2textformat::MakeLoop(str));
  }

  unique_ptr<const S2Loop> AddLoop(std::unique_ptr<const S2Loop> loop) {
    all_loops.push_back(&*loop);
    return loop;
  }

 public:
  S2LoopTestBase()
      // The empty loop.
    : empty_(AddLoop(make_unique<S2Loop>(S2Loop::kEmpty()))),

      // The full loop.
      full_(AddLoop(make_unique<S2Loop>(S2Loop::kFull()))),

      // The northern hemisphere, defined using two pairs of antipodal points.
      north_hemi_(AddLoop("0:-180, 0:-90, 0:0, 0:90")),

      // The northern hemisphere, defined using three points 120 degrees apart.
      north_hemi3_(AddLoop("0:-180, 0:-60, 0:60")),

      // The southern hemisphere, defined using two pairs of antipodal points.
      south_hemi_(AddLoop("0:90, 0:0, 0:-90, 0:-180")),

      // The western hemisphere, defined using two pairs of antipodal points.
      west_hemi_(AddLoop("0:-180, -90:0, 0:0, 90:0")),

      // The eastern hemisphere, defined using two pairs of antipodal points.
      east_hemi_(AddLoop("90:0, 0:0, -90:0, 0:-180")),

      // The "near" hemisphere, defined using two pairs of antipodal points.
      near_hemi_(AddLoop("0:-90, -90:0, 0:90, 90:0")),

      // The "far" hemisphere, defined using two pairs of antipodal points.
      far_hemi_(AddLoop("90:0, 0:90, -90:0, 0:-90")),

      // A spiral stripe that slightly over-wraps the equator.
      candy_cane_(AddLoop("-20:150, -20:-70, 0:70, 10:-150, 10:70, -10:-70")),

      // A small clockwise loop in the northern & eastern hemisperes.
      small_ne_cw_(AddLoop("35:20, 45:20, 40:25")),

      // Loop around the north pole at 80 degrees.
      arctic_80_(AddLoop("80:-150, 80:-30, 80:90")),

      // Loop around the south pole at 80 degrees.
      antarctic_80_(AddLoop("-80:120, -80:0, -80:-120")),

      // A completely degenerate triangle along the equator that Sign()
      // considers to be CCW.
      line_triangle_(AddLoop("0:1, 0:2, 0:3")),

      // A nearly-degenerate CCW chevron near the equator with very long sides
      // (about 80 degrees).  Its area is less than 1e-640, which is too small
      // to represent in double precision.
      skinny_chevron_(AddLoop("0:0, -1e-320:80, 0:1e-320, 1e-320:80")),

      // A diamond-shaped loop around the point 0:180.
      loop_a_(AddLoop("0:178, -1:180, 0:-179, 1:-180")),

      // Another diamond-shaped loop around the point 0:180.
      loop_b_(AddLoop("0:179, -1:180, 0:-178, 1:-180")),

      // The intersection of A and B.
      a_intersect_b_(AddLoop("0:179, -1:180, 0:-179, 1:-180")),

      // The union of A and B.
      a_union_b_(AddLoop("0:178, -1:180, 0:-178, 1:-180")),

      // A minus B (concave).
      a_minus_b_(AddLoop("0:178, -1:180, 0:179, 1:-180")),

      // B minus A (concave).
      b_minus_a_(AddLoop("0:-179, -1:180, 0:-178, 1:-180")),

      // A shape gotten from A by adding a triangle to one edge, and
      // subtracting a triangle from the opposite edge.
      loop_c_(AddLoop("0:178, 0:180, -1:180, 0:-179, 1:-179, 1:-180")),

      // A shape gotten from A by adding a triangle to one edge, and
      // adding another triangle to the opposite edge.
      loop_d_(AddLoop("0:178, -1:178, -1:180, 0:-179, 1:-179, 1:-180")),

      //   3------------2
      //   |            |               ^
      //   |  7-8  b-c  |               |
      //   |  | |  | |  |      Latitude |
      //   0--6-9--a-d--1               |
      //   |  | |       |               |
      //   |  f-e       |               +----------->
      //   |            |                 Longitude
      //   4------------5
      //
      // Important: It is not okay to skip over collinear vertices when
      // defining these loops (e.g. to define loop E as "0,1,2,3") because S2
      // uses symbolic perturbations to ensure that no three vertices are
      // *ever* considered collinear (e.g., vertices 0, 6, 9 are not
      // collinear).  In other words, it is unpredictable (modulo knowing the
      // details of the symbolic perturbations) whether 0123 contains 06123,
      // for example.
      //
      // Loop E:  0,6,9,a,d,1,2,3
      // Loop F:  0,4,5,1,d,a,9,6
      // Loop G:  0,6,7,8,9,a,b,c,d,1,2,3
      // Loop H:  0,6,f,e,9,a,b,c,d,1,2,3
      // Loop I:  7,6,f,e,9,8
      loop_e_(AddLoop("0:30, 0:34, 0:36, 0:39, 0:41, 0:44, 30:44, 30:30")),
      loop_f_(AddLoop("0:30, -30:30, -30:44, 0:44, 0:41, 0:39, 0:36, 0:34")),
      loop_g_(AddLoop("0:30, 0:34, 10:34, 10:36, 0:36, 0:39, 10:39, "
                      "10:41, 0:41, 0:44, 30:44, 30:30")),
      loop_h_(AddLoop("0:30, 0:34, -10:34, -10:36, 0:36, 0:39, "
                      "10:39, 10:41, 0:41, 0:44, 30:44, 30:30")),
      loop_i_(AddLoop("10:34, 0:34, -10:34, -10:36, 0:36, 10:36")) {
    // Like loop_a, but the vertices are at leaf cell centers.
    vector<S2Point> snapped_loop_a_vertices = {
        S2CellId(s2textformat::MakePoint("0:178")).ToPoint(),
        S2CellId(s2textformat::MakePoint("-1:180")).ToPoint(),
        S2CellId(s2textformat::MakePoint("0:-179")).ToPoint(),
        S2CellId(s2textformat::MakePoint("1:-180")).ToPoint()};
    snapped_loop_a_ = AddLoop(make_unique<S2Loop>(snapped_loop_a_vertices));
  }

  // Wrapper function that encodes "loop" into "encoder" using the private
  // EncodeCompressed() method.
  void TestEncodeCompressed(const S2Loop& loop, int level, Encoder* encoder) {
    absl::FixedArray<S2XYZFaceSiTi> points(loop.num_vertices());
    loop.GetXYZFaceSiTiVertices(points.data());
    loop.EncodeCompressed(encoder, points.data(), level);
  }

  // Wrapper function that decodes the contents of "encoder" into "loop" using
  // the private DecodeCompressed() method.
  void TestDecodeCompressed(const Encoder& encoder, int level, S2Loop* loop) {
    Decoder decoder(encoder.base(), encoder.length());
    ASSERT_TRUE(loop->DecodeCompressed(&decoder, level));
  }
};

static const S2LatLng kRectError = S2LatLngRectBounder::MaxErrorForTests();

TEST_F(S2LoopTestBase, GetRectBound) {
  EXPECT_TRUE(empty_->GetRectBound().is_empty());
  EXPECT_TRUE(full_->GetRectBound().is_full());
  EXPECT_TRUE(candy_cane_->GetRectBound().lng().is_full());
  EXPECT_LT(candy_cane_->GetRectBound().lat_lo().degrees(), -20);
  EXPECT_GT(candy_cane_->GetRectBound().lat_hi().degrees(), 10);
  EXPECT_TRUE(small_ne_cw_->GetRectBound().is_full());
  EXPECT_TRUE(arctic_80_->GetRectBound().ApproxEquals(
      S2LatLngRect(S2LatLng::FromDegrees(80, -180),
                   S2LatLng::FromDegrees(90, 180)), kRectError));
  EXPECT_TRUE(antarctic_80_->GetRectBound().ApproxEquals(
      S2LatLngRect(S2LatLng::FromDegrees(-90, -180),
                   S2LatLng::FromDegrees(-80, 180)), kRectError));

  // Create a loop that contains the complement of the "arctic_80" loop.
  unique_ptr<S2Loop> arctic_80_inv(arctic_80_->Clone());
  arctic_80_inv->Invert();
  // The highest latitude of each edge is attained at its midpoint.
  S2Point mid = 0.5 * (arctic_80_inv->vertex(0) + arctic_80_inv->vertex(1));
  EXPECT_NEAR(arctic_80_inv->GetRectBound().lat_hi().radians(),
              S2LatLng(mid).lat().radians(), kRectError.lat().radians());

  EXPECT_TRUE(south_hemi_->GetRectBound().lng().is_full());
  EXPECT_TRUE(south_hemi_->GetRectBound().lat().ApproxEquals(
      R1Interval(-M_PI_2, 0), kRectError.lat().radians()));
}

static void Rotate(unique_ptr<S2Loop>* ptr) {
  S2Loop* loop = ptr->get();
  vector<S2Point> vertices;
  for (int i = 1; i <= loop->num_vertices(); ++i) {
    vertices.push_back(loop->vertex(i));
  }
  ptr->reset(new S2Loop(vertices));
}

TEST_F(S2LoopTestBase, AreaConsistentWithCurvature) {
  // Check that the area computed using GetArea() is consistent with the
  // curvature of the loop computed using GetTurnAngle().  According to
  // the Gauss-Bonnet theorem, the area of the loop should be equal to 2*Pi
  // minus its curvature.
  for (const S2Loop* loop : all_loops) {
    double area = loop->GetArea();
    double gauss_area = 2 * M_PI - loop->GetCurvature();
    // The error bound is sufficient for current tests but not guaranteed.
    EXPECT_LE(fabs(area - gauss_area), 1e-14)
        << "Failed loop: " << s2textformat::ToString(*loop)
        << "\nArea = " << area << ", Gauss Area = " << gauss_area;
  }
}

TEST_F(S2LoopTestBase, GetAreaConsistentWithSign) {
  // Test that GetArea() returns an area near 0 for degenerate loops that
  // contain almost no points, and an area near 4*Pi for degenerate loops that
  // contain almost all points.

  S2Testing::Random* rnd = &S2Testing::rnd;
  static const int kMaxVertices = 6;
  for (int i = 0; i < 50; ++i) {
    int num_vertices = 3 + rnd->Uniform(kMaxVertices - 3 + 1);
    // Repeatedly choose N vertices that are exactly on the equator until we
    // find some that form a valid loop.
    S2Loop loop;
    loop.set_s2debug_override(S2Debug::DISABLE);
    do {
      vector<S2Point> vertices;
      for (int i = 0; i < num_vertices; ++i) {
        // We limit longitude to the range [0, 90] to ensure that the loop is
        // degenerate (as opposed to following the entire equator).
        vertices.push_back(
            S2LatLng::FromRadians(0, rnd->RandDouble() * M_PI_2).ToPoint());
      }
      loop.Init(vertices);
    } while (!loop.IsValid());
    bool ccw = loop.IsNormalized();
    EXPECT_NEAR(ccw ? 0 : 4 * M_PI, loop.GetArea(), 1e-15)
        << "Failed loop " << i << ": " << s2textformat::ToString(loop);
    EXPECT_EQ(!ccw, loop.Contains(S2Point(0, 0, 1)));
  }
}

TEST_F(S2LoopTestBase, GetAreaAccuracy) {
  // TODO(ericv): Test that GetArea() has an accuracy significantly better
  // than 1e-15 on loops whose area is small.
}

TEST_F(S2LoopTestBase, GetAreaAndCentroid) {
  EXPECT_EQ(0.0, empty_->GetArea());
  EXPECT_EQ(4 * M_PI, full_->GetArea());
  EXPECT_EQ(S2Point(0, 0, 0), empty_->GetCentroid());
  EXPECT_EQ(S2Point(0, 0, 0), full_->GetCentroid());

  EXPECT_DOUBLE_EQ(north_hemi_->GetArea(), 2 * M_PI);
  EXPECT_NEAR(east_hemi_->GetArea(), 2 * M_PI, 1e-15);

  // Construct spherical caps of random height, and approximate their boundary
  // with closely spaces vertices.  Then check that the area and centroid are
  // correct.

  for (int i = 0; i < 50; ++i) {
    // Choose a coordinate frame for the spherical cap.
    Vector3_d x, y, z;
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

    vector<S2Point> vertices;
    for (double theta = 0; theta < 2 * M_PI;
         theta += S2Testing::rnd.RandDouble() * max_dtheta) {
      vertices.push_back(cos(theta) * cos(phi) * x +
                         sin(theta) * cos(phi) * y +
                         sin(phi) * z);
    }
    S2Loop loop(vertices);
    double area = loop.GetArea();
    S2Point centroid = loop.GetCentroid();
    double expected_area = 2 * M_PI * height;
    EXPECT_LE(fabs(area - expected_area), 2 * M_PI * kMaxDist);
    S2Point expected_centroid = expected_area * (1 - 0.5 * height) * z;
    EXPECT_LE((centroid - expected_centroid).Norm(), 2 * kMaxDist);
  }
}

// Check that the curvature is *identical* when the vertex order is
// rotated, and that the sign is inverted when the vertices are reversed.
static void CheckCurvatureInvariants(const S2Loop& loop) {
  double expected = loop.GetCurvature();
  unique_ptr<S2Loop> loop_copy(loop.Clone());
  for (int i = 0; i < loop.num_vertices(); ++i) {
    loop_copy->Invert();
    EXPECT_EQ(-expected, loop_copy->GetCurvature());
    loop_copy->Invert();
    Rotate(&loop_copy);
    EXPECT_EQ(expected, loop_copy->GetCurvature());
  }
}

TEST_F(S2LoopTestBase, GetCurvature) {
  EXPECT_EQ(2 * M_PI, empty_->GetCurvature());
  EXPECT_EQ(-2 * M_PI, full_->GetCurvature());

  EXPECT_NEAR(0, north_hemi3_->GetCurvature(), 1e-15);
  CheckCurvatureInvariants(*north_hemi3_);

  EXPECT_NEAR(0, west_hemi_->GetCurvature(), 1e-15);
  CheckCurvatureInvariants(*west_hemi_);

  // We don't have an easy way to estimate the curvature of this loop, but
  // we can still check that the expected invariants hold.
  CheckCurvatureInvariants(*candy_cane_);

  EXPECT_DOUBLE_EQ(2 * M_PI, line_triangle_->GetCurvature());
  CheckCurvatureInvariants(*line_triangle_);

  EXPECT_DOUBLE_EQ(2 * M_PI, skinny_chevron_->GetCurvature());
  CheckCurvatureInvariants(*skinny_chevron_);

  // Build a narrow spiral loop starting at the north pole.  This is designed
  // to test that the error in GetCurvature is linear in the number of
  // vertices even when the partial sum of the curvatures gets very large.
  // The spiral consists of two "arms" defining opposite sides of the loop.
  const int kArmPoints = 10000;    // Number of vertices in each "arm"
  const double kArmRadius = 0.01;  // Radius of spiral.
  vector<S2Point> vertices(2 * kArmPoints);
  vertices[kArmPoints] = S2Point(0, 0, 1);
  for (int i = 0; i < kArmPoints; ++i) {
    double angle = (2 * M_PI / 3) * i;
    double x = cos(angle);
    double y = sin(angle);
    double r1 = i * kArmRadius / kArmPoints;
    double r2 = (i + 1.5) * kArmRadius / kArmPoints;
    vertices[kArmPoints - i - 1] = S2Point(r1 * x, r1 * y, 1).Normalize();
    vertices[kArmPoints + i] = S2Point(r2 * x, r2 * y, 1).Normalize();
  }
  // This is a pathological loop that contains many long parallel edges, and
  // takes tens of seconds to validate in debug mode.
  S2Loop spiral(vertices, S2Debug::DISABLE);

  // Check that GetCurvature() is consistent with GetArea() to within the
  // error bound of the former.  We actually use a tiny fraction of the
  // worst-case error bound, since the worst case only happens when all the
  // roundoff errors happen in the same direction and this test is not
  // designed to achieve that.  The error in GetArea() can be ignored for the
  // purposes of this test since it is generally much smaller.
  EXPECT_NEAR(2 * M_PI - spiral.GetArea(), spiral.GetCurvature(),
              0.01 * spiral.GetCurvatureMaxError());
}

// Checks that if a loop is normalized, it doesn't contain a
// point outside of it, and vice versa.
static void CheckNormalizeAndContains(const S2Loop& loop) {
  S2Point p = s2textformat::MakePoint("40:40");

  unique_ptr<S2Loop> flip(loop.Clone());
  flip->Invert();
  EXPECT_TRUE(loop.IsNormalized() ^ loop.Contains(p));
  EXPECT_TRUE(flip->IsNormalized() ^ flip->Contains(p));

  EXPECT_TRUE(loop.IsNormalized() ^ flip->IsNormalized());

  flip->Normalize();
  EXPECT_FALSE(flip->Contains(p));
}

TEST_F(S2LoopTestBase, NormalizedCompatibleWithContains) {
  CheckNormalizeAndContains(*line_triangle_);
  CheckNormalizeAndContains(*skinny_chevron_);
}

TEST_F(S2LoopTestBase, Contains) {
  // Check the full and empty loops have the correct containment relationship
  // with the special "vertex" that defines them.
  EXPECT_FALSE(empty_->Contains(S2Loop::kEmpty()[0]));
  EXPECT_TRUE(full_->Contains(S2Loop::kFull()[0]));

  EXPECT_TRUE(candy_cane_->Contains(S2LatLng::FromDegrees(5, 71).ToPoint()));

  // Create copies of these loops so that we can change the vertex order.
  unique_ptr<S2Loop> north_copy(north_hemi_->Clone());
  unique_ptr<S2Loop> south_copy(south_hemi_->Clone());
  unique_ptr<S2Loop> west_copy(west_hemi_->Clone());
  unique_ptr<S2Loop> east_copy(east_hemi_->Clone());
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(north_copy->Contains(S2Point(0, 0, 1)));
    EXPECT_FALSE(north_copy->Contains(S2Point(0, 0, -1)));
    EXPECT_FALSE(south_copy->Contains(S2Point(0, 0, 1)));
    EXPECT_TRUE(south_copy->Contains(S2Point(0, 0, -1)));
    EXPECT_FALSE(west_copy->Contains(S2Point(0, 1, 0)));
    EXPECT_TRUE(west_copy->Contains(S2Point(0, -1, 0)));
    EXPECT_TRUE(east_copy->Contains(S2Point(0, 1, 0)));
    EXPECT_FALSE(east_copy->Contains(S2Point(0, -1, 0)));
    Rotate(&north_copy);
    Rotate(&south_copy);
    Rotate(&east_copy);
    Rotate(&west_copy);
  }

  // This code checks each cell vertex is contained by exactly one of
  // the adjacent cells.
  for (int level = 0; level < 3; ++level) {
    vector<unique_ptr<S2Loop>> loops;
    vector<S2Point> loop_vertices;
    set<S2Point> points;
    for (S2CellId id = S2CellId::Begin(level);
         id != S2CellId::End(level); id = id.next()) {
      S2Cell cell(id);
      points.insert(cell.GetCenter());
      for (int k = 0; k < 4; ++k) {
        loop_vertices.push_back(cell.GetVertex(k));
        points.insert(cell.GetVertex(k));
      }
      loops.push_back(make_unique<S2Loop>(loop_vertices));
      loop_vertices.clear();
    }
    for (const S2Point& point : points) {
      int count = 0;
      for (const auto& loop : loops) {
        if (loop->Contains(point)) ++count;
      }
      EXPECT_EQ(count, 1);
    }
  }
}

TEST(S2Loop, ContainsMatchesCrossingSign) {
  // This test demonstrates a former incompatibility between CrossingSign()
  // and Contains(const S2Point&).  It constructs an S2Cell-based loop L and
  // an edge E from Origin to a0 that crosses exactly one edge of L.  Yet
  // previously, Contains() returned false for both endpoints of E.
  //
  // The reason for the bug was that the loop bound was sometimes too tight.
  // The Contains() code for a0 bailed out early because a0 was found not to
  // be inside the bound of L.

  // Start with a cell that ends up producing the problem.
  const S2CellId cell_id = S2CellId(S2Point(1, 1, 1)).parent(21);

  S2Cell children[4];
  S2Cell(cell_id).Subdivide(children);

  vector<S2Point> points(4);
  for (int i = 0; i < 4; ++i) {
    // Note extra normalization. GetCenter() is already normalized.
    // The test results will no longer be inconsistent if the extra
    // Normalize() is removed.
    points[i] = children[i].GetCenter().Normalize();
  }

  const S2Loop loop(points);

  // Get a vertex from a grandchild cell.
  // +---------------+---------------+
  // |               |               |
  // |    points[3]  |   points[2]   |
  // |       v       |       v       |
  // |       +-------+------ +       |
  // |       |       |       |       |
  // |       |       |       |       |
  // |       |       |       |       |
  // +-------+-------+-------+-------+
  // |       |       |       |       |
  // |       |    <----------------------- grandchild_cell
  // |       |       |       |       |
  // |       +-------+------ +       |
  // |       ^       |       ^       | <-- cell
  // | points[0]/a0  |     points[1] |
  // |               |               |
  // +---------------+---------------+
  const S2Cell grandchild_cell(cell_id.child(0).child(2));
  const S2Point a0 = grandchild_cell.GetVertex(0);

  // If this doesn't hold, the rest of the test is pointless.
  ASSERT_NE(points[0], a0)
      << "This test depends on rounding errors that should make "
         "a0 slightly different from points[0]"
      << std::setprecision(20) << "\npoints[0]:" << points[0]
      << "\n       a0:" << a0;

  // The edge from a0 to the origin crosses one boundary.
  EXPECT_EQ(-1, S2::CrossingSign(a0, S2::Origin(),
                                         loop.vertex(0), loop.vertex(1)));
  EXPECT_EQ(1, S2::CrossingSign(a0, S2::Origin(),
                                        loop.vertex(1), loop.vertex(2)));
  EXPECT_EQ(-1, S2::CrossingSign(a0, S2::Origin(),
                                         loop.vertex(2), loop.vertex(3)));
  EXPECT_EQ(-1, S2::CrossingSign(a0, S2::Origin(),
                                         loop.vertex(3), loop.vertex(4)));

  // Contains should return false for the origin, and true for a0.
  EXPECT_FALSE(loop.Contains(S2::Origin()));
  EXPECT_TRUE(loop.Contains(a0));

  // Since a0 is inside the loop, it should be inside the bound.
  const S2LatLngRect& bound = loop.GetRectBound();
  EXPECT_TRUE(bound.Contains(a0));
}

// Given a pair of loops where A contains B, check various identities.
static void TestOneNestedPair(const S2Loop& a, const S2Loop& b) {
  EXPECT_TRUE(a.Contains(&b));
  EXPECT_EQ(a.BoundaryEquals(&b), b.Contains(&a));
  EXPECT_EQ(!b.is_empty(), a.Intersects(&b));
  EXPECT_EQ(!b.is_empty(), b.Intersects(&a));
}

// Given a pair of disjoint loops A and B, check various identities.
static void TestOneDisjointPair(const S2Loop& a, const S2Loop& b) {
  EXPECT_FALSE(a.Intersects(&b));
  EXPECT_FALSE(b.Intersects(&a));
  EXPECT_EQ(b.is_empty(), a.Contains(&b));
  EXPECT_EQ(a.is_empty(), b.Contains(&a));
}

// Given loops A and B whose union covers the sphere, check various identities.
static void TestOneCoveringPair(const S2Loop& a, const S2Loop& b) {
  EXPECT_EQ(a.is_full(), a.Contains(&b));
  EXPECT_EQ(b.is_full(), b.Contains(&a));
  unique_ptr<S2Loop> a1(a.Clone());
  a1->Invert();
  bool complementary = a1->BoundaryEquals(&b);
  EXPECT_EQ(!complementary, a.Intersects(&b));
  EXPECT_EQ(!complementary, b.Intersects(&a));
}

// Given loops A and B such that both A and its complement intersect both B
// and its complement, check various identities.
static void TestOneOverlappingPair(const S2Loop& a, const S2Loop& b) {
  EXPECT_FALSE(a.Contains(&b));
  EXPECT_FALSE(b.Contains(&a));
  EXPECT_TRUE(a.Intersects(&b));
  EXPECT_TRUE(b.Intersects(&a));
}

// Given a pair of loops where A contains B, test various identities
// involving A, B, and their complements.
static void TestNestedPair(const S2Loop& a, const S2Loop& b) {
  unique_ptr<S2Loop> a1(a.Clone());
  unique_ptr<S2Loop> b1(b.Clone());
  a1->Invert();
  b1->Invert();
  TestOneNestedPair(a, b);
  TestOneNestedPair(*b1, *a1);
  TestOneDisjointPair(*a1, b);
  TestOneCoveringPair(a, *b1);
}

// Given a pair of disjoint loops A and B, test various identities
// involving A, B, and their complements.
static void TestDisjointPair(const S2Loop& a, const S2Loop& b) {
  unique_ptr<S2Loop> a1(a.Clone());
  a1->Invert();
  TestNestedPair(*a1, b);
}

// Given loops A and B whose union covers the sphere, test various identities
// involving A, B, and their complements.
static void TestCoveringPair(const S2Loop& a, const S2Loop& b) {
  unique_ptr<S2Loop> b1(b.Clone());
  b1->Invert();
  TestNestedPair(a, *b1);
}

// Given loops A and B such that both A and its complement intersect both B
// and its complement, test various identities involving these four loops.
static void TestOverlappingPair(const S2Loop& a, const S2Loop& b) {
  unique_ptr<S2Loop> a1(a.Clone());
  unique_ptr<S2Loop> b1(b.Clone());
  a1->Invert();
  b1->Invert();
  TestOneOverlappingPair(a, b);
  TestOneOverlappingPair(*a1, *b1);
  TestOneOverlappingPair(*a1, b);
  TestOneOverlappingPair(a, *b1);
}

enum RelationFlags {
  CONTAINS =  0x01,  // A contains B
  CONTAINED = 0x02,  // B contains A
  DISJOINT =  0x04,  // A and B are disjoint (intersection is empty)
  COVERS =    0x08,  // (A union B) covers the entire sphere
};

// Verify the relationship between two loops A and B.  "flags" is the set of
// RelationFlags that apply.  "shared_edge" means that the loops share at
// least one edge (possibly reversed).
static void TestRelationWithDesc(const S2Loop& a, const S2Loop& b,
                                 int flags, bool shared_edge,
                                 const char* test_description) {
  SCOPED_TRACE(test_description);
  if (flags & CONTAINS) {
    TestNestedPair(a, b);
  }
  if (flags & CONTAINED) {
    TestNestedPair(b, a);
  }
  if (flags & COVERS) {
    TestCoveringPair(a, b);
  }
  if (flags & DISJOINT) {
    TestDisjointPair(a, b);
  } else if (!(flags & (CONTAINS|CONTAINED|COVERS))) {
    TestOverlappingPair(a, b);
  }
  if (!shared_edge && (flags & (CONTAINS|CONTAINED|DISJOINT))) {
    EXPECT_EQ(a.Contains(&b), a.ContainsNested(&b));
  }
  // A contains the boundary of B if either A contains B, or the two loops
  // contain each other's boundaries and there are no shared edges (since at
  // least one such edge must be reversed, and therefore is not considered to
  // be contained according to the rules of CompareBoundary).
  int comparison = 0;
  if ((flags & CONTAINS) || ((flags & COVERS) && !shared_edge)) {
    comparison = 1;
  }
  // Similarly, A excludes the boundary of B if either A and B are disjoint,
  // or B contains A and there are no shared edges (since A is considered to
  // contain such edges according to the rules of CompareBoundary).
  if ((flags & DISJOINT) || ((flags & CONTAINED) && !shared_edge)) {
    comparison = -1;
  }
  // CompareBoundary requires that neither loop is empty.
  if (!a.is_empty() && !b.is_empty()) {
    EXPECT_EQ(comparison, a.CompareBoundary(&b));
  }
}

#define TestRelation(a, b, flags, shared_edge)                          \
  TestRelationWithDesc(*a, *b, flags, shared_edge, "args " #a ", " #b)

TEST_F(S2LoopTestBase, LoopRelations) {
  // Check full and empty relationships with normal loops and each other.
  TestRelation(full_, full_, CONTAINS|CONTAINED|COVERS, true);
  TestRelation(full_, north_hemi_, CONTAINS|COVERS, false);
  TestRelation(full_, empty_, CONTAINS|DISJOINT|COVERS, false);
  TestRelation(north_hemi_, full_, CONTAINED|COVERS, false);
  TestRelation(north_hemi_, empty_, CONTAINS|DISJOINT, false);
  TestRelation(empty_, full_, CONTAINED|DISJOINT|COVERS, false);
  TestRelation(empty_, north_hemi_, CONTAINED|DISJOINT, false);
  TestRelation(empty_, empty_, CONTAINS|CONTAINED|DISJOINT, false);

  TestRelation(north_hemi_, north_hemi_, CONTAINS|CONTAINED, true);
  TestRelation(north_hemi_, south_hemi_, DISJOINT|COVERS, true);
  TestRelation(north_hemi_, east_hemi_, 0, false);
  TestRelation(north_hemi_, arctic_80_, CONTAINS, false);
  TestRelation(north_hemi_, antarctic_80_, DISJOINT, false);
  TestRelation(north_hemi_, candy_cane_, 0, false);

  // We can't compare north_hemi3 vs. north_hemi or south_hemi because the
  // result depends on the "simulation of simplicity" implementation details.
  TestRelation(north_hemi3_, north_hemi3_, CONTAINS|CONTAINED, true);
  TestRelation(north_hemi3_, east_hemi_, 0, false);
  TestRelation(north_hemi3_, arctic_80_, CONTAINS, false);
  TestRelation(north_hemi3_, antarctic_80_, DISJOINT, false);
  TestRelation(north_hemi3_, candy_cane_, 0, false);

  TestRelation(south_hemi_, north_hemi_, DISJOINT|COVERS, true);
  TestRelation(south_hemi_, south_hemi_, CONTAINS|CONTAINED, true);
  TestRelation(south_hemi_, far_hemi_, 0, false);
  TestRelation(south_hemi_, arctic_80_, DISJOINT, false);
  TestRelation(south_hemi_, antarctic_80_, CONTAINS, false);
  TestRelation(south_hemi_, candy_cane_, 0, false);

  TestRelation(candy_cane_, north_hemi_, 0, false);
  TestRelation(candy_cane_, south_hemi_, 0, false);
  TestRelation(candy_cane_, arctic_80_, DISJOINT, false);
  TestRelation(candy_cane_, antarctic_80_, DISJOINT, false);
  TestRelation(candy_cane_, candy_cane_, CONTAINS|CONTAINED, true);

  TestRelation(near_hemi_, west_hemi_, 0, false);

  TestRelation(small_ne_cw_, south_hemi_, CONTAINS, false);
  TestRelation(small_ne_cw_, west_hemi_, CONTAINS, false);

  TestRelation(small_ne_cw_, north_hemi_, COVERS, false);
  TestRelation(small_ne_cw_, east_hemi_, COVERS, false);

  TestRelation(loop_a_, loop_a_, CONTAINS|CONTAINED, true);
  TestRelation(loop_a_, loop_b_, 0, false);
  TestRelation(loop_a_, a_intersect_b_, CONTAINS, true);
  TestRelation(loop_a_, a_union_b_, CONTAINED, true);
  TestRelation(loop_a_, a_minus_b_, CONTAINS, true);
  TestRelation(loop_a_, b_minus_a_, DISJOINT, true);

  TestRelation(loop_b_, loop_a_, 0, false);
  TestRelation(loop_b_, loop_b_, CONTAINS|CONTAINED, true);
  TestRelation(loop_b_, a_intersect_b_, CONTAINS, true);
  TestRelation(loop_b_, a_union_b_, CONTAINED, true);
  TestRelation(loop_b_, a_minus_b_, DISJOINT, true);
  TestRelation(loop_b_, b_minus_a_, CONTAINS, true);

  TestRelation(a_intersect_b_, loop_a_, CONTAINED, true);
  TestRelation(a_intersect_b_, loop_b_, CONTAINED, true);
  TestRelation(a_intersect_b_, a_intersect_b_, CONTAINS|CONTAINED, true);
  TestRelation(a_intersect_b_, a_union_b_, CONTAINED, false);
  TestRelation(a_intersect_b_, a_minus_b_, DISJOINT, true);
  TestRelation(a_intersect_b_, b_minus_a_, DISJOINT, true);

  TestRelation(a_union_b_, loop_a_, CONTAINS, true);
  TestRelation(a_union_b_, loop_b_, CONTAINS, true);
  TestRelation(a_union_b_, a_intersect_b_, CONTAINS, false);
  TestRelation(a_union_b_, a_union_b_, CONTAINS|CONTAINED, true);
  TestRelation(a_union_b_, a_minus_b_, CONTAINS, true);
  TestRelation(a_union_b_, b_minus_a_, CONTAINS, true);

  TestRelation(a_minus_b_, loop_a_, CONTAINED, true);
  TestRelation(a_minus_b_, loop_b_, DISJOINT, true);
  TestRelation(a_minus_b_, a_intersect_b_, DISJOINT, true);
  TestRelation(a_minus_b_, a_union_b_, CONTAINED, true);
  TestRelation(a_minus_b_, a_minus_b_, CONTAINS|CONTAINED, true);
  TestRelation(a_minus_b_, b_minus_a_, DISJOINT, false);

  TestRelation(b_minus_a_, loop_a_, DISJOINT, true);
  TestRelation(b_minus_a_, loop_b_, CONTAINED, true);
  TestRelation(b_minus_a_, a_intersect_b_, DISJOINT, true);
  TestRelation(b_minus_a_, a_union_b_, CONTAINED, true);
  TestRelation(b_minus_a_, a_minus_b_, DISJOINT, false);
  TestRelation(b_minus_a_, b_minus_a_, CONTAINS|CONTAINED, true);
}

// Make sure the relations are correct if the loop crossing happens on
// two ends of a shared boundary segment.
TEST_F(S2LoopTestBase, LoopRelationsWhenSameExceptPiecesStickingOutAndIn) {
  TestRelation(loop_a_, loop_c_, 0, true);
  TestRelation(loop_c_, loop_a_, 0, true);
  TestRelation(loop_a_, loop_d_, CONTAINED, true);
  TestRelation(loop_d_, loop_a_, CONTAINS, true);
  TestRelation(loop_e_, loop_f_, DISJOINT, true);
  TestRelation(loop_e_, loop_g_, CONTAINS, true);
  TestRelation(loop_e_, loop_h_, 0, true);
  TestRelation(loop_e_, loop_i_, 0, false);
  TestRelation(loop_f_, loop_g_, DISJOINT, true);
  TestRelation(loop_f_, loop_h_, 0, true);
  TestRelation(loop_f_, loop_i_, 0, false);
  TestRelation(loop_g_, loop_h_, CONTAINED, true);
  TestRelation(loop_h_, loop_g_, CONTAINS, true);
  TestRelation(loop_g_, loop_i_, DISJOINT, true);
  TestRelation(loop_h_, loop_i_, CONTAINS, true);
}

#undef TestRelation

static unique_ptr<S2Loop> MakeCellLoop(S2CellId begin, S2CellId end) {
  // Construct a CCW polygon whose boundary is the union of the cell ids
  // in the range [begin, end).  We add the edges one by one, removing
  // any edges that are already present in the opposite direction.

  map<S2Point, set<S2Point>> edges;
  for (S2CellId id = begin; id != end; id = id.next()) {
    S2Cell cell(id);
    for (int k = 0; k < 4; ++k) {
      S2Point a = cell.GetVertex(k);
      S2Point b = cell.GetVertex(k + 1);
      if (edges[b].erase(a) == 0) {
        edges[a].insert(b);
      } else if (edges[b].empty()) {
        edges.erase(b);
      }
    }
  }

  // The remaining edges form a single loop.  We simply follow it starting
  // at an arbitrary vertex and build up a list of vertices.

  vector<S2Point> vertices;
  S2Point p = edges.begin()->first;
  while (!edges.empty()) {
    S2_DCHECK_EQ(1, edges[p].size());
    S2Point next = *edges[p].begin();
    vertices.push_back(p);
    edges.erase(p);
    p = next;
  }

  return make_unique<S2Loop>(vertices);
}

TEST(S2Loop, LoopRelations2) {
  // Construct polygons consisting of a sequence of adjacent cell ids
  // at some fixed level.  Comparing two polygons at the same level
  // ensures that there are no T-vertices.
  for (int iter = 0; iter < 1000; ++iter) {
    S2Testing::Random& rnd = S2Testing::rnd;
    S2CellId begin = S2CellId(rnd.Rand64() | 1);
    if (!begin.is_valid()) continue;
    begin = begin.parent(rnd.Uniform(S2CellId::kMaxLevel));
    S2CellId a_begin = begin.advance(rnd.Skewed(6));
    S2CellId a_end = a_begin.advance(rnd.Skewed(6) + 1);
    S2CellId b_begin = begin.advance(rnd.Skewed(6));
    S2CellId b_end = b_begin.advance(rnd.Skewed(6) + 1);
    if (!a_end.is_valid() || !b_end.is_valid()) continue;

    unique_ptr<S2Loop> a(MakeCellLoop(a_begin, a_end));
    unique_ptr<S2Loop> b(MakeCellLoop(b_begin, b_end));
    if (a.get() && b.get()) {
      bool contained = (a_begin <= b_begin && b_end <= a_end);
      bool intersects = (a_begin < b_end && b_begin < a_end);
      S2_VLOG(1) << "Checking " << a->num_vertices() << " vs. "
              << b->num_vertices() << ", contained = " << contained
              << ", intersects = " << intersects;
      EXPECT_EQ(a->Contains(b.get()), contained);
      EXPECT_EQ(a->Intersects(b.get()), intersects);
    } else {
      S2_VLOG(1) << "MakeCellLoop failed to create a loop.";
    }
  }
}

TEST(S2Loop, BoundsForLoopContainment) {
  // To reliably test whether one loop contains another, the bounds of the
  // outer loop are expanded slightly.  This test constructs examples where
  // this expansion is necessary and verifies that it is sufficient.

  S2Testing::Random* rnd = &S2Testing::rnd;
  for (int iter = 0; iter < 1000; ++iter) {
    // We construct a triangle ABC such that A,B,C are nearly colinear, B is
    // the point of maximum latitude, and the edge AC passes very slightly
    // below B (i.e., ABC is CCW).
    S2Point b = (S2Testing::RandomPoint() + S2Point(0, 0, 1)).Normalize();
    S2Point v = b.CrossProd(S2Point(0, 0, 1)).Normalize();
    S2Point a = S2::Interpolate(rnd->RandDouble(), -v, b);
    S2Point c = S2::Interpolate(rnd->RandDouble(), b, v);
    if (s2pred::Sign(a, b, c) < 0) {
      --iter; continue;
    }
    // Now construct another point D directly below B, and create two loops
    // ABCD and ACD.
    S2Point d = S2Point(b.x(), b.y(), 0).Normalize();
    S2Point vertices[] = { c, d, a, b };  // Reordered for convenience
    S2Loop outer(vector<S2Point>(vertices, vertices + 4));
    S2Loop inner(vector<S2Point>(vertices, vertices + 3));
    // Now because the bounds calculation is less accurate when the maximum is
    // attained along an edge (rather than at a vertex), sometimes the inner
    // loop will have a *larger* bounding box than the outer loop.  We look
    // only for those cases.
    if (outer.GetRectBound().Contains(inner.GetRectBound())) {
      --iter; continue;
    }
    EXPECT_TRUE(outer.Contains(&inner));
  }
}

void DebugDumpCrossings(const S2Loop& loop) {
  // This function is useful for debugging.

  S2_LOG(INFO) << "Ortho(v1): " << S2::Ortho(loop.vertex(1));
  printf("Contains(kOrigin): %d\n", loop.Contains(S2::Origin()));
  for (int i = 1; i <= loop.num_vertices(); ++i) {
    S2Point a = S2::Ortho(loop.vertex(i));
    S2Point b = loop.vertex(i-1);
    S2Point c = loop.vertex(i+1);
    S2Point o = loop.vertex(i);
    printf("Vertex %d: [%.17g, %.17g, %.17g], "
           "%d%dR=%d, %d%d%d=%d, R%d%d=%d, inside: %d\n",
           i, loop.vertex(i).x(), loop.vertex(i).y(), loop.vertex(i).z(),
           i - 1, i, s2pred::Sign(b, o, a),
           i + 1, i, i - 1, s2pred::Sign(c, o, b),
           i, i + 1, s2pred::Sign(a, o, c),
           s2pred::OrderedCCW(a, b, c, o));
  }
  for (int i = 0; i < loop.num_vertices() + 2; ++i) {
    S2Point orig = S2::Origin();
    S2Point dest;
    if (i < loop.num_vertices()) {
      dest = loop.vertex(i);
      printf("Origin->%d crosses:", i);
    } else {
      dest = S2Point(0, 0, 1);
      if (i == loop.num_vertices() + 1) orig = loop.vertex(1);
      printf("Case %d:", i);
    }
    for (int j = 0; j < loop.num_vertices(); ++j) {
      printf(" %d", S2::EdgeOrVertexCrossing(
                 orig, dest, loop.vertex(j), loop.vertex(j+1)));
    }
    printf("\n");
  }
  for (int i = 0; i <= 2; i += 2) {
    printf("Origin->v1 crossing v%d->v1: ", i);
    S2Point a = S2::Ortho(loop.vertex(1));
    S2Point b = loop.vertex(i);
    S2Point c = S2::Origin();
    S2Point o = loop.vertex(1);
    printf("%d1R=%d, M1%d=%d, R1M=%d, crosses: %d\n",
           i, s2pred::Sign(b, o, a),
           i, s2pred::Sign(c, o, b),
           s2pred::Sign(a, o, c),
           S2::EdgeOrVertexCrossing(c, o, b, a));
  }
}

static void TestNear(const char* a_str, const char* b_str,
                     S1Angle max_error, bool expected) {
  unique_ptr<S2Loop> a(s2textformat::MakeLoop(a_str));
  unique_ptr<S2Loop> b(s2textformat::MakeLoop(b_str));
  EXPECT_EQ(a->BoundaryNear(*b, max_error), expected);
  EXPECT_EQ(b->BoundaryNear(*a, max_error), expected);
}

TEST(S2Loop, BoundaryNear) {
  S1Angle degree = S1Angle::Degrees(1);

  TestNear("0:0, 0:10, 5:5",
           "0:0.1, -0.1:9.9, 5:5.2",
           0.5 * degree, true);
  TestNear("0:0, 0:3, 0:7, 0:10, 3:7, 5:5",
           "0:0, 0:10, 2:8, 5:5, 4:4, 3:3, 1:1",
           S1Angle::Radians(1e-3), true);

  // All vertices close to some edge, but not equivalent.
  TestNear("0:0, 0:2, 2:2, 2:0",
           "0:0, 1.9999:1, 0:2, 2:2, 2:0",
           0.5 * degree, false);

  // Two triangles that backtrack a bit on different edges.  A simple
  // greedy matching algorithm would fail on this example.
  const char* t1 = "0.1:0, 0.1:1, 0.1:2, 0.1:3, 0.1:4, 1:4, 2:4, 3:4, "
                   "2:4.1, 1:4.1, 2:4.2, 3:4.2, 4:4.2, 5:4.2";
  const char* t2 = "0:0, 0:1, 0:2, 0:3, 0.1:2, 0.1:1, 0.2:2, 0.2:3, "
                   "0.2:4, 1:4.1, 2:4, 3:4, 4:4, 5:4";
  TestNear(t1, t2, 1.5 * degree, true);
  TestNear(t1, t2, 0.5 * degree, false);
}

static void CheckIdentical(const S2Loop& loop, const S2Loop& loop2) {
  EXPECT_EQ(loop.depth(), loop2.depth());
  EXPECT_EQ(loop.num_vertices(), loop2.num_vertices());
  for (int i = 0; i < loop.num_vertices(); ++i) {
    EXPECT_EQ(loop.vertex(i), loop2.vertex(i));
  }
  EXPECT_EQ(loop.is_empty(), loop2.is_empty());
  EXPECT_EQ(loop.is_full(), loop2.is_full());
  EXPECT_EQ(loop.depth(), loop2.depth());
  EXPECT_EQ(loop.IsNormalized(), loop2.IsNormalized());
  EXPECT_EQ(loop.Contains(S2::Origin()), loop2.Contains(S2::Origin()));
  EXPECT_EQ(loop.GetRectBound(), loop2.GetRectBound());
}

static void TestEncodeDecode(const S2Loop& loop) {
  Encoder encoder;
  loop.Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  S2Loop loop2;
  loop2.set_s2debug_override(loop.s2debug_override());
  ASSERT_TRUE(loop2.Decode(&decoder));
  CheckIdentical(loop, loop2);
}

TEST(S2Loop, EncodeDecode) {
  unique_ptr<S2Loop> l(s2textformat::MakeLoop("30:20, 40:20, 39:43, 33:35"));
  l->set_depth(3);
  TestEncodeDecode(*l);

  S2Loop empty(S2Loop::kEmpty());
  TestEncodeDecode(empty);
  S2Loop full(S2Loop::kFull());
  TestEncodeDecode(full);

  S2Loop uninitialized;
  TestEncodeDecode(uninitialized);
}

static void TestEmptyFullSnapped(const S2Loop& loop, int level) {
  S2_CHECK(loop.is_empty_or_full());
  S2CellId cellid = S2CellId(loop.vertex(0)).parent(level);
  vector<S2Point> vertices = {cellid.ToPoint()};
  S2Loop loop2(vertices);
  EXPECT_TRUE(loop.BoundaryEquals(&loop2));
  EXPECT_TRUE(loop.BoundaryApproxEquals(loop2));
  EXPECT_TRUE(loop.BoundaryNear(loop2));
}

// Test converting the empty/full loops to S2LatLng representations.  (We
// don't bother testing E5/E6/E7 because that test is less demanding.)
static void TestEmptyFullLatLng(const S2Loop& loop) {
  S2_CHECK(loop.is_empty_or_full());
  vector<S2Point> vertices = {S2LatLng(loop.vertex(0)).ToPoint()};
  S2Loop loop2(vertices);
  EXPECT_TRUE(loop.BoundaryEquals(&loop2));
  EXPECT_TRUE(loop.BoundaryApproxEquals(loop2));
  EXPECT_TRUE(loop.BoundaryNear(loop2));
}

static void TestEmptyFullConversions(const S2Loop& loop) {
  TestEmptyFullSnapped(loop, S2CellId::kMaxLevel);
  TestEmptyFullSnapped(loop, 1);  // Worst case for approximation
  TestEmptyFullSnapped(loop, 0);
  TestEmptyFullLatLng(loop);
}

TEST(S2Loop, EmptyFullLossyConversions) {
  // Verify that the empty and full loops can be encoded lossily.
  S2Loop empty(S2Loop::kEmpty());
  TestEmptyFullConversions(empty);

  S2Loop full(S2Loop::kFull());
  TestEmptyFullConversions(full);
}

TEST(S2Loop, EncodeDecodeWithinScope) {
  unique_ptr<S2Loop> l(s2textformat::MakeLoop("30:20, 40:20, 39:43, 33:35"));
  l->set_depth(3);
  Encoder encoder;
  l->Encode(&encoder);
  Decoder decoder1(encoder.base(), encoder.length());

  // Initialize the loop using DecodeWithinScope and check that it is the
  // same as the original loop.
  S2Loop loop1;
  ASSERT_TRUE(loop1.DecodeWithinScope(&decoder1));
  EXPECT_TRUE(l->BoundaryEquals(&loop1));
  EXPECT_EQ(l->depth(), loop1.depth());
  EXPECT_EQ(l->GetRectBound(), loop1.GetRectBound());

  // Initialize the same loop using Init with a vector of vertices, and
  // check that it doesn't deallocate the original memory.
  vector<S2Point> vertices = {loop1.vertex(0), loop1.vertex(2),
                              loop1.vertex(3)};
  loop1.Init(vertices);
  Decoder decoder2(encoder.base(), encoder.length());
  S2Loop loop2;
  ASSERT_TRUE(loop2.DecodeWithinScope(&decoder2));
  EXPECT_TRUE(l->BoundaryEquals(&loop2));
  EXPECT_EQ(l->vertex(1), loop2.vertex(1));
  EXPECT_NE(loop1.vertex(1), loop2.vertex(1));

  // Initialize loop2 using Decode with a decoder on different data.
  // Check that the original memory is not deallocated or overwritten.
  unique_ptr<S2Loop> l2(s2textformat::MakeLoop("30:40, 40:75, 39:43, 80:35"));
  l2->set_depth(2);
  Encoder encoder2;
  l2->Encode(&encoder2);
  Decoder decoder3(encoder2.base(), encoder2.length());
  ASSERT_TRUE(loop2.Decode(&decoder3));
  Decoder decoder4(encoder.base(), encoder.length());
  ASSERT_TRUE(loop1.DecodeWithinScope(&decoder4));
  EXPECT_TRUE(l->BoundaryEquals(&loop1));
  EXPECT_EQ(l->vertex(1), loop1.vertex(1));
  EXPECT_NE(loop1.vertex(1), loop2.vertex(1));
}

TEST_F(S2LoopTestBase, FourVertexCompressedLoopRequires36Bytes) {
  Encoder encoder;
  TestEncodeCompressed(*snapped_loop_a_, S2CellId::kMaxLevel, &encoder);

  // 1 byte for num_vertices
  // 1 byte for origin_inside and boolean indicating we did not
  //   encode the bound
  // 1 byte for depth
  // Vertices:
  // 1 byte for faces
  // 8 bytes for each vertex.
  // 1 byte indicating that there is no unsnapped vertex.
  EXPECT_EQ(37, encoder.length());
}

TEST_F(S2LoopTestBase, CompressedEncodedLoopDecodesApproxEqual) {
  unique_ptr<S2Loop> loop(snapped_loop_a_->Clone());
  loop->set_depth(3);

  Encoder encoder;
  TestEncodeCompressed(*loop, S2CellId::kMaxLevel, &encoder);
  S2Loop decoded_loop;
  TestDecodeCompressed(encoder, S2CellId::kMaxLevel, &decoded_loop);
  CheckIdentical(*loop, decoded_loop);
}

// This test checks that S2Loops created directly from S2Cells behave
// identically to S2Loops created from the vertices of those cells; this
// previously was not the case, because S2Cells calculate their bounding
// rectangles slightly differently, and S2Loops created from them just copied
// the S2Cell bounds.
TEST(S2Loop, S2CellConstructorAndContains) {
  S2Cell cell(S2CellId(S2LatLng::FromE6(40565459, -74645276)));
  S2Loop cell_as_loop(cell);

  vector<S2Point> vertices;
  for (int i = 0; i < cell_as_loop.num_vertices(); ++i) {
    vertices.push_back(cell_as_loop.vertex(i));
  }
  S2Loop loop_copy(vertices);
  EXPECT_TRUE(loop_copy.Contains(&cell_as_loop));
  EXPECT_TRUE(cell_as_loop.Contains(&loop_copy));

  // Demonstrates the reason for this test; the cell bounds are more
  // conservative than the resulting loop bounds.
  EXPECT_FALSE(loop_copy.GetRectBound().Contains(cell.GetRectBound()));
}

// Construct a loop using s2textformat::MakeLoop(str) and check that it
// produces a validation error that includes "snippet".
static void CheckLoopIsInvalid(const string& str, const string& snippet) {
  unique_ptr<S2Loop> loop(s2textformat::MakeLoop(str, S2Debug::DISABLE));
  S2Error error;
  EXPECT_TRUE(loop->FindValidationError(&error));
  EXPECT_NE(string::npos, error.text().find(snippet));
}

static void CheckLoopIsInvalid(const vector<S2Point>& points,
                               const string& snippet) {
  S2Loop l(points, S2Debug::DISABLE);
  S2Error error;
  EXPECT_TRUE(l.FindValidationError(&error));
  EXPECT_NE(string::npos, error.text().find(snippet));
}

TEST(S2Loop, IsValidDetectsInvalidLoops) {
  // Not enough vertices.  Note that all single-vertex loops are valid; they
  // are interpreted as being either empty or full.
  CheckLoopIsInvalid("", "at least 3 vertices");
  CheckLoopIsInvalid("20:20, 21:21", "at least 3 vertices");

  // There is a degenerate edge
  CheckLoopIsInvalid("20:20, 20:20, 20:21", "degenerate");
  CheckLoopIsInvalid("20:20, 20:21, 20:20", "degenerate");

  // There is a duplicate vertex
  CheckLoopIsInvalid("20:20, 21:21, 21:20, 20:20, 20:21", "duplicate vertex");

  // Some edges cross
  CheckLoopIsInvalid("20:20, 21:21, 21:20.5, 21:20, 20:21", "crosses");

  // Points with non-unit length (triggers S2_DCHECK failure in debug)
  EXPECT_DEBUG_DEATH(
      CheckLoopIsInvalid({S2Point(2, 0, 0), S2Point(0, 1, 0), S2Point(0, 0, 1)},
                         "unit length"),
      "IsUnitLength");

  // Adjacent antipodal vertices
  CheckLoopIsInvalid({S2Point(1, 0, 0), S2Point(-1, 0, 0), S2Point(0, 0, 1)},
                    "antipodal");
}

// Helper function for testing the distance methods.  "boundary_x" is the
// expected result of projecting "x" onto the loop boundary.  For convenience
// it can be set to S2Point() to indicate that (boundary_x == x).
static void TestDistanceMethods(const S2Loop& loop, const S2Point& x,
                                S2Point boundary_x) {
  // This error is not guaranteed by the implementation but is okay for tests.
  const S1Angle kMaxError = S1Angle::Radians(1e-15);

  if (boundary_x == S2Point()) boundary_x = x;
  EXPECT_LE(S1Angle(boundary_x, loop.ProjectToBoundary(x)), kMaxError);

  if (loop.is_empty_or_full()) {
    EXPECT_EQ(S1Angle::Infinity(), loop.GetDistanceToBoundary(x));
  } else {
    // EXPECT_NEAR only works with doubles.
    EXPECT_NEAR(S1Angle(x, boundary_x).degrees(),
                loop.GetDistanceToBoundary(x).degrees(), kMaxError.degrees());
  }
  if (loop.Contains(x)) {
    EXPECT_EQ(S1Angle::Zero(), loop.GetDistance(x));
    EXPECT_EQ(x, loop.Project(x));
  } else {
    EXPECT_EQ(loop.GetDistanceToBoundary(x), loop.GetDistance(x));
    EXPECT_EQ(loop.ProjectToBoundary(x), loop.Project(x));
  }
}

TEST_F(S2LoopTestBase, DistanceMethods) {
  // S2ClosestEdgeQuery is already tested, so just do a bit of sanity checking.

  // The empty and full loops don't have boundaries.
  TestDistanceMethods(*empty_, S2Point(0, 1, 0), S2Point());
  TestDistanceMethods(*full_, S2Point(0, 1, 0), S2Point());

  // A CCW square around the S2LatLng point (0,0).  Note that because lines of
  // latitude are curved on the sphere, it is not straightforward to project
  // points onto any edge except along the equator.  (The equator is the only
  // line of latitude that is also a geodesic.)
  unique_ptr<S2Loop> square(s2textformat::MakeLoop("-1:-1, -1:1, 1:1, 1:-1"));
  EXPECT_TRUE(square->IsNormalized());

  // A vertex.
  TestDistanceMethods(*square, S2LatLng::FromDegrees(1, -1).ToPoint(),
                      S2Point());
  // A point on one of the edges.
  TestDistanceMethods(*square, S2LatLng::FromDegrees(0.5, 1).ToPoint(),
                      S2Point());
  // A point inside the square.
  TestDistanceMethods(*square, S2LatLng::FromDegrees(0, 0.5).ToPoint(),
                      S2LatLng::FromDegrees(0, 1).ToPoint());
  // A point outside the square that projects onto an edge.
  TestDistanceMethods(*square, S2LatLng::FromDegrees(0, -2).ToPoint(),
                      S2LatLng::FromDegrees(0, -1).ToPoint());
  // A point outside the square that projects onto a vertex.
  TestDistanceMethods(*square, S2LatLng::FromDegrees(3, 4).ToPoint(),
                      S2LatLng::FromDegrees(1, 1).ToPoint());
}

TEST_F(S2LoopTestBase, MakeRegularLoop) {
  S2Point center = S2LatLng::FromDegrees(80, 135).ToPoint();
  S1Angle radius = S1Angle::Degrees(20);
  unique_ptr<S2Loop> loop(S2Loop::MakeRegularLoop(center, radius, 4));

  ASSERT_EQ(4, loop->num_vertices());
  S2Point p0 = loop->vertex(0);
  S2Point p1 = loop->vertex(1);
  S2Point p2 = loop->vertex(2);
  S2Point p3 = loop->vertex(3);
  // Make sure that the radius is correct.
  EXPECT_DOUBLE_EQ(20.0, S2LatLng(center).GetDistance(S2LatLng(p0)).degrees());
  EXPECT_DOUBLE_EQ(20.0, S2LatLng(center).GetDistance(S2LatLng(p1)).degrees());
  EXPECT_DOUBLE_EQ(20.0, S2LatLng(center).GetDistance(S2LatLng(p2)).degrees());
  EXPECT_DOUBLE_EQ(20.0, S2LatLng(center).GetDistance(S2LatLng(p3)).degrees());
  // Make sure that all angles of the polygon are the same.
  EXPECT_DOUBLE_EQ(M_PI_2, (p1 - p0).Angle(p3 - p0));
  EXPECT_DOUBLE_EQ(M_PI_2, (p2 - p1).Angle(p0 - p1));
  EXPECT_DOUBLE_EQ(M_PI_2, (p3 - p2).Angle(p1 - p2));
  EXPECT_DOUBLE_EQ(M_PI_2, (p0 - p3).Angle(p2 - p3));
  // Make sure that all edges of the polygon have the same length.
  EXPECT_DOUBLE_EQ(27.990890717782829,
                   S2LatLng(p0).GetDistance(S2LatLng(p1)).degrees());
  EXPECT_DOUBLE_EQ(27.990890717782829,
                   S2LatLng(p1).GetDistance(S2LatLng(p2)).degrees());
  EXPECT_DOUBLE_EQ(27.990890717782829,
                   S2LatLng(p2).GetDistance(S2LatLng(p3)).degrees());
  EXPECT_DOUBLE_EQ(27.990890717782829,
                   S2LatLng(p3).GetDistance(S2LatLng(p0)).degrees());

  // Check actual coordinates. This may change if we switch the algorithm
  // intentionally.
  EXPECT_DOUBLE_EQ(62.162880741097204, S2LatLng(p0).lat().degrees());
  EXPECT_DOUBLE_EQ(103.11051028343407, S2LatLng(p0).lng().degrees());
  EXPECT_DOUBLE_EQ(61.955157772928345, S2LatLng(p1).lat().degrees());
  EXPECT_DOUBLE_EQ(165.25681963683536, S2LatLng(p1).lng().degrees());
  EXPECT_DOUBLE_EQ(75.139812547718478, S2LatLng(p2).lat().degrees());
  EXPECT_DOUBLE_EQ(-119.13042521187423, S2LatLng(p2).lng().degrees());
  EXPECT_DOUBLE_EQ(75.524190079054392, S2LatLng(p3).lat().degrees());
  EXPECT_DOUBLE_EQ(26.392175948257943, S2LatLng(p3).lng().degrees());
}

TEST(S2LoopShape, Basic) {
  unique_ptr<S2Loop> loop = s2textformat::MakeLoop("0:0, 0:1, 1:0");
  S2Loop::Shape shape(loop.get());
  EXPECT_EQ(loop.get(), shape.loop());
  EXPECT_EQ(3, shape.num_edges());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_EQ(0, shape.chain(0).start);
  EXPECT_EQ(3, shape.chain(0).length);
  auto edge2 = shape.edge(2);
  EXPECT_EQ("1:0", s2textformat::ToString(edge2.v0));
  EXPECT_EQ("0:0", s2textformat::ToString(edge2.v1));
  EXPECT_EQ(2, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2LoopShape, EmptyLoop) {
  S2Loop loop(S2Loop::kEmpty());
  S2Loop::Shape shape(&loop);
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(0, shape.num_chains());
  EXPECT_TRUE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2LoopShape, FullLoop) {
  S2Loop loop(S2Loop::kFull());
  S2Loop::Shape shape(&loop);
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_TRUE(shape.is_full());
  EXPECT_TRUE(shape.GetReferencePoint().contained);
}

TEST(S2LoopOwningShape, Ownership) {
  // Debug mode builds will catch any memory leak below.
  auto loop = make_unique<S2Loop>(S2Loop::kEmpty());
  S2Loop::OwningShape shape(std::move(loop));
}

