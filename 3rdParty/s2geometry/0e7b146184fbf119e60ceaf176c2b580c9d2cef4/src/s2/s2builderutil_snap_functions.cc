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

#include "s2/s2builderutil_snap_functions.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s2cell_id.h"
#include "s2/s2latlng.h"
#include "s2/s2metrics.h"

using absl::make_unique;
using std::max;
using std::min;
using std::unique_ptr;

namespace s2builderutil {

const int IntLatLngSnapFunction::kMinExponent;
const int IntLatLngSnapFunction::kMaxExponent;

IdentitySnapFunction::IdentitySnapFunction()
    : snap_radius_(S1Angle::Zero()) {
}

IdentitySnapFunction::IdentitySnapFunction(S1Angle snap_radius) {
  set_snap_radius(snap_radius);
}

void IdentitySnapFunction::set_snap_radius(S1Angle snap_radius) {
  S2_DCHECK_LE(snap_radius, kMaxSnapRadius());
  snap_radius_ = snap_radius;
}

S1Angle IdentitySnapFunction::snap_radius() const {
  return snap_radius_;
}

S1Angle IdentitySnapFunction::min_vertex_separation() const {
  // Since SnapFunction does not move the input point, output vertices are
  // separated by the full snap_radius().
  return snap_radius_;
}

S1Angle IdentitySnapFunction::min_edge_vertex_separation() const {
  // In the worst case configuration, the edge separation is half of the
  // vertex separation.
  return 0.5 * snap_radius_;
}

S2Point IdentitySnapFunction::SnapPoint(const S2Point& point) const {
  return point;
}

unique_ptr<S2Builder::SnapFunction> IdentitySnapFunction::Clone() const {
  return make_unique<IdentitySnapFunction>(*this);
}


S2CellIdSnapFunction::S2CellIdSnapFunction() {
  set_level(S2CellId::kMaxLevel);
}

S2CellIdSnapFunction::S2CellIdSnapFunction(int level) {
  set_level(level);
}

void S2CellIdSnapFunction::set_level(int level) {
  S2_DCHECK_GE(level, 0);
  S2_DCHECK_LE(level, S2CellId::kMaxLevel);
  level_ = level;
  set_snap_radius(MinSnapRadiusForLevel(level));
}

int S2CellIdSnapFunction::level() const {
  return level_;
}

void S2CellIdSnapFunction::set_snap_radius(S1Angle snap_radius) {
  S2_DCHECK_GE(snap_radius, MinSnapRadiusForLevel(level()));
  S2_DCHECK_LE(snap_radius, kMaxSnapRadius());
  snap_radius_ = snap_radius;
}

S1Angle S2CellIdSnapFunction::snap_radius() const {
  return snap_radius_;
}

S1Angle S2CellIdSnapFunction::MinSnapRadiusForLevel(int level) {
  // snap_radius() needs to be an upper bound on the true distance that a
  // point can move when snapped, taking into account numerical errors.
  //
  // The maximum error when converting from an S2Point to an S2CellId is
  // S2::kMaxDiag.deriv() * DBL_EPSILON.  The maximum error when converting an
  // S2CellId center back to an S2Point is 1.5 * DBL_EPSILON.  These add up to
  // just slightly less than 4 * DBL_EPSILON.
  return S1Angle::Radians(0.5 * S2::kMaxDiag.GetValue(level) + 4 * DBL_EPSILON);
}

int S2CellIdSnapFunction::LevelForMaxSnapRadius(S1Angle snap_radius) {
  // When choosing a level, we need to acount for the error bound of
  // 4 * DBL_EPSILON that is added by MinSnapRadiusForLevel().
  return S2::kMaxDiag.GetLevelForMaxValue(
      2 * (snap_radius.radians() - 4 * DBL_EPSILON));
}

S1Angle S2CellIdSnapFunction::min_vertex_separation() const {
  // We have three different bounds for the minimum vertex separation: one is
  // a constant bound, one is proportional to snap_radius, and one is equal to
  // snap_radius minus a constant.  These bounds give the best results for
  // small, medium, and large snap radii respectively.  We return the maximum
  // of the three bounds.
  //
  // 1. Constant bound: Vertices are always separated by at least
  //    kMinEdge(level), the minimum edge length for the chosen snap level.
  //
  // 2. Proportional bound: It can be shown that in the plane, the worst-case
  //    configuration has a vertex separation of 2 / sqrt(13) * snap_radius.
  //    This is verified in the unit test, except that on the sphere the ratio
  //    is slightly smaller at cell level 2 (0.54849 vs. 0.55470).  We reduce
  //    that value a bit more below to be conservative.
  //
  // 3. Best asymptotic bound: This bound bound is derived by observing we
  //    only select a new site when it is at least snap_radius() away from all
  //    existing sites, and the site can move by at most 0.5 * kMaxDiag(level)
  //    when snapped.
  S1Angle min_edge = S1Angle::Radians(S2::kMinEdge.GetValue(level_));
  S1Angle max_diag = S1Angle::Radians(S2::kMaxDiag.GetValue(level_));
  return max(min_edge,
             max(0.548 * snap_radius_,  // 2 / sqrt(13) in the plane
                 snap_radius_ - 0.5 * max_diag));
}

S1Angle S2CellIdSnapFunction::min_edge_vertex_separation() const {
  // Similar to min_vertex_separation(), in this case we have four bounds: a
  // constant bound that holds only at the minimum snap radius, a constant
  // bound that holds for any snap radius, a bound that is proportional to
  // snap_radius, and a bound that is equal to snap_radius minus a constant.
  //
  // 1. Constant bounds:
  //
  //    (a) At the minimum snap radius for a given level, it can be shown that
  //    vertices are separated from edges by at least 0.5 * kMinDiag(level) in
  //    the plane.  The unit test verifies this, except that on the sphere the
  //    worst case is slightly better: 0.5652980068 * kMinDiag(level).
  //
  //    (b) Otherwise, for arbitrary snap radii the worst-case configuration
  //    in the plane has an edge-vertex separation of sqrt(3/19) *
  //    kMinDiag(level), where sqrt(3/19) is about 0.3973597071.  The unit
  //    test verifies that the bound is slighty better on the sphere:
  //    0.3973595687 * kMinDiag(level).
  //
  // 2. Proportional bound: In the plane, the worst-case configuration has an
  //    edge-vertex separation of 2 * sqrt(3/247) * snap_radius, which is
  //    about 0.2204155075.  The unit test verifies this, except that on the
  //    sphere the bound is slightly worse for certain large S2Cells: the
  //    minimum ratio occurs at cell level 6, and is about 0.2196666953.
  //
  // 3. Best asymptotic bound: If snap_radius() is large compared to the
  //    minimum snap radius, then the best bound is achieved by 3 sites on a
  //    circular arc of radius "snap_radius", spaced "min_vertex_separation"
  //    apart.  An input edge passing just to one side of the center of the
  //    circle intersects the Voronoi regions of the two end sites but not the
  //    Voronoi region of the center site, and gives an edge separation of
  //    (min_vertex_separation ** 2) / (2 * snap_radius).  This bound
  //    approaches 0.5 * snap_radius for large snap radii, i.e.  the minimum
  //    edge-vertex separation approaches half of the minimum vertex
  //    separation as the snap radius becomes large compared to the cell size.

  S1Angle min_diag = S1Angle::Radians(S2::kMinDiag.GetValue(level_));
  if (snap_radius() == MinSnapRadiusForLevel(level_)) {
    // This bound only holds when the minimum snap radius is being used.
    return 0.565 * min_diag;            // 0.500 in the plane
  }
  // Otherwise, these bounds hold for any snap_radius().
  S1Angle vertex_sep = min_vertex_separation();
  return max(0.397 * min_diag,          // sqrt(3 / 19) in the plane
             max(0.219 * snap_radius_,  // 2 * sqrt(3 / 247) in the plane
                 0.5 * (vertex_sep / snap_radius_) * vertex_sep));
}

S2Point S2CellIdSnapFunction::SnapPoint(const S2Point& point) const {
  return S2CellId(point).parent(level_).ToPoint();
}

unique_ptr<S2Builder::SnapFunction> S2CellIdSnapFunction::Clone() const {
  return make_unique<S2CellIdSnapFunction>(*this);
}

IntLatLngSnapFunction::IntLatLngSnapFunction()
    : exponent_(-1), snap_radius_(), from_degrees_(0), to_degrees_(0) {
}

IntLatLngSnapFunction::IntLatLngSnapFunction(int exponent) {
  set_exponent(exponent);
}

void IntLatLngSnapFunction::set_exponent(int exponent) {
  S2_DCHECK_GE(exponent, kMinExponent);
  S2_DCHECK_LE(exponent, kMaxExponent);
  exponent_ = exponent;
  set_snap_radius(MinSnapRadiusForExponent(exponent));

  // Precompute the scale factors needed for snapping.  Note that these
  // calculations need to exactly match the ones in s1angle.h to ensure
  // that the same S2Points are generated.
  double power = 1;
  for (int i = 0; i < exponent; ++i) power *= 10;
  from_degrees_ = power;
  to_degrees_ = 1 / power;
}

int IntLatLngSnapFunction::exponent() const {
  return exponent_;
}

void IntLatLngSnapFunction::set_snap_radius(S1Angle snap_radius) {
  S2_DCHECK_GE(snap_radius, MinSnapRadiusForExponent(exponent()));
  S2_DCHECK_LE(snap_radius, kMaxSnapRadius());
  snap_radius_ = snap_radius;
}

S1Angle IntLatLngSnapFunction::snap_radius() const {
  return snap_radius_;
}

S1Angle IntLatLngSnapFunction::MinSnapRadiusForExponent(int exponent) {
  // snap_radius() needs to be an upper bound on the true distance that a
  // point can move when snapped, taking into account numerical errors.
  //
  // The maximum errors in latitude and longitude can be bounded as
  // follows (as absolute errors in terms of DBL_EPSILON):
  //
  //                                      Latitude      Longitude
  // Convert to S2LatLng:                    1.000          1.000
  // Convert to degrees:                     1.032          2.063
  // Scale by 10**exp:                       0.786          1.571
  // Round to integer: 0.5 * S1Angle::Degrees(to_degrees_)
  // Scale by 10**(-exp):                    1.375          2.749
  // Convert to radians:                     1.252          1.503
  // ------------------------------------------------------------
  // Total (except for rounding)             5.445          8.886
  //
  // The maximum error when converting the S2LatLng back to an S2Point is
  //
  //   sqrt(2) * (maximum error in latitude or longitude) + 1.5 * DBL_EPSILON
  //
  // which works out to (9 * sqrt(2) + 1.5) * DBL_EPSILON radians.  Finally
  // we need to consider the effect of rounding to integer coordinates
  // (much larger than the errors above), which can change the position by
  // up to (sqrt(2) * 0.5 * to_degrees_) radians.
  double power = 1;
  for (int i = 0; i < exponent; ++i) power *= 10;
  return (S1Angle::Degrees(M_SQRT1_2 / power) +
          S1Angle::Radians((9 * M_SQRT2 + 1.5) * DBL_EPSILON));
}

int IntLatLngSnapFunction::ExponentForMaxSnapRadius(S1Angle snap_radius) {
  // When choosing an exponent, we need to acount for the error bound of
  // (9 * sqrt(2) + 1.5) * DBL_EPSILON added by MinSnapRadiusForExponent().
  snap_radius -= S1Angle::Radians((9 * M_SQRT2 + 1.5) * DBL_EPSILON);
  snap_radius = max(snap_radius, S1Angle::Radians(1e-30));
  double exponent = log10(M_SQRT1_2 / snap_radius.degrees());

  // There can be small errors in the calculation above, so to ensure that
  // this function is the inverse of MinSnapRadiusForExponent() we subtract a
  // small error tolerance.
  return max(kMinExponent,
             min(kMaxExponent,
                 static_cast<int>(std::ceil(exponent - 2 * DBL_EPSILON))));
}

S1Angle IntLatLngSnapFunction::min_vertex_separation() const {
  // We have two bounds for the minimum vertex separation: one is proportional
  // to snap_radius, and one is equal to snap_radius minus a constant.  These
  // bounds give the best results for small and large snap radii respectively.
  // We return the maximum of the two bounds.
  //
  // 1. Proportional bound: It can be shown that in the plane, the worst-case
  //    configuration has a vertex separation of (sqrt(2) / 3) * snap_radius.
  //    This is verified in the unit test, except that on the sphere the ratio
  //    is slightly smaller (0.471337 vs. 0.471404).  We reduce that value a
  //    bit more below to be conservative.
  //
  // 2. Best asymptotic bound: This bound bound is derived by observing we
  //    only select a new site when it is at least snap_radius() away from all
  //    existing sites, and snapping a vertex can move it by up to
  //    ((1 / sqrt(2)) * to_degrees_) degrees.
  return max(0.471 * snap_radius_,        // sqrt(2) / 3 in the plane
             snap_radius_ - S1Angle::Degrees(M_SQRT1_2 * to_degrees_));
}

S1Angle IntLatLngSnapFunction::min_edge_vertex_separation() const {
  // Similar to min_vertex_separation(), in this case we have three bounds:
  // one is a constant bound, one is proportional to snap_radius, and one is
  // equal to snap_radius minus a constant.
  //
  // 1. Constant bound: In the plane, the worst-case configuration has an
  //    edge-vertex separation of ((1 / sqrt(13)) * to_degrees_) degrees.
  //    The unit test verifies this, except that on the sphere the ratio is
  //    slightly lower when small exponents such as E1 are used
  //    (0.2772589 vs 0.2773501).
  //
  // 2. Proportional bound: In the plane, the worst-case configuration has an
  //    edge-vertex separation of (2 / 9) * snap_radius (0.222222222222).  The
  //    unit test verifies this, except that on the sphere the bound can be
  //    slightly worse with large exponents (e.g., E9) due to small numerical
  //    errors (0.222222126756717).
  //
  // 3. Best asymptotic bound: If snap_radius() is large compared to the
  //    minimum snap radius, then the best bound is achieved by 3 sites on a
  //    circular arc of radius "snap_radius", spaced "min_vertex_separation"
  //    apart (see S2CellIdSnapFunction::min_edge_vertex_separation).  This
  //    bound approaches 0.5 * snap_radius as the snap radius becomes large
  //    relative to the grid spacing.

  S1Angle vertex_sep = min_vertex_separation();
  return max(0.277 * S1Angle::Degrees(to_degrees_),  // 1/sqrt(13) in the plane
             max(0.222 * snap_radius_,               // 2/9 in the plane
                 0.5 * (vertex_sep / snap_radius_) * vertex_sep));
}

S2Point IntLatLngSnapFunction::SnapPoint(const S2Point& point) const {
  S2_DCHECK_GE(exponent_, 0);  // Make sure the snap function was initialized.
  S2LatLng input(point);
  int64 lat = MathUtil::FastInt64Round(input.lat().degrees() * from_degrees_);
  int64 lng = MathUtil::FastInt64Round(input.lng().degrees() * from_degrees_);
  return S2LatLng::FromDegrees(lat * to_degrees_, lng * to_degrees_).ToPoint();
}

unique_ptr<S2Builder::SnapFunction> IntLatLngSnapFunction::Clone() const {
  return make_unique<IntLatLngSnapFunction>(*this);
}

}  // namespace s2builderutil
