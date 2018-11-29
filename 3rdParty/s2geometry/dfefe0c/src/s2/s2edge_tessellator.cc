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

#include "s2/s2edge_tessellator.h"

#include <cmath>
#include "s2/s2latlng.h"
#include "s2/s2pointutil.h"

using std::vector;

S1Angle S2EdgeTessellator::kMinTolerance() {
  // This distance is less than 1 micrometer on the Earth's surface, but is
  // still much larger than the expected projection and interpolation errors.
  return S1Angle::Radians(1e-13);
}

S2EdgeTessellator::S2EdgeTessellator(const S2::Projection* projection,
                                     S1Angle tolerance)
    : proj_(*projection),
      tolerance_(std::max(tolerance, kMinTolerance())),
      wrap_distance_(projection->wrap_distance()) {
  if (tolerance < kMinTolerance()) S2_LOG(DFATAL) << "Tolerance too small";
}

void S2EdgeTessellator::AppendProjected(
    const S2Point& a, const S2Point& b, vector<R2Point>* vertices) const {
  R2Point pa = proj_.Project(a);
  if (vertices->empty()) {
    vertices->push_back(pa);
  } else {
    pa = WrapDestination(vertices->back(), pa);
    S2_DCHECK_EQ(vertices->back(), pa) << "Appended edges must form a chain";
  }
  R2Point pb = WrapDestination(pa, proj_.Project(b));
  AppendProjected(pa, a, pb, b, vertices);
}

// Given a geodesic edge AB, split the edge as necessary and append all
// projected vertices except the first to "vertices".
//
// The maximum recursion depth is (M_PI / kMinTolerance()) < 45, and the
// frame size is small so stack overflow should not be an issue.
void S2EdgeTessellator::AppendProjected(const R2Point& pa, const S2Point& a,
                                        const R2Point& pb, const S2Point& b,
                                        vector<R2Point>* vertices) const {
  // It's impossible to robustly test whether a projected edge is close enough
  // to a geodesic edge without knowing the details of the projection
  // function, but the following heuristic works well for a wide range of map
  // projections.  The idea is simply to test whether the midpoint of the
  // projected edge is close enough to the midpoint of the geodesic edge.
  //
  // This measures the distance between the two edges by treating them as
  // parametric curves rather than geometric ones.  The problem with
  // measuring, say, the minimum distance from the projected midpoint to the
  // geodesic edge is that this is a lower bound on the value we want, because
  // the maximum separation between the two curves is generally not attained
  // at the midpoint of the projected edge.  The distance between the curve
  // midpoints is at least an upper bound on the distance from either midpoint
  // to opposite curve.  It's not necessarily an upper bound on the maximum
  // distance between the two curves, but it is a powerful requirement because
  // it demands that the two curves stay parametrically close together.  This
  // turns out to be much more robust with respect for projections with
  // singularities (e.g., the North and South poles in the rectangular and
  // Mercator projections) because the curve parameterization speed changes
  // rapidly near such singularities.
  S2Point mid = (a + b).Normalize();
  S2Point test_mid = proj_.Unproject(proj_.Interpolate(0.5, pa, pb));
  if (S1ChordAngle(mid, test_mid) < tolerance_) {
    vertices->push_back(pb);
  } else {
    R2Point pmid = WrapDestination(pa, proj_.Project(mid));
    AppendProjected(pa, a, pmid, mid, vertices);
    AppendProjected(pmid, mid, pb, b, vertices);
  }
}

void S2EdgeTessellator::AppendUnprojected(
    const R2Point& pa, const R2Point& pb_in, vector<S2Point>* vertices) const {
  R2Point pb = WrapDestination(pa, pb_in);
  S2Point a = proj_.Unproject(pa);
  S2Point b = proj_.Unproject(pb);
  if (vertices->empty()) {
    vertices->push_back(a);
  } else {
    // Note that coordinate wrapping can create a small amount of error.  For
    // example in the edge chain "0:-175, 0:179, 0:-177", the first edge is
    // transformed into "0:-175, 0:-181" while the second is transformed into
    // "0:179, 0:183".  The two coordinate pairs for the middle vertex
    // ("0:-181" and "0:179") may not yield exactly the same S2Point.
    S2_DCHECK(S2::ApproxEquals(vertices->back(), a))
        << "Appended edges must form a chain";
  }
  AppendUnprojected(pa, a, pb, b, vertices);
}

// Like AppendProjected, but interpolates a projected edge and appends the
// corresponding points on the sphere.
void S2EdgeTessellator::AppendUnprojected(const R2Point& pa, const S2Point& a,
                                          const R2Point& pb, const S2Point& b,
                                          vector<S2Point>* vertices) const {
  // See notes above regarding measuring the interpolation error.
  R2Point pmid = proj_.Interpolate(0.5, pa, pb);
  S2Point mid = proj_.Unproject(pmid);
  S2Point test_mid = (a + b).Normalize();
  if (S1ChordAngle(mid, test_mid) < tolerance_) {
    vertices->push_back(b);
  } else {
    AppendUnprojected(pa, a, pmid, mid, vertices);
    AppendUnprojected(pmid, mid, pb, b, vertices);
  }
}

// Wraps the coordinates of the edge destination if necessary to obtain the
// shortest edge.
R2Point S2EdgeTessellator::WrapDestination(const R2Point& pa,
                                           const R2Point& pb) const {
  double x = pb.x(), y = pb.y();
  // The code below ensures that "pb" is unmodified unless wrapping is required.
  if (wrap_distance_.x() > 0 && fabs(x - pa.x()) > 0.5 * wrap_distance_.x()) {
    x = pa.x() + remainder(x - pa.x(), wrap_distance_.x());
  }
  if (wrap_distance_.y() > 0 && fabs(y - pa.y()) > 0.5 * wrap_distance_.y()) {
    y = pa.y() + remainder(y - pa.y(), wrap_distance_.y());
  }
  return R2Point(x, y);
}
