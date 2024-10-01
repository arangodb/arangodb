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

#include "s2/s2edge_distances.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "s2/base/logging.h"
#include "s2/s1chord_angle.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"

using std::max;
using std::min;

namespace S2 {

double GetDistanceFraction(const S2Point& x,
                           const S2Point& a, const S2Point& b) {
  S2_DCHECK_NE(a, b);
  double da = x.Angle(a);
  double db = x.Angle(b);
  return da / (da + db);
}

S2Point GetPointOnLine(const S2Point& a, const S2Point& b,
                      S1ChordAngle r) {
  // Use RobustCrossProd() to compute the tangent vector at A towards B.  This
  // technique is robust even when A and B are antipodal or nearly so.
  S2Point dir = S2::RobustCrossProd(a, b).CrossProd(a).Normalize();
  return GetPointOnRay(a, dir, r);
}

S2Point GetPointOnLine(const S2Point& a, const S2Point& b, S1Angle r) {
  // See comments above.
  S2Point dir = S2::RobustCrossProd(a, b).CrossProd(a).Normalize();
  return GetPointOnRay(a, dir, r);
}

S2Point GetPointToLeft(const S2Point& a, const S2Point& b, S1Angle r) {
  return GetPointOnRay(a, S2::RobustCrossProd(a, b).Normalize(), r);
}

S2Point GetPointToLeft(const S2Point& a, const S2Point& b, S1ChordAngle r) {
  return GetPointOnRay(a, S2::RobustCrossProd(a, b).Normalize(), r);
}

S2Point GetPointToRight(const S2Point& a, const S2Point& b, S1Angle r) {
  return GetPointOnRay(a, S2::RobustCrossProd(b, a).Normalize(), r);
}

S2Point GetPointToRight(const S2Point& a, const S2Point& b, S1ChordAngle r) {
  return GetPointOnRay(a, S2::RobustCrossProd(b, a).Normalize(), r);
}

S2Point Interpolate(const S2Point& a, const S2Point& b, double t) {
  if (t == 0) return a;
  if (t == 1) return b;
  return GetPointOnLine(a, b, t * S1Angle(a, b));
}

// If the minimum distance from X to AB is attained at an interior point of AB
// (i.e., not an endpoint), and that distance is less than "min_dist" or
// "always_update" is true, then update "min_dist" and return true.  Otherwise
// return false.
//
// The "Always" in the function name refers to the template argument, i.e.
// AlwaysUpdateMinInteriorDistance<true> always updates the given distance,
// while AlwaysUpdateMinInteriorDistance<false> does not.  This optimization
// increases the speed of GetDistance() by about 10% without creating code
// duplication.
template <bool always_update>
inline bool AlwaysUpdateMinInteriorDistance(
    const S2Point& x, const S2Point& a, const S2Point& b,
    double xa2, double xb2, S1ChordAngle* min_dist) {
  S2_DCHECK(S2::IsUnitLength(x) && S2::IsUnitLength(a) && S2::IsUnitLength(b));
  S2_DCHECK_EQ(xa2, (x - a).Norm2());
  S2_DCHECK_EQ(xb2, (x - b).Norm2());

  // The closest point on AB could either be one of the two vertices (the
  // "vertex case") or in the interior (the "interior case").  Let C = A x B.
  // If X is in the spherical wedge extending from A to B around the axis
  // through C, then we are in the interior case.  Otherwise we are in the
  // vertex case.
  //
  // Check whether we might be in the interior case.  For this to be true, XAB
  // and XBA must both be acute angles.  Checking this condition exactly is
  // expensive, so instead we consider the planar triangle ABX (which passes
  // through the sphere's interior).  The planar angles XAB and XBA are always
  // less than the corresponding spherical angles, so if we are in the
  // interior case then both of these angles must be acute.
  //
  // We check this by computing the squared edge lengths of the planar
  // triangle ABX, and testing whether angles XAB and XBA are both acute using
  // the law of cosines:
  //
  //            | XA^2 - XB^2 | < AB^2      (*)
  //
  // This test must be done conservatively (taking numerical errors into
  // account) since otherwise we might miss a situation where the true minimum
  // distance is achieved by a point on the edge interior.
  //
  // There are two sources of error in the expression above (*).  The first is
  // that points are not normalized exactly; they are only guaranteed to be
  // within 2 * DBL_EPSILON of unit length.  Under the assumption that the two
  // sides of (*) are nearly equal, the total error due to normalization errors
  // can be shown to be at most
  //
  //        2 * DBL_EPSILON * (XA^2 + XB^2 + AB^2) + 8 * DBL_EPSILON ^ 2 .
  //
  // The other source of error is rounding of results in the calculation of (*).
  // Each of XA^2, XB^2, AB^2 has a maximum relative error of 2.5 * DBL_EPSILON,
  // plus an additional relative error of 0.5 * DBL_EPSILON in the final
  // subtraction which we further bound as 0.25 * DBL_EPSILON * (XA^2 + XB^2 +
  // AB^2) for convenience.  This yields a final error bound of
  //
  //        4.75 * DBL_EPSILON * (XA^2 + XB^2 + AB^2) + 8 * DBL_EPSILON ^ 2 .
  double ab2 = (a - b).Norm2();
  double max_error = (4.75 * DBL_EPSILON * (xa2 + xb2 + ab2) +
                      8 * DBL_EPSILON * DBL_EPSILON);
  if (std::fabs(xa2 - xb2) >= ab2 + max_error) {
    return false;
  }

  // The minimum distance might be to a point on the edge interior.  Let R
  // be closest point to X that lies on the great circle through AB.  Rather
  // than computing the geodesic distance along the surface of the sphere,
  // instead we compute the "chord length" through the sphere's interior.
  // If the squared chord length exceeds min_dist.length2() then we can
  // return "false" immediately.
  //
  // The squared chord length XR^2 can be expressed as XQ^2 + QR^2, where Q
  // is the point X projected onto the plane through the great circle AB.
  // The distance XQ^2 can be written as (X.C)^2 / |C|^2 where C = A x B.
  // We ignore the QR^2 term and instead use XQ^2 as a lower bound, since it
  // is faster and the corresponding distance on the Earth's surface is
  // accurate to within 1% for distances up to about 1800km.
  S2Point c = S2::RobustCrossProd(a, b);
  double c2 = c.Norm2();
  double x_dot_c = x.DotProd(c);
  double x_dot_c2 = x_dot_c * x_dot_c;
  if (!always_update && x_dot_c2 > c2 * min_dist->length2()) {
    // The closest point on the great circle AB is too far away.  We need to
    // test this using ">" rather than ">=" because the actual minimum bound
    // on the distance is (x_dot_c2 / c2), which can be rounded differently
    // than the (more efficient) multiplicative test above.
    return false;
  }
  // Otherwise we do the exact, more expensive test for the interior case.
  // This test is very likely to succeed because of the conservative planar
  // test we did initially.
  //
  // TODO(ericv): Ensure that the errors in test are accurately reflected in the
  // GetUpdateMinInteriorDistanceMaxError().
  S2Point cx = c.CrossProd(x);
  if ((a - x).DotProd(cx) >= 0 || (b - x).DotProd(cx) <= 0) {
    return false;
  }
  // Compute the squared chord length XR^2 = XQ^2 + QR^2 (see above).
  // This calculation has good accuracy for all chord lengths since it
  // is based on both the dot product and cross product (rather than
  // deriving one from the other).  However, note that the chord length
  // representation itself loses accuracy as the angle approaches Pi.
  double qr = 1 - sqrt(cx.Norm2() / c2);
  double dist2 = (x_dot_c2 / c2) + (qr * qr);
  if (!always_update && dist2 >= min_dist->length2()) {
    return false;
  }
  *min_dist = S1ChordAngle::FromLength2(dist2);

  return true;
}

// This function computes the distance from a point X to a line segment AB.
// If the distance is less than "min_dist" or "always_update" is true, it
// updates "min_dist" and returns true.  Otherwise it returns false.
//
// The "Always" in the function name refers to the template argument, i.e.
// AlwaysUpdateMinDistance<true> always updates the given distance, while
// AlwaysUpdateMinDistance<false> does not.  This optimization increases the
// speed of GetDistance() by about 10% without creating code duplication.
template <bool always_update>
inline bool AlwaysUpdateMinDistance(const S2Point& x,
                                    const S2Point& a, const S2Point& b,
                                    S1ChordAngle* min_dist) {
  S2_DCHECK(S2::IsUnitLength(x) && S2::IsUnitLength(a) && S2::IsUnitLength(b));

  double xa2 = (x - a).Norm2(), xb2 = (x - b).Norm2();
  if (AlwaysUpdateMinInteriorDistance<always_update>(x, a, b, xa2, xb2,
                                                     min_dist)) {
    return true;  // Minimum distance is attained along the edge interior.
  }
  // Otherwise the minimum distance is to one of the endpoints.
  double dist2 = min(xa2, xb2);
  if (!always_update && dist2 >= min_dist->length2()) {
    return false;
  }
  *min_dist = S1ChordAngle::FromLength2(dist2);
  return true;
}

S1Angle GetDistance(const S2Point& x, const S2Point& a, const S2Point& b) {
  S1ChordAngle min_dist;
  AlwaysUpdateMinDistance<true>(x, a, b, &min_dist);
  return min_dist.ToAngle();
}

bool UpdateMinDistance(const S2Point& x, const S2Point& a, const S2Point& b,
                       S1ChordAngle* min_dist) {
  return AlwaysUpdateMinDistance<false>(x, a, b, min_dist);
}

bool UpdateMaxDistance(const S2Point& x, const S2Point& a, const S2Point& b,
                       S1ChordAngle* max_dist) {
  auto dist = max(S1ChordAngle(x, a), S1ChordAngle(x, b));
  if (dist > S1ChordAngle::Right()) {
    AlwaysUpdateMinDistance<true>(-x, a, b, &dist);
    dist = S1ChordAngle::Straight() - dist;
  }
  if (*max_dist < dist) {
    *max_dist = dist;
    return true;
  }

  return false;
}


bool UpdateMinInteriorDistance(const S2Point& x,
                               const S2Point& a, const S2Point& b,
                               S1ChordAngle* min_dist) {
  double xa2 = (x - a).Norm2(), xb2 = (x - b).Norm2();
  return AlwaysUpdateMinInteriorDistance<false>(x, a, b, xa2, xb2, min_dist);
}

// Returns the maximum error in the result of UpdateMinInteriorDistance,
// assuming that all input points are normalized to within the bounds
// guaranteed by S2Point::Normalize().  The error can be added or subtracted
// from an S1ChordAngle "x" using x.PlusError(error).
static double GetUpdateMinInteriorDistanceMaxError(S1ChordAngle dist) {
  // If a point is more than 90 degrees from an edge, then the minimum
  // distance is always to one of the endpoints, not to the edge interior.
  if (dist >= S1ChordAngle::Right()) return 0.0;

  // This bound includes all source of error, assuming that the input points
  // are normalized to within the bounds guaranteed to S2Point::Normalize().
  // "a" and "b" are components of chord length that are perpendicular and
  // parallel to the plane containing the edge respectively.
  double b = min(1.0, 0.5 * dist.length2());
  double a = sqrt(b * (2 - b));
  return ((2.5 + 2 * sqrt(3) + 8.5 * a) * a +
          (2 + 2 * sqrt(3) / 3 + 6.5 * (1 - b)) * b +
          (23 + 16 / sqrt(3)) * DBL_EPSILON) * DBL_EPSILON;
}

double GetUpdateMinDistanceMaxError(S1ChordAngle dist) {
  // There are two cases for the maximum error in UpdateMinDistance(),
  // depending on whether the closest point is interior to the edge.
  return max(GetUpdateMinInteriorDistanceMaxError(dist),
             dist.GetS2PointConstructorMaxError());
}

S2Point Project(const S2Point& x, const S2Point& a, const S2Point& b,
                const Vector3_d& a_cross_b) {
  S2_DCHECK(S2::IsUnitLength(a));
  S2_DCHECK(S2::IsUnitLength(b));
  S2_DCHECK(S2::IsUnitLength(x));

  // TODO(ericv): When X is nearly perpendicular to the plane containing AB,
  // the result is guaranteed to be close to the edge AB but may be far from
  // the true projected result.  This could be fixed by computing the product
  // (A x B) x X x (A x B) using methods similar to S2::RobustCrossProd() and
  // S2::GetIntersection().  However note that the error tolerance would need
  // to be significantly larger in order for this calculation to succeed in
  // double precision most of the time.  For example to avoid higher precision
  // when X is within 60 degrees of AB the minimum error would be 18 * DBL_ERR,
  // and to avoid higher precision when X is within 87 degrees of AB the
  // minimum error would be 120 * DBL_ERR.

  // The following is not necessary to meet accuracy guarantees but helps
  // to avoid unexpected results in unit tests.
  if (x == a || x == b) return x;

  // Find the closest point to X along the great circle through AB.  Note that
  // we use "n" rather than a_cross_b in the final cross product in order to
  // avoid the possibility of underflow.
  S2Point n = a_cross_b.Normalize();
  S2Point p = S2::RobustCrossProd(n, x).CrossProd(n).Normalize();

  // If this point is on the edge AB, then it's the closest point.
  S2Point pn = p.CrossProd(n);
  if (s2pred::Sign(p, n, a, pn) > 0 && s2pred::Sign(p, n, b, pn) < 0) {
    return p;
  }

  // Otherwise, the closest point is either A or B.
  return ((x - a).Norm2() <= (x - b).Norm2()) ? a : b;
}

S2Point Project(const S2Point& x, const S2Point& a, const S2Point& b) {
  return Project(x, a, b, S2::RobustCrossProd(a, b));
}

bool UpdateEdgePairMinDistance(
    const S2Point& a0, const S2Point& a1,
    const S2Point& b0, const S2Point& b1,
    S1ChordAngle* min_dist) {
  if (*min_dist == S1ChordAngle::Zero()) {
    return false;
  }
  if (S2::CrossingSign(a0, a1, b0, b1) >= 0) {
    *min_dist = S1ChordAngle::Zero();
    return true;
  }
  // Otherwise, the minimum distance is achieved at an endpoint of at least
  // one of the two edges.  We use "|" rather than "||" below to ensure that
  // all four possibilities are always checked.
  //
  // The calculation below computes each of the six vertex-vertex distances
  // twice (this could be optimized).
  //
  // We do not want the short circuit behavior of ||. Suppress
  // -Wbitwise-instead-of-logical errors by converting to int.
  return (int{UpdateMinDistance(a0, b0, b1, min_dist)} |
          int{UpdateMinDistance(a1, b0, b1, min_dist)} |
          int{UpdateMinDistance(b0, a0, a1, min_dist)} |
          int{UpdateMinDistance(b1, a0, a1, min_dist)});
}

bool UpdateEdgePairMaxDistance(
    const S2Point& a0, const S2Point& a1,
    const S2Point& b0, const S2Point& b1,
    S1ChordAngle* max_dist) {
  if (*max_dist == S1ChordAngle::Straight()) {
    return false;
  }
  if (S2::CrossingSign(a0, a1, -b0, -b1) >= 0) {
    *max_dist = S1ChordAngle::Straight();
    return true;
  }
  // Otherwise, the maximum distance is achieved at an endpoint of at least
  // one of the two edges.  We use "|" rather than "||" below to ensure that
  // all four possibilities are always checked.
  //
  // The calculation below computes each of the six vertex-vertex distances
  // twice (this could be optimized).
  //
  // We do not want the short circuit behavior of ||. Suppress
  // -Wbitwise-instead-of-logical errors by converting to int.
  return (int{UpdateMaxDistance(a0, b0, b1, max_dist)} |
          int{UpdateMaxDistance(a1, b0, b1, max_dist)} |
          int{UpdateMaxDistance(b0, a0, a1, max_dist)} |
          int{UpdateMaxDistance(b1, a0, a1, max_dist)});
}

std::pair<S2Point, S2Point> GetEdgePairClosestPoints(
      const S2Point& a0, const S2Point& a1,
      const S2Point& b0, const S2Point& b1) {
  if (S2::CrossingSign(a0, a1, b0, b1) > 0) {
    S2Point x = S2::GetIntersection(a0, a1, b0, b1);
    return std::make_pair(x, x);
  }
  // We save some work by first determining which vertex/edge pair achieves
  // the minimum distance, and then computing the closest point on that edge.
  S1ChordAngle min_dist;
  AlwaysUpdateMinDistance<true>(a0, b0, b1, &min_dist);
  enum { A0, A1, B0, B1 } closest_vertex = A0;
  if (UpdateMinDistance(a1, b0, b1, &min_dist)) { closest_vertex = A1; }
  if (UpdateMinDistance(b0, a0, a1, &min_dist)) { closest_vertex = B0; }
  if (UpdateMinDistance(b1, a0, a1, &min_dist)) { closest_vertex = B1; }
  switch (closest_vertex) {
    case A0: return std::make_pair(a0, Project(a0, b0, b1));
    case A1: return std::make_pair(a1, Project(a1, b0, b1));
    case B0: return std::make_pair(Project(b0, a0, a1), b0);
    case B1: return std::make_pair(Project(b1, a0, a1), b1);
    default: S2_LOG(FATAL) << "Unreached (to suppress Android compiler warning)";
  }
}

// TODO(ericv): Optimize this function to use S1ChordAngle rather than S1Angle.
bool IsEdgeBNearEdgeA(const S2Point& a0, const S2Point& a1,
                      const S2Point& b0, const S2Point& b1,
                      S1Angle tolerance) {
  S2_DCHECK_LT(tolerance.radians(), M_PI / 2);
  S2_DCHECK_GT(tolerance.radians(), 0);
  // The point on edge B=b0b1 furthest from edge A=a0a1 is either b0, b1, or
  // some interior point on B.  If it is an interior point on B, then it must be
  // one of the two points where the great circle containing B (circ(B)) is
  // furthest from the great circle containing A (circ(A)).  At these points,
  // the distance between circ(B) and circ(A) is the angle between the planes
  // containing them.

  Vector3_d a_ortho = S2::RobustCrossProd(a0, a1).Normalize();
  const S2Point a_nearest_b0 = Project(b0, a0, a1, a_ortho);
  const S2Point a_nearest_b1 = Project(b1, a0, a1, a_ortho);
  // If a_nearest_b0 and a_nearest_b1 have opposite orientation from a0 and a1,
  // we invert a_ortho so that it points in the same direction as a_nearest_b0 x
  // a_nearest_b1.  This helps us handle the case where A and B are oppositely
  // oriented but otherwise might be near each other.  We check orientation and
  // invert rather than computing a_nearest_b0 x a_nearest_b1 because those two
  // points might be equal, and have an unhelpful cross product.
  if (s2pred::Sign(a_ortho, a_nearest_b0, a_nearest_b1) < 0) a_ortho *= -1;

  // To check if all points on B are within tolerance of A, we first check to
  // see if the endpoints of B are near A.  If they are not, B is not near A.
  const S1Angle b0_distance(b0, a_nearest_b0);
  const S1Angle b1_distance(b1, a_nearest_b1);
  if (b0_distance > tolerance || b1_distance > tolerance)
    return false;

  // If b0 and b1 are both within tolerance of A, we check to see if the angle
  // between the planes containing B and A is greater than tolerance.  If it is
  // not, no point on B can be further than tolerance from A (recall that we
  // already know that b0 and b1 are close to A, and S2Edges are all shorter
  // than 180 degrees).  The angle between the planes containing circ(A) and
  // circ(B) is the angle between their normal vectors.
  const Vector3_d b_ortho = S2::RobustCrossProd(b0, b1).Normalize();
  const S1Angle planar_angle(a_ortho, b_ortho);
  if (planar_angle <= tolerance)
    return true;

  // When planar_angle >= Pi/2, there are only two possible scenarios:
  //
  //  1.) b0 and b1 are closest to A at distinct endpoints of A, in which case
  //      the opposite orientation of a_ortho and b_ortho means that A and B are
  //      in opposite hemispheres and hence not close to each other.
  //
  //  2.) b0 and b1 are closest to A at the same endpoint of A, in which case
  //      the orientation of a_ortho was chosen arbitrarily to be that of a0
  //      cross a1.  B must be shorter than 2*tolerance and all points in B are
  //      close to one endpoint of A, and hence to A.
  //
  // Note that this logic *must* be used when planar_angle >= Pi/2 because the
  // code beyond does not handle the case where the maximum distance is
  // attained at the interior point of B that is equidistant from the
  // endpoints of A.  This happens when B intersects the perpendicular
  // bisector of the endpoints of A in the hemisphere opposite A's midpoint.
  if (planar_angle >= S1Angle::Radians(M_PI_2)) {
    return ((S1Angle(b0, a0) < S1Angle(b0, a1)) ==
            (S1Angle(b1, a0) < S1Angle(b1, a1)));
  }

  // Otherwise, if either of the two points on circ(B) where circ(B) is
  // furthest from circ(A) lie on edge B, edge B is not near edge A.
  //
  // The normalized projection of a_ortho onto the plane of circ(B) is one of
  // the two points along circ(B) where it is furthest from circ(A).  The other
  // is -1 times the normalized projection.
  //
  // Note that the formula (A - (A.B) * B) loses accuracy when |A.B| ~= 1, so
  // instead we compute it using two cross products.  (The first product does
  // not need RobustCrossProd since its arguments are perpendicular.)
  S2Point furthest = b_ortho.CrossProd(S2::RobustCrossProd(a_ortho, b_ortho))
                     .Normalize();
  S2Point furthest_inv = -1 * furthest;

  // A point p lies on B if you can proceed from b_ortho to b0 to p to b1 and
  // back to b_ortho without ever turning right.  We test this for furthest and
  // furthest_inv, and return true if neither point lies on B.
  return !((s2pred::Sign(b_ortho, b0, furthest) > 0 &&
            s2pred::Sign(furthest, b1, b_ortho) > 0) ||
           (s2pred::Sign(b_ortho, b0, furthest_inv) > 0 &&
            s2pred::Sign(furthest_inv, b1, b_ortho) > 0));
}

}  // namespace S2
