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

#ifndef S2_S2BUILDERUTIL_SNAP_FUNCTIONS_H_
#define S2_S2BUILDERUTIL_SNAP_FUNCTIONS_H_

#include <memory>
#include "s2/s1angle.h"
#include "s2/s2builder.h"
#include "s2/s2cell_id.h"

namespace s2builderutil {

// A SnapFunction that snaps every vertex to itself.  It should be used when
// vertices do not need to be snapped to a discrete set of locations (such as
// E7 lat/lngs), or when maximum accuracy is desired.
//
// If the given "snap_radius" is zero, then all input vertices are preserved
// exactly.  Otherwise, S2Builder merges nearby vertices to ensure that no
// vertex pair is closer than "snap_radius".  Furthermore, vertices are
// separated from non-incident edges by at least "min_edge_vertex_separation",
// equal to (0.5 * snap_radius).  For example, if the snap_radius is 1km, then
// vertices will be separated from non-incident edges by at least 500m.
class IdentitySnapFunction : public S2Builder::SnapFunction {
 public:
  // The default constructor uses a snap_radius of zero (i.e., no snapping).
  IdentitySnapFunction();

  // Convenience constructor that calls set_snap_radius().
  explicit IdentitySnapFunction(S1Angle snap_radius);

  // REQUIRES: snap_radius <= SnapFunction::kMaxSnapRadius()
  void set_snap_radius(S1Angle snap_radius);
  S1Angle snap_radius() const override;

  // For the identity snap function, all vertex pairs are separated by at
  // least snap_radius().
  S1Angle min_vertex_separation() const override;

  // For the identity snap function, edges are separated from all non-incident
  // vertices by at least 0.5 * snap_radius().
  S1Angle min_edge_vertex_separation() const override;

  S2Point SnapPoint(const S2Point& point) const override;

  std::unique_ptr<SnapFunction> Clone() const override;

 private:
  // Copying and assignment are allowed.
  S1Angle snap_radius_;
};

// A SnapFunction that snaps vertices to S2CellId centers.  This can be useful
// if you want to encode your geometry compactly using S2Polygon::Encode(),
// for example.  You can snap to the centers of cells at any level.
//
// Every snap level has a corresponding minimum snap radius, which is simply
// the maximum distance that a vertex can move when snapped.  It is
// approximately equal to half of the maximum diagonal length for cells at the
// chosen level.  You can also set the snap radius to a larger value; for
// example, you could snap to the centers of leaf cells (1cm resolution) but
// set the snap_radius() to 10m.  This would result in significant extra
// simplification, without moving vertices unnecessarily (i.e., vertices that
// are at least 10m away from all other vertices will move by less than 1cm).
class S2CellIdSnapFunction : public S2Builder::SnapFunction {
 public:
  // The default constructor snaps to S2CellId::kMaxLevel (i.e., the centers
  // of leaf cells), and uses the minimum allowable snap radius at that level.
  S2CellIdSnapFunction();

  // Convenience constructor equivalent to calling set_level(level).
  explicit S2CellIdSnapFunction(int level);

  // Snaps vertices to S2Cell centers at the given level.  As a side effect,
  // this method also resets "snap_radius" to the minimum value allowed at
  // this level:
  //
  //   set_snap_radius(MinSnapRadiusForLevel(level))
  //
  // This means that if you want to use a larger snap radius than the minimum,
  // you must call set_snap_radius() *after* calling set_level().
  void set_level(int level);
  int level() const;

  // Defines the snap radius to be used (see s2builder.h).  The snap radius
  // must be at least the minimum value for the current level(), but larger
  // values can also be used (e.g., to simplify the geometry).
  //
  // REQUIRES: snap_radius >= MinSnapRadiusForLevel(level())
  // REQUIRES: snap_radius <= SnapFunction::kMaxSnapRadius()
  void set_snap_radius(S1Angle snap_radius);
  S1Angle snap_radius() const override;

  // Returns the minimum allowable snap radius for the given S2Cell level
  // (approximately equal to half of the maximum cell diagonal length).
  static S1Angle MinSnapRadiusForLevel(int level);

  // Returns the minimum S2Cell level (i.e., largest S2Cells) such that
  // vertices will not move by more than "snap_radius".  This can be useful
  // when choosing an appropriate level to snap to.  The return value is
  // always a valid level (out of range values are silently clamped).
  //
  // If you want to choose the snap level based on a distance, and then use
  // the minimum possible snap radius for the chosen level, do this:
  //
  //   S2CellIdSnapFunction f(
  //       S2CellIdSnapFunction::LevelForMaxSnapRadius(distance));
  static int LevelForMaxSnapRadius(S1Angle snap_radius);

  // For S2CellId snapping, the minimum separation between vertices depends on
  // level() and snap_radius().  It can vary between 0.5 * snap_radius()
  // and snap_radius().
  S1Angle min_vertex_separation() const override;

  // For S2CellId snapping, the minimum separation between edges and
  // non-incident vertices depends on level() and snap_radius().  It can
  // be as low as 0.219 * snap_radius(), but is typically 0.5 * snap_radius()
  // or more.
  S1Angle min_edge_vertex_separation() const override;

  S2Point SnapPoint(const S2Point& point) const override;

  std::unique_ptr<SnapFunction> Clone() const override;

 private:
  // Copying and assignment are allowed.
  int level_;
  S1Angle snap_radius_;
};

// A SnapFunction that snaps vertices to S2LatLng E5, E6, or E7 coordinates.
// These coordinates are expressed in degrees multiplied by a power of 10 and
// then rounded to the nearest integer.  For example, in E6 coordinates the
// point (23.12345651, -45.65432149) would become (23123457, -45654321).
//
// The main argument of the SnapFunction is the exponent for the power of 10
// that coordinates should be multipled by before rounding.  For example,
// IntLatLngSnapFunction(7) is a function that snaps to E7 coordinates.  The
// exponent can range from 0 to 10.
//
// Each exponent has a corresponding minimum snap radius, which is simply the
// maximum distance that a vertex can move when snapped.  It is approximately
// equal to 1/sqrt(2) times the nominal point spacing; for example, for
// snapping to E7 the minimum snap radius is (1e-7 / sqrt(2)) degrees.
// You can also set the snap radius to any value larger than this; this can
// result in significant extra simplification (similar to using a larger
// exponent) but does not move vertices unnecessarily.
class IntLatLngSnapFunction : public S2Builder::SnapFunction {
 public:
  // The default constructor yields an invalid snap function.  You must set
  // the exponent explicitly before using it.
  IntLatLngSnapFunction();

  // Convenience constructor equivalent to calling set_exponent(exponent).
  explicit IntLatLngSnapFunction(int exponent);

  // Snaps vertices to points whose (lat, lng) coordinates are integers after
  // converting to degrees and multiplying by 10 raised to the given exponent.
  // For example, (exponent == 7) yields E7 coordinates.  As a side effect,
  // this method also resets "snap_radius" to the minimum value allowed for
  // this exponent:
  //
  //   set_snap_radius(MinSnapRadiusForExponent(exponent))
  //
  // This means that if you want to use a larger snap radius than the minimum,
  // you must call set_snap_radius() *after* calling set_exponent().
  //
  // REQUIRES: kMinExponent <= exponent <= kMaxExponent
  void set_exponent(int exponent);
  int exponent() const;

  // The minum exponent supported for snapping.
  static const int kMinExponent = 0;

  // The maximum exponent supported for snapping.
  static const int kMaxExponent = 10;

  // Defines the snap radius to be used (see s2builder.h).  The snap radius
  // must be at least the minimum value for the current exponent(), but larger
  // values can also be used (e.g., to simplify the geometry).
  //
  // REQUIRES: snap_radius >= MinSnapRadiusForExponent(exponent())
  // REQUIRES: snap_radius <= SnapFunction::kMaxSnapRadius()
  void set_snap_radius(S1Angle snap_radius);
  S1Angle snap_radius() const override;

  // Returns the minimum allowable snap radius for the given exponent
  // (approximately equal to (pow(10, -exponent) / sqrt(2)) degrees).
  static S1Angle MinSnapRadiusForExponent(int exponent);

  // Returns the minimum exponent such that vertices will not move by more
  // than "snap_radius".  This can be useful when choosing an appropriate
  // exponent for snapping.  The return value is always a valid exponent
  // (out of range values are silently clamped).
  //
  // If you want to choose the exponent based on a distance, and then use
  // the minimum possible snap radius for that exponent, do this:
  //
  //   IntLatLngSnapFunction f(
  //       IntLatLngSnapFunction::ExponentForMaxSnapRadius(distance));
  static int ExponentForMaxSnapRadius(S1Angle snap_radius);

  // For IntLatLng snapping, the minimum separation between vertices depends on
  // exponent() and snap_radius().  It can vary between snap_radius()
  // and snap_radius().
  S1Angle min_vertex_separation() const override;

  // For IntLatLng snapping, the minimum separation between edges and
  // non-incident vertices depends on level() and snap_radius().  It can
  // be as low as 0.222 * snap_radius(), but is typically 0.39 * snap_radius()
  // or more.
  S1Angle min_edge_vertex_separation() const override;
  S2Point SnapPoint(const S2Point& point) const override;
  std::unique_ptr<SnapFunction> Clone() const override;

 private:
  // Copying and assignment are allowed.
  int exponent_;
  S1Angle snap_radius_;
  double from_degrees_, to_degrees_;
};

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_SNAP_FUNCTIONS_H_
