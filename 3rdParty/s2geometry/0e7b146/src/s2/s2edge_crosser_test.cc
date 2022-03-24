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

#include "s2/s2edge_crosser.h"

#include <limits>
#include <vector>

#include "s2/base/logging.h"
#include <gtest/gtest.h>
#include "s2/s2edge_crossings.h"
#include "s2/s2edge_distances.h"
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"
#include "s2/s2testing.h"

using std::vector;

#ifdef NDEBUG

// In non-debug builds, check that default-constructed and/or NaN S2Point
// arguments don't cause crashes, especially on the very first method call
// (since S2CopyingEdgeCrosser checks whether the first vertex of each edge is
// the same as the last vertex of the previous edge when deciding whether or
// not to call Restart).

void TestCrossingSignInvalid(const S2Point& point, int expected) {
  S2EdgeCrosser crosser(&point, &point);
  EXPECT_EQ(expected, crosser.CrossingSign(&point, &point));
  S2CopyingEdgeCrosser crosser2(point, point);
  EXPECT_EQ(expected, crosser2.CrossingSign(point, point));
}

void TestEdgeOrVertexCrossingInvalid(const S2Point& point) {
  S2EdgeCrosser crosser(&point, &point);
  EXPECT_EQ(false, crosser.EdgeOrVertexCrossing(&point, &point));
  S2CopyingEdgeCrosser crosser2(point, point);
  EXPECT_EQ(false, crosser2.EdgeOrVertexCrossing(point, point));
}

void TestSignedEdgeOrVertexCrossingInvalid(const S2Point& point) {
  S2EdgeCrosser crosser(&point, &point);
  EXPECT_EQ(0, crosser.SignedEdgeOrVertexCrossing(&point, &point));
  S2CopyingEdgeCrosser crosser2(point, point);
  EXPECT_EQ(0, crosser2.SignedEdgeOrVertexCrossing(point, point));
}

TEST(S2, InvalidDefaultPoints) {
  // Check that default-constructed S2Point arguments don't cause crashes.
  S2Point point(0, 0, 0);
  TestCrossingSignInvalid(point, 0);
  TestEdgeOrVertexCrossingInvalid(point);
  TestSignedEdgeOrVertexCrossingInvalid(point);
}

TEST(S2, InvalidNanPoints) {
  // Check that NaN S2Point arguments don't cause crashes.
  const double nan = std::numeric_limits<double>::quiet_NaN();
  S2Point point(nan, nan, nan);
  TestCrossingSignInvalid(point, -1);
  TestEdgeOrVertexCrossingInvalid(point);
  TestSignedEdgeOrVertexCrossingInvalid(point);
}

#endif

void TestCrossing(const S2Point& a, const S2Point& b,
                  const S2Point& c, const S2Point& d,
                  int crossing_sign, int signed_crossing_sign) {
  // For degenerate edges, CrossingSign() is documented to return 0 if two
  // vertices from different edges are the same and -1 otherwise.  The
  // TestCrossings() function below uses various argument permutations that
  // can sometimes create this case, so we fix it now if necessary.
  if (a == c || a == d || b == c || b == d) crossing_sign = 0;

  // As a sanity check, make sure that the expected value of
  // "signed_crossing_sign" is consistent with its documented properties.
  if (crossing_sign == 1) {
    EXPECT_EQ(signed_crossing_sign, s2pred::Sign(a, b, c));
  } else if (crossing_sign == 0 && signed_crossing_sign != 0) {
    EXPECT_EQ(signed_crossing_sign, (a == c || b == d) ? 1 : -1);
  }

  EXPECT_EQ(crossing_sign, S2::CrossingSign(a, b, c, d));
  S2EdgeCrosser crosser(&a, &b, &c);
  EXPECT_EQ(crossing_sign, crosser.CrossingSign(&d));
  EXPECT_EQ(crossing_sign, crosser.CrossingSign(&c));
  EXPECT_EQ(crossing_sign, crosser.CrossingSign(&d, &c));
  EXPECT_EQ(crossing_sign, crosser.CrossingSign(&c, &d));

  EXPECT_EQ(signed_crossing_sign != 0, S2::EdgeOrVertexCrossing(a, b, c, d));
  crosser.RestartAt(&c);
  EXPECT_EQ(signed_crossing_sign != 0, crosser.EdgeOrVertexCrossing(&d));
  EXPECT_EQ(signed_crossing_sign != 0, crosser.EdgeOrVertexCrossing(&c));
  EXPECT_EQ(signed_crossing_sign != 0, crosser.EdgeOrVertexCrossing(&d, &c));
  EXPECT_EQ(signed_crossing_sign != 0, crosser.EdgeOrVertexCrossing(&c, &d));

  crosser.RestartAt(&c);
  EXPECT_EQ(signed_crossing_sign, crosser.SignedEdgeOrVertexCrossing(&d));
  EXPECT_EQ(-signed_crossing_sign, crosser.SignedEdgeOrVertexCrossing(&c));
  EXPECT_EQ(-signed_crossing_sign, crosser.SignedEdgeOrVertexCrossing(&d, &c));
  EXPECT_EQ(signed_crossing_sign, crosser.SignedEdgeOrVertexCrossing(&c, &d));

  // Check that the crosser can be re-used.
  crosser.Init(&c, &d);
  crosser.RestartAt(&a);
  EXPECT_EQ(crossing_sign, crosser.CrossingSign(&b));
  EXPECT_EQ(crossing_sign, crosser.CrossingSign(&a));

  // Now try all the same tests with CopyingEdgeCrosser.
  S2CopyingEdgeCrosser crosser2(a, b, c);
  EXPECT_EQ(crossing_sign, crosser2.CrossingSign(d));
  EXPECT_EQ(crossing_sign, crosser2.CrossingSign(c));
  EXPECT_EQ(crossing_sign, crosser2.CrossingSign(d, c));
  EXPECT_EQ(crossing_sign, crosser2.CrossingSign(c, d));

  crosser2.RestartAt(c);
  EXPECT_EQ(signed_crossing_sign != 0, crosser2.EdgeOrVertexCrossing(d));
  EXPECT_EQ(signed_crossing_sign != 0, crosser2.EdgeOrVertexCrossing(c));
  EXPECT_EQ(signed_crossing_sign != 0, crosser2.EdgeOrVertexCrossing(d, c));
  EXPECT_EQ(signed_crossing_sign != 0, crosser2.EdgeOrVertexCrossing(c, d));

  crosser2.RestartAt(c);
  EXPECT_EQ(signed_crossing_sign, crosser2.SignedEdgeOrVertexCrossing(d));
  EXPECT_EQ(-signed_crossing_sign, crosser2.SignedEdgeOrVertexCrossing(c));
  EXPECT_EQ(-signed_crossing_sign, crosser2.SignedEdgeOrVertexCrossing(d, c));
  EXPECT_EQ(signed_crossing_sign, crosser2.SignedEdgeOrVertexCrossing(c, d));

  // Check that the crosser can be re-used.
  crosser2.Init(c, d);
  crosser2.RestartAt(a);
  EXPECT_EQ(crossing_sign, crosser2.CrossingSign(b));
  EXPECT_EQ(crossing_sign, crosser2.CrossingSign(a));
}

void TestCrossings(S2Point a, S2Point b, S2Point c, S2Point d,
                   int crossing_sign, int signed_crossing_sign) {
  a = a.Normalize();
  b = b.Normalize();
  c = c.Normalize();
  d = d.Normalize();
  TestCrossing(a, b, c, d, crossing_sign, signed_crossing_sign);
  TestCrossing(b, a, c, d, crossing_sign, -signed_crossing_sign);
  TestCrossing(a, b, d, c, crossing_sign, -signed_crossing_sign);
  TestCrossing(b, a, d, c, crossing_sign, signed_crossing_sign);
  TestCrossing(a, a, c, d, -1, 0);
  TestCrossing(a, b, c, c, -1, 0);
  TestCrossing(a, a, c, c, -1, 0);
  TestCrossing(a, b, a, b, 0, 1);
  if (crossing_sign == 0) {
    // For vertex crossings, if AB crosses CD then CD does not cross AB.
    // In order to get the crossing sign right in both cases, all tests are
    // specified such that AB crosses CD.  The other case is tested here.
    S2_DCHECK_NE(signed_crossing_sign, 0);
    TestCrossing(c, d, a, b, crossing_sign, 0);
  } else {
    // TODO(ericv): Document properties of SignedEdgeOrVertexCrossing.
    TestCrossing(c, d, a, b, crossing_sign, -signed_crossing_sign);
  }
}

TEST(S2, Crossings) {
  // The real tests of edge crossings are in s2{loop,polygon}_test,
  // but we do a few simple tests here.

  // Two regular edges that cross.
  TestCrossings(S2Point(1, 2, 1), S2Point(1, -3, 0.5),
                S2Point(1, -0.5, -3), S2Point(0.1, 0.5, 3), 1, 1);

  // Two regular edges that intersect antipodal points.
  TestCrossings(S2Point(1, 2, 1), S2Point(1, -3, 0.5),
                S2Point(-1, 0.5, 3), S2Point(-0.1, -0.5, -3), -1, 0);

  // Two edges on the same great circle that start at antipodal points.
  TestCrossings(S2Point(0, 0, -1), S2Point(0, 1, 0),
                S2Point(0, 0, 1), S2Point(0, 1, 1), -1, 0);

  // Two edges that cross where one vertex is S2::Origin().
  TestCrossings(S2Point(1, 0, 0), S2::Origin(),
                S2Point(1, -0.1, 1), S2Point(1, 1, -0.1), 1, 1);

  // Two edges that intersect antipodal points where one vertex is
  // S2::Origin().
  TestCrossings(S2Point(1, 0, 0), S2::Origin(),
                S2Point(-1, 0.1, -1), S2Point(-1, -1, 0.1), -1, 0);

  // Two edges that share an endpoint.  The Ortho() direction is (-4,0,2),
  // and edge AB is further CCW around (2,3,4) than CD.
  TestCrossings(S2Point(7, -2, 3), S2Point(2, 3, 4),
                S2Point(2, 3, 4), S2Point(-1, 2, 5), 0, -1);

  // Two edges that barely cross each other near the middle of one edge.  The
  // edge AB is approximately in the x=y plane, while CD is approximately
  // perpendicular to it and ends exactly at the x=y plane.
  TestCrossings(S2Point(1, 1, 1), S2Point(1, nextafter(1, 0), -1),
                S2Point(11, -12, -1), S2Point(10, 10, 1), 1, -1);

  // In this version, the edges are separated by a distance of about 1e-15.
  TestCrossings(S2Point(1, 1, 1), S2Point(1, nextafter(1, 2), -1),
                S2Point(1, -1, 0), S2Point(1, 1, 0), -1, 0);

  // Two edges that barely cross each other near the end of both edges.  This
  // example cannot be handled using regular double-precision arithmetic due
  // to floating-point underflow.
  TestCrossings(S2Point(0, 0, 1), S2Point(2, -1e-323, 1),
                S2Point(1, -1, 1), S2Point(1e-323, 0, 1), 1, -1);

  // In this version, the edges are separated by a distance of about 1e-640.
  TestCrossings(S2Point(0, 0, 1), S2Point(2, 1e-323, 1),
                S2Point(1, -1, 1), S2Point(1e-323, 0, 1), -1, 0);

  // Two edges that barely cross each other near the middle of one edge.
  // Computing the exact determinant of some of the triangles in this test
  // requires more than 2000 bits of precision.
  TestCrossings(S2Point(1, -1e-323, -1e-323), S2Point(1e-323, 1, 1e-323),
                S2Point(1, -1, 1e-323), S2Point(1, 1, 0),
                1, 1);

  // In this version, the edges are separated by a distance of about 1e-640.
  TestCrossings(S2Point(1, 1e-323, -1e-323), S2Point(-1e-323, 1, 1e-323),
                S2Point(1, -1, 1e-323), S2Point(1, 1, 0),
                -1, 0);
}

TEST(S2, CollinearEdgesThatDontTouch) {
  const int kIters = 500;
  for (int iter = 0; iter < kIters; ++iter) {
    S2Point a = S2Testing::RandomPoint();
    S2Point d = S2Testing::RandomPoint();
    S2Point b = S2::Interpolate(a, d, 0.05);
    S2Point c = S2::Interpolate(a, d, 0.95);
    EXPECT_GT(0, S2::CrossingSign(a, b, c, d));
    EXPECT_GT(0, S2::CrossingSign(a, b, c, d));
    S2EdgeCrosser crosser(&a, &b, &c);
    EXPECT_GT(0, crosser.CrossingSign(&d));
    EXPECT_GT(0, crosser.CrossingSign(&c));
  }
}


TEST(S2, CoincidentZeroLengthEdgesThatDontTouch) {
  // It is important that the edge primitives can handle vertices that exactly
  // exactly proportional to each other, i.e. that are not identical but are
  // nevertheless exactly coincident when projected onto the unit sphere.
  // There are various ways that such points can arise.  For example,
  // Normalize() itself is not idempotent: there exist distinct points A,B
  // such that Normalize(A) == B  and Normalize(B) == A.  Another issue is
  // that sometimes calls to Normalize() are skipped when the result of a
  // calculation "should" be unit length mathematically (e.g., when computing
  // the cross product of two orthonormal vectors).
  //
  // This test checks pairs of edges AB and CD where A,B,C,D are exactly
  // coincident on the sphere and the norms of A,B,C,D are monotonically
  // increasing.  Such edge pairs should never intersect.  (This is not
  // obvious, since it depends on the particular symbolic perturbations used
  // by s2pred::Sign().  It would be better to replace this with a test that
  // says that the CCW results must be consistent with each other.)
  const int kIters = 1000;
  for (int iter = 0; iter < kIters; ++iter) {
    // Construct a point P where every component is zero or a power of 2.
    S2Point p;
    for (int i = 0; i < 3; ++i) {
      int binary_exp = S2Testing::rnd.Skewed(11);
      p[i] = (binary_exp > 1022) ? 0 : pow(2, -binary_exp);
    }
    // If all components were zero, try again.  Note that normalization may
    // convert a non-zero point into a zero one due to underflow (!)
    p = p.Normalize();
    if (p == S2Point(0, 0, 0)) { --iter; continue; }

    // Now every non-zero component should have exactly the same mantissa.
    // This implies that if we scale the point by an arbitrary factor, every
    // non-zero component will still have the same mantissa.  Scale the points
    // so that they are all distinct and are still very likely to satisfy
    // S2::IsUnitLength (which allows for a small amount of error in the norm).
    S2Point a = (1-3e-16) * p;
    S2Point b = (1-1e-16) * p;
    S2Point c = p;
    S2Point d = (1+2e-16) * p;
    if (!S2::IsUnitLength(a) || !S2::IsUnitLength(d)) {
      --iter;
      continue;
    }
    // Verify that the expected edges do not cross.
    EXPECT_GT(0, S2::CrossingSign(a, b, c, d));
    S2EdgeCrosser crosser(&a, &b, &c);
    EXPECT_GT(0, crosser.CrossingSign(&d));
    EXPECT_GT(0, crosser.CrossingSign(&c));
  }
}

