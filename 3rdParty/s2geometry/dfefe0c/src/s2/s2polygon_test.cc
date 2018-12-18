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
//

#include "s2/s2polygon.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "s2/base/casts.h"
#include "s2/base/commandlineflags.h"
#include "s2/base/logging.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/r1interval.h"
#include "s2/s1angle.h"
#include "s2/s2builder.h"
#include "s2/s2builderutil_s2polygon_layer.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2closest_edge_query.h"
#include "s2/s2coords.h"
#include "s2/s2debug.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2edge_distances.h"
#include "s2/s2error.h"
#include "s2/s2latlng.h"
#include "s2/s2loop.h"
#include "s2/s2metrics.h"
#include "s2/s2padded_cell.h"
#include "s2/s2pointutil.h"
#include "s2/s2polyline.h"
#include "s2/s2region_coverer.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"
#include "s2/strings/serialize.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/third_party/absl/container/fixed_array.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/third_party/absl/strings/str_cat.h"
#include "s2/util/coding/coder.h"
#include "s2/util/gtl/legacy_random_shuffle.h"
#include "s2/util/math/matrix3x3.h"

using absl::StrCat;
using absl::make_unique;
using s2builderutil::IntLatLngSnapFunction;
using s2builderutil::S2PolygonLayer;
using std::max;
using std::min;
using std::numeric_limits;
using std::swap;
using std::unique_ptr;
using std::vector;

// A set of nested loops around the point 0:0 (lat:lng).
// Every vertex of kNear0 is a vertex of kNear1.
const char kNearPoint[] = "0:0";
const string kNear0 = "-1:0, 0:1, 1:0, 0:-1;";
const string kNear1 = "-1:-1, -1:0, -1:1, 0:1, 1:1, 1:0, 1:-1, 0:-1;";
const string kNear2 = "-1:-2, -2:5, 5:-2;";
const string kNear3 = "-2:-2, -3:6, 6:-3;";
const string kNearHemi = "0:-90, -90:0, 0:90, 90:0;";

// A set of nested loops around the point 0:180 (lat:lng).
// Every vertex of kFar0 and kFar2 belongs to kFar1, and all
// the loops except kFar2 are non-convex.
const string kFar0 = "0:179, 1:180, 0:-179, 2:-180;";
const string kFar1 =
  "0:179, -1:179, 1:180, -1:-179, 0:-179, 3:-178, 2:-180, 3:178;";
const string kFar2 = "3:-178, 3:178, -1:179, -1:-179;";
const string kFar3 = "-3:-178, 4:-177, 4:177, -3:178, -2:179;";
const string kFarHemi = "0:-90, 60:90, -60:90;";

// A set of nested loops around the point -90:0 (lat:lng).
const string kSouthPoint = "-89.9999:0.001";
const string kSouth0a = "-90:0, -89.99:0.01, -89.99:0;";
const string kSouth0b = "-90:0, -89.99:0.03, -89.99:0.02;";
const string kSouth0c = "-90:0, -89.99:0.05, -89.99:0.04;";
const string kSouth1 = "-90:0, -89.9:0.1, -89.9:-0.1;";
const string kSouth2 = "-90:0, -89.8:0.2, -89.8:-0.2;";
const string kSouthHemi = "0:-180, 0:60, 0:-60;";

// Two different loops that surround all the Near and Far loops except
// for the hemispheres.
const string kNearFar1 = "-1:-9, -9:-9, -9:9, 9:9, 9:-9, 1:-9, "
                         "1:-175, 9:-175, 9:175, -9:175, -9:-175, -1:-175;";
const string kNearFar2 = "-2:15, -2:170, -8:-175, 8:-175, "
                         "2:170, 2:15, 8:-4, -8:-4;";

// Loops that result from intersection of other loops.
const string kFarHSouthH = "0:-180, 0:90, -60:90, 0:-90;";

// Rectangles that form a cross, with only shared vertices, no crossing edges.
// Optional holes outside the intersecting region.
const string kCross1 = "-2:1, -1:1, 1:1, 2:1, 2:-1, 1:-1, -1:-1, -2:-1;";
const string kCross1SideHole = "-1.5:0.5, -1.2:0.5, -1.2:-0.5, -1.5:-0.5;";
const string kCross2 = "1:-2, 1:-1, 1:1, 1:2, -1:2, -1:1, -1:-1, -1:-2;";
const string kCross2SideHole = "0.5:-1.5, 0.5:-1.2, -0.5:-1.2, -0.5:-1.5;";
const string kCrossCenterHole = "-0.5:0.5, 0.5:0.5, 0.5:-0.5, -0.5:-0.5;";

// Two rectangles that intersect, but no edges cross and there's always
// local containment (rather than crossing) at each shared vertex.
// In this ugly ASCII art, 1 is A+B, 2 is B+C:
//      +---+---+---+
//      | A | B | C |
//      +---+---+---+
const string kOverlap1 = "0:1, 1:1, 2:1, 2:0, 1:0, 0:0;";
const string kOverlap1SideHole = "0.2:0.8, 0.8:0.8, 0.8:0.2, 0.2:0.2;";
const string kOverlap2 = "1:1, 2:1, 3:1, 3:0, 2:0, 1:0;";
const string kOverlap2SideHole = "2.2:0.8, 2.8:0.8, 2.8:0.2, 2.2:0.2;";
const string kOverlapCenterHole = "1.2:0.8, 1.8:0.8, 1.8:0.2, 1.2:0.2;";

// An empty polygon.
const string kEmpty = "";
// By symmetry, the intersection of the two polygons has almost half the area
// of either polygon.
const string kOverlap3 = "-10:10, 0:10, 0:-10, -10:-10, -10:0";
const string kOverlap4 = "-10:0, 10:0, 10:-10, -10:-10";

class S2PolygonTestBase : public testing::Test {
 public:
  S2PolygonTestBase();

 protected:
  // Some standard polygons to use in the tests.
  const unique_ptr<const S2Polygon> empty_;
  const unique_ptr<const S2Polygon> full_;
  const unique_ptr<const S2Polygon> near_0_;
  const unique_ptr<const S2Polygon> near_10_;
  const unique_ptr<const S2Polygon> near_30_;
  const unique_ptr<const S2Polygon> near_32_;
  const unique_ptr<const S2Polygon> near_3210_;
  const unique_ptr<const S2Polygon> near_H3210_;

  const unique_ptr<const S2Polygon> far_10_;
  const unique_ptr<const S2Polygon> far_21_;
  const unique_ptr<const S2Polygon> far_321_;
  const unique_ptr<const S2Polygon> far_H20_;
  const unique_ptr<const S2Polygon> far_H3210_;

  const unique_ptr<const S2Polygon> south_0ab_;
  const unique_ptr<const S2Polygon> south_2_;
  const unique_ptr<const S2Polygon> south_210b_;
  const unique_ptr<const S2Polygon> south_H21_;
  const unique_ptr<const S2Polygon> south_H20abc_;

  const unique_ptr<const S2Polygon> nf1_n10_f2_s10abc_;

  const unique_ptr<const S2Polygon> nf2_n2_f210_s210ab_;

  const unique_ptr<const S2Polygon> f32_n0_;
  const unique_ptr<const S2Polygon> n32_s0b_;

  const unique_ptr<const S2Polygon> cross1_;
  const unique_ptr<const S2Polygon> cross1_side_hole_;
  const unique_ptr<const S2Polygon> cross1_center_hole_;
  const unique_ptr<const S2Polygon> cross2_;
  const unique_ptr<const S2Polygon> cross2_side_hole_;
  const unique_ptr<const S2Polygon> cross2_center_hole_;

  const unique_ptr<const S2Polygon> overlap1_;
  const unique_ptr<const S2Polygon> overlap1_side_hole_;
  const unique_ptr<const S2Polygon> overlap1_center_hole_;
  const unique_ptr<const S2Polygon> overlap2_;
  const unique_ptr<const S2Polygon> overlap2_side_hole_;
  const unique_ptr<const S2Polygon> overlap2_center_hole_;

  const unique_ptr<const S2Polygon> far_H_;
  const unique_ptr<const S2Polygon> south_H_;
  const unique_ptr<const S2Polygon> far_H_south_H_;
};

static bool TestEncodeDecode(const S2Polygon* src) {
  Encoder encoder;
  src->Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  S2Polygon dst;
  dst.Decode(&decoder);
  return src->Equals(&dst);
}

static unique_ptr<S2Polygon> MakePolygon(const string& str) {
  unique_ptr<S2Polygon> polygon(s2textformat::MakeVerbatimPolygon(str));

  // Check that InitToSnapped() is idempotent.
  S2Polygon snapped1, snapped2;
  snapped1.InitToSnapped(polygon.get());
  snapped2.InitToSnapped(&snapped1);
  EXPECT_TRUE(snapped1.Equals(&snapped2));

  // Check that Decode(Encode(x)) is the identity function.
  EXPECT_TRUE(TestEncodeDecode(polygon.get()));
  return polygon;
}

static void CheckContains(const string& a_str, const string& b_str) {
  unique_ptr<S2Polygon> a = MakePolygon(a_str);
  unique_ptr<S2Polygon> b = MakePolygon(b_str);
  EXPECT_TRUE(a->Contains(b.get()));
  EXPECT_TRUE(a->ApproxContains(b.get(), S1Angle::Radians(1e-15)));
  EXPECT_FALSE(a->ApproxDisjoint(b.get(), S1Angle::Radians(1e-15)));
}

static void CheckContainsPoint(const string& a_str, const string& b_str) {
  unique_ptr<S2Polygon> a(s2textformat::MakePolygon(a_str));
  EXPECT_TRUE(a->Contains(s2textformat::MakePoint(b_str)))
    << " " << a_str << " did not contain " << b_str;
}

TEST(S2Polygon, Init) {
  CheckContains(kNear1, kNear0);
  CheckContains(kNear2, kNear1);
  CheckContains(kNear3, kNear2);
  CheckContains(kNearHemi, kNear3);
  CheckContains(kFar1, kFar0);
  CheckContains(kFar2, kFar1);
  CheckContains(kFar3, kFar2);
  CheckContains(kFarHemi, kFar3);
  CheckContains(kSouth1, kSouth0a);
  CheckContains(kSouth1, kSouth0b);
  CheckContains(kSouth1, kSouth0c);
  CheckContains(kSouthHemi, kSouth2);
  CheckContains(kNearFar1, kNear3);
  CheckContains(kNearFar1, kFar3);
  CheckContains(kNearFar2, kNear3);
  CheckContains(kNearFar2, kFar3);

  CheckContainsPoint(kNear0, kNearPoint);
  CheckContainsPoint(kNear1, kNearPoint);
  CheckContainsPoint(kNear2, kNearPoint);
  CheckContainsPoint(kNear3, kNearPoint);
  CheckContainsPoint(kNearHemi, kNearPoint);
  CheckContainsPoint(kSouth0a, kSouthPoint);
  CheckContainsPoint(kSouth1, kSouthPoint);
  CheckContainsPoint(kSouth2, kSouthPoint);
  CheckContainsPoint(kSouthHemi, kSouthPoint);
}

TEST(S2Polygon, OverlapFractions) {
  unique_ptr<S2Polygon> a(MakePolygon(kEmpty));
  unique_ptr<S2Polygon> b(MakePolygon(kEmpty));
  auto result = S2Polygon::GetOverlapFractions(a.get(), b.get());
  EXPECT_DOUBLE_EQ(1.0, result.first);
  EXPECT_DOUBLE_EQ(1.0, result.second);

  b = MakePolygon(kOverlap3);
  result = S2Polygon::GetOverlapFractions(a.get(), b.get());
  EXPECT_DOUBLE_EQ(1.0, result.first);
  EXPECT_DOUBLE_EQ(0.0, result.second);

  a = MakePolygon(kOverlap4);
  result = S2Polygon::GetOverlapFractions(a.get(), b.get());
  EXPECT_NEAR(0.5, result.first, 1e-14);
  EXPECT_NEAR(0.5, result.second, 1e-14);
}

TEST(S2Polygon, OriginNearPole) {
  // S2Polygon operations are more efficient if S2::Origin() is near a pole.
  // (Loops that contain a pole tend to have very loose bounding boxes because
  // they span the full longitude range.  S2Polygon canonicalizes all loops so
  // that they don't contain S2::Origin(), thus by placing S2::Origin() near a
  // pole we minimize the number of canonical loops which contain that pole.)
  EXPECT_GE(S2LatLng::Latitude(S2::Origin()).degrees(), 80);
}

S2PolygonTestBase::S2PolygonTestBase()
  : empty_(new S2Polygon()),
    full_(MakePolygon("full")),
    near_0_(MakePolygon(kNear0)),
    near_10_(MakePolygon(kNear0 + kNear1)),
    near_30_(MakePolygon(kNear3 + kNear0)),
    near_32_(MakePolygon(kNear2 + kNear3)),
    near_3210_(MakePolygon(kNear0 + kNear2 + kNear3 + kNear1)),
    near_H3210_(MakePolygon(kNear0 + kNear2 + kNear3 + kNearHemi + kNear1)),

    far_10_(MakePolygon(kFar0 + kFar1)),
    far_21_(MakePolygon(kFar2 + kFar1)),
    far_321_(MakePolygon(kFar2 + kFar3 + kFar1)),
    far_H20_(MakePolygon(kFar2 + kFarHemi + kFar0)),
    far_H3210_(MakePolygon(kFar2 + kFarHemi + kFar0 + kFar1 + kFar3)),

    south_0ab_(MakePolygon(kSouth0a + kSouth0b)),
    south_2_(MakePolygon(kSouth2)),
    south_210b_(MakePolygon(kSouth2 + kSouth0b + kSouth1)),
    south_H21_(MakePolygon(kSouth2 + kSouthHemi + kSouth1)),
    south_H20abc_(MakePolygon(kSouth2 + kSouth0b + kSouthHemi +
                              kSouth0a + kSouth0c)),

    nf1_n10_f2_s10abc_(MakePolygon(kSouth0c + kFar2 + kNear1 + kNearFar1 +
                                   kNear0 + kSouth1 + kSouth0b + kSouth0a)),

    nf2_n2_f210_s210ab_(MakePolygon(kFar2 + kSouth0a + kFar1 + kSouth1 + kFar0 +
                                    kSouth0b + kNearFar2 + kSouth2 + kNear2)),

    f32_n0_(MakePolygon(kFar2 + kNear0 + kFar3)),
    n32_s0b_(MakePolygon(kNear3 + kSouth0b + kNear2)),

    cross1_(MakePolygon(kCross1)),
    cross1_side_hole_(MakePolygon(kCross1 + kCross1SideHole)),
    cross1_center_hole_(MakePolygon(kCross1 + kCrossCenterHole)),
    cross2_(MakePolygon(kCross2)),
    cross2_side_hole_(MakePolygon(kCross2 + kCross2SideHole)),
    cross2_center_hole_(MakePolygon(kCross2 + kCrossCenterHole)),

    overlap1_(MakePolygon(kOverlap1)),
    overlap1_side_hole_(MakePolygon(kOverlap1 + kOverlap1SideHole)),
    overlap1_center_hole_(MakePolygon(kOverlap1 + kOverlapCenterHole)),
    overlap2_(MakePolygon(kOverlap2)),
    overlap2_side_hole_(MakePolygon(kOverlap2 + kOverlap2SideHole)),
    overlap2_center_hole_(MakePolygon(kOverlap2 + kOverlapCenterHole)),

    far_H_(MakePolygon(kFarHemi)),
    south_H_(MakePolygon(kSouthHemi)),
    far_H_south_H_(MakePolygon(kFarHSouthH)) {
}

static void CheckEqual(const S2Polygon& a, const S2Polygon& b,
                       S1Angle max_error = S1Angle::Zero()) {
  if (a.BoundaryApproxEquals(b, max_error)) return;
  S2Builder builder{S2Builder::Options()};
  S2Polygon a2, b2;
  builder.StartLayer(make_unique<S2PolygonLayer>(&a2));
  builder.AddPolygon(a);
  S2Error error;
  ASSERT_TRUE(builder.Build(&error)) << error;
  builder.StartLayer(make_unique<S2PolygonLayer>(&b2));
  builder.AddPolygon(b);
  ASSERT_TRUE(builder.Build(&error)) << error;
  EXPECT_TRUE(a2.BoundaryApproxEquals(b2, max_error))
      << "\na: " << s2textformat::ToString(a)
      << "\nb: " << s2textformat::ToString(b)
      << "\na2: " << s2textformat::ToString(a2)
      << "\nb2: " << s2textformat::ToString(b2);
}

static void CheckComplementary(const S2Polygon& a, const S2Polygon& b) {
  S2Polygon b1;
  b1.InitToComplement(&b);
  CheckEqual(a, b1);
}

TEST(S2Polygon, TestApproxContainsAndDisjoint) {
  // We repeatedly choose a random cell id and intersect its bounding polygon
  // "A" with the bounding polygon "B" of one its child cells.  The result may
  // not be contained by either A or B, because the vertices of B near the
  // edge midpoints of A may be slightly outside A, and even when the crossing
  // edges are intersected, the intersection point may also be slightly
  // outside A and/or B.
  //
  // We repeat the test many times and expect that some fraction of the exact
  // tests should fail, while all of the approximate test should succeed.
  const int kIters = 1000;
  int exact_contains = 0, exact_disjoint = 0;
  S2Testing::rnd.Reset(FLAGS_s2_random_seed);
  for (int iter = 0; iter < kIters; ++iter) {
    S2CellId id = S2Testing::GetRandomCellId(10);
    S2Polygon parent_polygon((S2Cell(id)));
    S2Polygon child_polygon(S2Cell(id.child(0)));

    // Get the intersection.  There is no guarantee that the intersection will
    // be contained by A or B.  Similarly, the intersection may slightly
    // overlap an adjacent disjoint polygon C.
    S2Polygon intersection;
    intersection.InitToIntersection(&parent_polygon, &child_polygon);
    if (parent_polygon.Contains(&intersection)) {
      ++exact_contains;
    }
    EXPECT_TRUE(parent_polygon.ApproxContains(
        &intersection, S2::kIntersectionMergeRadius));

    S2Polygon adjacent_polygon(S2Cell(id.child(1)));
    if (!adjacent_polygon.Intersects(&intersection)) {
      ++exact_disjoint;
    }
    EXPECT_TRUE(adjacent_polygon.ApproxDisjoint(
        &intersection, S2::kIntersectionMergeRadius));
  }
  // All of the approximate results are true, so we check that at least some
  // of the exact results are false in order to make sure that this test
  // actually tests something.
  //
  // There are two vertices in each child cell that have a 50% chance of being
  // outside the parent cell.  When a vertex is outside, an intersection point
  // is computed near that vertex that also has a 50% chance of being
  // outside.  Snapping used to choose one of these vertices at random, but
  // currently the vertex whose S2CellId is smaller is always chosen.  For the
  // exact containment test, it turns out that one vertex is adjacent to a
  // lower-numbered S2CellId and the other is adjacent to a higher-numbered
  // S2CellId, which means that one vertex will always be chosen outside the
  // parent if possible, and the other will always be chosen inside if
  // possible.  This works out to an expectation that 0.5 * 0.75 = 37.5% of
  // the exact containment tests will succeed.
  //
  // For the exact disjoint test, there is one shared vertex that might be
  // replaced by a computed intersection point.  The shared vertex is inside
  // the parent 50% of the time.  Otherwise there is a 50% chance that the
  // intersection point will not be chosen for snapping because it has a
  // higher S2CellId that the shared vertex, and otherwise there is still a
  // 50% chance that intersection point is on the side of the shared edge that
  // results in no intersection.  This works out to an expectation that
  // (1 - 0.5 * 0.5 * 0.5) = 87.5% of the exact disjoint tests will succeed.
  EXPECT_LT(exact_contains, 0.40 * kIters);  // about 37.5% succeed
  EXPECT_LT(exact_disjoint, 0.90 * kIters);  // about 87.5% succeed
}

// Given a pair of polygons where A contains B, check that various identities
// involving union, intersection, and difference operations hold true.
static void TestOneNestedPair(const S2Polygon& a, const S2Polygon& b) {
  EXPECT_TRUE(a.Contains(&b));
  EXPECT_EQ(!b.is_empty(), a.Intersects(&b));
  EXPECT_EQ(!b.is_empty(), b.Intersects(&a));

  S2Polygon c, d, e, f, g;
  c.InitToUnion(&a, &b);
  CheckEqual(c, a);

  d.InitToIntersection(&a, &b);
  CheckEqual(d, b);

  e.InitToDifference(&b, &a);
  EXPECT_TRUE(e.is_empty());

  f.InitToDifference(&a, &b);
  g.InitToSymmetricDifference(&a, &b);
  CheckEqual(f, g);
}

// Given a pair of disjoint polygons A and B, check that various identities
// involving union, intersection, and difference operations hold true.
static void TestOneDisjointPair(const S2Polygon& a, const S2Polygon& b) {
  EXPECT_FALSE(a.Intersects(&b));
  EXPECT_FALSE(b.Intersects(&a));
  EXPECT_EQ(b.is_empty(), a.Contains(&b));
  EXPECT_EQ(a.is_empty(), b.Contains(&a));

  S2Polygon ab, c, d, e, f, g;
  S2Builder builder{S2Builder::Options()};
  builder.StartLayer(make_unique<S2PolygonLayer>(&ab));
  builder.AddPolygon(a);
  builder.AddPolygon(b);
  S2Error error;
  ASSERT_TRUE(builder.Build(&error)) << error;

  c.InitToUnion(&a, &b);
  CheckEqual(c, ab);

  d.InitToIntersection(&a, &b);
  EXPECT_TRUE(d.is_empty());

  e.InitToDifference(&a, &b);
  CheckEqual(e, a);

  f.InitToDifference(&b, &a);
  CheckEqual(f, b);

  g.InitToSymmetricDifference(&a, &b);
  CheckEqual(g, ab);
}

// Given polygons A and B whose union covers the sphere, check that various
// identities involving union, intersection, and difference hold true.
static void TestOneCoveringPair(const S2Polygon& a, const S2Polygon& b) {
  EXPECT_EQ(a.is_full(), a.Contains(&b));
  EXPECT_EQ(b.is_full(), b.Contains(&a));

  S2Polygon c, d, e, f;
  c.InitToUnion(&a, &b);
  EXPECT_TRUE(c.is_full());
}

// Given polygons A and B such that both A and its complement intersect both B
// and its complement, check that various identities involving union,
// intersection, and difference hold true.
static void TestOneOverlappingPair(const S2Polygon& a, const S2Polygon& b) {
  EXPECT_FALSE(a.Contains(&b));
  EXPECT_FALSE(b.Contains(&a));
  EXPECT_TRUE(a.Intersects(&b));

  S2Polygon c, d, e, f, g, h;
  c.InitToUnion(&a, &b);
  EXPECT_FALSE(c.is_full());

  d.InitToIntersection(&a, &b);
  EXPECT_FALSE(d.is_empty());

  e.InitToDifference(&b, &a);
  EXPECT_FALSE(e.is_empty());

  f.InitToDifference(&a, &b);
  g.InitToUnion(&e, &f);
  h.InitToSymmetricDifference(&a, &b);
  CheckEqual(g, h);
}

// Given a pair of polygons where A contains B, test various identities
// involving A, B, and their complements.
static void TestNestedPair(const S2Polygon& a, const S2Polygon& b) {
  S2Polygon a1, b1;
  a1.InitToComplement(&a);
  b1.InitToComplement(&b);

  TestOneNestedPair(a, b);
  TestOneNestedPair(b1, a1);
  TestOneDisjointPair(a1, b);
  TestOneCoveringPair(a, b1);
}

// Given a pair of disjoint polygons A and B, test various identities
// involving A, B, and their complements.
static void TestDisjointPair(const S2Polygon& a, const S2Polygon& b) {
  S2Polygon a1, b1;
  a1.InitToComplement(&a);
  b1.InitToComplement(&b);

  TestOneDisjointPair(a, b);
  TestOneCoveringPair(a1, b1);
  TestOneNestedPair(a1, b);
  TestOneNestedPair(b1, a);
}

// Given polygons A and B such that both A and its complement intersect both B
// and its complement, test various identities involving these four polygons.
static void TestOverlappingPair(const S2Polygon& a, const S2Polygon& b) {
  S2Polygon a1, b1;
  a1.InitToComplement(&a);
  b1.InitToComplement(&b);

  TestOneOverlappingPair(a, b);
  TestOneOverlappingPair(a1, b1);
  TestOneOverlappingPair(a1, b);
  TestOneOverlappingPair(a, b1);
}

// "a1" is the complement of "a", and "b1" is the complement of "b".
static void TestOneComplementPair(const S2Polygon& a, const S2Polygon& a1,
                                  const S2Polygon& b, const S2Polygon& b1) {
  // Check DeMorgan's Law and that subtraction is the same as intersection
  // with the complement.  This function is called multiple times in order to
  // test the various combinations of complements.

  S2Polygon a1_or_b, a_and_b1, a_minus_b;
  a_and_b1.InitToIntersection(&a, &b1);
  a1_or_b.InitToUnion(&a1, &b);
  a_minus_b.InitToDifference(&a, &b);

  CheckComplementary(a1_or_b, a_and_b1);
  CheckEqual(a_minus_b, a_and_b1);
}

// Test identities that should hold for any pair of polygons A, B and their
// complements.
static void TestComplements(const S2Polygon& a, const S2Polygon& b) {
  S2Polygon a1, b1;
  a1.InitToComplement(&a);
  b1.InitToComplement(&b);

  TestOneComplementPair(a, a1, b, b1);
  TestOneComplementPair(a1, a, b, b1);
  TestOneComplementPair(a, a1, b1, b);
  TestOneComplementPair(a1, a, b1, b);

  // There is a lot of redundancy if we do this test for each complementary
  // pair, so we just do it once instead.
  S2Polygon a_xor_b1, a1_xor_b;
  a_xor_b1.InitToSymmetricDifference(&a, &b1);
  a1_xor_b.InitToSymmetricDifference(&a1, &b);
  CheckEqual(a_xor_b1, a1_xor_b);
}

static void TestDestructiveUnion(const S2Polygon& a, const S2Polygon& b) {
  S2Polygon c;
  c.InitToUnion(&a, &b);
  vector<unique_ptr<S2Polygon>> polygons;
  polygons.emplace_back(a.Clone());
  polygons.emplace_back(b.Clone());
  unique_ptr<S2Polygon> c_destructive =
      S2Polygon::DestructiveUnion(std::move(polygons));
  CheckEqual(c, *c_destructive);
}

static void TestRelationWithDesc(const S2Polygon& a, const S2Polygon& b,
                                 bool contains, bool contained,
                                 bool intersects, const char* description) {
  SCOPED_TRACE(description);
  EXPECT_EQ(contains, a.Contains(&b));
  EXPECT_EQ(contained, b.Contains(&a));
  EXPECT_EQ(intersects, a.Intersects(&b));
  if (contains) TestNestedPair(a, b);
  if (contained) TestNestedPair(b, a);
  if (!intersects) TestDisjointPair(a, b);
  if (intersects && !(contains | contained)) {
    TestOverlappingPair(a, b);  // See TestOverlappingPair for definition
  }
  TestDestructiveUnion(a, b);
  TestComplements(a, b);
}

TEST_F(S2PolygonTestBase, Relations) {
#define TestRelation(a, b, contains, contained, intersects)   \
  TestRelationWithDesc(a, b, contains, contained, intersects, \
                       "args " #a ", " #b)

  TestRelation(*near_10_, *empty_, true, false, false);
  TestRelation(*near_10_, *near_10_, true, true, true);
  TestRelation(*full_, *near_10_, true, false, true);
  TestRelation(*near_10_, *near_30_, false, true, true);
  TestRelation(*near_10_, *near_32_, false, false, false);
  TestRelation(*near_10_, *near_3210_, false, true, true);
  TestRelation(*near_10_, *near_H3210_, false, false, false);
  TestRelation(*near_30_, *near_32_, true, false, true);
  TestRelation(*near_30_, *near_3210_, true, false, true);
  TestRelation(*near_30_, *near_H3210_, false, false, true);
  TestRelation(*near_32_, *near_3210_, false, true, true);
  TestRelation(*near_32_, *near_H3210_, false, false, false);
  TestRelation(*near_3210_, *near_H3210_, false, false, false);

  TestRelation(*far_10_, *far_21_, false, false, false);
  TestRelation(*far_10_, *far_321_, false, true, true);
  TestRelation(*far_10_, *far_H20_, false, false, false);
  TestRelation(*far_10_, *far_H3210_, false, false, false);
  TestRelation(*far_21_, *far_321_, false, false, false);
  TestRelation(*far_21_, *far_H20_, false, false, false);
  TestRelation(*far_21_, *far_H3210_, false, true, true);
  TestRelation(*far_321_, *far_H20_, false, false, true);
  TestRelation(*far_321_, *far_H3210_, false, false, true);
  TestRelation(*far_H20_, *far_H3210_, false, false, true);

  TestRelation(*south_0ab_, *south_2_, false, true, true);
  TestRelation(*south_0ab_, *south_210b_, false, false, true);
  TestRelation(*south_0ab_, *south_H21_, false, true, true);
  TestRelation(*south_0ab_, *south_H20abc_, false, true, true);
  TestRelation(*south_2_, *south_210b_, true, false, true);
  TestRelation(*south_2_, *south_H21_, false, false, true);
  TestRelation(*south_2_, *south_H20abc_, false, false, true);
  TestRelation(*south_210b_, *south_H21_, false, false, true);
  TestRelation(*south_210b_, *south_H20abc_, false, false, true);
  TestRelation(*south_H21_, *south_H20abc_, true, false, true);

  TestRelation(*nf1_n10_f2_s10abc_, *nf2_n2_f210_s210ab_, false, false, true);
  TestRelation(*nf1_n10_f2_s10abc_, *near_32_, true, false, true);
  TestRelation(*nf1_n10_f2_s10abc_, *far_21_, false, false, false);
  TestRelation(*nf1_n10_f2_s10abc_, *south_0ab_, false, false, false);
  TestRelation(*nf1_n10_f2_s10abc_, *f32_n0_, true, false, true);

  TestRelation(*nf2_n2_f210_s210ab_, *near_10_, false, false, false);
  TestRelation(*nf2_n2_f210_s210ab_, *far_10_, true, false, true);
  TestRelation(*nf2_n2_f210_s210ab_, *south_210b_, true, false, true);
  TestRelation(*nf2_n2_f210_s210ab_, *south_0ab_, true, false, true);
  TestRelation(*nf2_n2_f210_s210ab_, *n32_s0b_, true, false, true);

  TestRelation(*cross1_, *cross2_, false, false, true);
  TestRelation(*cross1_side_hole_, *cross2_, false, false, true);
  TestRelation(*cross1_center_hole_, *cross2_, false, false, true);
  TestRelation(*cross1_, *cross2_side_hole_, false, false, true);
  TestRelation(*cross1_, *cross2_center_hole_, false, false, true);
  TestRelation(*cross1_side_hole_, *cross2_side_hole_, false, false, true);
  TestRelation(*cross1_center_hole_, *cross2_side_hole_, false, false, true);
  TestRelation(*cross1_side_hole_, *cross2_center_hole_, false, false, true);
  TestRelation(*cross1_center_hole_, *cross2_center_hole_, false, false, true);

  // These cases_, when either polygon has a hole, test a different code path
  // from the other cases.
  TestRelation(*overlap1_, *overlap2_, false, false, true);
  TestRelation(*overlap1_side_hole_, *overlap2_, false, false, true);
  TestRelation(*overlap1_center_hole_, *overlap2_, false, false, true);
  TestRelation(*overlap1_, *overlap2_side_hole_, false, false, true);
  TestRelation(*overlap1_, *overlap2_center_hole_, false, false, true);
  TestRelation(*overlap1_side_hole_, *overlap2_side_hole_, false, false, true);
  TestRelation(*overlap1_center_hole_, *overlap2_side_hole_,
               false, false, true);
  TestRelation(*overlap1_side_hole_, *overlap2_center_hole_,
               false, false, true);
  TestRelation(*overlap1_center_hole_, *overlap2_center_hole_,
               false, false, true);
#undef TestRelation
}

TEST_F(S2PolygonTestBase, EmptyAndFull) {
  EXPECT_TRUE(empty_->is_empty());
  EXPECT_FALSE(full_->is_empty());
  EXPECT_FALSE(empty_->is_full());
  EXPECT_TRUE(full_->is_full());

  TestNestedPair(*empty_, *empty_);
  TestNestedPair(*full_, *empty_);
  TestNestedPair(*full_, *full_);
}

struct TestCase {
  const char* a;
  const char* b;
  const char* a_and_b;
  const char* a_or_b;
  const char* a_minus_b;
  const char* a_xor_b;
};

TestCase test_cases[] = {
  // Two triangles that share an edge.
  { "4:2, 3:1, 3:3;",

    "3:1, 2:2, 3:3;",

    "",  // and

    "4:2, 3:1, 2:2, 3:3;",  // or

    "4:2, 3:1, 3:3;",  // minus

    "4:2, 3:1, 2:2, 3:3;"  // xor
  },

  // Two vertical bars and a horizontal bar connecting them.
  { "0:0, 0:2, 3:2, 3:0;   0:3, 0:5, 3:5, 3:3;",

    "1:1, 1:4, 2:4, 2:1;",

    "1:1, 1:2, 2:2, 2:1;   1:3, 1:4, 2:4, 2:3;",  // and

    "0:0, 0:2, 1:2, 1:3, 0:3, 0:5, 3:5, 3:3, 2:3, 2:2, 3:2, 3:0;",  // or

    "0:0, 0:2, 1:2, 1:1, 2:1, 2:2, 3:2, 3:0;   "  // minus
    "0:3, 0:5, 3:5, 3:3, 2:3, 2:4, 1:4, 1:3;",

    "0:0, 0:2, 1:2, 1:1, 2:1, 2:2, 3:2, 3:0;   "  // xor
    "0:3, 0:5, 3:5, 3:3, 2:3, 2:4, 1:4, 1:3;   "
    "1:2, 1:3, 2:3, 2:2"
  },

  // Two vertical bars and two horizontal bars.
  { "1:88, 1:93, 2:93, 2:88;   -1:88, -1:93, 0:93, 0:88;",

    "-2:89, -2:90, 3:90, 3:89;   -2:91, -2:92, 3:92, 3:91;",

    "1:89, 1:90, 2:90, 2:89;   1:91, 1:92, 2:92, 2:91;   "  // and
    "-1:89, -1:90, 0:90, 0:89;   -1:91, -1:92, 0:92, 0:91;",

    "-1:88, -1:89, -2:89, -2:90, -1:90, -1:91, -2:91, -2:92, -1:92, "  // or
    "-1:93, 0:93, 0:92, 1:92, 1:93, 2:93, 2:92, 3:92, 3:91, 2:91, "
    "2:90, 3:90, 3:89, 2:89, 2:88, 1:88, 1:89, 0:89, 0:88;   "
    "0:90, 0:91, 1:91, 1:90;",

    "1:88, 1:89, 2:89, 2:88;   1:90, 1:91, 2:91, 2:90;   "  // minus
    "1:92, 1:93, 2:93, 2:92;   -1:88, -1:89, 0:89, 0:88;   "
    "-1:90, -1:91, 0:91, 0:90;   -1:92, -1:93, 0:93, 0:92;",

    "1:88, 1:89, 2:89, 2:88;   -1:88, -1:89, 0:89, 0:88;   "  // xor
    "1:90, 1:91, 2:91, 2:90;   -1:90, -1:91, 0:91, 0:90;   "
    "1:92, 1:93, 2:93, 2:92;   -1:92, -1:93, 0:93, 0:92;   "
    "-2:89, -2:90, -1:90, -1:89;   -2:91, -2:92, -1:92, -1:91;   "
    "0:89, 0:90, 1:90, 1:89;   0:91, 0:92, 1:92, 1:91;   "
    "2:89, 2:90, 3:90, 3:89;   2:91, 2:92, 3:92, 3:91;"
},

  // Two interlocking square doughnuts.
  { "-1:-93, -1:-89, 3:-89, 3:-93;   0:-92, 0:-90, 2:-90, 2:-92;",

    "-3:-91, -3:-87, 1:-87, 1:-91;   -2:-90, -2:-88, 0:-88, 0:-90;",

    "-1:-91, -1:-90, 0:-90, 0:-91;   0:-90, 0:-89, 1:-89, 1:-90;",  // and

    "-1:-93, -1:-91, -3:-91, -3:-87, 1:-87, 1:-89, 3:-89, 3:-93;   "  // or
    "0:-92, 0:-91, 1:-91, 1:-90, 2:-90, 2:-92;   "
    "-2:-90, -2:-88, 0:-88, 0:-89, -1:-89, -1:-90;",

    "-1:-93, -1:-91, 0:-91, 0:-92, 2:-92, 2:-90, "  // minus
    "1:-90, 1:-89, 3:-89, 3:-93;   "
    "-1:-90, -1:-89, 0:-89, 0:-90;",

    "-1:-93, -1:-91, 0:-91, 0:-92, 2:-92, 2:-90, "  // xor
    "1:-90, 1:-89, 3:-89, 3:-93;   "
    "-3:-91, -3:-87, 1:-87, 1:-89, 0:-89, 0:-88, "
    "-2:-88, -2:-90, -1:-90, -1:-91;   "
    "-1:-90, -1:-89, 0:-89, 0:-90;   "
    "1:-91, 0:-91, 0:-90, 1:-90;"
  },

  // An incredibly thin triangle intersecting a square, such that the two
  // intersection points of the triangle with the square are identical.
  // This results in a degenerate loop that needs to be handled correctly.
  { "10:44, 10:46, 12:46, 12:44;",

    "11:45, 89:45.00000000000001, 90:45;",

    "",  // Empty intersection!

    // Original square with extra vertex, and triangle disappears (due to
    // default vertex_merge_radius of S2::kIntersectionMergeRadius).
    "10:44, 10:46, 12:46, 12:45.001774937, 12:44;",  // or

    "10:44, 10:46, 12:46, 12:45.001774937, 12:44;",  // minus

    "10:44, 10:46, 12:46, 12:45.001774937, 12:44;",  // xor
  },
};

TEST_F(S2PolygonTestBase, Operations) {
  S2Polygon far_south;
  far_south.InitToIntersection(far_H_.get(), south_H_.get());
  CheckEqual(far_south, *far_H_south_H_, S1Angle::Radians(1e-15));

  int i = 0;
  for (const TestCase& test : test_cases) {
    SCOPED_TRACE(StrCat("Polygon operation test case ", i++));
    unique_ptr<S2Polygon> a(MakePolygon(test.a));
    unique_ptr<S2Polygon> b(MakePolygon(test.b));
    unique_ptr<S2Polygon> expected_a_and_b(MakePolygon(test.a_and_b));
    unique_ptr<S2Polygon> expected_a_or_b(MakePolygon(test.a_or_b));
    unique_ptr<S2Polygon> expected_a_minus_b(MakePolygon(test.a_minus_b));
    unique_ptr<S2Polygon> expected_a_xor_b(MakePolygon(test.a_xor_b));

    // The intersections in the "expected" data were computed in lat-lng
    // space, while the actual intersections are computed using geodesics.
    // The error due to this depends on the length and direction of the line
    // segment being intersected, and how close the intersection is to the
    // endpoints of the segment.  The worst case is for a line segment between
    // two points at the same latitude, where the intersection point is in the
    // middle of the segment.  In this case the error is approximately
    // (p * t^2) / 8, where "p" is the absolute latitude in radians, "t" is
    // the longitude difference in radians, and both "p" and "t" are small.
    // The test cases all have small latitude and longitude differences.
    // If "p" and "t" are converted to degrees, the following error bound is
    // valid as long as (p * t^2 < 150).

    static const S1Angle kMaxError = S1Angle::Radians(1e-4);

    S2Polygon a_and_b, a_or_b, a_minus_b, a_xor_b;
    a_and_b.InitToIntersection(a.get(), b.get());
    CheckEqual(a_and_b, *expected_a_and_b, kMaxError);
    a_or_b.InitToUnion(a.get(), b.get());
    CheckEqual(a_or_b, *expected_a_or_b, kMaxError);
    TestDestructiveUnion(*a, *b);
    a_minus_b.InitToDifference(a.get(), b.get());
    CheckEqual(a_minus_b, *expected_a_minus_b, kMaxError);
    a_xor_b.InitToSymmetricDifference(a.get(), b.get());
    CheckEqual(a_xor_b, *expected_a_xor_b, kMaxError);
  }
}

TEST(S2Polygon, IntersectionSnapFunction) {
  // This tests that an intersection point is rounded to the nearest allowable
  // vertex position (using E0 coordinates, i.e. integer lat/lng values).
  unique_ptr<S2Polygon> a = MakePolygon("0:0, 0:10, 1:10, 1:0");
  unique_ptr<S2Polygon> b = MakePolygon("0:0, 0:10, 3:0");
  unique_ptr<S2Polygon> expected = MakePolygon("0:0, 0:10, 1:7, 1:0");
  S2Polygon actual;
  actual.InitToIntersection(*a, *b, IntLatLngSnapFunction(0));  // E0 coords
  CheckEqual(*expected, actual);
}

TEST(S2Polygon, IntersectionPreservesLoopOrder) {
  unique_ptr<S2Polygon> a = MakePolygon("0:0, 0:10, 10:10, 10:0");
  unique_ptr<S2Polygon> b = MakePolygon("1:1, 1:9, 9:5; 2:2, 2:8, 8:5");
  S2Polygon actual;
  actual.InitToIntersection(a.get(), b.get());
  EXPECT_EQ(s2textformat::ToString(*b), s2textformat::ToString(actual));
}

// Verifies that S2Polygon does not destroy or replace pointers to S2Loop, so
// caller can rely on using raw pointers.
TEST(S2Polygon, LoopPointers) {
  vector<unique_ptr<S2Loop>> loops;
  loops.emplace_back(s2textformat::MakeLoop("4:4, 4:6, 6:6, 6:4"));
  loops.emplace_back(s2textformat::MakeLoop("3:3, 3:7, 7:7, 7:3"));
  loops.emplace_back(s2textformat::MakeLoop("2:2, 2:8, 8:8, 8:2"));
  loops.emplace_back(s2textformat::MakeLoop("1:1, 1:9, 9:9, 9:1"));
  loops.emplace_back(s2textformat::MakeLoop("10:10, 15:15, 20:10"));
  loops.emplace_back(s2textformat::MakeLoop("-1:-1, -9:-1, -9:-9, -1:-9"));
  loops.emplace_back(s2textformat::MakeLoop("-5:-5, -6:-5, -6:-6, -5:-6"));

  std::set<const S2Loop*> loops_raw_ptrs;
  for (auto& loop : loops) {
    loops_raw_ptrs.insert(loop.get());
  }
  S2Polygon polygon(std::move(loops));

  // Check that loop pointers didn't change (but could've gotten reordered).
  EXPECT_EQ(loops_raw_ptrs.size(), polygon.num_loops());
  for (int i = 0; i < polygon.num_loops(); i++) {
     EXPECT_EQ(1, loops_raw_ptrs.count(polygon.loop(i))) << "loop " << i;
  }
}

static vector<unique_ptr<S2Loop>> MakeLoops(
    const vector<vector<S2Point>>& loop_vertices) {
  vector<unique_ptr<S2Loop>> result;
  for (const auto& vertices : loop_vertices) {
    result.emplace_back(new S2Loop(vertices));
    S2Error error;
    EXPECT_FALSE(result.back()->FindValidationError(&error))
        << "Loop " << result.size() - 1 << ": " << error;
  }
  return result;
}

// The "Bug" tests are regression tests from previous versions of the algorithm.
TEST(S2Polygon, Bug1) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10531193335759943, -0.80522214810955617, 0.58354664670985534},
      {-0.10531194840431297, -0.80522215192439039, 0.58354663873039425},
      {-0.10531192794033867, -0.80522217497559767, 0.58354661061568747},
      {-0.10531191284235047, -0.80522217121852058, 0.58354661852470402}
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10531174240075937, -0.80522236320875284, 0.58354638436119843},
      {-0.1053119128423491, -0.80522217121852213, 0.58354661852470235},
      {-0.10531192039134209, -0.80522217309706012, 0.58354661457019508},  // A
      {-0.10531191288915481, -0.80522217116640804, 0.5835466185881667},   // B
      {-0.10531191288915592, -0.8052221711664066, 0.58354661858816803},   // B
      {-0.10531192039151964, -0.80522217309710431, 0.58354661457010204},  // A
      {-0.10531192794033779, -0.80522217497559878, 0.58354661061568636},
      {-0.1053117575499668, -0.80522236690813498, 0.58354637652254981},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Given edges do not form loops (indegree != outdegree)
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug2) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10618951389689163, -0.80546461394606728, 0.58305277875939732},
      {-0.10618904764039243, -0.8054645437464607, 0.58305296065497536},
      {-0.10618862643748632, -0.80546451917975415, 0.58305307130470341},
      {-0.10617606798507535, -0.80544758470051458, 0.58307875187433833},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10618668131028208, -0.80544613076731553, 0.58307882755616247},
      {-0.10618910658843225, -0.80546454998744921, 0.58305294129732887},
      {-0.10618904764039225, -0.80546454374646081, 0.58305296065497536},
      {-0.10618898834264634, -0.80546453817003949, 0.58305297915823251},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Given edges do not form loops (indegree != outdegree)
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug3) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10703494861068318, -0.80542232562508131, 0.58295659972299307},
      {-0.10703494998722708, -0.80542232255642865, 0.58295660370995028},
      {-0.10703495367938694, -0.80542232008675829, 0.58295660644418046},
      {-0.10703495869785147, -0.80542231887781635, 0.58295660719304865},
      {-0.10703496369792719, -0.80542231925353791, 0.58295660575589636},
      {-0.10703496733984781, -0.80542232111324863, 0.58295660251780734},
      {-0.10703496864776367, -0.80542232395864055, 0.58295659834642488},
      {-0.10703496727121976, -0.80542232702729322, 0.58295659435946767},
      {-0.10703496357905991, -0.80542232949696357, 0.5829565916252375},
      {-0.10703495856059538, -0.80542233070590552, 0.58295659087636931},
      {-0.10703495356051966, -0.80542233033018396, 0.58295659231352159},
      {-0.10703494991859903, -0.80542232847047324, 0.58295659555161061},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10703494861068762, -0.80542232562508098, 0.58295659972299274},
      {-0.10703494998723152, -0.80542232255642832, 0.58295660370994995},
      {-0.10703495367939138, -0.80542232008675796, 0.58295660644418013},
      {-0.10703495869785591, -0.80542231887781601, 0.58295660719304832},
      {-0.10703496369793163, -0.80542231925353758, 0.58295660575589603},
      {-0.10703496733985225, -0.8054223211132483, 0.58295660251780701},
      {-0.10703496864776811, -0.80542232395864022, 0.58295659834642455},
      {-0.1070349672712242, -0.80542232702729288, 0.58295659435946734},
      {-0.10703496357906438, -0.80542232949696346, 0.58295659162523727},
      {-0.10703495856059982, -0.80542233070590519, 0.58295659087636897},
      {-0.1070349535605241, -0.80542233033018362, 0.58295659231352126},
      {-0.10703494991860348, -0.8054223284704729, 0.58295659555161028},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Given edges do not form loops (indegree != outdegree)
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug4) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10667065556339718, -0.80657502337947207, 0.58142764201754193},
      {-0.10667064691895933, -0.80657502457251051, 0.58142764194845853},
      {-0.10667064691930939, -0.80657502457246333, 0.58142764194845975},
      {-0.10667065556339746, -0.80657502337947395, 0.5814276420175396},
      {-0.10667077559567185, -0.80657589269604968, 0.58142641405029793},
      {-0.10667077059539463, -0.80657589232162286, 0.58142641548708696},
      {-0.10667063827452879, -0.80657502576554818, 0.58142764187937435},
      {-0.10667063169531328, -0.80657498170361974, 0.58142770421053058},
      {-0.10667064898418178, -0.8065749793175444, 0.58142770434869739},
    },
    {
      {-0.10667064691897719, -0.80657502457250896, 0.58142764194845697},
      {-0.10667063827452879, -0.80657502576554818, 0.58142764187937435},
      {-0.10667064691861985, -0.80657502457255736, 0.58142764194845586},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10667064691896312, -0.80657502457251107, 0.58142764194845697},
      {-0.10667064691896297, -0.80657502457251007, 0.58142764194845853},
      {-0.10667064033974753, -0.80657498051058207, 0.58142770427961399},
      {-0.10667064076268165, -0.80657498045444342, 0.58142770427989865},
      {-0.10667051785242875, -0.80657409963649807, 0.58142894872603923},
      {-0.1066707756642685, -0.80657588679775971, 0.58142642222003538},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Loop 1: Edge 1 crosses edge 3
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug5) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10574444273627338, -0.80816264611829447, 0.57938868667714882},
      {-0.10574444845633162, -0.80816268110163325, 0.57938863683652475},
      {-0.10574444825833453, -0.80816268112970524, 0.57938863683350494},
      {-0.10574444253827629, -0.80816264614636646, 0.57938868667412902},
      {-0.10574408792844124, -0.80816047738475361, 0.57939177648757634},
      {-0.10574408812643833, -0.80816047735668162, 0.57939177649059592},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.1057440881264381, -0.80816047735668017, 0.57939177649059825},
      {-0.10574408802743954, -0.80816047737071606, 0.57939177648908835},
      {-0.10574408812649677, -0.8081604773570521, 0.57939177649006868},
      {-0.10574408812649701, -0.80816047735705354, 0.57939177649006646},
      {-0.10574408802703171, -0.80816047737077379, 0.57939177648908202},
      {-0.10574408792844098, -0.80816047738475194, 0.57939177648757834},
      {-0.10574408792838257, -0.80816047738438168, 0.5793917764881058},
      {-0.1057440879283823, -0.80816047738438002, 0.57939177648810791},
      {-0.10574407993470979, -0.80816042849578984, 0.57939184613891748},
      {-0.10574408013270691, -0.80816042846771807, 0.57939184614193739},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Loop 0 edge 8 crosses loop 1 edge 0
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug6) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10618849949725141, -0.80552159562437586, 0.58297423747304822},
      {-0.10618849959636036, -0.80552159561106063, 0.58297423747339361},
      {-0.10618849949722192, -0.80552159562415893, 0.5829742374733532},
      {-0.10618834540082922, -0.80552043435619214, 0.58297587011440333},
      {-0.10618834559910612, -0.80552043432999554, 0.58297587011448437},
      {-0.10618849969546933, -0.80552159559774539, 0.58297423747373922},
      {-0.10618849969546955, -0.80552159559774716, 0.582974237473737},
      {-0.10618849969549882, -0.80552159559796233, 0.58297423747343424},
      {-0.10618849959710704, -0.80552159561096182, 0.58297423747339394},
      {-0.10618849949725161, -0.80552159562437742, 0.58297423747304589},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10618856154870562, -0.80552206324314812, 0.58297358004005528},
      {-0.10618849949722212, -0.80552159562416048, 0.58297423747335086},
      {-0.10618849969549901, -0.80552159559796388, 0.58297423747343191},
      {-0.10618856174698249, -0.8055220632169513, 0.58297358004013622},
      {-0.10618857104277038, -0.80552213326985989, 0.58297348155149287},
      {-0.10618857084449349, -0.80552213329605649, 0.58297348155141182},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Loop 0 edge 0 crosses loop 1 edge 4
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug7) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10651728339354898, -0.80806023027835039, 0.57938996589599123},
      {-0.10651728368541774, -0.80806023024121265, 0.57938996589412783},
      {-0.10651743884289547, -0.80806147782022508, 0.5793881973990701},
      {-0.1065172793067945, -0.80806153133252501, 0.5793881520963412},
      {-0.10651707335497011, -0.80806158532388361, 0.57938811465868356},
      {-0.10651593657771009, -0.80806167503227055, 0.57938819853274059},
      {-0.10651567693742285, -0.80806182530835402, 0.57938803667826444},
      {-0.10651496089498214, -0.80806213485510237, 0.57938773659696563},
      {-0.10651453461919227, -0.80806229235522298, 0.57938759530083062},
      {-0.10651448583749658, -0.80806230280784852, 0.57938758969074455},
      {-0.10651428153471061, -0.80806061225022852, 0.57938998503506256},
      {-0.10651428161845182, -0.8080606122395747, 0.57938998503452654},
      {-0.10651427761078044, -0.80806057978063328, 0.57939003104095654},
      {-0.10651427761077951, -0.80806057978062562, 0.57939003104096709},
      {-0.10651387099203104, -0.8080572864940091, 0.5793946988282096},
      {-0.10651387099202798, -0.80805728649398445, 0.57939469882824468},
      {-0.10651386444607201, -0.80805723347699177, 0.57939477397218053},
      {-0.10651386444607169, -0.8080572334769891, 0.57939477397218409},
      {-0.106513765993723, -0.80805643609199118, 0.57939590414857456},
      {-0.10651376671438624, -0.8080564359989727, 0.57939590414581921},
      {-0.10651368187839319, -0.80805575808078389, 0.57939686520139033},
      {-0.10651465698432123, -0.80805552598235797, 0.57939700963750851},
      {-0.1065149024434091, -0.80805548225095913, 0.57939702550292815},
      {-0.10651504788182964, -0.80805555533715756, 0.5793968968362615},
      {-0.10651511658091152, -0.80805559604710031, 0.57939682743066534},
      {-0.10651517919248171, -0.80805562751022852, 0.57939677204023521},
      {-0.10651528575974038, -0.80805561374213786, 0.57939677165077275},
      {-0.10651648823358072, -0.80805539171529139, 0.57939686023850034},
      {-0.10651666406737116, -0.80805537863686483, 0.57939684615295572},
      {-0.10651674780673852, -0.80805605121551227, 0.57939589274577097},
      {-0.10651674667750256, -0.80805605136137271, 0.57939589274994641},
      {-0.10651678418140036, -0.80805634336988752, 0.57939547860450136},
      {-0.10651680240261223, -0.80805648524178364, 0.57939527739240138},
      {-0.10651680240261237, -0.80805648524178486, 0.57939527739239993},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10651727337444802, -0.80806023111043901, 0.57938996657744879},
      {-0.10651727440799089, -0.80806022882029649, 0.57938996958144073},
      {-0.10651679374955145, -0.80805648637258243, 0.57939527740611751},
      {-0.10651677552833975, -0.80805634450068775, 0.57939547861821594},
      {-0.10651673802444192, -0.80805605249217261, 0.57939589276366099},
      {-0.10651674651102909, -0.80805605138312775, 0.5793958927502102},
      {-0.10651673915225639, -0.80805605233507238, 0.57939589277542292},
      {-0.10651665541288889, -0.80805537975642383, 0.57939684618260878},
      {-0.10651667272185343, -0.80805537751730583, 0.57939684612330267},
      {-0.1065167564612207, -0.8080560500959526, 0.57939589271611924},
      {-0.1065167553320342, -0.80805605024202609, 0.57939589271998793},
      {-0.10651679283446101, -0.80805634223908773, 0.57939547859078699},
      {-0.10651681105567287, -0.80805648411098374, 0.57939527737868723},
      {-0.10651680240318392, -0.80805648524170914, 0.5793952773924006},
      {-0.10651680240261234, -0.80805648524178475, 0.57939527739239982},
      {-0.1065168110556733, -0.80805648411098718, 0.57939527737868224},
      {-0.10651729169518892, -0.80806022641135866, 0.57938996976297907},
      {-0.10651729210462238, -0.80806022661896348, 0.579389969398166},
      {-0.1065172934126499, -0.80806022944626155, 0.57938996521453356},
      {-0.10651729203606744, -0.80806023249651726, 0.57938996121349717},
      {-0.1065172883437291, -0.80806023495241674, 0.57938995846713126},
      {-0.10651728332499401, -0.80806023615590394, 0.5793899577113224},
      {-0.10651727832462815, -0.80806023578450537, 0.57938995914858893},
      {-0.10651727468247554, -0.80806023393773707, 0.57938996239381635},
    },
    {
      {-0.10651680240204828, -0.80805648524185858, 0.57939527739240082},
      {-0.10651679861449742, -0.80805648573682254, 0.57939527739840524},
      {-0.10651680240261419, -0.80805648524178353, 0.57939527739240138},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Loop 0: Edge 33 crosses edge 35
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug8) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10703872198218529, -0.80846112144645677, 0.57873424566545062},
      {-0.10703872122182066, -0.80846111957630917, 0.57873424841857957},
      {-0.10703873813385757, -0.80846111582010538, 0.57873425053786276},
      {-0.1070387388942222, -0.80846111769025297, 0.57873424778473381},
      {-0.10703873050793056, -0.80846111955286837, 0.57873424673382978},
      {-0.1070387388942227, -0.80846111769025419, 0.57873424778473193},
      {-0.10703919382477994, -0.80846223660916783, 0.57873260056976505},
      {-0.10703917691274406, -0.80846224036537406, 0.57873259845047831},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10703917691274355, -0.80846224036537273, 0.57873259845047997},
      {-0.1070391853685064, -0.8084622384873289, 0.57873259951008804},
      {-0.10703919381027188, -0.80846223657409677, 0.57873260062144094},
      {-0.10703919381027233, -0.80846223657409788, 0.57873260062143939},
      {-0.10703918536876245, -0.80846223848727206, 0.57873259951012024},
      {-0.10703919382478132, -0.80846223660917116, 0.57873260056976017},
      {-0.10703957146434441, -0.80846316542623331, 0.57873123320737097},
      {-0.10703955455230836, -0.8084631691824391, 0.57873123108808489},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2_VLOG(1) << "\nS2Polygon: " << s2textformat::ToString(a);
  S2_VLOG(1) << "\nS2Polygon: " << s2textformat::ToString(b);
  S2Polygon c;
  c.InitToUnion(&a, &b);
  //  Loop 1: Edge 1 crosses edge 3
  S2_VLOG(1) << "\nS2Polygon: " << s2textformat::ToString(c);
}

TEST(S2Polygon, Bug9) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10639937100501309, -0.80810205676564995, 0.57935329437301375},
      {-0.10639937101137514, -0.80810205688156922, 0.57935329421015713},
      {-0.10639937101137305, -0.80810205688156944, 0.57935329421015713},
      {-0.106399371005011, -0.80810205676565017, 0.57935329437301375},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10639937099530022, -0.8081020567669569, 0.57935329437297489},
      {-0.10639937102108385, -0.80810205688026293, 0.5793532942101961},
      {-0.10639937102108181, -0.80810205688026326, 0.5793532942101961},
      {-0.10639937099529816, -0.80810205676695701, 0.57935329437297478},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Given edges do not form loops (indegree != outdegree)
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug10) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10592889932808099, -0.80701394501854917, 0.58095400922339757},
      {-0.10592787800899696, -0.8070140771413753, 0.58095401191158469},
      {-0.1059270044681431, -0.80701419014619669, 0.58095401421031945},
      {-0.10592685562894633, -0.80701420940058122, 0.58095401460194696},
      {-0.10592685502239066, -0.80701420947920588, 0.58095401460332308},
      {-0.10592681668594067, -0.80701421444855337, 0.5809540146902914},
      {-0.10592586497682262, -0.8070143378130904, 0.58095401684902004},
      {-0.10592586434121586, -0.80701433789547994, 0.58095401685046155},
      {-0.10592585898876766, -0.80701428569270217, 0.58095409034224832},
      {-0.10592585898876755, -0.80701428569270128, 0.58095409034224987},
      {-0.10592571912106936, -0.8070129215545373, 0.58095601078971082},
      {-0.10592571912106795, -0.80701292155452331, 0.58095601078973025},
      {-0.10592546626664477, -0.80701045545315664, 0.58095948256783148},
      {-0.10592546630689463, -0.80701045544795602, 0.58095948256771723},
      {-0.10592538513536764, -0.80700975616910509, 0.58096046873415197},
      {-0.10592564439344856, -0.80700971612782446, 0.58096047708524956},
      {-0.1059267844512099, -0.80700966174311928, 0.58096034476466896},
      {-0.10592686088387009, -0.80700965393230761, 0.58096034167862642},
      {-0.10592691331665709, -0.80700961093727019, 0.58096039184274961},
      {-0.10592705773734933, -0.80700947507458121, 0.58096055423665138},
      {-0.10592721940752658, -0.80700934249808198, 0.58096070892049412},
      {-0.10592756003095027, -0.80700933299293154, 0.58096066001769275},
      {-0.10592832507751106, -0.80700935762745474, 0.58096048630521868},
      {-0.1059284165295875, -0.80701007424011018, 0.58095947418602778},
      {-0.10592841614913188, -0.80701007428931704, 0.58095947418704452},
      {-0.10592864947042728, -0.8070119434176124, 0.58095683523192998},
      {-0.1059286884898481, -0.80701225600079662, 0.58095639390519271},
      {-0.10592868927069989, -0.80701225581371527, 0.58095639402269295},
      {-0.10592869427137827, -0.80701225619024619, 0.58095639258785126},
      {-0.10592869791375134, -0.80701225804491505, 0.58095638934738025},
      {-0.10592869922184817, -0.80701226088076483, 0.5809563851695615},
      {-0.10592869922184843, -0.80701226088076705, 0.58095638516955805},
      {-0.10592869784516552, -0.80701226393793402, 0.58095638117383475},
      {-0.10592869415258396, -0.80701226639725276, 0.58095637843085768},
      {-0.10592868991437976, -0.80701226741266929, 0.58095637779310561},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10592564460843924, -0.80700972122716552, 0.58096046996257766},
      {-0.10592539435053176, -0.80700975987840939, 0.58096046190138972},
      {-0.10592547496472972, -0.80701045435596641, 0.58095948250602925},
      {-0.10592546630689462, -0.80701045544795591, 0.58095948256771723},
      {-0.10592546630693271, -0.80701045544826022, 0.58095948256728758},
      {-0.1059254749287661, -0.80701045440038255, 0.5809594824508878},
      {-0.10592572778318898, -0.80701292050174633, 0.58095601067279068},
      {-0.1059257191207934, -0.80701292155455673, 0.58095601078973391},
      {-0.1059257194541381, -0.80701292151405679, 0.58095601078521419},
      {-0.10592572778319062, -0.80701292050176254, 0.58095601067276803},
      {-0.10592586765088864, -0.80701428463992497, 0.58095409022530931},
      {-0.10592585899855227, -0.80701428569151201, 0.58095409034211776},
      {-0.10592585898857355, -0.80701428569272593, 0.58095409034225098},
      {-0.10592586765088888, -0.80701428463992686, 0.58095409022530675},
      {-0.10592587247896063, -0.80701433172842685, 0.58095402393347073},
      {-0.10592681605007616, -0.80701420941876889, 0.58095402179319922},
      {-0.10592685438651758, -0.80701420444942229, 0.58095402170623067},
      {-0.10592685499307326, -0.80701420437079774, 0.58095402170485466},
      {-0.10592685562894634, -0.80701420940058122, 0.58095401460194696},
      {-0.10592685499689927, -0.80701420437030225, 0.58095402170484534},
      {-0.10592700383609792, -0.80701418511591771, 0.58095402131321794},
      {-0.10592787737695626, -0.80701407211109533, 0.58095401901448296},
      {-0.10592889869604118, -0.80701393998826909, 0.58095401632629584},
      {-0.10592889996012077, -0.80701395004882903, 0.58095400212049919},
      {-0.10592787864104941, -0.80701408217165349, 0.58095400480868631},
      {-0.10592787800903029, -0.80701407714164064, 0.58095401191120999},
      {-0.10592787864103763, -0.80701408217165482, 0.5809540048086862},
      {-0.10592700510019466, -0.80701419517647521, 0.58095400710742118},
      {-0.1059270044681431, -0.80701419014619669, 0.58095401421031934},
      {-0.10592700510018833, -0.8070141951764761, 0.58095400710742118},
      {-0.10592685626275877, -0.80701421443063182, 0.58095400749904391},
      {-0.10592685565826369, -0.80701421450898914, 0.58095400750041526},
      {-0.10592685502239063, -0.80701420947920566, 0.58095401460332308},
      {-0.10592685565826078, -0.80701421450898947, 0.58095400750041526},
      {-0.10592681732181129, -0.80701421947833718, 0.58095400758738369},
      {-0.10592681668594069, -0.80701421444855348, 0.58095401469029151},
      {-0.10592681732180521, -0.80701421947833796, 0.58095400758738369},
      {-0.10592586561269894, -0.80701434284287321, 0.58095400974611222},
      {-0.10592586497746249, -0.80701433781815202, 0.58095401684187198},
      {-0.10592586561268771, -0.80701434284287465, 0.58095400974611222},
      {-0.10592586497708102, -0.80701434292526464, 0.58095400974755396},
      {-0.10592586434121586, -0.80701433789548005, 0.58095401685046166},
      {-0.10592585567909471, -0.80701433894825569, 0.58095401696740323},
      {-0.1059258503266465, -0.80701428674547793, 0.58095409045919011},
      {-0.10592571045894811, -0.80701292260731206, 0.58095601090665361},
      {-0.10592571912060067, -0.80701292155459425, 0.58095601078971715},
      {-0.10592571878923682, -0.80701292159485349, 0.58095601079421},
      {-0.10592571045894694, -0.80701292260730051, 0.58095601090666993},
      {-0.10592545760452345, -0.80701045650593073, 0.58095948268477515},
      {-0.10592545764454649, -0.80701045650106651, 0.58095948268423492},
      {-0.10592537647753246, -0.80700975726109381, 0.58096046879584118},
      {-0.10592538513536764, -0.80700975616910509, 0.58096046873415197},
      {-0.10592538413784101, -0.80700975119062324, 0.58096047583161736},
      {-0.10592564339592514, -0.80700971114934217, 0.58096048418271495},
      {-0.10592564439344856, -0.80700971612782446, 0.58096047708524956},
      {-0.10592564496449927, -0.80700971099098684, 0.58096048411668999},
      {-0.10592678502227458, -0.80700965660628099, 0.58096035179610783},
      {-0.10592678388014524, -0.80700966687995779, 0.58096033773323019},
    },
    {
      {-0.10592585898876757, -0.80701428569270128, 0.58095409034224987},
      {-0.10592585897888845, -0.80701428569390288, 0.58095409034238166},
      {-0.1059258503266465, -0.80701428674547793, 0.58095409045919011},
    },
    {
      {-0.10592546626664477, -0.80701045545315664, 0.58095948256783148},
      {-0.10592546623958927, -0.8070104554564449, 0.58095948256819674},
      {-0.10592546626662946, -0.80701045545303429, 0.580959482568004},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2_VLOG(1) << "\nS2Polygon: " << s2textformat::ToString(a);
  S2_VLOG(1) << "\nS2Polygon: " << s2textformat::ToString(b);
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Inconsistent loop orientations detected
  S2_VLOG(1) << "\nS2Polygon: " << s2textformat::ToString(c);
}

TEST(S2Polygon, Bug11) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10727349803435572, -0.80875763107088172, 0.57827631008375979},
      {-0.10727349807040805, -0.80875763112192245, 0.57827631000568813},
      {-0.10727349807040625, -0.80875763112192278, 0.57827631000568813},
    },
    {
      {-0.1072729603486537, -0.80875606054879057, 0.57827860629945249},
      {-0.10727299870478688, -0.80875633377729705, 0.57827821705818028},
      {-0.10727299875560981, -0.80875633413933223, 0.57827821654242495},
      {-0.10727309272230967, -0.80875700360375646, 0.57827726282438607},
      {-0.10727318660000487, -0.80875767243400742, 0.57827631000742785},
      {-0.10727349802669105, -0.80875763101356435, 0.57827631016534387},
      {-0.10727349803435525, -0.80875763107087817, 0.57827631008376468},
      {-0.10727349803435572, -0.80875763107088172, 0.57827631008375979},
      {-0.1072734980420204, -0.80875763112819909, 0.57827631000217561},
      {-0.10727318657570066, -0.80875767255391384, 0.57827630984423972},
      {-0.10727318651657966, -0.80875767256177711, 0.57827630984420975},
      {-0.10727318650891528, -0.80875767250445951, 0.57827630992579371},
      {-0.10727318640981781, -0.80875767251785957, 0.57827630992543622},
      {-0.10727309252411468, -0.80875700363055636, 0.57827726282367087},
      {-0.10727299855741491, -0.8087563341661328, 0.57827821654170874},
      {-0.10727299850659211, -0.8087563338040985, 0.57827821705746318},
      {-0.10727296014242577, -0.80875606051836801, 0.57827860638025652},
      {-0.10727296024152315, -0.80875606050496729, 0.57827860638061501},
      {-0.10727296023340849, -0.8087560604477102, 0.57827860646219797},
      {-0.10727348576547496, -0.80875598914629976, 0.57827860869282954},
      {-0.1072734857817042, -0.80875598926081438, 0.57827860852966395},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.1072734857735896, -0.80875598920355718, 0.5782786086112468},
      {-0.10727348576547457, -0.80875598914629976, 0.57827860869282954},
      {-0.10727839137361543, -0.80875532356817348, 0.57827862950694298},
      {-0.10727839137881608, -0.80875532356471602, 0.57827862951081388},
      {-0.10727839143632178, -0.80875532355090063, 0.5782786295194674},
      {-0.10727839149361706, -0.80875532355509905, 0.57827862950296649},
      {-0.1072783915353497, -0.80875532357618651, 0.57827862946573261},
      {-0.10727839154773799, -0.80875532360290581, 0.57827862942606567},
      {-0.10727848921795155, -0.80875531035110082, 0.57827862984032907},
      {-0.1072784892332832, -0.80875531046514559, 0.57827862967798682},
      {-0.10727971608197531, -0.8087551454635169, 0.57827863284376713},
      {-0.10727986275126807, -0.80875539440654376, 0.57827825747332484},
      {-0.10727959167812619, -0.80875599171505064, 0.57827747239052929},
      {-0.10727974196569352, -0.80875625444235633, 0.57827707706958686},
      {-0.10727993501555312, -0.80875677560355186, 0.57827631237878363},
      {-0.10727870858143702, -0.80875693828645479, 0.57827631237896882},
      {-0.1072787085493927, -0.80875693804871851, 0.5782763127174031},
      {-0.10727615977928232, -0.80875727704955946, 0.57827631143112901},
      {-0.10727615977915911, -0.80875727704957578, 0.57827631143112901},
      {-0.10727349803435751, -0.80875763107088128, 0.57827631008375968},
      {-0.10727349803435574, -0.80875763107088183, 0.57827631008375979},
      {-0.10727318656803594, -0.80875767249659658, 0.57827630992582391},
      {-0.10727318650891531, -0.80875767250445962, 0.57827630992579382},
      {-0.10727309262321218, -0.80875700361715641, 0.57827726282402847},
      {-0.10727299865651231, -0.80875633415273218, 0.57827821654206735},
      {-0.10727299860568951, -0.80875633379069789, 0.57827821705782179},
      {-0.10727296024152314, -0.80875606050496718, 0.57827860638061501},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Given edges do not form loops (indegree != outdegree)
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug12) {
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.10772916872905106, -0.80699542608967267, 0.58064861015531188},
      {-0.10772916892726483, -0.80699542606300401, 0.58064861015560143},
      {-0.10772916892726613, -0.80699542606301333, 0.58064861015558844},
      {-0.10772916872905235, -0.806995426089682, 0.58064861015529889},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.10772916872905348, -0.80699542608969022, 0.58064861015528724},
      {-0.10772916892726496, -0.80699542606300489, 0.58064861015559999},
      {-0.10772930108168739, -0.80699639165138115, 0.58064724364290399},
      {-0.10772930088347589, -0.80699639167806647, 0.58064724364259113},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Given edges do not form loops (indegree != outdegree)
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug13) {
  // This test exercises a rare special case in GetCrossedVertexIndex where
  // two crossing edge chains snap to a different permutation of the same
  // vertices.  In this example one input edge crosses another edge from right
  // to left, the first edge snaps to BCD and the second snaps to ABDC, and
  // triangle BCD is CCW.  Since BCD is to the right of BD, this means that
  // the first edge has not yet crossed the second at vertex B, leaving C or D
  // as the possible crossing vertices.
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.38306437985388492, -0.74921955334206214, 0.54030708099846292},
      {-0.3830643798552798, -0.74921955334134249, 0.5403070809984718},
      {-0.38306437985529124, -0.74921955334136414, 0.54030708099843361},
      {-0.38306437985389635, -0.74921955334208379, 0.54030708099842473},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.38306437985390962, -0.74921955334210588, 0.54030708099838465},
      {-0.38306437985527797, -0.74921955334134205, 0.54030708099847369},
      {-0.38306437985527941, -0.74921955334134405, 0.54030708099847014},
      {-0.38306437985391095, -0.74921955334210777, 0.54030708099838098},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Given edges do not form loops (indegree != outdegree)
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

TEST(S2Polygon, Bug14) {
  // This test exercises another rare case where the crossing vertices chosen
  // by GetCrossedVertexIndex() are not ordered correctly along the edge being
  // crossed.  This is handled by adding extra edges to the output in order to
  // link up the crossings in the correct order.
  vector<vector<S2Point>> a_vertices = {
    {
      {-0.3837392878495085, -0.7477800800281974, 0.5418201831546835},
      {-0.38373928785696076, -0.7477800800212292, 0.54182018315902258},
      {-0.38373928785701278, -0.74778008002124685, 0.5418201831589613},
      {-0.38373928785703426, -0.7477800800212544, 0.54182018315893576},
      {-0.38373947205489456, -0.74778014227795497, 0.5418199667802881},
      {-0.38373947204434411, -0.74778014228781997, 0.54181996677414512},
      {-0.38373947205872994, -0.74778014228185352, 0.54181996677219124},
      {-0.38373947218468357, -0.74778014288930306, 0.54181996584462788},
      {-0.3837396702525171, -0.74778021044361542, 0.54181973233114322},
      {-0.38373967023137123, -0.74778021046333043, 0.54181973231891067},
      {-0.38373947216030285, -0.74778014290791484, 0.54181996583620895},
      {-0.38373947217087578, -0.74778014289805739, 0.54181996584232528},
      {-0.38373947215649007, -0.74778014290402395, 0.54181996584427927},
      {-0.3837394720305386, -0.74778014229658485, 0.5418199667718262},
      {-0.38373928783585998, -0.74778008004095942, 0.54182018314673686},
      {-0.38373928784641037, -0.7477800800310942, 0.54182018315287972},
      {-0.38373928783578648, -0.74778008004093421, 0.54182018314682368},
      {-0.383739287835765, -0.74778008004092666, 0.54182018314684921},
    },
  };
  vector<vector<S2Point>> b_vertices = {
    {
      {-0.38373923813692823, -0.7477800632164362, 0.54182024156551456},
      {-0.3837392878569364, -0.74778008002122087, 0.54182018315905123},
      {-0.38373928784640354, -0.74778008003106944, 0.54182018315291858},
      {-0.38373928784638789, -0.74778008003108642, 0.54182018315290648},
      {-0.38373928784638023, -0.74778008003109453, 0.54182018315290048},
      {-0.38373928783692102, -0.74778008004124585, 0.54182018314559},
      {-0.38373928783691913, -0.74778008004124541, 0.54182018314559188},
      {-0.38373928784636568, -0.74778008003110774, 0.54182018315289271},
      {-0.38373928784637329, -0.74778008003109953, 0.54182018315289848},
      {-0.38373928783583561, -0.74778008004095109, 0.5418201831467655},
      {-0.38373923811582744, -0.74778006323616641, 0.54182024155322883},
      {-0.38373857650312843, -0.74777983961840766, 0.54182101875399913},
      {-0.38373857652422921, -0.74777983959867744, 0.54182101876628486},
    },
  };
  S2Polygon a(MakeLoops(a_vertices));
  S2Polygon b(MakeLoops(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  // Given edges do not form loops (indegree != outdegree)
  EXPECT_FALSE(c.is_empty())
      << "\nS2Polygon: " << s2textformat::ToString(a)
      << "\nS2Polygon: " << s2textformat::ToString(b);
}

static void PolylineIntersectionSharedEdgeTest(const S2Polygon& p,
                                               int start_vertex,
                                               int direction) {
  SCOPED_TRACE(StrCat("Polyline intersection shared edge test"
                      " start=", start_vertex,
                      " direction=", direction));
  vector<S2Point> points = {p.loop(0)->vertex(start_vertex),
                            p.loop(0)->vertex(start_vertex + direction)};
  S2Polyline polyline(points);
  vector<unique_ptr<S2Polyline>> polylines;
  if (direction < 0) {
    polylines = p.IntersectWithPolyline(polyline);
    EXPECT_EQ(0, polylines.size());
    polylines = p.SubtractFromPolyline(polyline);
    ASSERT_EQ(1, polylines.size());
    ASSERT_EQ(2, polylines[0]->num_vertices());
    EXPECT_EQ(points[0], polylines[0]->vertex(0));
    EXPECT_EQ(points[1], polylines[0]->vertex(1));
    EXPECT_FALSE(p.Intersects(polyline));
    EXPECT_FALSE(p.Contains(polyline));
  } else {
    polylines = p.IntersectWithPolyline(polyline);
    ASSERT_EQ(1, polylines.size());
    ASSERT_EQ(2, polylines[0]->num_vertices());
    EXPECT_EQ(points[0], polylines[0]->vertex(0));
    EXPECT_EQ(points[1], polylines[0]->vertex(1));
    polylines = p.SubtractFromPolyline(polyline);
    EXPECT_EQ(0, polylines.size());
    EXPECT_TRUE(p.Intersects(polyline));
    EXPECT_TRUE(p.Contains(polyline));
  }
}

// This tests polygon-polyline intersections.
// It covers the same edge cases as TestOperations and also adds some
// extra tests for shared edges.
TEST_F(S2PolygonTestBase, PolylineIntersection) {
  for (int v = 0; v < 3; ++v) {
    PolylineIntersectionSharedEdgeTest(*cross1_, v, 1);
    PolylineIntersectionSharedEdgeTest(*cross1_, v + 1, -1);
    PolylineIntersectionSharedEdgeTest(*cross1_side_hole_, v, 1);
    PolylineIntersectionSharedEdgeTest(*cross1_side_hole_, v + 1, -1);
  }

  // See comments in TestOperations about the vlue of this constant.
  static const S1Angle kMaxError = S1Angle::Radians(1e-4);

  // This duplicates some of the tests in TestOperations by
  // converting the outline of polygon A to a polyline then intersecting
  // it with the polygon B. It then converts B to a polyline and intersects
  // it with A. It then feeds all of the results into a polygon builder and
  // tests that the output is equal to doing an intersection between A and B.
  int i = 0;
  for (const TestCase& test : test_cases) {
    SCOPED_TRACE(StrCat("Polyline intersection test case ", i++));
    unique_ptr<S2Polygon> a(MakePolygon(test.a));
    unique_ptr<S2Polygon> b(MakePolygon(test.b));
    unique_ptr<S2Polygon> expected_a_and_b(MakePolygon(test.a_and_b));

    vector<S2Point> points;
    vector<unique_ptr<S2Polyline>> polylines;
    for (int ab = 0; ab < 2; ab++) {
      S2Polygon *tmp = ab ? a.get() : b.get();
      S2Polygon *tmp2 = ab ? b.get() : a.get();
      for (int l = 0; l < tmp->num_loops(); l++) {
        points.clear();
        if (tmp->loop(l)->is_hole()) {
          for (int v = tmp->loop(l)->num_vertices(); v >=0 ; v--) {
            points.push_back(tmp->loop(l)->vertex(v));
          }
        } else {
          for (int v = 0; v <= tmp->loop(l)->num_vertices(); v++) {
            points.push_back(tmp->loop(l)->vertex(v));
          }
        }
        S2Polyline polyline(points);
        vector<unique_ptr<S2Polyline>> tmp =
            tmp2->IntersectWithPolyline(polyline);
        polylines.insert(polylines.end(),
                         std::make_move_iterator(tmp.begin()),
                         std::make_move_iterator(tmp.end()));
      }
    }

    S2Builder builder{S2Builder::Options()};
    S2Polygon a_and_b;
    builder.StartLayer(make_unique<s2builderutil::S2PolygonLayer>(&a_and_b));
    for (const auto& polyline : polylines) {
      builder.AddPolyline(*polyline);
    }

    S2Error error;
    ASSERT_TRUE(builder.Build(&error)) << error;
    CheckEqual(a_and_b, *expected_a_and_b, kMaxError);
  }
}

static void CheckCoveringIsConservative(const S2Polygon& polygon,
                                        const vector<S2CellId>& cells) {
  // Check that Contains(S2Cell) and MayIntersect(S2Cell) are implemented
  // conservatively, by comparing against the Contains/Intersect result with
  // the "cell polygon" defined by the four cell vertices.  Please note that
  // the cell polygon is *not* an exact representation of the S2Cell: cell
  // vertices are rounded from their true mathematical positions, which leads
  // to tiny cracks and overlaps between the cell polygons at different cell
  // levels.  That is why Contains(S2Cell) and MayIntersect(S2Cell) cannot be
  // implemented by simply converting the cell to an S2Polygon.  But it is
  // still useful to do this as a sanity check.  In particular:
  //
  //  - If Contains(cell) is true, the polygon must contain the cell polygon.
  //  - If the polygon intersects the cell polygon, then MayIntersect(cell)
  //    must return true.
  //
  for (S2CellId cell_id : cells) {
    S2Cell cell(cell_id);
    S2Polygon cell_poly(cell);
    if (polygon.Contains(cell)) {
      EXPECT_TRUE(polygon.Contains(&cell_poly));
    }
    if (polygon.Intersects(&cell_poly)) {
      EXPECT_TRUE(polygon.MayIntersect(cell));
    }
  }
}

// Remove a random polygon from "pieces" and return it.
static unique_ptr<S2Polygon> ChoosePiece(
    vector<unique_ptr<S2Polygon>> *pieces) {
  int i = S2Testing::rnd.Uniform(pieces->size());
  unique_ptr<S2Polygon> result = std::move((*pieces)[i]);
  pieces->erase(pieces->begin() + i);
  return result;
}

static void SplitAndAssemble(const S2Polygon& polygon) {
  // Normalize the polygon's loop structure by rebuilding it with S2Builder.
  S2Builder builder{S2Builder::Options()};
  S2Polygon expected;
  builder.StartLayer(make_unique<s2builderutil::S2PolygonLayer>(&expected));
  builder.AddPolygon(polygon);

  S2Error error;
  ASSERT_TRUE(builder.Build(&error)) << error;

  for (int iter = 0; iter < (google::DEBUG_MODE ? 3 : 10); ++iter) {
    S2RegionCoverer coverer;
    // Compute the minimum level such that the polygon's bounding
    // cap is guaranteed to be cut.
    double diameter = 2 * polygon.GetCapBound().GetRadius().radians();
    int min_level = S2::kMaxWidth.GetLevelForMaxValue(diameter);

    // Now choose a level that has up to 500 cells in the covering.
    int level = min_level + S2Testing::rnd.Uniform(google::DEBUG_MODE ? 4 : 6);
    coverer.mutable_options()->set_min_level(min_level);
    coverer.mutable_options()->set_max_level(level);
    coverer.mutable_options()->set_max_cells(500);

    vector<S2CellId> cells;
    coverer.GetCovering(polygon, &cells);
    S2CellUnion covering;
    covering.Init(cells);
    S2Testing::CheckCovering(polygon, covering, false);
    CheckCoveringIsConservative(polygon, cells);
    S2_VLOG(2) << cells.size() << " cells in covering";
    vector<unique_ptr<S2Polygon>> pieces;
    int i = 0;
    for (S2CellId cell_id : cells) {
      S2Cell cell(cell_id);
      S2Polygon window(cell);
      auto piece = make_unique<S2Polygon>();
      piece->InitToIntersection(&polygon, &window);
      S2_VLOG(4) << "\nPiece " << i++ << ":\n  Window: "
              << s2textformat::ToString(window)
              << "\n  Piece: " << s2textformat::ToString(*piece);
      pieces.push_back(std::move(piece));
    }

    // Now we repeatedly remove two random pieces, compute their union, and
    // insert the result as a new piece until only one piece is left.
    //
    // We don't use S2Polygon::DestructiveUnion() because it joins the pieces
    // in a mostly deterministic order.  We don't just call random_shuffle()
    // on the pieces and repeatedly join the last two pieces in the vector
    // because this always joins a single original piece to the current union
    // rather than doing the unions according to a random tree structure.
    while (pieces.size() > 1) {
      unique_ptr<S2Polygon> a(ChoosePiece(&pieces));
      unique_ptr<S2Polygon> b(ChoosePiece(&pieces));
      auto c = make_unique<S2Polygon>();
      c->InitToUnion(a.get(), b.get());
      S2_VLOG(4) << "\nJoining piece a: " << s2textformat::ToString(*a)
              << "\n  With piece b: " << s2textformat::ToString(*b)
              << "\n  To get piece c: " << s2textformat::ToString(*c);
      pieces.push_back(std::move(c));
    }
    unique_ptr<S2Polygon> result(std::move(pieces[0]));
    pieces.pop_back();

    // The moment of truth!
    EXPECT_TRUE(expected.BoundaryNear(*result, S1Angle::Radians(2e-15)))
        << "\nActual:\n" << s2textformat::ToString(*result)
        << "\nExpected:\n" << s2textformat::ToString(expected);

    // Check that ApproxEquals produces the same result.
    if (!expected.ApproxEquals(result.get(),
                               S2::kIntersectionMergeRadius)) {
      S2Polygon symmetric_difference;
      symmetric_difference.InitToApproxSymmetricDifference(
          &expected, result.get(), S2::kIntersectionMergeRadius);
      ADD_FAILURE() << s2textformat::ToString(symmetric_difference);
    }
  }
}

TEST_F(S2PolygonTestBase, Splitting) {
  // It takes too long to test all the polygons in debug mode, so we just pick
  // out some of the more interesting ones.

  SplitAndAssemble(*near_10_);
  SplitAndAssemble(*near_H3210_);
  SplitAndAssemble(*far_H3210_);
  SplitAndAssemble(*south_0ab_);
  SplitAndAssemble(*south_210b_);
  SplitAndAssemble(*south_H20abc_);
  SplitAndAssemble(*nf1_n10_f2_s10abc_);
  SplitAndAssemble(*nf2_n2_f210_s210ab_);
  SplitAndAssemble(*far_H_);
  SplitAndAssemble(*south_H_);
  SplitAndAssemble(*far_H_south_H_);
}

TEST(S2Polygon, InitToCellUnionBorder) {
  // Test S2Polygon::InitToCellUnionBorder().
  // The main thing to check is that adjacent cells of different sizes get
  // merged correctly.  To do this we generate two random adjacent cells,
  // convert to polygon, and make sure the polygon only has a single loop.
  for (int iter = 0; iter < 200; ++iter) {
    SCOPED_TRACE(StrCat("Iteration ", iter));

    // Choose a random non-leaf cell.
    S2CellId big_cell =
        S2Testing::GetRandomCellId(S2Testing::rnd.Uniform(S2CellId::kMaxLevel));
    // Get all neighbors at some smaller level.
    int small_level = big_cell.level() +
        S2Testing::rnd.Uniform(min(16, S2CellId::kMaxLevel - big_cell.level()));
    vector<S2CellId> neighbors;
    big_cell.AppendAllNeighbors(small_level, &neighbors);
    // Pick one at random.
    S2CellId small_cell = neighbors[S2Testing::rnd.Uniform(neighbors.size())];
    // If it's diagonally adjacent, bail out.
    S2CellId edge_neighbors[4];
    big_cell.GetEdgeNeighbors(edge_neighbors);
    bool diagonal = true;
    for (int i = 0; i < 4; ++i) {
      if (edge_neighbors[i].contains(small_cell)) {
        diagonal = false;
      }
    }
    S2_VLOG(3) << iter << ": big_cell " << big_cell <<
        " small_cell " << small_cell;
    if (diagonal) {
      S2_VLOG(3) << "  diagonal - bailing out!";
      continue;
    }

    vector<S2CellId> cells;
    cells.push_back(big_cell);
    cells.push_back(small_cell);
    S2CellUnion cell_union;
    cell_union.Init(cells);
    EXPECT_EQ(2, cell_union.num_cells());
    S2Polygon poly;
    poly.InitToCellUnionBorder(cell_union);
    EXPECT_EQ(1, poly.num_loops());
    // If the conversion were perfect we could test containment, but due to
    // rounding the polygon won't always exactly contain both cells.  We can
    // at least test intersection.
    EXPECT_TRUE(poly.MayIntersect(S2Cell(big_cell)));
    EXPECT_TRUE(poly.MayIntersect(S2Cell(small_cell)));
  }
}

TEST(S2Polygon, UnionWithAmbgiuousCrossings) {
  vector<S2Point> a_vertices = {
    S2Point(0.044856812877680216, -0.80679210859571904, 0.5891301722422051),
    S2Point(0.044851868273159699, -0.80679240802900054, 0.5891301386444033),
    S2Point(0.044854246527738666, -0.80679240292188514, 0.58912996457145106)
  };
  vector<S2Point> b_vertices = {
    S2Point(0.044849715793028468, -0.80679253837178111, 0.58913012401412856),
    S2Point(0.044855344598821352, -0.80679219751320641, 0.589130162266992),
    S2Point(0.044854017712818696, -0.80679210327223405, 0.58913039235179754)
  };
  S2Polygon a(make_unique<S2Loop>(a_vertices));
  S2Polygon b(make_unique<S2Loop>(b_vertices));
  S2Polygon c;
  c.InitToUnion(&a, &b);
  EXPECT_FALSE(c.is_empty());
}

TEST(S2Polygon, InitToSloppySupportsEmptyPolygons) {
  S2Polygon empty_polygon;
  S2Polygon polygon;
  polygon.InitToSnapped(&empty_polygon);
  // InitToSloppy is further tested by SnapSplitsPolygon.
}

TEST(S2Polygon, InitToSnappedDoesNotRotateVertices) {
  // This particular example came from MapFacts, but in fact InitToSnapped
  // used to cyclically rotate the vertices of all "hole" loops.
  unique_ptr<S2Polygon> polygon(s2textformat::MakePolygon(
      "49.9305505:-124.8345463, 49.9307448:-124.8299657, "
      "49.9332101:-124.8301996, 49.9331224:-124.8341368; "
      "49.9311087:-124.8327042, 49.9318176:-124.8312621, "
      "49.9318866:-124.8334451"));
  S2Polygon polygon2, polygon3;
  polygon2.InitToSnapped(polygon.get());

  // Check that the first vertex is the same when converted to E7.
  EXPECT_EQ(S2LatLng::Latitude(polygon->loop(0)->vertex(0)).e7(),
            S2LatLng::Latitude(polygon2.loop(0)->vertex(0)).e7());
  EXPECT_EQ(S2LatLng::Longitude(polygon->loop(0)->vertex(0)).e7(),
            S2LatLng::Longitude(polygon2.loop(0)->vertex(0)).e7());

  // Check that snapping twice doesn't rotate the vertices.
  polygon3.InitToSnapped(&polygon2);
  EXPECT_TRUE(polygon2.Equals(&polygon3));
}

TEST(S2Polygon, InitToSnappedWithSnapLevel) {
  const unique_ptr<const S2Polygon> polygon(
      s2textformat::MakePolygon("0:0, 0:2, 2:0; 0:0, 0:-2, -2:-2, -2:0"));
  for (int level = 0; level <= S2CellId::kMaxLevel; ++level) {
    S2Polygon snapped_polygon;
    snapped_polygon.InitToSnapped(polygon.get(), level);
    EXPECT_TRUE(snapped_polygon.IsValid());
    S1Angle merge_radius = min(S1Angle::Radians(S2::kMaxDiag.GetValue(level)),
                               S2Builder::SnapFunction::kMaxSnapRadius());
    EXPECT_TRUE(snapped_polygon.ApproxContains(polygon.get(), merge_radius));
  }
}

TEST(S2Polygon, InitToSnappedIsValid_A) {
  std::unique_ptr<S2Polygon> poly(s2textformat::MakePolygon(
      "53.1328020478452:6.39444903453293, 53.1328019:6.394449, "
      "53.1327091:6.3961766, 53.1313753:6.3958652, 53.1312825:6.3975924, "
      "53.132616:6.3979042, 53.1326161348736:6.39790423150577"));
  S2_LOG(INFO) << "\nInput: " << s2textformat::ToString(*poly);
  EXPECT_TRUE(poly->IsValid());
  S2Polygon poly_snapped;
  poly_snapped.set_s2debug_override(S2Debug::DISABLE);
  poly_snapped.InitToSnapped(poly.get());
  S2_LOG(INFO) << "\nSnapped: " << s2textformat::ToString(poly_snapped);
  S2Error error;
  EXPECT_FALSE(poly_snapped.FindValidationError(&error)) << error;
}

TEST(S2Polygon, InitToSnappedIsValid_B) {
  std::unique_ptr<S2Polygon> poly(s2textformat::MakePolygon(
      "51.6621651:4.9858102, 51.6620965:4.9874227, 51.662028:4.9890355, "
      "51.6619796006122:4.99017864445347, 51.6622335420397:4.98419752545216, "
      "51.6622334:4.9841975; 51.66189957578:4.99206198576131, "
      "51.6618911:4.9922612, 51.6618224:4.9938741, 51.6605122:4.993639, "
      "51.6604437:4.9952519, 51.6603751:4.9968648, 51.6603064:4.9984777, "
      "51.6602379:5.0000907, 51.660169:5.0017037, 51.6601003:5.0033165, "
      "51.6600318:5.0049298, 51.659963:5.0065427, 51.6598943:5.0081561, "
      "51.6612044207178:5.00839208571886, 51.6612732068132:5.00677860122814, "
      "51.6612732:5.0067786, 51.6613418:5.0051654, 51.6614106:5.0035525, "
      "51.6614793:5.0019393, 51.6615479:5.0003263, "
      "51.6615946694783:4.99923124520759, 51.6616389353165:4.99819106536521, "
      "51.6616852:4.9971, 51.6617538:4.995487, "
      "51.661753964726:4.99548702962593"));
  S2_LOG(INFO) << "\nInput: " << s2textformat::ToString(*poly);
  EXPECT_TRUE(poly->IsValid());
  S2Polygon poly_snapped;
  poly_snapped.set_s2debug_override(S2Debug::DISABLE);
  poly_snapped.InitToSnapped(poly.get());
  S2_LOG(INFO) << "\nSnapped: " << s2textformat::ToString(poly_snapped);
  S2Error error;
  EXPECT_FALSE(poly_snapped.FindValidationError(&error)) << error;
}

TEST(S2Polygon, InitToSnappedIsValid_C) {
  std::unique_ptr<S2Polygon> poly(s2textformat::MakePolygon(
      "53.5316236236404:19.5841192796855, 53.5416584:19.5915903, "
      "53.5416584189104:19.5915901888287; 53.5416584:19.5915903, "
      "53.5363122:19.62299, 53.5562817:19.6378935, 53.5616342:19.606474; "
      "53.5616342:19.606474, 53.5916039:19.6288326, 53.5912689:19.6307982, "
      "53.5925176:19.6317308, 53.5928526:19.6297652, 53.6015949:19.6362943, "
      "53.6015950436033:19.6362944072725, 53.6015950814439:19.6362941852262, "
      "53.5616342380536:19.6064737764314"));
  S2_LOG(INFO) << "\nInput: " << s2textformat::ToString(*poly);
  EXPECT_TRUE(poly->IsValid());
  S2Polygon poly_snapped;
  poly_snapped.set_s2debug_override(S2Debug::DISABLE);
  poly_snapped.InitToSnapped(poly.get());
  S2_LOG(INFO) << "\nSnapped: " << s2textformat::ToString(poly_snapped);
  S2Error error;
  EXPECT_FALSE(poly_snapped.FindValidationError(&error)) << error;
}

TEST(S2Polygon, InitToSnappedIsValid_D) {
  std::unique_ptr<S2Polygon> poly(s2textformat::MakePolygon(
      "52.0909316:4.8673826, 52.0909317627574:4.86738262858533, "
      "52.0911338452911:4.86248482549567, 52.0911337:4.8624848, "
      "52.0910665:4.8641176, 52.090999:4.8657502"));
  S2_LOG(INFO) << "\nInput: " << s2textformat::ToString(*poly);
  EXPECT_TRUE(poly->IsValid());
  S2Polygon poly_snapped;
  poly_snapped.set_s2debug_override(S2Debug::DISABLE);
  poly_snapped.InitToSnapped(poly.get());
  S2_LOG(INFO) << "\nSnapped: " << s2textformat::ToString(poly_snapped);
  S2Error error;
  EXPECT_FALSE(poly_snapped.FindValidationError(&error)) << error;
}

TEST(S2Polygon, MultipleInit) {
  unique_ptr<S2Polygon> polygon(s2textformat::MakePolygon("0:0, 0:2, 2:0"));
  EXPECT_EQ(1, polygon->num_loops());
  EXPECT_EQ(3, polygon->num_vertices());
  S2LatLngRect bound1 = polygon->GetRectBound();

  vector<unique_ptr<S2Loop>> loops;
  loops.push_back(s2textformat::MakeLoop("10:0, -10:-20, -10:20"));
  loops.push_back(s2textformat::MakeLoop("40:30, 20:10, 20:50"));
  polygon->InitNested(std::move(loops));
  EXPECT_TRUE(polygon->IsValid());
  EXPECT_EQ(2, polygon->num_loops());
  EXPECT_EQ(6, polygon->num_vertices());
  EXPECT_TRUE(bound1 != polygon->GetRectBound());
}

TEST(S2Polygon, InitSingleLoop) {
  S2Polygon polygon(make_unique<S2Loop>(S2Loop::kEmpty()));
  EXPECT_TRUE(polygon.is_empty());
  polygon.Init(make_unique<S2Loop>(S2Loop::kFull()));
  EXPECT_TRUE(polygon.is_full());
  polygon.Init(s2textformat::MakeLoop("0:0, 0:10, 10:0"));
  EXPECT_EQ(3, polygon.num_vertices());
}

TEST_F(S2PolygonTestBase, TestSimpleEncodeDecode) {
  Encoder encoder;
  cross1_->Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  S2Polygon decoded_polygon;
  ASSERT_TRUE(decoded_polygon.Decode(&decoder));
  EXPECT_TRUE(cross1_->BoundaryEquals(&decoded_polygon));
  EXPECT_EQ(cross1_->GetRectBound(), decoded_polygon.GetRectBound());
}

TEST(S2Polygon, TestEncodeDecodeDefaultPolygon) {
  S2Polygon polygon;
  EXPECT_TRUE(TestEncodeDecode(&polygon));
}

TEST(S2Polygon, CompressedEmptyPolygonRequires3Bytes) {
  S2Polygon empty_polygon;
  Encoder encoder;

  S2Polygon snapped_empty_polygon;
  snapped_empty_polygon.InitToSnapped(&empty_polygon);

  snapped_empty_polygon.Encode(&encoder);
  // 1 byte for version, 1 for the level, 1 for the length.
  EXPECT_EQ(1 + 1 + 1, encoder.length());

  EXPECT_TRUE(snapped_empty_polygon.is_empty());
  EXPECT_EQ(S2LatLngRect::Empty(), snapped_empty_polygon.GetRectBound());
}

TEST(S2Polygon, CompressedEncodedPolygonRequires69Bytes) {
  const unique_ptr<const S2Polygon> polygon(
      s2textformat::MakePolygon("0:0, 0:2, 2:0; 0:0, 0:-2, -2:-2, -2:0"));

  S2Polygon snapped_polygon;
  snapped_polygon.InitToSnapped(polygon.get());

  Encoder encoder;
  snapped_polygon.Encode(&encoder);

  // 2 loops, one with 3 vertices, one with 4.
  // Polygon:
  //   1 byte for version
  //   1 byte for level
  //   1 byte for num_loops
  // Loops:
  //   5 bytes overhead
  //   8 bytes per vertex
  EXPECT_EQ(1 + 1 + 1 + 2 * 5 + 7 * 8, encoder.length());
}

TEST_F(S2PolygonTestBase, CompressedEncodedPolygonDecodesApproxEqual) {
  // To compare the boundaries, etc we want to snap first.
  S2Polygon snapped;
  snapped.InitToSnapped(near_30_.get());
  ASSERT_EQ(2, snapped.num_loops());
  EXPECT_EQ(0, snapped.loop(0)->depth());
  EXPECT_EQ(1, snapped.loop(1)->depth());

  Encoder encoder;
  snapped.Encode(&encoder);

  Decoder decoder(encoder.base(), encoder.length());

  S2Polygon decoded_polygon;
  ASSERT_TRUE(decoded_polygon.Decode(&decoder));
  ASSERT_TRUE(decoded_polygon.IsValid());
  EXPECT_TRUE(snapped.BoundaryEquals(&decoded_polygon));
  EXPECT_EQ(snapped.GetRectBound(), decoded_polygon.GetRectBound());
  EXPECT_EQ(snapped.num_vertices(), decoded_polygon.num_vertices());
  EXPECT_EQ(2, decoded_polygon.num_loops());
  EXPECT_EQ(0, decoded_polygon.loop(0)->depth());
  EXPECT_EQ(1, decoded_polygon.loop(1)->depth());
}

// This test checks that S2Polygons created directly from S2Cells behave
// identically to S2Polygons created from the vertices of those cells; this
// previously was not the case, because S2Cells calculate their bounding
// rectangles slightly differently, and S2Polygons created from them just
// copied the S2Cell bounds.
TEST(S2Polygon, TestS2CellConstructorAndContains) {
  S2LatLng latlng(S1Angle::E6(40565459), S1Angle::E6(-74645276));
  S2Cell cell(latlng);
  S2Polygon cell_as_polygon(cell);
  S2Polygon empty;
  S2Polygon polygon_copy;
  polygon_copy.InitToUnion(&cell_as_polygon, &empty);
  EXPECT_TRUE(polygon_copy.Contains(&cell_as_polygon));
  EXPECT_TRUE(cell_as_polygon.Contains(&polygon_copy));
}

TEST(S2PolygonTest, Project) {
  unique_ptr<S2Polygon> polygon(MakePolygon(kNear0 + kNear2));
  S2Point point;
  S2Point projected;

  // The point inside the polygon should be projected into itself.
  point = s2textformat::MakePoint("1.1:0");
  projected = polygon->Project(point);
  EXPECT_TRUE(S2::ApproxEquals(point, projected));

  // The point is on the outside of the polygon.
  point = s2textformat::MakePoint("5.1:-2");
  projected = polygon->Project(point);
  EXPECT_TRUE(S2::ApproxEquals(s2textformat::MakePoint("5:-2"), projected));

  // The point is inside the hole in the polygon.
  point = s2textformat::MakePoint("-0.49:-0.49");
  projected = polygon->Project(point);
  EXPECT_TRUE(S2::ApproxEquals(s2textformat::MakePoint("-0.5:-0.5"),
                               projected, S1Angle::Radians(1e-6)));

  point = s2textformat::MakePoint("0:-3");
  projected = polygon->Project(point);
  EXPECT_TRUE(S2::ApproxEquals(s2textformat::MakePoint("0:-2"), projected));
}

// Helper function for testing the distance methods.  "boundary_x" is the
// expected result of projecting "x" onto the polygon boundary.  For
// convenience it can be set to S2Point() to indicate that (boundary_x == x).
static void TestDistanceMethods(const S2Polygon& polygon, const S2Point& x,
                                S2Point boundary_x) {
  // This error is not guaranteed by the implementation but is okay for tests.
  const S1Angle kMaxError = S1Angle::Radians(1e-15);

  if (boundary_x == S2Point()) boundary_x = x;
  EXPECT_LE(S1Angle(boundary_x, polygon.ProjectToBoundary(x)), kMaxError);

  if (polygon.is_empty() || polygon.is_full()) {
    EXPECT_EQ(S1Angle::Infinity(), polygon.GetDistanceToBoundary(x));
  } else {
    // EXPECT_NEAR only works with doubles.
    EXPECT_NEAR(S1Angle(x, boundary_x).degrees(),
                polygon.GetDistanceToBoundary(x).degrees(),
                kMaxError.degrees());
  }
  if (polygon.Contains(x)) {
    EXPECT_EQ(S1Angle::Zero(), polygon.GetDistance(x));
    EXPECT_EQ(x, polygon.Project(x));
  } else {
    EXPECT_EQ(polygon.GetDistanceToBoundary(x), polygon.GetDistance(x));
    EXPECT_EQ(polygon.ProjectToBoundary(x), polygon.Project(x));
  }
}

TEST_F(S2PolygonTestBase, GetDistance) {
  // The empty and full loops don't have boundaries.
  TestDistanceMethods(*empty_, S2Point(0, 1, 0), S2Point());
  TestDistanceMethods(*full_, S2Point(0, 1, 0), S2Point());

  // A polygon consisting of two nested rectangles centered around
  // S2LatLng(0,0).  Note that because lines of latitude are curved on the
  // sphere, it is not straightforward to project points onto any edge except
  // along the equator.  (The equator is the only line of latitude that is
  // also a geodesic.)
  unique_ptr<S2Polygon> nested(s2textformat::MakePolygon(
      "3:1, 3:-1, -3:-1, -3:1; 4:2, 4:-2, -4:-2, -4:2;"));

  // All points on the boundary of the polygon should be at distance zero.
  for (int i = 0; i < nested->num_loops(); i++) {
    const S2Loop* loop = nested->loop(i);
    for (int j = 0; j < loop->num_vertices(); j++) {
      // A vertex.
      TestDistanceMethods(*nested, loop->vertex(j), S2Point());
      // A point along an edge.
      TestDistanceMethods(*nested, S2::Interpolate(
          S2Testing::rnd.RandDouble(), loop->vertex(j), loop->vertex(j+1)),
                          S2Point());
    }
  }
  // A point outside the outer shell that projects to an edge.
  TestDistanceMethods(*nested, S2LatLng::FromDegrees(0, -4.7).ToPoint(),
                      S2LatLng::FromDegrees(0, -2).ToPoint());
  // A point outside the outer shell that projects to a vertex.
  TestDistanceMethods(*nested, S2LatLng::FromDegrees(6, -3).ToPoint(),
                      S2LatLng::FromDegrees(4, -2).ToPoint());
  // A point inside the polygon that projects to an outer edge.
  TestDistanceMethods(*nested, S2LatLng::FromDegrees(0, 1.7).ToPoint(),
                      S2LatLng::FromDegrees(0, 2).ToPoint());
  // A point inside the polygon that projects to an inner vertex.
  TestDistanceMethods(*nested, S2LatLng::FromDegrees(-3.3, -1.3).ToPoint(),
                      S2LatLng::FromDegrees(-3, -1).ToPoint());
  // A point inside the inner hole.
  TestDistanceMethods(*nested, S2LatLng::FromDegrees(0, 0.1).ToPoint(),
                      S2LatLng::FromDegrees(0, 1).ToPoint());
}

TEST_F(S2PolygonTestBase, Area) {
  EXPECT_DOUBLE_EQ(0.0, empty_->GetArea());
  EXPECT_DOUBLE_EQ(4 * M_PI, full_->GetArea());
  EXPECT_DOUBLE_EQ(2 * M_PI, south_H_->GetArea());
  EXPECT_DOUBLE_EQ(M_PI, far_H_south_H_->GetArea());

  unique_ptr<S2Polygon> two_shells(
      MakePolygon(kCross1SideHole + kCrossCenterHole));
  EXPECT_DOUBLE_EQ(
      two_shells->loop(0)->GetArea() + two_shells->loop(1)->GetArea(),
      two_shells->GetArea());

  unique_ptr<S2Polygon> holey_shell(MakePolygon(kCross1 + kCrossCenterHole));
  EXPECT_DOUBLE_EQ(
      holey_shell->loop(0)->GetArea() - holey_shell->loop(1)->GetArea(),
      holey_shell->GetArea());
}

TEST(S2Polygon, UninitializedIsValid) {
  S2Polygon polygon;
  EXPECT_TRUE(polygon.IsValid());
}

class IsValidTest : public testing::Test {
 public:
  IsValidTest() {
    init_oriented_ = false;
    modify_polygon_hook_ = nullptr;
    rnd_ = &S2Testing::rnd;
    rnd_->Reset(FLAGS_s2_random_seed);
  }

  ~IsValidTest() override { Reset(); }

  vector<S2Point>* AddLoop() {
    vloops_.push_back(make_unique<vector<S2Point>>());
    return vloops_.back().get();
  }

  // Create "num_loops" nested regular loops around a common center point.
  // All loops have the same number of vertices (at least "min_vertices").
  // Furthermore, the vertices at the same index position are collinear with
  // the common center point of all the loops.  The loop radii decrease
  // exponentially in order to prevent accidental loop crossings when one of
  // the loops is modified.
  void AddConcentricLoops(int num_loops, int min_vertices) {
    S2_DCHECK_LE(num_loops, 10);  // Because radii decrease exponentially.
    S2Point center = S2Testing::RandomPoint();
    int num_vertices = min_vertices + rnd_->Uniform(10);
    for (int i = 0; i < num_loops; ++i) {
      S1Angle radius = S1Angle::Degrees(80 * pow(0.1, i));
      *AddLoop() = S2Testing::MakeRegularPoints(center, radius, num_vertices);
    }
  }

  void Reset() {
    vloops_.clear();
  }

  void CheckInvalid(const string& snippet) {
    vector<unique_ptr<S2Loop>> loops;
    for (const auto& vloop : vloops_) {
      loops.push_back(make_unique<S2Loop>(*vloop, S2Debug::DISABLE));
    }
    // Cannot replace with std::shuffle (b/65670707) since this uses an
    // incompatible random source which is also used as a source of randomness
    // in the surrounding code.
    // NOLINTNEXTLINE
    gtl::legacy_random_shuffle(loops.begin(), loops.end(), *rnd_);
    S2Polygon polygon;
    polygon.set_s2debug_override(S2Debug::DISABLE);
    if (init_oriented_) {
      polygon.InitOriented(std::move(loops));
    } else {
      polygon.InitNested(std::move(loops));
    }
    if (modify_polygon_hook_) (*modify_polygon_hook_)(&polygon);
    S2Error error;
    EXPECT_TRUE(polygon.FindValidationError(&error));
    EXPECT_TRUE(error.text().find(snippet) != string::npos)
        << "\nActual error: " << error << "\nExpected substring: " << snippet;
    Reset();
  }

 protected:
  static const int kIters = 100;

  bool init_oriented_;
  void (*modify_polygon_hook_)(S2Polygon*);
  S2Testing::Random* rnd_;
  vector<unique_ptr<vector<S2Point>>> vloops_;
};

TEST_F(IsValidTest, UnitLength) {
  // This test can only be run in optimized builds because there are
  // S2_DCHECK(IsUnitLength()) calls scattered throughout the S2 code.
  if (google::DEBUG_MODE) return;
  for (int iter = 0; iter < kIters; ++iter) {
    AddConcentricLoops(1 + rnd_->Uniform(6), 3 /*min_vertices*/);
    vector<S2Point>* vloop = vloops_[rnd_->Uniform(vloops_.size())].get();
    S2Point* p = &(*vloop)[rnd_->Uniform(vloop->size())];
    switch (rnd_->Uniform(3)) {
      case 0: *p = S2Point(0, 0, 0); break;
      case 1: *p *= 1e-30 * pow(1e60, rnd_->RandDouble()); break;
      case 2: *p = numeric_limits<double>::quiet_NaN() * S2Point(); break;
    }
    CheckInvalid("unit length");
  }
}

TEST_F(IsValidTest, VertexCount) {
  for (int iter = 0; iter < kIters; ++iter) {
    vector<S2Point>* vloop = AddLoop();
    if (rnd_->OneIn(2)) {
      vloop->push_back(S2Testing::RandomPoint());
      vloop->push_back(S2Testing::RandomPoint());
    }
    CheckInvalid("at least 3 vertices");
  }
}

TEST_F(IsValidTest, DuplicateVertex) {
  for (int iter = 0; iter < kIters; ++iter) {
    AddConcentricLoops(1, 3 /*min_vertices*/);
    vector<S2Point>* vloop = vloops_[0].get();
    int n = vloop->size();
    int i = rnd_->Uniform(n);
    int j = rnd_->Uniform(n - 1);
    (*vloop)[i] = (*vloop)[j + (j >= i)];
    CheckInvalid("duplicate vertex");
  }
}

TEST_F(IsValidTest, SelfIntersection) {
  for (int iter = 0; iter < kIters; ++iter) {
    // Use multiple loops so that we can test both holes and shells.  We need
    // at least 5 vertices so that the modified edges don't intersect any
    // nested loops.
    AddConcentricLoops(1 + rnd_->Uniform(6), 5 /*min_vertices*/);
    vector<S2Point>* vloop = vloops_[rnd_->Uniform(vloops_.size())].get();
    int n = vloop->size();
    int i = rnd_->Uniform(n);
    swap((*vloop)[i], (*vloop)[(i+1) % n]);
    CheckInvalid("crosses edge");
  }
}

TEST_F(IsValidTest, EmptyLoop) {
  for (int iter = 0; iter < kIters; ++iter) {
    AddConcentricLoops(rnd_->Uniform(5), 3 /*min_vertices*/);
    *AddLoop() = S2Loop::kEmpty();
    CheckInvalid("empty loop");
  }
}

TEST_F(IsValidTest, FullLoop) {
  for (int iter = 0; iter < kIters; ++iter) {
    // This is only an error if there is at least one other loop.
    AddConcentricLoops(1 + rnd_->Uniform(5), 3 /*min_vertices*/);
    *AddLoop() = S2Loop::kFull();
    CheckInvalid("full loop");
  }
}

TEST_F(IsValidTest, LoopsCrossing) {
  for (int iter = 0; iter < kIters; ++iter) {
    AddConcentricLoops(2, 4 /*min_vertices*/);
    // Both loops have the same number of vertices, and vertices at the same
    // index position are collinear with the center point, so we can create a
    // crossing by simply exchanging two vertices at the same index position.
    int n = vloops_[0]->size();
    int i = rnd_->Uniform(n);
    swap((*vloops_[0])[i], (*vloops_[1])[i]);
    if (rnd_->OneIn(2)) {
      // By copy the two adjacent vertices from one loop to the other, we can
      // ensure that the crossings happen at vertices rather than edges.
      (*vloops_[0])[(i+1) % n] = (*vloops_[1])[(i+1) % n];
      (*vloops_[0])[(i+n-1) % n] = (*vloops_[1])[(i+n-1) % n];
    }
    CheckInvalid("crosses loop");
  }
}

TEST_F(IsValidTest, DuplicateEdge) {
  for (int iter = 0; iter < kIters; ++iter) {
    AddConcentricLoops(2, 4 /*min_vertices*/);
    int n = vloops_[0]->size();
    if (rnd_->OneIn(2)) {
      // Create a shared edge (same direction in both loops).
      int i = rnd_->Uniform(n);
      (*vloops_[0])[i] = (*vloops_[1])[i];
      (*vloops_[0])[(i+1) % n] = (*vloops_[1])[(i+1) % n];
    } else {
      // Create a reversed edge (opposite direction in each loop) by cutting
      // loop 0 into two halves along one of its diagonals and replacing both
      // loops with the result.
      int split = 2 + rnd_->Uniform(n - 3);
      vloops_[1]->clear();
      vloops_[1]->push_back((*vloops_[0])[0]);
      for (int i = split; i < n; ++i) {
        vloops_[1]->push_back((*vloops_[0])[i]);
      }
      vloops_[0]->resize(split + 1);
    }
    CheckInvalid("has duplicate");
  }
}

TEST_F(IsValidTest, InconsistentOrientations) {
  for (int iter = 0; iter < kIters; ++iter) {
    AddConcentricLoops(2 + rnd_->Uniform(5), 3 /*min_vertices*/);
    init_oriented_ = true;
    CheckInvalid("Inconsistent loop orientations");
  }
}

static void SetInvalidLoopDepth(S2Polygon* polygon) {
  int i = S2Testing::rnd.Uniform(polygon->num_loops());
  if (i == 0 || S2Testing::rnd.OneIn(3)) {
    polygon->loop(i)->set_depth(-1);
  } else {
    polygon->loop(i)->set_depth(polygon->loop(i-1)->depth() + 2);
  }
}

TEST_F(IsValidTest, LoopDepthNegative) {
  modify_polygon_hook_ = SetInvalidLoopDepth;
  for (int iter = 0; iter < kIters; ++iter) {
    AddConcentricLoops(1 + rnd_->Uniform(4), 3 /*min_vertices*/);
    CheckInvalid("invalid loop depth");
  }
}

static void SetInvalidLoopNesting(S2Polygon* polygon) {
  int i = S2Testing::rnd.Uniform(polygon->num_loops());
  polygon->loop(i)->Invert();
}

TEST_F(IsValidTest, LoopNestingInvalid) {
  modify_polygon_hook_ = SetInvalidLoopNesting;
  for (int iter = 0; iter < kIters; ++iter) {
    AddConcentricLoops(2 + rnd_->Uniform(4), 3 /*min_vertices*/);
    // Randomly invert all the loops in order to generate cases where the
    // outer loop encompasses almost the entire sphere.  This tests different
    // code paths because bounding box checks are not as useful.
    if (rnd_->OneIn(2)) {
      for (const auto& loop : vloops_) {
        std::reverse(loop->begin(), loop->end());
      }
    }
    CheckInvalid("Invalid nesting");
  }
}

TEST_F(IsValidTest, FuzzTest) {
  // Check that the S2Loop/S2Polygon constructors and IsValid() don't crash
  // when they receive arbitrary invalid input.  (We don't test large inputs;
  // it is assumed that the client enforces their own size limits before even
  // attempting to construct geometric objects.)
  if (google::DEBUG_MODE)
    return;  // Requires unit length vertices.
  for (int iter = 0; iter < kIters; ++iter) {
    int num_loops = 1 + rnd_->Uniform(10);
    for (int i = 0; i < num_loops; ++i) {
      int num_vertices = rnd_->Uniform(10);
      vector<S2Point>* vloop = AddLoop();
      while (vloop->size() < num_vertices) {
        // Since the number of vertices is random, we automatically test empty
        // loops, full loops, and invalid vertex counts.  Also since most
        // vertices are random, we automatically get self-intersections and
        // loop crossings.  That leaves zero and NaN vertices, duplicate
        // vertices, and duplicate edges to be created explicitly.
        if (rnd_->OneIn(10)) {
          // Zero vertex.
          vloop->push_back(S2Point(0, 0, 0));
        } else if (rnd_->OneIn(10)) {
          // NaN vertex.
          vloop->push_back(numeric_limits<double>::quiet_NaN() * S2Point());
        } else if (rnd_->OneIn(10) && !vloop->empty()) {
          // Duplicate vertex.
          vloop->push_back((*vloop)[rnd_->Uniform(vloop->size())]);
        } else if (rnd_->OneIn(10) && vloop->size() + 2 <= num_vertices) {
          // Try to copy an edge from a random loop.
          vector<S2Point>* other = vloops_[rnd_->Uniform(vloops_.size())].get();
          int n = other->size();
          if (n >= 2) {
            int k0 = rnd_->Uniform(n);
            int k1 = (k0 + 1) % n;
            if (rnd_->OneIn(2)) swap(k0, k1);  // Copy reversed edge.
            vloop->push_back((*other)[k0]);
            vloop->push_back((*other)[k1]);
          }
        } else {
          // Random non-unit-length point.
          S2Point p = S2Testing::RandomPoint();
          vloop->push_back(1e-30 * pow(1e60, rnd_->RandDouble()) * p);
        }
      }
    }
    CheckInvalid("");  // We could get any error message.
  }
}

// Returns the diameter of a loop (maximum distance between any two
// points in the loop).
S1Angle LoopDiameter(const S2Loop& loop) {
  S1Angle diameter;
  for (int i = 0; i < loop.num_vertices(); ++i) {
    S2Point test_point = loop.vertex(i);
    for (int j = i + 1; j < loop.num_vertices(); ++j) {
      diameter = max(diameter,
                     S2::GetDistance(test_point, loop.vertex(j),
                                             loop.vertex(j+1)));
    }
  }
  return diameter;
}

// Returns the maximum distance from any vertex of poly_a to poly_b, that is,
// the directed Haussdorf distance of the set of vertices of poly_a to the
// boundary of poly_b.
//
// Doesn't consider loops from poly_a that have diameter less than min_diameter
// in degrees.
double MaximumDistanceInDegrees(const S2Polygon& poly_a,
                                const S2Polygon& poly_b,
                                double min_diameter_in_degrees) {
  double min_distance = 360;
  bool has_big_loops = false;
  for (int l = 0; l < poly_a.num_loops(); ++l) {
    const S2Loop* a_loop = poly_a.loop(l);
    if (LoopDiameter(*a_loop).degrees() <= min_diameter_in_degrees) {
      continue;
    }
    has_big_loops = true;
    for (int v = 0; v < a_loop->num_vertices(); ++v) {
      double distance = poly_b.GetDistance(a_loop->vertex(v)).degrees();
      if (distance < min_distance) {
        min_distance = distance;
      }
    }
  }
  if (has_big_loops) {
    return min_distance;
  } else {
    return 0.;  // As if the first polygon were empty.
  }
}

class S2PolygonSimplifierTest : public ::testing::Test {
 protected:
  void SetInput(unique_ptr<S2Polygon> poly, double tolerance_in_degrees) {
    original = std::move(poly);

    simplified = make_unique<S2Polygon>();
    simplified->InitToSimplified(*original,
                                 s2builderutil::IdentitySnapFunction(
                                     S1Angle::Degrees(tolerance_in_degrees)));
  }

  void SetInput(const string& poly, double tolerance_in_degrees) {
    SetInput(s2textformat::MakePolygon(poly), tolerance_in_degrees);
  }

  unique_ptr<S2Polygon> simplified;
  unique_ptr<S2Polygon> original;
};

TEST_F(S2PolygonSimplifierTest, NoSimplification) {
  SetInput("0:0, 0:20, 20:20, 20:0", 1.0);
  EXPECT_EQ(4, simplified->num_vertices());

  EXPECT_EQ(0, MaximumDistanceInDegrees(*simplified, *original, 0));
  EXPECT_EQ(0, MaximumDistanceInDegrees(*original, *simplified, 0));
}

// Here, 10:-2 will be removed and  0:0-20:0 will intersect two edges.
// (The resulting polygon will in fact probably have more edges.)
TEST_F(S2PolygonSimplifierTest, SimplifiedLoopSelfIntersects) {
  SetInput("0:0, 0:20, 10:-0.1, 20:20, 20:0, 10:-0.2", 0.22);

  // The simplified polygon has the same number of vertices but it should now
  // consists of two loops rather than one.
  EXPECT_EQ(2, simplified->num_loops());
  EXPECT_GE(0.22, MaximumDistanceInDegrees(*simplified, *original, 0));
  EXPECT_GE(0.22, MaximumDistanceInDegrees(*original, *simplified, 0.22));
}

TEST_F(S2PolygonSimplifierTest, NoSimplificationManyLoops) {
  SetInput("0:0,    0:1,   1:0;   0:20, 0:21, 1:20; "
           "20:20, 20:21, 21:20; 20:0, 20:1, 21:0", 0.01);
  EXPECT_EQ(0, MaximumDistanceInDegrees(*simplified, *original, 0));
  EXPECT_EQ(0, MaximumDistanceInDegrees(*original, *simplified, 0));
}

TEST_F(S2PolygonSimplifierTest, TinyLoopDisappears) {
  SetInput("0:0, 0:1, 1:1, 1:0", 1.1);
  EXPECT_TRUE(simplified->is_empty());
}

TEST_F(S2PolygonSimplifierTest, StraightLinesAreSimplified) {
  SetInput("0:0, 1:0, 2:0, 3:0, 4:0, 5:0, 6:0,"
           "6:1, 5:1, 4:1, 3:1, 2:1, 1:1, 0:1", 0.01);
  EXPECT_EQ(4, simplified->num_vertices());
}

TEST_F(S2PolygonSimplifierTest, EdgeSplitInManyPieces) {
  // near_square's right four-point side will be simplified to a vertical
  // line at lng=7.9, that will cut the 9 teeth of the saw (the edge will
  // therefore be broken into 19 pieces).
  const string saw =
      "1:1, 1:8, 2:2, 2:8, 3:2, 3:8, 4:2, 4:8, 5:2, 5:8,"
      "6:2, 6:8, 7:2, 7:8, 8:2, 8:8, 9:2, 9:8, 10:1";
  const string near_square =
      "0:0, 0:7.9, 1:8.1, 10:8.1, 11:7.9, 11:0";
  SetInput(saw + ";" + near_square, 0.21);

  EXPECT_TRUE(simplified->IsValid());
  EXPECT_GE(0.11, MaximumDistanceInDegrees(*simplified, *original, 0));
  EXPECT_GE(0.11, MaximumDistanceInDegrees(*original, *simplified, 0));
  // The resulting polygon's 9 little teeth are very small and disappear
  // due to the vertex_merge_radius of the polygon builder.  There remains
  // nine loops.
  EXPECT_EQ(9, simplified->num_loops());
}

TEST_F(S2PolygonSimplifierTest, EdgesOverlap) {
  // Two loops, One edge of the second one ([0:1 - 0:2]) is part of an
  // edge of the first one..
  SetInput("0:0, 0:3, 1:0; 0:1, -1:1, 0:2", 0.01);
  unique_ptr<S2Polygon> true_poly(
      s2textformat::MakePolygon("0:3, 1:0, 0:0, 0:1, -1:1, 0:2"));
  EXPECT_TRUE(simplified->BoundaryApproxEquals(*true_poly,
                                               S1Angle::Radians(1e-15)));
}

// Creates a polygon from loops specified as a comma separated list of u:v
// coordinates relative to a cell. The loop "0:0, 1:0, 1:1, 0:1" is
// counter-clockwise.
unique_ptr<S2Polygon> MakeCellPolygon(
    const S2Cell& cell, const vector<const char *>& strs) {
  vector<unique_ptr<S2Loop>> loops;
  for (auto str : strs) {
    vector<S2LatLng> points = s2textformat::ParseLatLngs(str);
    vector<S2Point> loop_vertices;
    R2Rect uv = cell.GetBoundUV();
    for (const S2LatLng& p : points) {
      double u = p.lat().degrees(), v = p.lng().degrees();
      loop_vertices.push_back(
          S2::FaceUVtoXYZ(cell.face(),
                          uv[0][0] * (1 - u) + uv[0][1] * u,
                          uv[1][0] * (1 - v) + uv[1][1] * v).Normalize());
    }
    loops.emplace_back(new S2Loop(loop_vertices));
  }
  return make_unique<S2Polygon>(std::move(loops));
}

TEST(InitToSimplifiedInCell, PointsOnCellBoundaryKept) {
  S2Cell cell(S2CellId::FromToken("89c25c"));
  auto polygon = MakeCellPolygon(cell, {"0.1:0, 0.2:0, 0.2:0.5"});
  S1Angle tolerance =
      S1Angle(polygon->loop(0)->vertex(0), polygon->loop(0)->vertex(1)) * 1.1;
  S2Polygon simplified;
  simplified.InitToSimplified(*polygon,
                              s2builderutil::IdentitySnapFunction(tolerance));
  EXPECT_TRUE(simplified.is_empty());
  S2Polygon simplified_in_cell;
  simplified_in_cell.InitToSimplifiedInCell(polygon.get(), cell, tolerance);
  EXPECT_TRUE(simplified_in_cell.BoundaryEquals(polygon.get()));
  EXPECT_EQ(3, simplified_in_cell.num_vertices());
  EXPECT_EQ(-1, simplified.GetSnapLevel());
}

TEST(InitToSimplifiedInCell, PointsInsideCellSimplified) {
  S2CellId cell_id = S2CellId::FromToken("89c25c");
  S2Cell cell(cell_id);
  auto polygon = MakeCellPolygon(
      cell, {"0.3:0, 0.4:0, 0.4:0.5, 0.4:0.8, 0.2:0.8"});
  S1Angle tolerance =
      S1Angle(polygon->loop(0)->vertex(0), polygon->loop(0)->vertex(1)) * 1.1;
  S2Polygon simplified;
  simplified.InitToSimplifiedInCell(polygon.get(), cell, tolerance);
  EXPECT_TRUE(simplified.BoundaryNear(*polygon, S1Angle::Radians(1e-15)));
  EXPECT_EQ(4, simplified.num_vertices());
  EXPECT_EQ(-1, simplified.GetSnapLevel());
}

TEST(InitToSimplifiedInCell, CellCornerKept) {
  S2Cell cell(S2CellId::FromToken("00001"));
  auto input = MakeCellPolygon(cell, {"1:0, 1:0.05, 0.99:0"});
  S1Angle tolerance = 0.02 * S1Angle(cell.GetVertex(0), cell.GetVertex(1));
  S2Polygon simplified;
  simplified.InitToSimplifiedInCell(input.get(), cell, tolerance);
  EXPECT_TRUE(simplified.BoundaryNear(*input, S1Angle::Radians(1e-15)));
}

TEST(InitToSimplifiedInCell, NarrowStripRemoved) {
  S2Cell cell(S2CellId::FromToken("00001"));
  auto input = MakeCellPolygon(cell, {"0.9:0, 0.91:0, 0.91:1, 0.9:1"});
  S1Angle tolerance = 0.02 * S1Angle(cell.GetVertex(0), cell.GetVertex(1));
  S2Polygon simplified;
  simplified.InitToSimplifiedInCell(input.get(), cell, tolerance);
  EXPECT_TRUE(simplified.is_empty());
}

TEST(InitToSimplifiedInCell, NarrowGapRemoved) {
  S2Cell cell(S2CellId::FromToken("00001"));
  auto input = MakeCellPolygon(
      cell, {"0.7:0, 0.75:0, 0.75:1, 0.7:1", "0.76:0, 0.8:0, 0.8:1, 0.76:1"});
  auto expected = MakeCellPolygon(cell, {"0.7:0, 0.8:0, 0.8:1, 0.7:1"});
  S1Angle tolerance = 0.02 * S1Angle(cell.GetVertex(0), cell.GetVertex(1));
  S2Polygon simplified;
  simplified.InitToSimplifiedInCell(input.get(), cell, tolerance);
  EXPECT_TRUE(simplified.BoundaryNear(*expected, S1Angle::Radians(1e-15)));
}

TEST(InitToSimplifiedInCell, CloselySpacedEdgeVerticesKept) {
  S2Cell cell(S2CellId::FromToken("00001"));
  auto input = MakeCellPolygon(
      cell, {"0:0.303, 0:0.302, 0:0.301, 0:0.3, 0.1:0.3, 0.1:0.4"});
  S1Angle tolerance = 0.02 * S1Angle(cell.GetVertex(0), cell.GetVertex(1));
  S2Polygon simplified;
  simplified.InitToSimplifiedInCell(input.get(), cell, tolerance);
  EXPECT_TRUE(simplified.BoundaryApproxEquals(*input, S1Angle::Radians(1e-15)));
}

TEST(InitToSimplifiedInCell, PolylineAssemblyBug) {
  S2Cell cell(S2CellId::FromToken("5701"));
  auto polygon = MakePolygon(
      "55.8699252:-163.9412145, "  // South-west corner of 5701
      "54.7672352:-166.7579678, "  // North-east corner of 5701
      /* Offending part: a tiny triangle near south-east corner */
      "54.7109214:-164.6376338, "  // forced vertex, on edge 4
      "54.7140193:-164.6398404, "
      "54.7113202:-164.6374015");  // forced vertex, on edge 4
  S1Angle tolerance = S1Angle::Radians(2.138358e-05);  // 136.235m
  S1Angle max_dist = S1Angle::Radians(2.821947e-09);  // 18mm
  S2Polygon simplified_in_cell;
  simplified_in_cell.InitToSimplifiedInCell(polygon.get(), cell, tolerance,
                                            max_dist);
  EXPECT_FALSE(simplified_in_cell.is_empty());
}

TEST(InitToSimplifiedInCell, InteriorEdgesSnappedToBoundary) {
  auto polygon = s2textformat::MakePolygonOrDie(
      "37.8011672:-122.3247322, 37.8011648:-122.3247399, "
      "37.8011647:-122.3247403, 37.8011646:-122.3247408, "
      "37.8011645:-122.3247411, 37.8011633:-122.3247449, "
      "37.8011621:-122.3247334");
  S2Cell cell(S2CellId::FromDebugString("4/001013300"));
  S1Angle snap_radius = S2Testing::MetersToAngle(1.0);
  S1Angle boundary_tolerance =
      S1Angle::Radians(0.5 * S2::kMaxWidth.GetValue(S2CellId::kMaxLevel - 1)) +
      s2builderutil::IntLatLngSnapFunction::MinSnapRadiusForExponent(7);
  S2Polygon simplified_polygon;
  simplified_polygon.set_s2debug_override(S2Debug::DISABLE);
  simplified_polygon.InitToSimplifiedInCell(polygon.get(), cell, snap_radius,
                                            boundary_tolerance);
  S2Error error;
  EXPECT_FALSE(simplified_polygon.FindValidationError(&error)) << error;
}


unique_ptr<S2Polygon> MakeRegularPolygon(
    const string& center, int num_points, double radius_in_degrees) {
  S1Angle radius = S1Angle::Degrees(radius_in_degrees);
  return make_unique<S2Polygon>(S2Loop::MakeRegularLoop(
      s2textformat::MakePoint(center), radius, num_points));
}

// Tests that a regular polygon with many points gets simplified
// enough.
TEST_F(S2PolygonSimplifierTest, LargeRegularPolygon) {
  const double kRadius = 2.;  // in degrees
  const int num_initial_points = 1000;
  const int num_desired_points = 250;
  double tolerance = 1.05 * kRadius * (1 - cos(M_PI / num_desired_points));

  SetInput(MakeRegularPolygon("0:0", num_initial_points, kRadius), tolerance);

  EXPECT_GE(tolerance, MaximumDistanceInDegrees(*simplified, *original, 0));
  EXPECT_GE(tolerance, MaximumDistanceInDegrees(*original, *simplified, 0));
  EXPECT_GE(250, simplified->num_vertices());
  EXPECT_LE(200, simplified->num_vertices());
}

class S2PolygonDecodeTest : public ::testing::Test {
 protected:
  S2PolygonDecodeTest() : data_array_(kMaxBytes) {
    encoder_.reset(data_array_.data(), kMaxBytes);
  }

  ~S2PolygonDecodeTest() override {}

  void AppendByte(int value) {
    encoder_.put8(value);
  }

  void AppendInt32(int value) {
    encoder_.put32(value);
  }

  void AppendRandomData(int size) {
    for (int i = 0; i < size && encoder_.avail() > 0; ++i) {
      AppendByte(random_.Uniform(256));
    }
  }

  void AppendRandomData() {
    AppendRandomData(random_.Uniform(kMaxBytes));
  }

  void AppendFakeUncompressedEncodingData() {
    AppendByte(1);                      // polygon number
    AppendByte(0);                      // unused
    AppendByte(0);                      // "has holes" flag
    AppendInt32(PickRandomCount());     // num loops
    AppendByte(1);                      // loop version
    AppendInt32(PickRandomCount());     // num vertices
    AppendRandomData();                 // junk to fill out the buffer
  }

  void AppendFakeCompressedEncodingData() {
    AppendByte(4);                      // polygon number
    AppendByte(random_.Uniform(50));    // snap level
    AppendInt32(PickRandomCount());     // num loops
    AppendInt32(PickRandomCount());     // num vertices
    AppendRandomData();                 // junk to fill out the buffer
  }

  int32 PickRandomCount() {
    if (random_.OneIn(10)) {
      return -1;
    }
    if (random_.OneIn(10)) {
      return 0;
    }
    if (random_.OneIn(10)) {
      return 1000000000;
    }
    if (random_.OneIn(2)) {
      return random_.Uniform(1000000000);
    }
    return random_.Uniform(1000);
  }

  bool Test() {
    decoder_.reset(data_array_.data(), encoder_.length());
    encoder_.clear();
    S2Polygon polygon;
    polygon.set_s2debug_override(S2Debug::DISABLE);
    return polygon.Decode(&decoder_);
  }

  // Random number generator.
  S2Testing::Random random_;

  // Maximum size of the data array.
  const int kMaxBytes = 256;

  // The data array.
  absl::FixedArray<int8> data_array_;

  // Encoder that is used to put data into the array.
  Encoder encoder_;

  // Decoder used to extract data from the array.
  Decoder decoder_;
};

TEST_F(S2PolygonDecodeTest, FuzzUncompressedEncoding) {
  // Some parts of the S2 library S2_DCHECK on invalid data, even if we set
  // FLAGS_s2debug to false or use S2Polygon::set_s2debug_override. So we
  // only run this test in opt mode.
#ifdef NDEBUG
  for (int i = 0; i < 100000; ++i) {
    AppendFakeUncompressedEncodingData();
    Test();
  }
#endif
}

TEST_F(S2PolygonDecodeTest, FuzzCompressedEncoding) {
  // Some parts of the S2 library S2_DCHECK on invalid data, even if we set
  // FLAGS_s2debug to false or use S2Polygon::set_s2debug_override. So we
  // only run this test in opt mode.
#ifdef NDEBUG
  for (int i = 0; i < 100000; ++i) {
    AppendFakeCompressedEncodingData();
    Test();
  }
#endif
}

TEST_F(S2PolygonDecodeTest, FuzzEverything) {
  // Some parts of the S2 library S2_DCHECK on invalid data, even if we set
  // FLAGS_s2debug to false or use S2Polygon::set_s2debug_override. So we
  // only run this test in opt mode.
#ifdef NDEBUG
  for (int i = 0; i < 100000; ++i) {
    AppendRandomData();
    Test();
  }
#endif
}

TEST_F(S2PolygonTestBase, FullPolygonShape) {
  S2Polygon::Shape shape(full_.get());
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(2, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_TRUE(shape.is_full());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_EQ(0, shape.chain(0).start);
  EXPECT_EQ(0, shape.chain(0).length);
  EXPECT_TRUE(shape.GetReferencePoint().contained);
}

TEST_F(S2PolygonTestBase, EmptyPolygonShape) {
  S2Polygon::Shape shape(empty_.get());
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(2, shape.dimension());
  EXPECT_TRUE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_EQ(0, shape.num_chains());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

void TestPolygonShape(const S2Polygon& polygon) {
  S2_DCHECK(!polygon.is_full());
  S2Polygon::Shape shape(&polygon);
  EXPECT_EQ(&polygon, shape.polygon());
  EXPECT_EQ(polygon.num_vertices(), shape.num_edges());
  EXPECT_EQ(polygon.num_loops(), shape.num_chains());
  for (int e = 0, i = 0; i < polygon.num_loops(); ++i) {
    const S2Loop* loop_i = polygon.loop(i);
    EXPECT_EQ(e, shape.chain(i).start);
    EXPECT_EQ(loop_i->num_vertices(), shape.chain(i).length);
    for (int j = 0; j < loop_i->num_vertices(); ++j, ++e) {
      auto edge = shape.edge(e);
      EXPECT_EQ(loop_i->oriented_vertex(j), edge.v0);
      EXPECT_EQ(loop_i->oriented_vertex(j+1), edge.v1);
    }
  }
  EXPECT_EQ(2, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_EQ(polygon.Contains(S2::Origin()),
            shape.GetReferencePoint().contained);
}

TEST_F(S2PolygonTestBase, OneLoopPolygonShape) {
  TestPolygonShape(*near_0_);
}

TEST_F(S2PolygonTestBase, SeveralLoopPolygonShape) {
  TestPolygonShape(*near_3210_);
}

TEST(S2Polygon, ManyLoopPolygonShape) {
  const int kNumLoops = 100;
  const int kNumVerticesPerLoop = 6;
  S2Polygon polygon;
  S2Testing::ConcentricLoopsPolygon(S2Point(1, 0, 0), kNumLoops,
                                    kNumVerticesPerLoop, &polygon);
  TestPolygonShape(polygon);
}

TEST(S2PolygonOwningShape, Ownership) {
  // Debug mode builds will catch any memory leak below.
  vector<unique_ptr<S2Loop>> loops;
  auto polygon = make_unique<S2Polygon>(std::move(loops));
  S2Polygon::OwningShape shape(std::move(polygon));
}

TEST(S2Polygon, PointInBigLoop) {
  // This code used to demonstrate a bug in S2ShapeIndex.
  S2LatLng center = S2LatLng::FromRadians(0.3, 2);
  S1Angle radius = S1Angle::Degrees(80);
  S2Polygon poly(S2Loop::MakeRegularLoop(center.ToPoint(), radius, 10));
  EXPECT_TRUE(poly.MayIntersect(S2Cell(S2CellId(center))));
}

TEST(S2Polygon, Sizes) {
  // This isn't really a test.  It just prints the sizes of various classes.
  S2_LOG(INFO) << "sizeof(S2Loop): " << sizeof(S2Loop);
  S2_LOG(INFO) << "sizeof(S2Polygon): " << sizeof(S2Polygon);
  S2_LOG(INFO) << "sizeof(S2Polyline): " << sizeof(S2Polyline);
  S2_LOG(INFO) << "sizeof(MutableS2ShapeIndex): " << sizeof(MutableS2ShapeIndex);
  S2_LOG(INFO) << "sizeof(S2Polygon::Shape): " << sizeof(S2Polygon::Shape);
  S2_LOG(INFO) << "sizeof(S2Cell): " << sizeof(S2Cell);
  S2_LOG(INFO) << "sizeof(S2PaddedCell): " << sizeof(S2PaddedCell);
}

TEST_F(S2PolygonTestBase, IndexContainsOnePolygonShape) {
  const MutableS2ShapeIndex& index = near_0_->index();
  ASSERT_EQ(1, index.num_shape_ids());
  S2Polygon::Shape* shape = down_cast<S2Polygon::Shape*>(index.shape(0));
  EXPECT_EQ(near_0_.get(), shape->polygon());
}

TEST_F(S2PolygonTestBase, PolygonPolygonDistance) {
  // Verify that the example code for S2Polygon::index() actually works.
  const S2Polygon& polygon1 = *near_0_;
  const S2Polygon& polygon2 = *far_10_;
  S2ClosestEdgeQuery query(&polygon1.index());
  S2ClosestEdgeQuery::ShapeIndexTarget target(&polygon2.index());
  S1ChordAngle distance = query.GetDistance(&target);
  EXPECT_GT(distance, S1ChordAngle(S1Angle::Degrees(175)));
}

