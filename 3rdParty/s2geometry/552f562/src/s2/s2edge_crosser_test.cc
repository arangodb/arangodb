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
#include <string>
#include <vector>

#include "s2/base/logging.h"
#include <gtest/gtest.h>
#include "s2/s2edge_crossings.h"
#include "s2/s2edge_distances.h"
#include "s2/s2pointutil.h"
#include "s2/s2testing.h"

using std::vector;

#ifdef NDEBUG

// In non-debug builds, check that default-constructed and/or NaN S2Point
// arguments don't cause crashes, especially on the very first method call
// (since S2CopyingEdgeCrosser checks whether the first vertex of each edge is
// the same as the last vertex of the previous edged when deciding whether or
// not to call Restart).

void TestCrossingSignInvalid(const S2Point& point, int expected) {
  S2EdgeCrosser crosser(&point, &point);
  EXPECT_EQ(expected, crosser.CrossingSign(&point, &point));
  S2CopyingEdgeCrosser crosser2(point, point);
  EXPECT_EQ(expected, crosser2.CrossingSign(point, point));
}

void TestEdgeOrVertexCrossingInvalid(const S2Point& point, bool expected) {
  S2EdgeCrosser crosser(&point, &point);
  EXPECT_EQ(expected, crosser.EdgeOrVertexCrossing(&point, &point));
  S2CopyingEdgeCrosser crosser2(point, point);
  EXPECT_EQ(expected, crosser2.EdgeOrVertexCrossing(point, point));
}

TEST(S2EdgeUtil, InvalidDefaultPoints) {
  // Check that default-constructed S2Point arguments don't cause crashes.
  S2Point point(0, 0, 0);
  TestCrossingSignInvalid(point, 0);
  TestEdgeOrVertexCrossingInvalid(point, false);
}

TEST(S2EdgeUtil, InvalidNanPoints) {
  // Check that NaN S2Point arguments don't cause crashes.
  const double nan = std::numeric_limits<double>::quiet_NaN();
  S2Point point(nan, nan, nan);
  TestCrossingSignInvalid(point, -1);
  TestEdgeOrVertexCrossingInvalid(point, false);
}

#endif

void TestCrossing(const S2Point& a, const S2Point& b,
                  const S2Point& c, const S2Point& d,
                  int robust, bool edge_or_vertex) {
  // Modify the expected result if two vertices from different edges match.
  if (a == c || a == d || b == c || b == d) robust = 0;
  EXPECT_EQ(robust, S2::CrossingSign(a, b, c, d));
  S2EdgeCrosser crosser(&a, &b, &c);
  EXPECT_EQ(robust, crosser.CrossingSign(&d));
  EXPECT_EQ(robust, crosser.CrossingSign(&c));
  EXPECT_EQ(robust, crosser.CrossingSign(&d, &c));
  EXPECT_EQ(robust, crosser.CrossingSign(&c, &d));

  EXPECT_EQ(edge_or_vertex, S2::EdgeOrVertexCrossing(a, b, c, d));
  crosser.RestartAt(&c);
  EXPECT_EQ(edge_or_vertex, crosser.EdgeOrVertexCrossing(&d));
  EXPECT_EQ(edge_or_vertex, crosser.EdgeOrVertexCrossing(&c));
  EXPECT_EQ(edge_or_vertex, crosser.EdgeOrVertexCrossing(&d, &c));
  EXPECT_EQ(edge_or_vertex, crosser.EdgeOrVertexCrossing(&c, &d));

  // Check that the crosser can be re-used.
  crosser.Init(&c, &d);
  crosser.RestartAt(&a);
  EXPECT_EQ(robust, crosser.CrossingSign(&b));
  EXPECT_EQ(robust, crosser.CrossingSign(&a));

  // Now try all the same tests with CopyingEdgeCrosser.
  S2CopyingEdgeCrosser crosser2(a, b, c);
  EXPECT_EQ(robust, crosser2.CrossingSign(d));
  EXPECT_EQ(robust, crosser2.CrossingSign(c));
  EXPECT_EQ(robust, crosser2.CrossingSign(d, c));
  EXPECT_EQ(robust, crosser2.CrossingSign(c, d));

  EXPECT_EQ(edge_or_vertex, S2::EdgeOrVertexCrossing(a, b, c, d));
  crosser2.RestartAt(c);
  EXPECT_EQ(edge_or_vertex, crosser2.EdgeOrVertexCrossing(d));
  EXPECT_EQ(edge_or_vertex, crosser2.EdgeOrVertexCrossing(c));
  EXPECT_EQ(edge_or_vertex, crosser2.EdgeOrVertexCrossing(d, c));
  EXPECT_EQ(edge_or_vertex, crosser2.EdgeOrVertexCrossing(c, d));

  // Check that the crosser can be re-used.
  crosser2.Init(c, d);
  crosser2.RestartAt(a);
  EXPECT_EQ(robust, crosser2.CrossingSign(b));
  EXPECT_EQ(robust, crosser2.CrossingSign(a));
}

void TestCrossings(S2Point a, S2Point b, S2Point c, S2Point d,
                   int robust, bool edge_or_vertex) {
  a = a.Normalize();
  b = b.Normalize();
  c = c.Normalize();
  d = d.Normalize();
  TestCrossing(a, b, c, d, robust, edge_or_vertex);
  TestCrossing(b, a, c, d, robust, edge_or_vertex);
  TestCrossing(a, b, d, c, robust, edge_or_vertex);
  TestCrossing(b, a, d, c, robust, edge_or_vertex);
  TestCrossing(a, a, c, d, -1, false);
  TestCrossing(a, b, c, c, -1, false);
  TestCrossing(a, a, c, c, -1, false);
  TestCrossing(a, b, a, b, 0, true);
  TestCrossing(c, d, a, b, robust, edge_or_vertex != (robust == 0));
}

TEST(S2EdgeUtil, Crossings) {
  // The real tests of edge crossings are in s2{loop,polygon}_test,
  // but we do a few simple tests here.

  // Two regular edges that cross.
  TestCrossings(S2Point(1, 2, 1), S2Point(1, -3, 0.5),
                S2Point(1, -0.5, -3), S2Point(0.1, 0.5, 3), 1, true);

  // Two regular edges that intersect antipodal points.
  TestCrossings(S2Point(1, 2, 1), S2Point(1, -3, 0.5),
                S2Point(-1, 0.5, 3), S2Point(-0.1, -0.5, -3), -1, false);

  // Two edges on the same great circle that start at antipodal points.
  TestCrossings(S2Point(0, 0, -1), S2Point(0, 1, 0),
                S2Point(0, 0, 1), S2Point(0, 1, 1), -1, false);

  // Two edges that cross where one vertex is S2::Origin().
  TestCrossings(S2Point(1, 0, 0), S2::Origin(),
                S2Point(1, -0.1, 1), S2Point(1, 1, -0.1), 1, true);

  // Two edges that intersect antipodal points where one vertex is
  // S2::Origin().
  TestCrossings(S2Point(1, 0, 0), S2::Origin(),
                S2Point(-1, 0.1, -1), S2Point(-1, -1, 0.1), -1, false);

  // Two edges that share an endpoint.  The Ortho() direction is (-4,0,2),
  // and edge CD is further CCW around (2,3,4) than AB.
  TestCrossings(S2Point(2, 3, 4), S2Point(-1, 2, 5),
                S2Point(7, -2, 3), S2Point(2, 3, 4), 0, false);

  // Two edges that barely cross each other near the middle of one edge.  The
  // edge AB is approximately in the x=y plane, while CD is approximately
  // perpendicular to it and ends exactly at the x=y plane.
  TestCrossings(S2Point(1, 1, 1), S2Point(1, nextafter(1, 0), -1),
                S2Point(11, -12, -1), S2Point(10, 10, 1), 1, true);

  // In this version, the edges are separated by a distance of about 1e-15.
  TestCrossings(S2Point(1, 1, 1), S2Point(1, nextafter(1, 2), -1),
                S2Point(1, -1, 0), S2Point(1, 1, 0), -1, false);

  // Two edges that barely cross each other near the end of both edges.  This
  // example cannot be handled using regular double-precision arithmetic due
  // to floating-point underflow.
  TestCrossings(S2Point(0, 0, 1), S2Point(2, -1e-323, 1),
                S2Point(1, -1, 1), S2Point(1e-323, 0, 1), 1, true);

  // In this version, the edges are separated by a distance of about 1e-640.
  TestCrossings(S2Point(0, 0, 1), S2Point(2, 1e-323, 1),
                S2Point(1, -1, 1), S2Point(1e-323, 0, 1), -1, false);

  // Two edges that barely cross each other near the middle of one edge.
  // Computing the exact determinant of some of the triangles in this test
  // requires more than 2000 bits of precision.
  TestCrossings(S2Point(1, -1e-323, -1e-323), S2Point(1e-323, 1, 1e-323),
                S2Point(1, -1, 1e-323), S2Point(1, 1, 0),
                1, true);

  // In this version, the edges are separated by a distance of about 1e-640.
  TestCrossings(S2Point(1, 1e-323, -1e-323), S2Point(-1e-323, 1, 1e-323),
                S2Point(1, -1, 1e-323), S2Point(1, 1, 0),
                -1, false);
}

TEST(S2EdgeUtil, CollinearEdgesThatDontTouch) {
  const int kIters = 500;
  for (int iter = 0; iter < kIters; ++iter) {
    S2Point a = S2Testing::RandomPoint();
    S2Point d = S2Testing::RandomPoint();
    S2Point b = S2::Interpolate(0.05, a, d);
    S2Point c = S2::Interpolate(0.95, a, d);
    EXPECT_GT(0, S2::CrossingSign(a, b, c, d));
    EXPECT_GT(0, S2::CrossingSign(a, b, c, d));
    S2EdgeCrosser crosser(&a, &b, &c);
    EXPECT_GT(0, crosser.CrossingSign(&d));
    EXPECT_GT(0, crosser.CrossingSign(&c));
  }
}


TEST(S2EdgeUtil, CoincidentZeroLengthEdgesThatDontTouch) {
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

