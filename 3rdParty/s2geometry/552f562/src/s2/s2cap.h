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

#ifndef S2_S2CAP_H_
#define S2_S2CAP_H_

#include <algorithm>
#include <cmath>
#include <iosfwd>
#include <vector>

#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2pointutil.h"
#include "s2/s2region.h"

class Decoder;
class Encoder;
class S2Cell;
class S2LatLngRect;

// S2Cap represents a disc-shaped region defined by a center and radius.
// Technically this shape is called a "spherical cap" (rather than disc)
// because it is not planar; the cap represents a portion of the sphere that
// has been cut off by a plane.  The boundary of the cap is the circle defined
// by the intersection of the sphere and the plane.  For containment purposes,
// the cap is a closed set, i.e. it contains its boundary.
//
// For the most part, you can use a spherical cap wherever you would use a
// disc in planar geometry.  The radius of the cap is measured along the
// surface of the sphere (rather than the straight-line distance through the
// interior).  Thus a cap of radius Pi/2 is a hemisphere, and a cap of radius
// Pi covers the entire sphere.
//
// A cap can also be defined by its center point and height.  The height is
// simply the distance from the center point to the cutoff plane.  There is
// also support for empty and full caps, which contain no points and all
// points respectively.
//
// This class is intended to be copied by value as desired.  It uses the
// default copy constructor and assignment operator, however it is not a
// "plain old datatype" (POD) because it has virtual functions.
class S2Cap final : public S2Region {
 public:
  // The default constructor returns an empty S2Cap.
  S2Cap() : center_(1, 0, 0), radius_(S1ChordAngle::Negative()) {}

  // Constructs a cap with the given center and radius.  A negative radius
  // yields an empty cap; a radius of 180 degrees or more yields a full cap
  // (containing the entire sphere).  "center" should be unit length.
  S2Cap(const S2Point& center, S1Angle radius);

  // Constructs a cap where the angle is expressed as an S1ChordAngle.  This
  // constructor is more efficient than the one above.
  S2Cap(const S2Point& center, S1ChordAngle radius);

  // Convenience function that creates a cap containing a single point.  This
  // method is more efficient that the S2Cap(center, radius) constructor.
  static S2Cap FromPoint(const S2Point& center);

  // Returns a cap with the given center and height (see comments above).  A
  // negative height yields an empty cap; a height of 2 or more yields a full
  // cap.  "center" should be unit length.
  static S2Cap FromCenterHeight(const S2Point& center, double height);

  // Return a cap with the given center and surface area.  Note that the area
  // can also be interpreted as the solid angle subtended by the cap (because
  // the sphere has unit radius).  A negative area yields an empty cap; an
  // area of 4*Pi or more yields a full cap.  "center" should be unit length.
  static S2Cap FromCenterArea(const S2Point& center, double area);

  // Return an empty cap, i.e. a cap that contains no points.
  static S2Cap Empty();

  // Return a full cap, i.e. a cap that contains all points.
  static S2Cap Full();

  ~S2Cap() override {}

  // Accessor methods.
  const S2Point& center() const { return center_; }
  S1ChordAngle radius() const { return radius_; }

  // Returns the height of the cap, i.e. the distance from the center point to
  // the cutoff plane.
  double height() const;

  // Return the cap radius as an S1Angle.  (Note that the cap angle is stored
  // internally as an S1ChordAngle, so this method requires a trigonometric
  // operation and may yield a slightly different result than the value passed
  // to the (S2Point, S1Angle) constructor.)
  S1Angle GetRadius() const;

  // Return the area of the cap.
  double GetArea() const;

  // Return the true centroid of the cap multiplied by its surface area (see
  // s2centroids.h for details on centroids). The result lies on the ray from
  // the origin through the cap's center, but it is not unit length. Note that
  // if you just want the "surface centroid", i.e. the normalized result, then
  // it is much simpler just to call center().
  //
  // The reason for multiplying the result by the cap area is to make it
  // easier to compute the centroid of more complicated shapes.  The centroid
  // of a union of disjoint regions can be computed simply by adding their
  // GetCentroid() results. Caveat: for caps that contain a single point
  // (i.e., zero radius), this method always returns the origin (0, 0, 0).
  // This is because shapes with no area don't affect the centroid of a
  // union whose total area is positive.
  S2Point GetCentroid() const;

  // We allow negative heights (to represent empty caps) but heights are
  // normalized so that they do not exceed 2.
  bool is_valid() const;

  // Return true if the cap is empty, i.e. it contains no points.
  bool is_empty() const;

  // Return true if the cap is full, i.e. it contains all points.
  bool is_full() const;

  // Return the complement of the interior of the cap.  A cap and its
  // complement have the same boundary but do not share any interior points.
  // The complement operator is not a bijection because the complement of a
  // singleton cap (containing a single point) is the same as the complement
  // of an empty cap.
  S2Cap Complement() const;

  // Return true if and only if this cap contains the given other cap
  // (in a set containment sense, e.g. every cap contains the empty cap).
  bool Contains(const S2Cap& other) const;

  // Return true if and only if this cap intersects the given other cap,
  // i.e. whether they have any points in common.
  bool Intersects(const S2Cap& other) const;

  // Return true if and only if the interior of this cap intersects the
  // given other cap.  (This relationship is not symmetric, since only
  // the interior of this cap is used.)
  bool InteriorIntersects(const S2Cap& other) const;

  // Return true if and only if the given point is contained in the interior
  // of the cap (i.e. the cap excluding its boundary).  "p" should be be a
  // unit-length vector.
  bool InteriorContains(const S2Point& p) const;

  // Increase the cap height if necessary to include the given point.  If the
  // cap is empty then the center is set to the given point, but otherwise the
  // center is not changed.  "p" should be a unit-length vector.
  void AddPoint(const S2Point& p);

  // Increase the cap height if necessary to include "other".  If the current
  // cap is empty it is set to the given other cap.
  void AddCap(const S2Cap& other);

  // Return a cap that contains all points within a given distance of this
  // cap.  Note that any expansion of the empty cap is still empty.
  S2Cap Expanded(S1Angle distance) const;

  // Return the smallest cap which encloses this cap and "other".
  S2Cap Union(const S2Cap& other) const;

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  S2Cap* Clone() const override;
  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;
  void GetCellUnionBound(std::vector<S2CellId> *cell_ids) const override;
  bool Contains(const S2Cell& cell) const override;
  bool MayIntersect(const S2Cell& cell) const override;

  // The point "p" should be a unit-length vector.
  bool Contains(const S2Point& p) const override;

  // Appends a serialized representation of the S2Cap to "encoder".
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void Encode(Encoder* const encoder) const;

  // Decodes an S2Cap encoded with Encode().  Returns true on success.
  bool Decode(Decoder* const decoder);

  ///////////////////////////////////////////////////////////////////////
  // The following static methods are convenience functions for assertions
  // and testing purposes only.

  // Return true if two caps are identical.
  bool operator==(const S2Cap& other) const;

  // Return true if the cap center and height differ by at most "max_error"
  // from the given cap "other".
  bool ApproxEquals(const S2Cap& other,
                    S1Angle max_error = S1Angle::Radians(1e-14)) const;

 private:
  // Here are some useful relationships between the cap height (h), the cap
  // radius (r), the maximum chord length from the cap's center (d), and the
  // radius of cap's base (a).
  //
  //     h = 1 - cos(r)
  //       = 2 * sin^2(r/2)
  //   d^2 = 2 * h
  //       = a^2 + h^2

  // Return true if the cap intersects "cell", given that the cap does contain
  // any of the cell vertices (supplied in "vertices", an array of length 4).
  bool Intersects(const S2Cell& cell, const S2Point* vertices) const;

  S2Point center_;
  S1ChordAngle radius_;
};

std::ostream& operator<<(std::ostream& os, const S2Cap& cap);


//////////////////   Implementation details follow   ////////////////////


inline S2Cap::S2Cap(const S2Point& center, S1Angle radius)
    : center_(center), radius_(std::min(radius, S1Angle::Radians(M_PI))) {
  // The "min" calculation above is necessary to handle S1Angle::Infinity().
  S2_DCHECK(is_valid());
}

inline S2Cap::S2Cap(const S2Point& center, S1ChordAngle radius)
    : center_(center), radius_(radius) {
  S2_DCHECK(is_valid());
}

inline S2Cap S2Cap::FromPoint(const S2Point& center) {
  return S2Cap(center, S1ChordAngle::Zero());
}

inline S2Cap S2Cap::FromCenterHeight(const S2Point& center, double height) {
  return S2Cap(center, S1ChordAngle::FromLength2(2 * height));
}

inline S2Cap S2Cap::FromCenterArea(const S2Point& center, double area) {
  return S2Cap(center, S1ChordAngle::FromLength2(area / M_PI));
}

inline S2Cap S2Cap::Empty() { return S2Cap(); }

inline S2Cap S2Cap::Full() {
  return S2Cap(S2Point(1, 0, 0), S1ChordAngle::Straight());
}

inline double S2Cap::height() const {
  return 0.5 * radius_.length2();
}

inline S1Angle S2Cap::GetRadius() const {
  return radius_.ToAngle();
}

inline bool S2Cap::is_valid() const {
  return S2::IsUnitLength(center_) && radius_.length2() <= 4;
}

inline bool S2Cap::is_empty() const {
  return radius_.is_negative();
}

inline bool S2Cap::is_full() const {
  return radius_.length2() == 4;
}

#endif  // S2_S2CAP_H_
