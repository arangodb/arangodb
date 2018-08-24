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

#ifndef S2_S2LATLNG_RECT_H_
#define S2_S2LATLNG_RECT_H_

#include <cmath>
#include <iosfwd>
#include <iostream>

#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/r1interval.h"
#include "s2/s1angle.h"
#include "s2/s1interval.h"
#include "s2/s2latlng.h"
#include "s2/s2region.h"

class Decoder;
class Encoder;
class S2Cap;
class S2Cell;

// An S2LatLngRect represents a closed latitude-longitude rectangle.  It is
// capable of representing the empty and full rectangles as well as single
// points.  Note that the latitude-longitude space is considered to have a
// *cylindrical* topology rather than a spherical one, i.e. the poles have
// multiple lat/lng representations.  An S2LatLngRect may be defined so that
// includes some representations of a pole but not others.  Use the
// PolarClosure() method if you want to expand a rectangle so that it contains
// all possible representations of any contained poles.
//
// Because S2LatLngRect uses S1Interval to store the longitude range,
// longitudes of -180 degrees are treated specially.  Except for empty
// and full longitude spans, -180 degree longitudes will turn into +180
// degrees.  This sign flip causes lng_lo() to be greater than lng_hi(),
// indicating that the rectangle will wrap around through -180 instead of
// through +179. Thus the math is consistent within the library, but the sign
// flip can be surprising, especially when working with map projections where
// -180 and +180 are at opposite ends of the flattened map.  See the comments
// on S1Interval for more details.
//
// This class is intended to be copied by value as desired.  It uses
// the default copy constructor and assignment operator, however it is
// not a "plain old datatype" (POD) because it has virtual functions.
class S2LatLngRect final : public S2Region {
 public:
  // Construct a rectangle from minimum and maximum latitudes and longitudes.
  // If lo.lng() > hi.lng(), the rectangle spans the 180 degree longitude
  // line. Both points must be normalized, with lo.lat() <= hi.lat().
  // The rectangle contains all the points p such that 'lo' <= p <= 'hi',
  // where '<=' is defined in the obvious way.
  S2LatLngRect(const S2LatLng& lo, const S2LatLng& hi);

  // Construct a rectangle from latitude and longitude intervals.  The two
  // intervals must either be both empty or both non-empty, and the latitude
  // interval must not extend outside [-90, +90] degrees.
  // Note that both intervals (and hence the rectangle) are closed.
  S2LatLngRect(const R1Interval& lat, const S1Interval& lng);

  // The default constructor creates an empty S2LatLngRect.
  S2LatLngRect();

  // Construct a rectangle of the given size centered around the given point.
  // "center" needs to be normalized, but "size" does not.  The latitude
  // interval of the result is clamped to [-90,90] degrees, and the longitude
  // interval of the result is Full() if and only if the longitude size is
  // 360 degrees or more.  Examples of clamping (in degrees):
  //
  //   center=(80,170),  size=(40,60)   -> lat=[60,90],   lng=[140,-160]
  //   center=(10,40),   size=(210,400) -> lat=[-90,90],  lng=[-180,180]
  //   center=(-90,180), size=(20,50)   -> lat=[-90,-80], lng=[155,-155]
  static S2LatLngRect FromCenterSize(const S2LatLng& center,
                                     const S2LatLng& size);

  // Construct a rectangle containing a single (normalized) point.
  static S2LatLngRect FromPoint(const S2LatLng& p);

  // Construct the minimal bounding rectangle containing the two given
  // normalized points.  This is equivalent to starting with an empty
  // rectangle and calling AddPoint() twice.  Note that it is different than
  // the S2LatLngRect(lo, hi) constructor, where the first point is always
  // used as the lower-left corner of the resulting rectangle.
  static S2LatLngRect FromPointPair(const S2LatLng& p1, const S2LatLng& p2);

  // Accessor methods.
  S1Angle lat_lo() const { return S1Angle::Radians(lat_.lo()); }
  S1Angle lat_hi() const { return S1Angle::Radians(lat_.hi()); }
  S1Angle lng_lo() const { return S1Angle::Radians(lng_.lo()); }
  S1Angle lng_hi() const { return S1Angle::Radians(lng_.hi()); }
  const R1Interval& lat() const { return lat_; }
  const S1Interval& lng() const { return lng_; }
  R1Interval *mutable_lat() { return &lat_; }
  S1Interval *mutable_lng() { return &lng_; }
  S2LatLng lo() const { return S2LatLng(lat_lo(), lng_lo()); }
  S2LatLng hi() const { return S2LatLng(lat_hi(), lng_hi()); }

  // The canonical empty and full rectangles, as derived from the Empty
  // and Full R1 and S1 Intervals.
  // Empty: lat_lo=1, lat_hi=0, lng_lo=Pi, lng_hi=-Pi (radians)
  static S2LatLngRect Empty();
  // Full: lat_lo=-Pi/2, lat_hi=Pi/2, lng_lo=-Pi, lng_hi=Pi (radians)
  static S2LatLngRect Full();

  // The full allowable range of latitudes and longitudes.
  static R1Interval FullLat() { return R1Interval(-M_PI_2, M_PI_2); }
  static S1Interval FullLng() { return S1Interval::Full(); }

  // Returns true if the rectangle is valid, which essentially just means
  // that the latitude bounds do not exceed Pi/2 in absolute value and
  // the longitude bounds do not exceed Pi in absolute value.  Also, if
  // either the latitude or longitude bound is empty then both must be.
  bool is_valid() const;

  // Returns true if the rectangle is empty, i.e. it contains no points at all.
  bool is_empty() const;

  // Returns true if the rectangle is full, i.e. it contains all points.
  bool is_full() const;

  // Returns true if the rectangle is a point, i.e. lo() == hi()
  bool is_point() const;

  // Returns true if lng_.lo() > lng_.hi(), i.e. the rectangle crosses
  // the 180 degree longitude line.
  bool is_inverted() const { return lng_.is_inverted(); }

  // Returns the k-th vertex of the rectangle (k = 0,1,2,3) in CCW order
  // (lower left, lower right, upper right, upper left).  For convenience, the
  // argument is reduced modulo 4 to the range [0..3].
  S2LatLng GetVertex(int k) const;

  // Returns the center of the rectangle in latitude-longitude space
  // (in general this is not the center of the region on the sphere).
  S2LatLng GetCenter() const;

  // Returns the width and height of this rectangle in latitude-longitude
  // space.  Empty rectangles have a negative width and height.
  S2LatLng GetSize() const;

  // Returns the surface area of this rectangle on the unit sphere.
  double Area() const;

  // Returns the true centroid of the rectangle multiplied by its surface area
  // (see s2centroids.h for details on centroids).  The result is not unit
  // length, so you may want to normalize it.  Note that in general the
  // centroid is *not* at the center of the rectangle, and in fact it may not
  // even be contained by the rectangle.  (It is the "center of mass" of the
  // rectangle viewed as subset of the unit sphere, i.e. it is the point in
  // space about which this curved shape would rotate.)
  //
  // The reason for multiplying the result by the rectangle area is to make it
  // easier to compute the centroid of more complicated shapes.  The centroid
  // of a union of disjoint regions can be computed simply by adding their
  // GetCentroid() results.
  S2Point GetCentroid() const;

  // More efficient version of Contains() that accepts a S2LatLng rather than
  // an S2Point.  The argument must be normalized.
  bool Contains(const S2LatLng& ll) const;

  // Returns true if and only if the given point is contained in the interior
  // of the region (i.e. the region excluding its boundary).  The point 'p'
  // does not need to be normalized.
  bool InteriorContains(const S2Point& p) const;

  // More efficient version of InteriorContains() that accepts a S2LatLng
  // rather than an S2Point.  The argument must be normalized.
  bool InteriorContains(const S2LatLng& ll) const;

  // Returns true if and only if the rectangle contains the given other
  // rectangle.
  bool Contains(const S2LatLngRect& other) const;

  // Returns true if and only if the interior of this rectangle contains all
  // points of the given other rectangle (including its boundary).
  bool InteriorContains(const S2LatLngRect& other) const;

  // Returns true if this rectangle and the given other rectangle have any
  // points in common.
  bool Intersects(const S2LatLngRect& other) const;

  // Returns true if this rectangle intersects the given cell.  (This is an
  // exact test and may be fairly expensive, see also MayIntersect below.)
  bool Intersects(const S2Cell& cell) const;

  // Returns true if and only if the interior of this rectangle intersects
  // any point (including the boundary) of the given other rectangle.
  bool InteriorIntersects(const S2LatLngRect& other) const;

  // Returns true if the boundary of this rectangle intersects the given
  // geodesic edge (v0, v1).
  bool BoundaryIntersects(const S2Point& v0, const S2Point& v1) const;

  // Increase the size of the bounding rectangle to include the given point.
  // The rectangle is expanded by the minimum amount possible.  The S2LatLng
  // argument must be normalized.
  void AddPoint(const S2Point& p);
  void AddPoint(const S2LatLng& ll);

  // Returns a rectangle that has been expanded by margin.lat() on each side in
  // the latitude direction, and by margin.lng() on each side in the longitude
  // direction.  If either margin is negative, then shrinks the rectangle on
  // the corresponding sides instead.  The resulting rectangle may be empty.
  //
  // As noted above, the latitude-longitude space has the topology of a
  // cylinder.  Longitudes "wrap around" at +/-180 degrees, while latitudes
  // are clamped to range [-90, 90].  This means that any expansion (positive
  // or negative) of the full longitude range remains full (since the
  // "rectangle" is actually a continuous band around the cylinder), while
  // expansion of the full latitude range remains full only if the margin is
  // positive.
  //
  // If either the latitude or longitude interval becomes empty after
  // expansion by a negative margin, the result is empty.
  //
  // Note that if an expanded rectangle contains a pole, it may not contain
  // all possible lat/lng representations of that pole (see header above).
  // Use the PolarClosure() method if you do not want this behavior.
  //
  // If you are trying to grow a rectangle by a certain *distance* on the
  // sphere (e.g. 5km), use the ExpandedByDistance() method instead.
  S2LatLngRect Expanded(const S2LatLng& margin) const;

  // If the rectangle does not include either pole, returns it unmodified.
  // Otherwise expands the longitude range to Full() so that the rectangle
  // contains all possible representations of the contained pole(s).
  S2LatLngRect PolarClosure() const;

  // Returns the smallest rectangle containing the union of this rectangle and
  // the given rectangle.
  S2LatLngRect Union(const S2LatLngRect& other) const;

  // Returns the smallest rectangle containing the intersection of this
  // rectangle and the given rectangle.  Note that the region of intersection
  // may consist of two disjoint rectangles, in which case a single rectangle
  // spanning both of them is returned.
  S2LatLngRect Intersection(const S2LatLngRect& other) const;

  // Expands this rectangle so that it contains all points within the given
  // distance of the boundary, and return the smallest such rectangle.  If the
  // distance is negative, then instead shrinks this rectangle so that it
  // excludes all points within the given absolute distance of the boundary,
  // and returns the largest such rectangle.
  //
  // Unlike Expanded(), this method treats the rectangle as a set of points on
  // the sphere, and measures distances on the sphere.  For example, you can
  // use this method to find a rectangle that contains all points within 5km
  // of a given rectangle.  Because this method uses the topology of the
  // sphere, note the following:
  //
  //  - The full and empty rectangles have no boundary on the sphere.  Any
  //    expansion (positive or negative) of these rectangles leaves them
  //    unchanged.
  //
  //  - Any rectangle that covers the full longitude range does not have an
  //    east or west boundary, therefore no expansion (positive or negative)
  //    will occur in that direction.
  //
  //  - Any rectangle that covers the full longitude range and also includes
  //    a pole will not be expanded or contracted at that pole, because it
  //    does not have a boundary there.
  //
  //  - If a rectangle is within the given distance of a pole, the result will
  //    include the full longitude range (because all longitudes are present
  //    at the poles).
  //
  // Expansion and contraction are defined such that they are inverses whenver
  // possible, i.e.
  //
  //   rect.ExpandedByDistance(x).ExpandedByDistance(-x) == rect
  //
  // (approximately), so long as the first operation does not cause a
  // rectangle boundary to disappear (i.e., the longitude range newly becomes
  // full or empty, or the latitude range expands to include a pole).
  S2LatLngRect ExpandedByDistance(S1Angle distance) const;

  // Returns the minimum distance (measured along the surface of the sphere) to
  // the given S2LatLngRect. Both S2LatLngRects must be non-empty.
  S1Angle GetDistance(const S2LatLngRect& other) const;

  // Returns the minimum distance (measured along the surface of the sphere)
  // from a given point to the rectangle (both its boundary and its interior).
  // The latlng must be valid.
  S1Angle GetDistance(const S2LatLng& p) const;

  // Returns the (directed or undirected) Hausdorff distance (measured along the
  // surface of the sphere) to the given S2LatLngRect. The directed Hausdorff
  // distance from rectangle A to rectangle B is given by
  //     h(A, B) = max_{p in A} min_{q in B} d(p, q).
  // The Hausdorff distance between rectangle A and rectangle B is given by
  //     H(A, B) = max{h(A, B), h(B, A)}.
  S1Angle GetDirectedHausdorffDistance(const S2LatLngRect& other) const;
  S1Angle GetHausdorffDistance(const S2LatLngRect& other) const;

  // Returns true if two rectangles contains the same set of points.
  bool operator==(const S2LatLngRect& other) const;

  // Returns the opposite of what operator == returns.
  bool operator!=(const S2LatLngRect& other) const;

  // Returns true if the latitude and longitude intervals of the two rectangles
  // are the same up to the given tolerance (see r1interval.h and s1interval.h
  // for details).
  bool ApproxEquals(const S2LatLngRect& other,
                    S1Angle max_error = S1Angle::Radians(1e-15)) const;

  // ApproxEquals() with separate tolerances for latitude and longitude.
  bool ApproxEquals(const S2LatLngRect& other, const S2LatLng& max_error) const;

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  S2LatLngRect* Clone() const override;
  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;
  bool Contains(const S2Cell& cell) const override;

  // This test is cheap but is NOT exact.  Use Intersects() if you want a more
  // accurate and more expensive test.  Note that when this method is used by
  // an S2RegionCoverer, the accuracy isn't all that important since if a cell
  // may intersect the region then it is subdivided, and the accuracy of this
  // method goes up as the cells get smaller.
  bool MayIntersect(const S2Cell& cell) const override;

  // The point 'p' does not need to be normalized.
  bool Contains(const S2Point& p) const override;

  // Appends a serialized representation of the S2LatLngRect to "encoder".
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void Encode(Encoder* const encoder) const;

  // Decodes an S2LatLngRect encoded with Encode().  Returns true on success.
  bool Decode(Decoder* const decoder);

  // Returns true if the edge AB intersects the given edge of constant
  // longitude.
  static bool IntersectsLngEdge(const S2Point& a, const S2Point& b,
                                const R1Interval& lat, double lng);

  // Returns true if the edge AB intersects the given edge of constant
  // latitude.  Requires the vectors to have unit length.
  static bool IntersectsLatEdge(const S2Point& a, const S2Point& b,
                                double lat, const S1Interval& lng);

 private:
  // Helper function. See .cc for description.
  static S1Angle GetDirectedHausdorffDistance(double lng_diff,
                                              const R1Interval& a_lat,
                                              const R1Interval& b_lat);

  // Helper function. See .cc for description.
  static S1Angle GetInteriorMaxDistance(const R1Interval& a_lat,
                                        const S2Point& b);

  // Helper function. See .cc for description.
  static S2Point GetBisectorIntersection(const R1Interval& lat, double lng);

  R1Interval lat_;
  S1Interval lng_;
};

inline S2LatLngRect::S2LatLngRect(const S2LatLng& lo, const S2LatLng& hi)
  : lat_(lo.lat().radians(), hi.lat().radians()),
    lng_(lo.lng().radians(), hi.lng().radians()) {
  S2_DLOG_IF(ERROR, !is_valid())
      << "Invalid rect: " << lo << ", " << hi;
}

inline S2LatLngRect::S2LatLngRect(const R1Interval& lat, const S1Interval& lng)
  : lat_(lat), lng_(lng) {
  S2_DLOG_IF(ERROR, !is_valid())
      << "Invalid rect: " << lat << ", " << lng;
}

inline S2LatLngRect::S2LatLngRect()
    : lat_(R1Interval::Empty()), lng_(S1Interval::Empty()) {
}

inline S2LatLngRect S2LatLngRect::Empty() {
  return S2LatLngRect();
}

inline S2LatLngRect S2LatLngRect::Full() {
  return S2LatLngRect(FullLat(), FullLng());
}

inline bool S2LatLngRect::is_valid() const {
  // The lat/lng ranges must either be both empty or both non-empty.
  return (std::fabs(lat_.lo()) <= M_PI_2 &&
          std::fabs(lat_.hi()) <= M_PI_2 &&
          lng_.is_valid() &&
          lat_.is_empty() == lng_.is_empty());
}

inline bool S2LatLngRect::is_empty() const {
  return lat_.is_empty();
}

inline bool S2LatLngRect::is_full() const {
  return lat_ == FullLat() && lng_.is_full();
}

inline bool S2LatLngRect::is_point() const {
  return lat_.lo() == lat_.hi() && lng_.lo() == lng_.hi();
}

inline bool S2LatLngRect::operator==(const S2LatLngRect& other) const {
  return lat() == other.lat() && lng() == other.lng();
}

inline bool S2LatLngRect::operator!=(const S2LatLngRect& other) const {
  return !operator==(other);
}

std::ostream& operator<<(std::ostream& os, const S2LatLngRect& r);

#endif  // S2_S2LATLNG_RECT_H_
