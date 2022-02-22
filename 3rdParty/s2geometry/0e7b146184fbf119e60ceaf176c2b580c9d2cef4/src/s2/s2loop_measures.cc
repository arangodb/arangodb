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

#include <cfloat>
#include <cmath>
#include <vector>
#include "s2/base/logging.h"
#include "s2/third_party/absl/container/inlined_vector.h"
#include "s2/s1angle.h"
#include "s2/s2centroids.h"
#include "s2/s2edge_distances.h"
#include "s2/s2measures.h"
#include "s2/s2pointutil.h"

using std::fabs;
using std::max;
using std::min;
using std::vector;

namespace S2 {

S1Angle GetPerimeter(S2PointLoopSpan loop) {
  S1Angle perimeter = S1Angle::Zero();
  if (loop.size() <= 1) return perimeter;
  for (int i = 0; i < loop.size(); ++i) {
    perimeter += S1Angle(loop[i], loop[i + 1]);
  }
  return perimeter;
}

double GetArea(S2PointLoopSpan loop) {
  double area = GetSignedArea(loop);
  S2_DCHECK_LE(fabs(area), 2 * M_PI);
  if (area < 0.0) area += 4 * M_PI;
  return area;
}

double GetSignedArea(S2PointLoopSpan loop) {
  // It is suprisingly difficult to compute the area of a loop robustly.  The
  // main issues are (1) whether degenerate loops are considered to be CCW or
  // not (i.e., whether their area is close to 0 or 4*Pi), and (2) computing
  // the areas of small loops with good relative accuracy.
  //
  // With respect to degeneracies, we would like GetArea() to be consistent
  // with S2Loop::Contains(S2Point) in that loops that contain many points
  // should have large areas, and loops that contain few points should have
  // small areas.  For example, if a degenerate triangle is considered CCW
  // according to s2pred::Sign(), then it will contain very few points and
  // its area should be approximately zero.  On the other hand if it is
  // considered clockwise, then it will contain virtually all points and so
  // its area should be approximately 4*Pi.
  //
  // More precisely, let U be the set of S2Points for which S2::IsUnitLength()
  // is true, let P(U) be the projection of those points onto the mathematical
  // unit sphere, and let V(P(U)) be the Voronoi diagram of the projected
  // points.  Then for every loop x, we would like GetArea() to approximately
  // equal the sum of the areas of the Voronoi regions of the points p for
  // which x.Contains(p) is true.
  //
  // The second issue is that we want to compute the area of small loops
  // accurately.  This requires having good relative precision rather than
  // good absolute precision.  For example, if the area of a loop is 1e-12 and
  // the error is 1e-15, then the area only has 3 digits of accuracy.  (For
  // reference, 1e-12 is about 40 square meters on the surface of the earth.)
  // We would like to have good relative accuracy even for small loops.
  //
  // To achieve these goals, we combine two different methods of computing the
  // area.  This first method is based on the Gauss-Bonnet theorem, which says
  // that the area enclosed by the loop equals 2*Pi minus the total geodesic
  // curvature of the loop (i.e., the sum of the "turning angles" at all the
  // loop vertices).  The big advantage of this method is that as long as we
  // use s2pred::Sign() to compute the turning angle at each vertex, then
  // degeneracies are always handled correctly.  In other words, if a
  // degenerate loop is CCW according to the symbolic perturbations used by
  // s2pred::Sign(), then its turning angle will be approximately 2*Pi.
  //
  // The disadvantage of the Gauss-Bonnet method is that its absolute error is
  // about 2e-15 times the number of vertices (see GetCurvatureMaxError).
  // So, it cannot compute the area of small loops accurately.
  //
  // The second method is based on splitting the loop into triangles and
  // summing the area of each triangle.  To avoid the difficulty and expense
  // of decomposing the loop into a union of non-overlapping triangles,
  // instead we compute a signed sum over triangles that may overlap (see the
  // comments for S2Loop::GetSurfaceIntegral).  The advantage of this method
  // is that the area of each triangle can be computed with much better
  // relative accuracy (using l'Huilier's theorem).  The disadvantage is that
  // the result is a signed area: CCW loops may yield a small positive value,
  // while CW loops may yield a small negative value (which is converted to a
  // positive area by adding 4*Pi).  This means that small errors in computing
  // the signed area may translate into a very large error in the result (if
  // the sign of the sum is incorrect).
  //
  // So, our strategy is to combine these two methods as follows.  First we
  // compute the area using the "signed sum over triangles" approach (since it
  // is generally more accurate).  We also estimate the maximum error in this
  // result.  If the signed area is too close to zero (i.e., zero is within
  // the error bounds), then we double-check the sign of the result using the
  // Gauss-Bonnet method.  If the two methods disagree, we return the smallest
  // possible positive or negative area based on the result of GetCurvature().
  // Otherwise we return the area that we computed originally.

  // The signed area should be between approximately -4*Pi and 4*Pi.
  // Normalize it to be in the range [-2*Pi, 2*Pi].
  double area = GetSurfaceIntegral(loop, S2::SignedArea);
  double max_error = GetCurvatureMaxError(loop);
  S2_DCHECK_LE(fabs(area), 4 * M_PI + max_error);
  area = remainder(area, 4 * M_PI);

  // If the area is a small negative or positive number, verify that the sign
  // of the result is consistent with the loop orientation.
  if (fabs(area) <= max_error) {
    double curvature = GetCurvature(loop);
    // Zero-area loops should have a curvature of approximately +/- 2*Pi.
    S2_DCHECK(!(area == 0 && curvature == 0));
    if (curvature == 2 * M_PI) return 0.0;  // Degenerate
    if (area <= 0 && curvature > 0) {
      return std::numeric_limits<double>::min();
    }
    // Full loops are handled by the case below.
    if (area >= 0 && curvature < 0) {
      return -std::numeric_limits<double>::min();
    }
  }
  return area;
}

double GetApproxArea(S2PointLoopSpan loop) {
  return 2 * M_PI - GetCurvature(loop);
}

S2PointLoopSpan PruneDegeneracies(S2PointLoopSpan loop,
                                  vector<S2Point>* new_vertices) {
  vector<S2Point>& vertices = *new_vertices;
  vertices.clear();
  vertices.reserve(loop.size());
  for (const S2Point& v : loop) {
    // Remove duplicate vertices.
    if (vertices.empty() || v != vertices.back()) {
      // Remove edge pairs of the form ABA.
      if (vertices.size() >= 2 && v == vertices.end()[-2]) {
        vertices.pop_back();
      } else {
        vertices.push_back(v);
      }
    }
  }
  // Check whether the loop was completely degenerate.
  if (vertices.size() < 3) return S2PointLoopSpan();

  // Otherwise some portion of the loop is guaranteed to be non-degenerate.
  // However there may still be some degenerate portions to remove.
  if (vertices[0] == vertices.back()) vertices.pop_back();

  // If the loop begins with BA and ends with A, then there is an edge pair of
  // the form ABA at the end/start of the loop.  Remove all such pairs.  As
  // noted above, this is guaranteed to leave a non-degenerate loop.
  int k = 0;
  while (vertices[k + 1] == vertices.end()[-(k + 1)]) ++k;
  return S2PointLoopSpan(vertices.data() + k, vertices.size() - 2 * k);
}

double GetCurvature(S2PointLoopSpan loop) {
  // By convention, a loop with no vertices contains all points on the sphere.
  if (loop.empty()) return -2 * M_PI;

  // Remove any degeneracies from the loop.
  vector<S2Point> vertices;
  loop = PruneDegeneracies(loop, &vertices);

  // If the entire loop was degenerate, it's turning angle is defined as 2*Pi.
  if (loop.empty()) return 2 * M_PI;

  // To ensure that we get the same result when the vertex order is rotated,
  // and that the result is negated when the vertex order is reversed, we need
  // to add up the individual turn angles in a consistent order.  (In general,
  // adding up a set of numbers in a different order can change the sum due to
  // rounding errors.)
  //
  // Furthermore, if we just accumulate an ordinary sum then the worst-case
  // error is quadratic in the number of vertices.  (This can happen with
  // spiral shapes, where the partial sum of the turning angles can be linear
  // in the number of vertices.)  To avoid this we use the Kahan summation
  // algorithm (http://en.wikipedia.org/wiki/Kahan_summation_algorithm).
  LoopOrder order = GetCanonicalLoopOrder(loop);
  int i = order.first, dir = order.dir, n = loop.size();
  double sum = S2::TurnAngle(loop[(i + n - dir) % n], loop[i],
                             loop[(i + dir) % n]);
  double compensation = 0;  // Kahan summation algorithm
  while (--n > 0) {
    i += dir;
    double angle = S2::TurnAngle(loop[i - dir], loop[i], loop[i + dir]);
    double old_sum = sum;
    angle += compensation;
    sum += angle;
    compensation = (old_sum - sum) + angle;
  }
  constexpr double kMaxCurvature = 2 * M_PI - 4 * DBL_EPSILON;
  sum += compensation;
  return max(-kMaxCurvature, min(kMaxCurvature, dir * sum));
}

double GetCurvatureMaxError(S2PointLoopSpan loop) {
  // The maximum error can be bounded as follows:
  //   2.24 * DBL_EPSILON    for RobustCrossProd(b, a)
  //   2.24 * DBL_EPSILON    for RobustCrossProd(c, b)
  //   3.25 * DBL_EPSILON    for Angle()
  //   2.00 * DBL_EPSILON    for each addition in the Kahan summation
  //   ------------------
  //   9.73 * DBL_EPSILON
  //
  // TODO(ericv): This error estimate is approximate.  There are two issues:
  // (1) SignedArea needs some improvements to ensure that its error is
  // actually never higher than GirardArea, and (2) although the number of
  // triangles in the sum is typically N-2, in theory it could be as high as
  // 2*N for pathological inputs.  But in other respects this error bound is
  // very conservative since it assumes that the maximum error is achieved on
  // every triangle.
  const double kMaxErrorPerVertex = 9.73 * DBL_EPSILON;
  return kMaxErrorPerVertex * loop.size();
}

S2Point GetCentroid(S2PointLoopSpan loop) {
  // GetSurfaceIntegral() returns either the integral of position over loop
  // interior, or the negative of the integral of position over the loop
  // exterior.  But these two values are the same (!), because the integral of
  // position over the entire sphere is (0, 0, 0).
  return GetSurfaceIntegral(loop, S2::TrueCentroid);
}

static inline bool IsOrderLess(LoopOrder order1, LoopOrder order2,
                               S2PointLoopSpan loop) {
  if (order1 == order2) return false;

  int i1 = order1.first, i2 = order2.first;
  int dir1 = order1.dir, dir2 = order2.dir;
  S2_DCHECK_EQ(loop[i1], loop[i2]);
  for (int n = loop.size(); --n > 0; ) {
    i1 += dir1;
    i2 += dir2;
    if (loop[i1] < loop[i2]) return true;
    if (loop[i1] > loop[i2]) return false;
  }
  return false;
}

LoopOrder GetCanonicalLoopOrder(S2PointLoopSpan loop) {
  // In order to handle loops with duplicate vertices and/or degeneracies, we
  // return the LoopOrder that minimizes the entire corresponding vertex
  // *sequence*.  For example, suppose that vertices are sorted
  // alphabetically, and consider the loop CADBAB.  The canonical loop order
  // would be (4, 1), corresponding to the vertex sequence ABCADB.  (For
  // comparison, loop order (4, -1) yields the sequence ABDACB.)
  //
  // If two or more loop orders yield identical minimal vertex sequences, then
  // it doesn't matter which one we return (since they yield the same result).

  // For efficiency, we divide the process into two steps.  First we find the
  // smallest vertex, and the set of vertex indices where that vertex occurs
  // (noting that the loop may contain duplicate vertices).  Then we consider
  // both possible directions starting from each such vertex index, and return
  // the LoopOrder corresponding to the smallest vertex sequence.
  int n = loop.size();
  if (n == 0) return LoopOrder(0, 1);

  absl::InlinedVector<int, 4> min_indices;
  min_indices.push_back(0);
  for (int i = 1; i < n; ++i) {
    if (loop[i] <= loop[min_indices[0]]) {
      if (loop[i] < loop[min_indices[0]]) min_indices.clear();
      min_indices.push_back(i);
    }
  }
  LoopOrder min_order(min_indices[0], 1);
  for (int min_index : min_indices) {
    LoopOrder order1(min_index, 1);
    LoopOrder order2(min_index + n, -1);
    if (IsOrderLess(order1, min_order, loop)) min_order = order1;
    if (IsOrderLess(order2, min_order, loop)) min_order = order2;
  }
  return min_order;
}

bool IsNormalized(S2PointLoopSpan loop) {
  // We allow some error so that hemispheres are always considered normalized.
  //
  // TODO(ericv): This is no longer required by the S2Polygon implementation,
  // so alternatively we could create the invariant that a loop is normalized
  // if and only if its complement is not normalized.
  return GetCurvature(loop) >= -GetCurvatureMaxError(loop);
}

std::ostream& operator<<(std::ostream& os, LoopOrder order) {
  return os << "(" << order.first << ", " << order.dir << ")";
}

}  // namespace S2
