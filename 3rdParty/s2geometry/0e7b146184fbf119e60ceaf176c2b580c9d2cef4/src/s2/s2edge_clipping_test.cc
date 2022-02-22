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

#include "s2/s2edge_clipping.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

#include "s2/base/logging.h"
#include <gtest/gtest.h>
#include "s2/third_party/absl/strings/str_cat.h"
#include "s2/r1interval.h"
#include "s2/r2rect.h"
#include "s2/s1chord_angle.h"
#include "s2/s1interval.h"
#include "s2/s2coords.h"
#include "s2/s2pointutil.h"
#include "s2/s2testing.h"

using absl::StrCat;
using std::fabs;
using std::max;

void TestFaceClipping(const S2Point& a_raw, const S2Point& b_raw) {
  S2Point a = a_raw.Normalize();
  S2Point b = b_raw.Normalize();
  // TODO(ericv): Remove the following line once S2::RobustCrossProd is
  // extended to use simulation of simplicity.
  if (a == -b) return;

  // First we test GetFaceSegments.
  S2::FaceSegmentVector segments;
  S2::GetFaceSegments(a, b, &segments);
  int n = segments.size();
  EXPECT_GE(n, 1);

  ::testing::Message msg;
  msg << "\nA=" << a_raw << "\nB=" << b_raw;
  msg << "\nN=" << S2::RobustCrossProd(a, b) << "\nSegments:\n";
  int i = 0;
  for (const S2::FaceSegment& s : segments) {
    msg << i++ << ": face=" << s.face << ", a=" << s.a << ", b=" << s.b << "\n";
  }
  SCOPED_TRACE(msg);

  R2Rect biunit(R1Interval(-1, 1), R1Interval(-1, 1));
  const double kErrorRadians = S2::kFaceClipErrorRadians;

  // The first and last vertices should approximately equal A and B.
  EXPECT_LE(a.Angle(S2::FaceUVtoXYZ(segments[0].face, segments[0].a)),
            kErrorRadians);
  EXPECT_LE(b.Angle(S2::FaceUVtoXYZ(segments[n-1].face, segments[n-1].b)),
            kErrorRadians);

  S2Point norm = S2::RobustCrossProd(a, b).Normalize();
  S2Point a_tangent = norm.CrossProd(a);
  S2Point b_tangent = b.CrossProd(norm);
  for (int i = 0; i < n; ++i) {
    // Vertices may not protrude outside the biunit square.
    EXPECT_TRUE(biunit.Contains(segments[i].a));
    EXPECT_TRUE(biunit.Contains(segments[i].b));
    if (i == 0) continue;

    // The two representations of each interior vertex (on adjacent faces)
    // must correspond to exactly the same S2Point.
    EXPECT_NE(segments[i-1].face, segments[i].face);
    EXPECT_EQ(S2::FaceUVtoXYZ(segments[i-1].face, segments[i-1].b),
              S2::FaceUVtoXYZ(segments[i].face, segments[i].a));

    // Interior vertices should be in the plane containing A and B, and should
    // be contained in the wedge of angles between A and B (i.e., the dot
    // products with a_tangent and b_tangent should be non-negative).
    S2Point p = S2::FaceUVtoXYZ(segments[i].face, segments[i].a).Normalize();
    EXPECT_LE(fabs(p.DotProd(norm)), kErrorRadians);
    EXPECT_GE(p.DotProd(a_tangent), -kErrorRadians);
    EXPECT_GE(p.DotProd(b_tangent), -kErrorRadians);
  }

  // Now we test ClipToPaddedFace (sometimes with a padding of zero).  We do
  // this by defining an (x,y) coordinate system for the plane containing AB,
  // and converting points along the great circle AB to angles in the range
  // [-Pi, Pi].  We then accumulate the angle intervals spanned by each
  // clipped edge; the union over all 6 faces should approximately equal the
  // interval covered by the original edge.
  S2Testing::Random* rnd = &S2Testing::rnd;
  double padding = rnd->OneIn(10) ? 0.0 : 1e-10 * pow(1e-5, rnd->RandDouble());
  S2Point x_axis = a, y_axis = a_tangent;
  S1Interval expected_angles(0, a.Angle(b));
  S1Interval max_angles = expected_angles.Expanded(kErrorRadians);
  S1Interval actual_angles;
  for (int face = 0; face < 6; ++face) {
    R2Point a_uv, b_uv;
    if (S2::ClipToPaddedFace(a, b, face, padding, &a_uv, &b_uv)) {
      S2Point a_clip = S2::FaceUVtoXYZ(face, a_uv).Normalize();
      S2Point b_clip = S2::FaceUVtoXYZ(face, b_uv).Normalize();
      EXPECT_LE(fabs(a_clip.DotProd(norm)), kErrorRadians);
      EXPECT_LE(fabs(b_clip.DotProd(norm)), kErrorRadians);
      if (a_clip.Angle(a) > kErrorRadians) {
        EXPECT_DOUBLE_EQ(1 + padding, max(fabs(a_uv[0]), fabs(a_uv[1])));
      }
      if (b_clip.Angle(b) > kErrorRadians) {
        EXPECT_DOUBLE_EQ(1 + padding, max(fabs(b_uv[0]), fabs(b_uv[1])));
      }
      double a_angle = atan2(a_clip.DotProd(y_axis), a_clip.DotProd(x_axis));
      double b_angle = atan2(b_clip.DotProd(y_axis), b_clip.DotProd(x_axis));
      // Rounding errors may cause b_angle to be slightly less than a_angle.
      // We handle this by constructing the interval with FromPointPair(),
      // which is okay since the interval length is much less than M_PI.
      S1Interval face_angles = S1Interval::FromPointPair(a_angle, b_angle);
      EXPECT_TRUE(max_angles.Contains(face_angles));
      actual_angles = actual_angles.Union(face_angles);
    }
  }
  EXPECT_TRUE(actual_angles.Expanded(kErrorRadians).Contains(expected_angles));
}

void TestFaceClippingEdgePair(const S2Point& a, const S2Point& b) {
  TestFaceClipping(a, b);
  TestFaceClipping(b, a);
}

// This function is designed to choose line segment endpoints that are
// difficult to handle correctly.  Given two adjacent cube vertices P and Q,
// it returns either an edge midpoint, face midpoint, or corner vertex that is
// in the plane of PQ and that has been perturbed slightly.  It also sometimes
// returns a random point from anywhere on the sphere.
S2Point PerturbedCornerOrMidpoint(const S2Point& p, const S2Point& q) {
  S2Testing::Random* rnd = &S2Testing::rnd;
  S2Point a = (rnd->Uniform(3) - 1) * p + (rnd->Uniform(3) - 1) * q;
  if (rnd->OneIn(10)) {
    // This perturbation often has no effect except on coordinates that are
    // zero, in which case the perturbed value is so small that operations on
    // it often result in underflow.
    a += pow(1e-300, rnd->RandDouble()) * S2Testing::RandomPoint();
  } else if (rnd->OneIn(2)) {
    // For coordinates near 1 (say > 0.5), this perturbation yields values
    // that are only a few representable values away from the initial value.
    a += 4 * DBL_EPSILON * S2Testing::RandomPoint();
  } else {
    // A perturbation whose magnitude is in the range [1e-25, 1e-10].
    a += 1e-10 * pow(1e-15, rnd->RandDouble()) * S2Testing::RandomPoint();
  }
  if (a.Norm2() < DBL_MIN) {
    // If a.Norm2() is denormalized, Normalize() loses too much precision.
    return PerturbedCornerOrMidpoint(p, q);
  }
  return a;
}

TEST(S2EdgeUtil, FaceClipping) {
  // Start with a few simple cases.
  // An edge that is entirely contained within one cube face:
  TestFaceClippingEdgePair(S2Point(1, -0.5, -0.5), S2Point(1, 0.5, 0.5));
  // An edge that crosses one cube edge:
  TestFaceClippingEdgePair(S2Point(1, 0, 0), S2Point(0, 1, 0));
  // An edge that crosses two opposite edges of face 0:
  TestFaceClippingEdgePair(S2Point(0.75, 0, -1), S2Point(0.75, 0, 1));
  // An edge that crosses two adjacent edges of face 2:
  TestFaceClippingEdgePair(S2Point(1, 0, 0.75), S2Point(0, 1, 0.75));
  // An edges that crosses three cube edges (four faces):
  TestFaceClippingEdgePair(S2Point(1, 0.9, 0.95), S2Point(-1, 0.95, 0.9));

  // Comprehensively test edges that are difficult to handle, especially those
  // that nearly follow one of the 12 cube edges.
  S2Testing::Random* rnd = &S2Testing::rnd;
  R2Rect biunit(R1Interval(-1, 1), R1Interval(-1, 1));
  const int kIters = 1000;  // Test passes with 1e6 iterations
  for (int iter = 0; iter < kIters; ++iter) {
    SCOPED_TRACE(StrCat("Iteration ", iter));
    // Choose two adjacent cube corners P and Q.
    int face = rnd->Uniform(6);
    int i = rnd->Uniform(4);
    int j = (i + 1) & 3;
    S2Point p = S2::FaceUVtoXYZ(face, biunit.GetVertex(i));
    S2Point q = S2::FaceUVtoXYZ(face, biunit.GetVertex(j));

    // Now choose two points that are nearly in the plane of PQ, preferring
    // points that are near cube corners, face midpoints, or edge midpoints.
    S2Point a = PerturbedCornerOrMidpoint(p, q);
    S2Point b = PerturbedCornerOrMidpoint(p, q);
    TestFaceClipping(a, b);
  }
}

// Choose a random point in the rectangle defined by points A and B, sometimes
// returning a point on the edge AB or the points A and B themselves.
R2Point ChooseRectPoint(const R2Point& a, const R2Point& b) {
  S2Testing::Random* rnd = &S2Testing::rnd;
  if (rnd->OneIn(5)) {
    return rnd->OneIn(2) ? a : b;
  } else if (rnd->OneIn(3)) {
    return a + rnd->RandDouble() * (b - a);
  } else {
    // a[i] may be >, <, or == b[i], so we write it like this instead
    // of using UniformDouble.
    return R2Point(a[0] + rnd->RandDouble() * (b[0] - a[0]),
                   a[1] + rnd->RandDouble() * (b[1] - a[1]));
  }
}

// Given a point X on the line AB (which is checked), return the fraction "t"
// such that x = (1-t)*a + t*b.  Return 0 if A = B.
double GetFraction(const R2Point& x, const R2Point& a, const R2Point& b) {
  // A bound for the error in edge clipping plus the error in the calculation
  // below (which is similar to IntersectsRect).
  const double kError = (S2::kEdgeClipErrorUVDist +
                         S2::kIntersectsRectErrorUVDist);
  if (a == b) return 0.0;
  R2Point dir = (b - a).Normalize();
  EXPECT_LE(fabs((x - a).DotProd(dir.Ortho())), kError);
  return (x - a).DotProd(dir);
}

// Given a point P representing a possibly clipped endpoint A of an edge AB,
// verify that "clip" contains P, and that if clipping occurred (i.e., P != A)
// then P is on the boundary of "clip".
void CheckPointOnBoundary(const R2Point& p, const R2Point& a,
                          const R2Rect& clip) {
  EXPECT_TRUE(clip.Contains(p));
  if (p != a) {
    EXPECT_FALSE(clip.Contains(R2Point(nextafter(p[0], a[0]),
                                       nextafter(p[1], a[1]))));
  }
}

// Given an edge AB and a rectangle "clip", verify that IntersectsRect(),
// ClipEdge(), and ClipEdgeBound() produce consistent results.
void TestClipEdge(const R2Point& a, const R2Point& b, const R2Rect& clip) {
  // A bound for the error in edge clipping plus the error in the
  // IntersectsRect calculation below.
  const double kError = (S2::kEdgeClipErrorUVDist +
                         S2::kIntersectsRectErrorUVDist);
  R2Point a_clipped, b_clipped;
  if (!S2::ClipEdge(a, b, clip, &a_clipped, &b_clipped)) {
    EXPECT_FALSE(S2::IntersectsRect(a, b, clip.Expanded(-kError)));
  } else {
    EXPECT_TRUE(S2::IntersectsRect(a, b, clip.Expanded(kError)));
    // Check that the clipped points lie on the edge AB, and that the points
    // have the expected order along the segment AB.
    EXPECT_LE(GetFraction(a_clipped, a, b), GetFraction(b_clipped, a, b));
    // Check that the clipped portion of AB is as large as possible.
    CheckPointOnBoundary(a_clipped, a, clip);
    CheckPointOnBoundary(b_clipped, b, clip);
  }
  // Choose a random initial bound to pass to ClipEdgeBound.
  R2Rect initial_clip = R2Rect::FromPointPair(ChooseRectPoint(a, b),
                                              ChooseRectPoint(a, b));
  R2Rect bound = S2::GetClippedEdgeBound(a, b, initial_clip);
  if (bound.is_empty()) return;  // Precondition of ClipEdgeBound not met
  R2Rect max_bound = bound.Intersection(clip);
  if (!S2::ClipEdgeBound(a, b, clip, &bound)) {
    EXPECT_FALSE(S2::IntersectsRect(a, b, max_bound.Expanded(-kError)));
  } else {
    EXPECT_TRUE(S2::IntersectsRect(a, b, max_bound.Expanded(kError)));
    // Check that the bound is as large as possible.
    int ai = (a[0] > b[0]), aj = (a[1] > b[1]);
    CheckPointOnBoundary(bound.GetVertex(ai, aj), a, max_bound);
    CheckPointOnBoundary(bound.GetVertex(1-ai, 1-aj), b, max_bound);
  }
}

// Given an interval "clip", randomly choose either a value in the interval, a
// value outside the interval, or one of the two interval endpoints, ensuring
// that all cases have reasonable probability for any interval "clip".
double ChooseEndpoint(const R1Interval& clip) {
  S2Testing::Random* rnd = &S2Testing::rnd;
  if (rnd->OneIn(5)) {
    return rnd->OneIn(2) ? clip.lo() : clip.hi();
  } else {
    switch (rnd->Uniform(3)) {
      case 0:  return clip.lo() - rnd->RandDouble();
      case 1:  return clip.hi() + rnd->RandDouble();
      default: return clip.lo() + rnd->RandDouble() * clip.GetLength();
    }
  }
}

// Given a rectangle "clip", choose a point that may lie in the rectangle
// interior, along an extended edge, exactly at a vertex, or in one of the
// eight regions exterior to "clip" that are separated by its extended edges.
// Also sometimes return points that are exactly on one of the extended
// diagonals of "clip".  All cases are reasonably likely to occur for any
// given rectangle "clip".
R2Point ChooseEndpoint(const R2Rect& clip) {
  if (S2Testing::rnd.OneIn(10)) {
    // Return a point on one of the two extended diagonals.
    int diag = S2Testing::rnd.Uniform(2);
    double t = S2Testing::rnd.UniformDouble(-1, 2);
    return (1 - t) * clip.GetVertex(diag) + t * clip.GetVertex(diag + 2);
  } else {
    return R2Point(ChooseEndpoint(clip[0]), ChooseEndpoint(clip[1]));
  }
}

// Given a rectangle "clip", test the S2EdgeUtil edge clipping methods using
// many edges that are randomly constructed to trigger special cases.
void TestEdgeClipping(const R2Rect& clip) {
  const int kIters = 1000;  // Test passes with 1e6 iterations
  for (int iter = 0; iter < kIters; ++iter) {
    SCOPED_TRACE(StrCat("Iteration ", iter));
    TestClipEdge(ChooseEndpoint(clip), ChooseEndpoint(clip), clip);
  }
}

TEST(S2EdgeUtil, EdgeClipping) {
  S2Testing::Random* rnd = &S2Testing::rnd;
  // Test clipping against random rectangles.
  for (int i = 0; i < 5; ++i) {
    TestEdgeClipping(R2Rect::FromPointPair(
        R2Point(rnd->UniformDouble(-1, 1), rnd->UniformDouble(-1, 1)),
        R2Point(rnd->UniformDouble(-1, 1), rnd->UniformDouble(-1, 1))));
  }
  // Also clip against one-dimensional, singleton, and empty rectangles.
  TestEdgeClipping(R2Rect(R1Interval(-0.7, -0.7), R1Interval(0.3, 0.35)));
  TestEdgeClipping(R2Rect(R1Interval(0.2, 0.5), R1Interval(0.3, 0.3)));
  TestEdgeClipping(R2Rect(R1Interval(-0.7, 0.3), R1Interval(0, 0)));
  TestEdgeClipping(R2Rect::FromPoint(R2Point(0.3, 0.8)));
  TestEdgeClipping(R2Rect::Empty());
}
