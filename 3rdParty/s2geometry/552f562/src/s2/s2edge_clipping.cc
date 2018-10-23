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

#include <cfloat>
#include <cmath>

#include "s2/base/logging.h"
#include "s2/r1interval.h"
#include "s2/s2coords.h"
#include "s2/s2pointutil.h"
#include "s2/util/math/vector.h"

namespace S2 {

using std::fabs;
using std::max;
using std::min;

// Error constant definitions.  See the header file for details.
const double kFaceClipErrorRadians = 3 * DBL_EPSILON;
const double kFaceClipErrorUVDist = 9 * DBL_EPSILON;
const double kFaceClipErrorUVCoord = 9 * M_SQRT1_2 * DBL_EPSILON;
const double kIntersectsRectErrorUVDist = 3 * M_SQRT2 * DBL_EPSILON;
const double kEdgeClipErrorUVCoord = 2.25 * DBL_EPSILON;
const double kEdgeClipErrorUVDist = 2.25 * DBL_EPSILON;

// S2PointUVW is used to document that a given S2Point is expressed in the
// (u,v,w) coordinates of some cube face.
using S2PointUVW = S2Point;

// The three functions below all compare a sum (u + v) to a third value w.
// They are implemented in such a way that they produce an exact result even
// though all calculations are done with ordinary floating-point operations.
// Here are the principles on which these functions are based:
//
// A. If u + v < w in floating-point, then u + v < w in exact arithmetic.
//
// B. If u + v < w in exact arithmetic, then at least one of the following
//    expressions is true in floating-point:
//       u + v < w
//       u < w - v
//       v < w - u
//
//    Proof: By rearranging terms and substituting ">" for "<", we can assume
//    that all values are non-negative.  Now clearly "w" is not the smallest
//    value, so assume WLOG that "u" is the smallest.  We want to show that
//    u < w - v in floating-point.  If v >= w/2, the calculation of w - v is
//    exact since the result is smaller in magnitude than either input value,
//    so the result holds.  Otherwise we have u <= v < w/2 and w - v >= w/2
//    (even in floating point), so the result also holds.

// Return true if u + v == w exactly.
inline static bool SumEquals(double u, double v, double w) {
  return (u + v == w) && (u == w - v) && (v == w - u);
}

// Return true if a given directed line L intersects the cube face F.  The
// line L is defined by its normal N in the (u,v,w) coordinates of F.
inline static bool IntersectsFace(const S2PointUVW& n) {
  // L intersects the [-1,1]x[-1,1] square in (u,v) if and only if the dot
  // products of N with the four corner vertices (-1,-1,1), (1,-1,1), (1,1,1),
  // and (-1,1,1) do not all have the same sign.  This is true exactly when
  // |Nu| + |Nv| >= |Nw|.  The code below evaluates this expression exactly
  // (see comments above).
  double u = fabs(n[0]), v = fabs(n[1]), w = fabs(n[2]);
  // We only need to consider the cases where u or v is the smallest value,
  // since if w is the smallest then both expressions below will have a
  // positive LHS and a negative RHS.
  return (v >= w - u) && (u >= w - v);
}

// Given a directed line L intersecting a cube face F, return true if L
// intersects two opposite edges of F (including the case where L passes
// exactly through a corner vertex of F).  The line L is defined by its
// normal N in the (u,v,w) coordinates of F.
inline static bool IntersectsOppositeEdges(const S2PointUVW& n) {
  // The line L intersects opposite edges of the [-1,1]x[-1,1] (u,v) square if
  // and only exactly two of the corner vertices lie on each side of L.  This
  // is true exactly when ||Nu| - |Nv|| >= |Nw|.  The code below evaluates this
  // expression exactly (see comments above).
  double u = fabs(n[0]), v = fabs(n[1]), w = fabs(n[2]);
  // If w is the smallest, the following line returns an exact result.
  if (fabs(u - v) != w) return fabs(u - v) >= w;
  // Otherwise u - v = w exactly, or w is not the smallest value.  In either
  // case the following line returns the correct result.
  return (u >= v) ? (u - w >= v) : (v - w >= u);
}

// Given cube face F and a directed line L (represented by its CCW normal N in
// the (u,v,w) coordinates of F), compute the axis of the cube face edge where
// L exits the face: return 0 if L exits through the u=-1 or u=+1 edge, and 1
// if L exits through the v=-1 or v=+1 edge.  Either result is acceptable if L
// exits exactly through a corner vertex of the cube face.
static int GetExitAxis(const S2PointUVW& n) {
  S2_DCHECK(IntersectsFace(n));
  if (IntersectsOppositeEdges(n)) {
    // The line passes through through opposite edges of the face.
    // It exits through the v=+1 or v=-1 edge if the u-component of N has a
    // larger absolute magnitude than the v-component.
    return (fabs(n[0]) >= fabs(n[1])) ? 1 : 0;
  } else {
    // The line passes through through two adjacent edges of the face.
    // It exits the v=+1 or v=-1 edge if an even number of the components of N
    // are negative.  We test this using signbit() rather than multiplication
    // to avoid the possibility of underflow.
    S2_DCHECK(n[0] != 0 && n[1] != 0  && n[2] != 0);
    using std::signbit;
    return ((signbit(n[0]) ^ signbit(n[1]) ^ signbit(n[2])) == 0) ? 1 : 0;
  }
}

// Given a cube face F, a directed line L (represented by its CCW normal N in
// the (u,v,w) coordinates of F), and result of GetExitAxis(N), return the
// (u,v) coordinates of the point where L exits the cube face.
static R2Point GetExitPoint(const S2PointUVW& n, int axis) {
  if (axis == 0) {
    double u = (n[1] > 0) ? 1.0 : -1.0;
    return R2Point(u, (-u * n[0] - n[2]) / n[1]);
  } else {
    double v = (n[0] < 0) ? 1.0 : -1.0;
    return R2Point((-v * n[1] - n[2]) / n[0], v);
  }
}

// Given a line segment AB whose origin A has been projected onto a given cube
// face, determine whether it is necessary to project A onto a different face
// instead.  This can happen because the normal of the line AB is not computed
// exactly, so that the line AB (defined as the set of points perpendicular to
// the normal) may not intersect the cube face containing A.  Even if it does
// intersect the face, the "exit point" of the line from that face may be on
// the wrong side of A (i.e., in the direction away from B).  If this happens,
// we reproject A onto the adjacent face where the line AB approaches A most
// closely.  This moves the origin by a small amount, but never more than the
// error tolerances documented in the header file.
static int MoveOriginToValidFace(int face, const S2Point& a,
                                 const S2Point& ab, R2Point* a_uv) {
  // Fast path: if the origin is sufficiently far inside the face, it is
  // always safe to use it.
  const double kMaxSafeUVCoord = 1 - kFaceClipErrorUVCoord;
  if (max(fabs((*a_uv)[0]), fabs((*a_uv)[1])) <= kMaxSafeUVCoord) {
    return face;
  }
  // Otherwise check whether the normal AB even intersects this face.
  S2PointUVW n = S2::FaceXYZtoUVW(face, ab);
  if (IntersectsFace(n)) {
    // Check whether the point where the line AB exits this face is on the
    // wrong side of A (by more than the acceptable error tolerance).
    S2Point exit = S2::FaceUVtoXYZ(face, GetExitPoint(n, GetExitAxis(n)));
    S2Point a_tangent = ab.Normalize().CrossProd(a);
    if ((exit - a).DotProd(a_tangent) >= -kFaceClipErrorRadians) {
      return face;  // We can use the given face.
    }
  }
  // Otherwise we reproject A to the nearest adjacent face.  (If line AB does
  // not pass through a given face, it must pass through all adjacent faces.)
  if (fabs((*a_uv)[0]) >= fabs((*a_uv)[1])) {
    face = S2::GetUVWFace(face, 0 /*U axis*/, (*a_uv)[0] > 0);
  } else {
    face = S2::GetUVWFace(face, 1 /*V axis*/, (*a_uv)[1] > 0);
  }
  S2_DCHECK(IntersectsFace(S2::FaceXYZtoUVW(face, ab)));
  S2::ValidFaceXYZtoUV(face, a, a_uv);
  (*a_uv)[0] = max(-1.0, min(1.0, (*a_uv)[0]));
  (*a_uv)[1] = max(-1.0, min(1.0, (*a_uv)[1]));
  return face;
}

// Return the next face that should be visited by GetFaceSegments, given that
// we have just visited "face" and we are following the line AB (represented
// by its normal N in the (u,v,w) coordinates of that face).  The other
// arguments include the point where AB exits "face", the corresponding
// exit axis, and the "target face" containing the destination point B.
static int GetNextFace(int face, const R2Point& exit, int axis,
                       const S2PointUVW& n, int target_face) {
  // We return the face that is adjacent to the exit point along the given
  // axis.  If line AB exits *exactly* through a corner of the face, there are
  // two possible next faces.  If one is the "target face" containing B, then
  // we guarantee that we advance to that face directly.
  //
  // The three conditions below check that (1) AB exits approximately through
  // a corner, (2) the adjacent face along the non-exit axis is the target
  // face, and (3) AB exits *exactly* through the corner.  (The SumEquals()
  // code checks whether the dot product of (u,v,1) and "n" is exactly zero.)
  if (fabs(exit[1 - axis]) == 1 &&
      S2::GetUVWFace(face, 1 - axis, exit[1 - axis] > 0) == target_face &&
      SumEquals(exit[0] * n[0], exit[1] * n[1], -n[2])) {
    return target_face;
  }
  // Otherwise return the face that is adjacent to the exit point in the
  // direction of the exit axis.
  return S2::GetUVWFace(face, axis, exit[axis] > 0);
}

void GetFaceSegments(const S2Point& a, const S2Point& b,
                     FaceSegmentVector* segments) {
  S2_DCHECK(S2::IsUnitLength(a));
  S2_DCHECK(S2::IsUnitLength(b));
  segments->clear();

  // Fast path: both endpoints are on the same face.
  FaceSegment segment;
  int a_face = S2::XYZtoFaceUV(a, &segment.a);
  int b_face = S2::XYZtoFaceUV(b, &segment.b);
  if (a_face == b_face) {
    segment.face = a_face;
    segments->push_back(segment);
    return;
  }
  // Starting at A, we follow AB from face to face until we reach the face
  // containing B.  The following code is designed to ensure that we always
  // reach B, even in the presence of numerical errors.
  //
  // First we compute the normal to the plane containing A and B.  This normal
  // becomes the ultimate definition of the line AB; it is used to resolve all
  // questions regarding where exactly the line goes.  Unfortunately due to
  // numerical errors, the line may not quite intersect the faces containing
  // the original endpoints.  We handle this by moving A and/or B slightly if
  // necessary so that they are on faces intersected by the line AB.
  S2Point ab = S2::RobustCrossProd(a, b);
  a_face = MoveOriginToValidFace(a_face, a, ab, &segment.a);
  b_face = MoveOriginToValidFace(b_face, b, -ab, &segment.b);

  // Now we simply follow AB from face to face until we reach B.
  segment.face = a_face;
  R2Point b_saved = segment.b;
  for (int face = a_face; face != b_face; ) {
    // Complete the current segment by finding the point where AB exits the
    // current face.
    S2PointUVW n = S2::FaceXYZtoUVW(face, ab);
    int exit_axis = GetExitAxis(n);
    segment.b = GetExitPoint(n, exit_axis);
    segments->push_back(segment);

    // Compute the next face intersected by AB, and translate the exit point
    // of the current segment into the (u,v) coordinates of the next face.
    // This becomes the first point of the next segment.
    S2Point exit_xyz = S2::FaceUVtoXYZ(face, segment.b);
    face = GetNextFace(face, segment.b, exit_axis, n, b_face);
    S2PointUVW exit_uvw = S2::FaceXYZtoUVW(face, exit_xyz);
    segment.face = face;
    segment.a = R2Point(exit_uvw[0], exit_uvw[1]);
  }
  // Finish the last segment.
  segment.b = b_saved;
  segments->push_back(segment);
}

// This helper function does two things.  First, it clips the line segment AB
// to find the clipped destination B' on a given face.  (The face is specified
// implicitly by expressing *all arguments* in the (u,v,w) coordinates of that
// face.)  Second, it partially computes whether the segment AB intersects
// this face at all.  The actual condition is fairly complicated, but it turns
// out that it can be expressed as a "score" that can be computed
// independently when clipping the two endpoints A and B.  This function
// returns the score for the given endpoint, which is an integer ranging from
// 0 to 3.  If the sum of the two scores is 3 or more, then AB does not
// intersect this face.  See the calling function for the meaning of the
// various parameters.
static int ClipDestination(
    const S2PointUVW& a, const S2PointUVW& b, const S2PointUVW& scaled_n,
    const S2PointUVW& a_tangent, const S2PointUVW& b_tangent, double scale_uv,
    R2Point* uv) {
  S2_DCHECK(IntersectsFace(scaled_n));

  // Optimization: if B is within the safe region of the face, use it.
  const double kMaxSafeUVCoord = 1 - kFaceClipErrorUVCoord;
  if (b[2] > 0) {
    *uv = R2Point(b[0] / b[2], b[1] / b[2]);
    if (max(fabs((*uv)[0]), fabs((*uv)[1])) <= kMaxSafeUVCoord)
      return 0;
  }
  // Otherwise find the point B' where the line AB exits the face.
  *uv = scale_uv * GetExitPoint(scaled_n, GetExitAxis(scaled_n));
  S2PointUVW p((*uv)[0], (*uv)[1], 1.0);

  // Determine if the exit point B' is contained within the segment.  We do this
  // by computing the dot products with two inward-facing tangent vectors at A
  // and B.  If either dot product is negative, we say that B' is on the "wrong
  // side" of that point.  As the point B' moves around the great circle AB past
  // the segment endpoint B, it is initially on the wrong side of B only; as it
  // moves further it is on the wrong side of both endpoints; and then it is on
  // the wrong side of A only.  If the exit point B' is on the wrong side of
  // either endpoint, we can't use it; instead the segment is clipped at the
  // original endpoint B.
  //
  // We reject the segment if the sum of the scores of the two endpoints is 3
  // or more.  Here is what that rule encodes:
  //  - If B' is on the wrong side of A, then the other clipped endpoint A'
  //    must be in the interior of AB (otherwise AB' would go the wrong way
  //    around the circle).  There is a similar rule for A'.
  //  - If B' is on the wrong side of either endpoint (and therefore we must
  //    use the original endpoint B instead), then it must be possible to
  //    project B onto this face (i.e., its w-coordinate must be positive).
  //    This rule is only necessary to handle certain zero-length edges (A=B).
  int score = 0;
  if ((p - a).DotProd(a_tangent) < 0) {
    score = 2;  // B' is on wrong side of A.
  } else if ((p - b).DotProd(b_tangent) < 0) {
    score = 1;  // B' is on wrong side of B.
  }
  if (score > 0) {  // B' is not in the interior of AB.
    if (b[2] <= 0) {
      score = 3;    // B cannot be projected onto this face.
    } else {
      *uv = R2Point(b[0] / b[2], b[1] / b[2]);
    }
  }
  return score;
}

bool ClipToPaddedFace(const S2Point& a_xyz, const S2Point& b_xyz, int face,
                      double padding, R2Point* a_uv, R2Point* b_uv) {
  S2_DCHECK_GE(padding, 0);
  // Fast path: both endpoints are on the given face.
  if (S2::GetFace(a_xyz) == face && S2::GetFace(b_xyz) == face) {
    S2::ValidFaceXYZtoUV(face, a_xyz, a_uv);
    S2::ValidFaceXYZtoUV(face, b_xyz, b_uv);
    return true;
  }
  // Convert everything into the (u,v,w) coordinates of the given face.  Note
  // that the cross product *must* be computed in the original (x,y,z)
  // coordinate system because RobustCrossProd (unlike the mathematical cross
  // product) can produce different results in different coordinate systems
  // when one argument is a linear multiple of the other, due to the use of
  // symbolic perturbations.
  S2PointUVW n = S2::FaceXYZtoUVW(face, S2::RobustCrossProd(a_xyz, b_xyz));
  S2PointUVW a = S2::FaceXYZtoUVW(face, a_xyz);
  S2PointUVW b = S2::FaceXYZtoUVW(face, b_xyz);

  // Padding is handled by scaling the u- and v-components of the normal.
  // Letting R=1+padding, this means that when we compute the dot product of
  // the normal with a cube face vertex (such as (-1,-1,1)), we will actually
  // compute the dot product with the scaled vertex (-R,-R,1).  This allows
  // methods such as IntersectsFace(), GetExitAxis(), etc, to handle padding
  // with no further modifications.
  const double scale_uv = 1 + padding;
  S2PointUVW scaled_n(scale_uv * n[0], scale_uv * n[1], n[2]);
  if (!IntersectsFace(scaled_n)) return false;

  // TODO(ericv): This is a temporary hack until I rewrite S2::RobustCrossProd;
  // it avoids loss of precision in Normalize() when the vector is so small
  // that it underflows.
  if (max(fabs(n[0]), max(fabs(n[1]), fabs(n[2]))) < ldexp(1, -511)) {
    n *= ldexp(1, 563);
  }  // END OF HACK
  n = n.Normalize();
  S2PointUVW a_tangent = n.CrossProd(a);
  S2PointUVW b_tangent = b.CrossProd(n);
  // As described above, if the sum of the scores from clipping the two
  // endpoints is 3 or more, then the segment does not intersect this face.
  int a_score = ClipDestination(b, a, -scaled_n, b_tangent, a_tangent,
                                scale_uv, a_uv);
  int b_score = ClipDestination(a, b, scaled_n, a_tangent, b_tangent,
                                scale_uv, b_uv);
  return a_score + b_score < 3;
}

bool IntersectsRect(const R2Point& a, const R2Point& b, const R2Rect& rect) {
  // First check whether the bound of AB intersects "rect".
  R2Rect bound = R2Rect::FromPointPair(a, b);
  if (!rect.Intersects(bound)) return false;

  // Otherwise AB intersects "rect" if and only if all four vertices of "rect"
  // do not lie on the same side of the extended line AB.  We test this by
  // finding the two vertices of "rect" with minimum and maximum projections
  // onto the normal of AB, and computing their dot products with the edge
  // normal.
  R2Point n = (b - a).Ortho();
  int i = (n[0] >= 0) ? 1 : 0;
  int j = (n[1] >= 0) ? 1 : 0;
  double max = n.DotProd(rect.GetVertex(i, j) - a);
  double min = n.DotProd(rect.GetVertex(1-i, 1-j) - a);
  return (max >= 0) && (min <= 0);
}

inline static bool UpdateEndpoint(R1Interval* bound, int end, double value) {
  if (end == 0) {
    if (bound->hi() < value) return false;
    if (bound->lo() < value) bound->set_lo(value);
  } else {
    if (bound->lo() > value) return false;
    if (bound->hi() > value) bound->set_hi(value);
  }
  return true;
}

// Given a line segment from (a0,a1) to (b0,b1) and a bounding interval for
// each axis, clip the segment further if necessary so that "bound0" does not
// extend outside the given interval "clip".  "diag" is a a precomputed helper
// variable that indicates which diagonal of the bounding box is spanned by AB:
// it is 0 if AB has positive slope, and 1 if AB has negative slope.
inline static bool ClipBoundAxis(double a0, double b0, R1Interval* bound0,
                                 double a1, double b1, R1Interval* bound1,
                                 int diag, const R1Interval& clip0) {
  if (bound0->lo() < clip0.lo()) {
    if (bound0->hi() < clip0.lo()) return false;
    (*bound0)[0] = clip0.lo();
    if (!UpdateEndpoint(bound1, diag,
                        InterpolateDouble(clip0.lo(), a0, b0, a1, b1)))
      return false;
  }
  if (bound0->hi() > clip0.hi()) {
    if (bound0->lo() > clip0.hi()) return false;
    (*bound0)[1] = clip0.hi();
    if (!UpdateEndpoint(bound1, 1-diag,
                        InterpolateDouble(clip0.hi(), a0, b0, a1, b1)))
      return false;
  }
  return true;
}

R2Rect GetClippedEdgeBound(const R2Point& a, const R2Point& b,
                           const R2Rect& clip) {
  R2Rect bound = R2Rect::FromPointPair(a, b);
  if (ClipEdgeBound(a, b, clip, &bound)) return bound;
  return R2Rect::Empty();
}

bool ClipEdgeBound(const R2Point& a, const R2Point& b, const R2Rect& clip,
                   R2Rect* bound) {
  // "diag" indicates which diagonal of the bounding box is spanned by AB: it
  // is 0 if AB has positive slope, and 1 if AB has negative slope.  This is
  // used to determine which interval endpoints need to be updated each time
  // the edge is clipped.
  int diag = (a[0] > b[0]) != (a[1] > b[1]);
  return (ClipBoundAxis(a[0], b[0], &(*bound)[0], a[1], b[1], &(*bound)[1],
                        diag, clip[0]) &&
          ClipBoundAxis(a[1], b[1], &(*bound)[1], a[0], b[0], &(*bound)[0],
                        diag, clip[1]));
}

bool ClipEdge(const R2Point& a, const R2Point& b, const R2Rect& clip,
              R2Point* a_clipped, R2Point* b_clipped) {
  // Compute the bounding rectangle of AB, clip it, and then extract the new
  // endpoints from the clipped bound.
  R2Rect bound = R2Rect::FromPointPair(a, b);
  if (ClipEdgeBound(a, b, clip, &bound)) {
    int ai = (a[0] > b[0]), aj = (a[1] > b[1]);
    *a_clipped = bound.GetVertex(ai, aj);
    *b_clipped = bound.GetVertex(1-ai, 1-aj);
    return true;
  }
  return false;
}

}  // namespace S2
