// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "s2/s2polyline_simplifier.h"

#include <cfloat>

#include "s2/s1chord_angle.h"
#include "s2/s1interval.h"

void S2PolylineSimplifier::Init(const S2Point& src) {
  src_ = src;
  window_ = S1Interval::Full();

  // Precompute basis vectors for the tangent space at "src".  This is similar
  // to GetFrame() except that we don't normalize the vectors.  As it turns
  // out, the two basis vectors below have the same magnitude (up to the
  // length error in S2Point::Normalize).

  // Find the index of the component whose magnitude is smallest.
  S2Point tmp = src.Abs();
  int i = (tmp[0] < tmp[1] ?
           (tmp[0] < tmp[2] ? 0 : 2) : (tmp[1] < tmp[2] ? 1 : 2));

  // We define the "y" basis vector as the cross product of "src" and the
  // basis vector for axis "i".  Let "j" and "k" be the indices of the other
  // two components in cyclic order.
  int j = (i == 2 ? 0 : i + 1), k = (i == 0 ? 2 : i - 1);
  y_dir_[i] = 0;
  y_dir_[j] = src[k];
  y_dir_[k] = -src[j];

  // Compute the cross product of "y_dir" and "src".  We write out the cross
  // product here mainly for documentation purposes; it also happens to save a
  // few multiplies because unfortunately the optimizer does *not* get rid of
  // multiplies by zero (since these multiplies propagate NaN, for example).
  x_dir_[i] = src[j] * src[j] + src[k] * src[k];
  x_dir_[j] = -src[j] * src[i];
  x_dir_[k] = -src[k] * src[i];
}

bool S2PolylineSimplifier::Extend(const S2Point& dst) const {
  // We limit the maximum edge length to 90 degrees in order to simplify the
  // error bounds.  (The error gets arbitrarily large as the edge length
  // approaches 180 degrees.)
  if (S1ChordAngle(src_, dst) > S1ChordAngle::Right()) return false;

  // Otherwise check whether this vertex is in the acceptable angle range.
  return window_.Contains(GetAngle(dst));
}

bool S2PolylineSimplifier::TargetDisc(const S2Point& p, S1ChordAngle r) {
  // Shrink the target interval by the maximum error from all sources.  This
  // guarantees that the output edge will intersect the given disc.
  double semiwidth = GetSemiwidth(p, r, -1 /*round down*/);
  if (semiwidth >= M_PI) {
    // The target disc contains "src", so there is nothing to do.
    return true;
  }
  if (semiwidth < 0) {
    window_ = S1Interval::Empty();
    return false;
  }
  // Otherwise compute the angle interval corresponding to the target disc and
  // intersect it with the current window.
  double center = GetAngle(p);
  S1Interval target = S1Interval::FromPoint(center).Expanded(semiwidth);
  window_ = window_.Intersection(target);
  return !window_.is_empty();
}

bool S2PolylineSimplifier::AvoidDisc(const S2Point& p, S1ChordAngle r,
                                     bool disc_on_left) {
  // Expand the interval by the maximum error from all sources.  This
  // guarantees that the final output edge will avoid the given disc.
  double semiwidth = GetSemiwidth(p, r, 1 /*round up*/);
  if (semiwidth >= M_PI) {
    // The avoidance disc contains "src", so it is impossible to avoid.
    window_ = S1Interval::Empty();
    return false;
  }
  double center = GetAngle(p);
  double opposite = (center > 0) ? center - M_PI : center + M_PI;
  S1Interval target = (disc_on_left ? S1Interval(opposite, center) :
                       S1Interval(center, opposite));
  window_ = window_.Intersection(target.Expanded(-semiwidth));
  return !window_.is_empty();
}

double S2PolylineSimplifier::GetAngle(const S2Point& p) const {
  return atan2(p.DotProd(y_dir_), p.DotProd(x_dir_));
}

double S2PolylineSimplifier::GetSemiwidth(const S2Point& p, S1ChordAngle r,
                                          int round_direction) const {
  double constexpr DBL_ERR = 0.5 * DBL_EPSILON;

  // Using spherical trigonometry,
  //
  //   sin(semiwidth) = sin(r) / sin(a)
  //
  // where "a" is the angle between "src" and "p".  Rather than measuring
  // these angles, instead we measure the squared chord lengths through the
  // interior of the sphere (i.e., Cartersian distance).  Letting "r2" be the
  // squared chord distance corresponding to "r", and "a2" be the squared
  // chord distance corresponding to "a", we use the relationships
  //
  //    sin^2(r) = r2 (1 - r2 / 4)
  //    sin^2(a) = d2 (1 - d2 / 4)
  //
  // which follow from the fact that r2 = (2 * sin(r / 2)) ^ 2, etc.

  // "a2" has a relative error up to 5 * DBL_ERR, plus an absolute error of up
  // to 64 * DBL_ERR^2 (because "src" and "p" may differ from unit length by
  // up to 4 * DBL_ERR).  We can correct for the relative error later, but for
  // the absolute error we use "round_direction" to account for it now.
  double r2 = r.length2();
  double a2 = S1ChordAngle(src_, p).length2();
  a2 -= 64 * DBL_ERR * DBL_ERR * round_direction;
  if (a2 <= r2) return M_PI;  // The given disc contains "src".

  double sin2_r = r2 * (1 - 0.25 * r2);
  double sin2_a = a2 * (1 - 0.25 * a2);
  double semiwidth = asin(sqrt(sin2_r / sin2_a));

  // We compute bounds on the errors from all sources:
  //
  //   - The call to GetSemiwidth (this call).
  //   - The call to GetAngle that computes the center of the interval.
  //   - The call to GetAngle in Extend that tests whether a given point
  //     is an acceptable destination vertex.
  //
  // Summary of the errors in GetAngle:
  //
  // y_dir_ has no error.
  //
  // x_dir_ has a relative error of DBL_ERR in two components, a relative
  // error of 2 * DBL_ERR in the other component, plus an overall relative
  // length error of 4 * DBL_ERR (compared to y_dir_) because "src" is assumed
  // to be normalized only to within the tolerances of S2Point::Normalize().
  //
  // p.DotProd(y_dir_) has a relative error of 1.5 * DBL_ERR and an
  // absolute error of 1.5 * DBL_ERR * y_dir_.Norm().
  //
  // p.DotProd(x_dir_) has a relative error of 5.5 * DBL_ERR and an absolute
  // error of 3.5 * DBL_ERR * y_dir_.Norm() (noting that x_dir_ and y_dir_
  // have the same length to within a relative error of 4 * DBL_ERR).
  //
  // It's possible to show by taking derivatives that these errors can affect
  // the angle atan2(y, x) by up 7.093 * DBL_ERR radians.  Rounding up and
  // including the call to atan2 gives a final error bound of 10 * DBL_ERR.
  //
  // Summary of the errors in GetSemiwidth:
  //
  // The distance a2 has a relative error of 5 * DBL_ERR plus an absolute
  // error of 64 * DBL_ERR^2 because the points "src" and "p" may differ from
  // unit length (by up to 4 * DBL_ERR).  We have already accounted for the
  // absolute error above, leaving only the relative error.
  //
  // sin2_r has a relative error of 2 * DBL_ERR.
  //
  // sin2_a has a relative error of 12 * DBL_ERR assuming that a2 <= 2,
  // i.e. distance(src, p) <= 90 degrees.  (The relative error gets
  // arbitrarily larger as this distance approaches 180 degrees.)
  //
  // semiwidth has a relative error of 17 * DBL_ERR.
  //
  // Finally, (center +/- semiwidth) has a rounding error of up to 4 * DBL_ERR
  // because in theory, the result magnitude may be as large as 1.5 * M_PI
  // which is larger than 4.0.  This gives a total error of:
  double error = (2 * 10 + 4) * DBL_ERR + 17 * DBL_ERR * semiwidth;
  return semiwidth + round_direction * error;
}
