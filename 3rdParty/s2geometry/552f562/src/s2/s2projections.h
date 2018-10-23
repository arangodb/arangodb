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
//
// Defines a few simple map projections.  (Clients that need more complex
// projections should use a third-party library such as GeographicLib to
// implement their own projection subtypes.)

#ifndef S2_S2PROJECTIONS_H_
#define S2_S2PROJECTIONS_H_

#include "s2/r2.h"
#include "s2/s2latlng.h"
#include "s2/s2point.h"

namespace S2 {

// For the purposes of the S2 library, a projection is a function that maps
// between S2Points and R2Points.  It can also define the coordinate wrapping
// behavior along each axis.
class Projection {
 public:
  virtual ~Projection() {}

  // Converts a point on the sphere to a projected 2D point.
  virtual R2Point Project(const S2Point& p) const = 0;

  // Converts a projected 2D point to a point on the sphere.
  //
  // If wrapping is defined for a given axis (see below), then this method
  // should accept any real number for the corresponding coordinate.
  virtual S2Point Unproject(const R2Point& p) const = 0;

  // Convenience function equivalent to Project(ll.ToPoint()), but the
  // implementation may be more efficient.
  virtual R2Point FromLatLng(const S2LatLng& ll) const = 0;

  // Convenience function equivalent to S2LatLng(Unproject(p)), but the
  // implementation may be more efficient.
  virtual S2LatLng ToLatLng(const R2Point& p) const = 0;

  // Returns the point obtained by interpolating the given fraction of the
  // distance along the line from A to B.  Almost all projections should
  // use the default implementation of this method, which simply interpolates
  // linearly in R2 space.  Fractions < 0 or > 1 result in extrapolation
  // instead.
  //
  // The only reason to override this method is if you want edges to be
  // defined as something other than straight lines in the 2D projected
  // coordinate system.  For example, using a third-party library such as
  // GeographicLib you could define edges as geodesics over an ellipsoid model
  // of the Earth.  (Note that very few data sets define edges this way.)
  //
  // Also note that there is no reason to define a projection where edges are
  // geodesics over the sphere, because this is the native S2 interpretation.
  virtual R2Point Interpolate(double f, const R2Point& a, const R2Point& b)
      const;

  // Defines the coordinate wrapping distance along each axis.  If this value
  // is non-zero for a given axis, the coordinates are assumed to "wrap" with
  // the given period.  For example, if wrap_distance.y() == 360 then (x, y)
  // and (x, y + 360) should map to the same S2Point.
  //
  // This information is used to ensure that edges takes the shortest path
  // between two given points.  For example, if coordinates represent
  // (latitude, longitude) pairs in degrees and wrap_distance().y() == 360,
  // then the edge (5:179, 5:-179) would be interpreted as spanning 2 degrees
  // of longitude rather than 358 degrees.
  //
  // If a given axis does not wrap, its wrap_distance should be set to zero.
  virtual R2Point wrap_distance() const = 0;
};

// PlateCarreeProjection defines the "plate carree" (square plate) projection,
// which converts points on the sphere to (longitude, latitude) pairs.
// Coordinates can be scaled so that they represent radians, degrees, etc, but
// the projection is always centered around (latitude=0, longitude=0).
//
// Note that (x, y) coordinates are backwards compared to the usual (latitude,
// longitude) ordering, in order to match the usual convention for graphs in
// which "x" is horizontal and "y" is vertical.
class PlateCarreeProjection final : public Projection {
 public:
  // Constructs the plate carree projection where the x coordinates
  // (longitude) span [-x_scale, x_scale] and the y coordinates (latitude)
  // span [-x_scale/2, x_scale/2].  For example if x_scale==180 then the x
  // range is [-180, 180] and the y range is [-90, 90].
  //
  // By default coordinates are expressed in radians, i.e. the x range is
  // [-Pi, Pi] and the y range is [-Pi/2, Pi/2].
  explicit PlateCarreeProjection(double x_scale = M_PI);

  R2Point Project(const S2Point& p) const override;
  S2Point Unproject(const R2Point& p) const override;
  R2Point FromLatLng(const S2LatLng& ll) const override;
  S2LatLng ToLatLng(const R2Point& p) const override;
  R2Point wrap_distance() const override;

 private:
  double x_wrap_;
  double to_radians_;    // Multiplier to convert coordinates to radians.
  double from_radians_;  // Multiplier to convert coordinates from radians.
};

// MercatorProjection defines the spherical Mercator projection.  Google Maps
// uses this projection together with WGS84 coordinates, in which case it is
// known as the "Web Mercator" projection (see Wikipedia).  This class makes
// no assumptions regarding the coordinate system of its input points, but
// simply applies the spherical Mercator projection to them.
//
// The Mercator projection is finite in width (x) but infinite in height (y).
// "x" corresponds to longitude, and spans a finite range such as [-180, 180]
// (with coordinate wrapping), while "y" is a function of latitude and spans
// an infinite range.  (As "y" coordinates get larger, points get closer to
// the north pole but never quite reach it.)  The north and south poles have
// infinite "y" values.  (Note that this will cause problems if you tessellate
// a Mercator edge where one endpoint is a pole.  If you need to do this, clip
// the edge first so that the "y" coordinate is no more than about 5 * max_x.)
class MercatorProjection final : public Projection {
 public:
  // Constructs a Mercator projection where "x" corresponds to longitude in
  // the range [-max_x, max_x] , and "y" corresponds to latitude and can be
  // any real number.  The horizontal and vertical scales are equal locally.
  explicit MercatorProjection(double max_x);

  R2Point Project(const S2Point& p) const override;
  S2Point Unproject(const R2Point& p) const override;
  R2Point FromLatLng(const S2LatLng& ll) const override;
  S2LatLng ToLatLng(const R2Point& p) const override;
  R2Point wrap_distance() const override;

 private:
  double x_wrap_;
  double to_radians_;    // Multiplier to convert coordinates to radians.
  double from_radians_;  // Multiplier to convert coordinates from radians.
};

}  // namespace S2


#endif  // S2_S2PROJECTIONS_H_
