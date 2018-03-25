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

#ifndef S2_S2R2RECT_H_
#define S2_S2R2RECT_H_

#include <iosfwd>

#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/r1interval.h"
#include "s2/r2.h"
#include "s2/r2rect.h"
#include "s2/s1angle.h"
#include "s2/s2region.h"

class Decoder;
class Encoder;
class R1Interval;
class S2Cap;
class S2Cell;
class S2CellId;
class S2LatLngRect;

// This class is a stopgap measure that allows some of the S2 spherical
// geometry machinery to be applied to planar geometry.  An S2R2Rect
// represents a closed axis-aligned rectangle in the (x,y) plane (an R2Rect),
// but it also happens to be a subtype of S2Region, which means that you can
// use an S2RegionCoverer to approximate it as a collection of S2CellIds.
//
// With respect to the S2Cell decomposition, an S2R2Rect is interpreted as a
// region of (s,t)-space on face 0.  In particular, the rectangle [0,1]x[0,1]
// corresponds to the S2CellId that covers all of face 0.  This means that
// only rectangles that are subsets of [0,1]x[0,1] can be approximated using
// the S2RegionCoverer interface.
//
// The S2R2Rect class is also a convenient way to find the (s,t)-region
// covered by a given S2CellId (see the FromCell and FromCellId methods).
//
// TODO(ericv): If the geometry library is extended to have better support
// for planar geometry, then this class should no longer be necessary.
//
// This class is intended to be copied by value as desired.  It uses
// the default copy constructor and assignment operator, however it is
// not a "plain old datatype" (POD) because it has virtual functions.
class S2R2Rect final : public S2Region {
 public:
  // Construct a rectangle from an R2Rect.
  explicit S2R2Rect(const R2Rect& rect);

  // Construct a rectangle from the given lower-left and upper-right points.
  S2R2Rect(const R2Point& lo, const R2Point& hi);

  // Construct a rectangle from the given intervals in x and y.  The two
  // intervals must either be both empty or both non-empty.
  S2R2Rect(const R1Interval& x, const R1Interval& y);

  // The canonical empty rectangle.  Use is_empty() to test for empty
  // rectangles, since they have more than one representation.
  static S2R2Rect Empty();

  // Construct a rectangle that corresponds to the boundary of the given cell
  // is (s,t)-space.  Such rectangles are always a subset of [0,1]x[0,1].
  static S2R2Rect FromCell(const S2Cell& cell);
  static S2R2Rect FromCellId(S2CellId id);

  // Construct a rectangle from a center point and size in each dimension.
  // Both components of size should be non-negative, i.e. this method cannot
  // be used to create an empty rectangle.
  static S2R2Rect FromCenterSize(const R2Point& center, const R2Point& size);

  // Convenience method to construct a rectangle containing a single point.
  static S2R2Rect FromPoint(const R2Point& p);

  // Convenience method to construct the minimal bounding rectangle containing
  // the two given points.  This is equivalent to starting with an empty
  // rectangle and calling AddPoint() twice.  Note that it is different than
  // the S2R2Rect(lo, hi) constructor, where the first point is always
  // used as the lower-left corner of the resulting rectangle.
  static S2R2Rect FromPointPair(const R2Point& p1, const R2Point& p2);

  // Accessor methods.
  const R1Interval& x() const;
  const R1Interval& y() const;
  R2Point lo() const;
  R2Point hi() const;

  // Methods that allow the S2R2Rect to be accessed as a vector.
  const R1Interval& operator[](int i) const;
  R1Interval& operator[](int i);

  // Return true if the rectangle is valid, which essentially just means
  // that if the bound for either axis is empty then both must be.
  bool is_valid() const;

  // Return true if the rectangle is empty, i.e. it contains no points at all.
  bool is_empty() const;

  // Return the k-th vertex of the rectangle (k = 0,1,2,3) in CCW order.
  // Vertex 0 is in the lower-left corner.  For convenience, the argument is
  // reduced modulo 4 to the range [0..3].
  R2Point GetVertex(int k) const;

  // Return the vertex in direction "i" along the x-axis (0=left, 1=right) and
  // direction "j" along the y-axis (0=down, 1=up).  Equivalently, return the
  // vertex constructed by selecting endpoint "i" of the x-interval (0=lo,
  // 1=hi) and vertex "j" of the y-interval.
  R2Point GetVertex(int i, int j) const;

  // Return the center of the rectangle in (x,y)-space
  // (in general this is not the center of the region on the sphere).
  R2Point GetCenter() const;

  // Return the width and height of this rectangle in (x,y)-space.  Empty
  // rectangles have a negative width and height.
  R2Point GetSize() const;

  // Return true if the rectangle contains the given point.  Note that
  // rectangles are closed regions, i.e. they contain their boundary.
  bool Contains(const R2Point& p) const;

  // Return true if and only if the given point is contained in the interior
  // of the region (i.e. the region excluding its boundary).
  bool InteriorContains(const R2Point& p) const;

  // Return true if and only if the rectangle contains the given other
  // rectangle.
  bool Contains(const S2R2Rect& other) const;

  // Return true if and only if the interior of this rectangle contains all
  // points of the given other rectangle (including its boundary).
  bool InteriorContains(const S2R2Rect& other) const;

  // Return true if this rectangle and the given other rectangle have any
  // points in common.
  bool Intersects(const S2R2Rect& other) const;

  // Return true if and only if the interior of this rectangle intersects
  // any point (including the boundary) of the given other rectangle.
  bool InteriorIntersects(const S2R2Rect& other) const;

  // Increase the size of the bounding rectangle to include the given point.
  // The rectangle is expanded by the minimum amount possible.
  void AddPoint(const R2Point& p);

  // Return the closest point in the rectangle to the given point "p".
  // The rectangle must be non-empty.
  R2Point Project(const R2Point& p) const;

  // Return a rectangle that has been expanded on each side in the x-direction
  // by margin.x(), and on each side in the y-direction by margin.y().  If
  // either margin is negative, then shrink the interval on the corresponding
  // sides instead.  The resulting rectangle may be empty.  Any expansion of
  // an empty rectangle remains empty.
  S2R2Rect Expanded(const R2Point& margin) const;
  S2R2Rect Expanded(double margin) const;

  // Return the smallest rectangle containing the union of this rectangle and
  // the given rectangle.
  S2R2Rect Union(const S2R2Rect& other) const;

  // Return the smallest rectangle containing the intersection of this
  // rectangle and the given rectangle.
  S2R2Rect Intersection(const S2R2Rect& other) const;

  // Return true if two rectangles contains the same set of points.
  bool operator==(const S2R2Rect& other) const;

  // Return true if the x- and y-intervals of the two rectangles are the same
  // up to the given tolerance (see r1interval.h for details).
  bool ApproxEquals(const S2R2Rect& other,
                    S1Angle max_error = S1Angle::Radians(1e-15)) const;

  // Return the unit-length S2Point corresponding to the given point "p" in
  // the (s,t)-plane.  "p" need not be restricted to the range [0,1]x[0,1].
  static S2Point ToS2Point(const R2Point& p);

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  S2R2Rect* Clone() const override;
  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;
  bool Contains(const S2Point& p) const override;
  bool Contains(const S2Cell& cell) const override;
  bool MayIntersect(const S2Cell& cell) const override;

 private:
  R2Rect rect_;
};

std::ostream& operator<<(std::ostream& os, const S2R2Rect& r);


//////////////////   Implementation details follow   ////////////////////


inline S2R2Rect::S2R2Rect(const R2Rect& rect) : rect_(rect) {}

inline S2R2Rect::S2R2Rect(const R2Point& lo, const R2Point& hi)
    : rect_(lo, hi) {}

inline S2R2Rect::S2R2Rect(const R1Interval& x, const R1Interval& y)
    : rect_(x, y) {}

inline S2R2Rect S2R2Rect::FromCenterSize(const R2Point& center,
                                         const R2Point& size) {
  return S2R2Rect(R2Rect::FromCenterSize(center, size));
}

inline S2R2Rect S2R2Rect::FromPoint(const R2Point& p) {
  return S2R2Rect(R2Rect::FromPoint(p));
}

inline S2R2Rect S2R2Rect::FromPointPair(const R2Point& p1, const R2Point& p2) {
  return S2R2Rect(R2Rect::FromPointPair(p1, p2));
}

inline const R1Interval& S2R2Rect::x() const { return rect_.x(); }
inline const R1Interval& S2R2Rect::y() const { return rect_.y(); }
inline R2Point S2R2Rect::lo() const { return rect_.lo(); }
inline R2Point S2R2Rect::hi() const { return rect_.hi(); }
inline const R1Interval& S2R2Rect::operator[](int i) const { return rect_[i]; }
inline R1Interval& S2R2Rect::operator[](int i) { return rect_[i]; }
inline S2R2Rect S2R2Rect::Empty() { return S2R2Rect(R2Rect::Empty()); }
inline bool S2R2Rect::is_valid() const { return rect_.is_valid(); }
inline bool S2R2Rect::is_empty() const { return rect_.is_empty(); }
inline R2Point S2R2Rect::GetVertex(int k) const { return rect_.GetVertex(k); }
inline R2Point S2R2Rect::GetVertex(int i, int j) const {
  return rect_.GetVertex(i, j);
}
inline R2Point S2R2Rect::GetCenter() const { return rect_.GetCenter(); }
inline R2Point S2R2Rect::GetSize() const { return rect_.GetSize(); }
inline bool S2R2Rect::Contains(const R2Point& p) const {
  return rect_.Contains(p);
}
inline bool S2R2Rect::InteriorContains(const R2Point& p) const {
  return rect_.InteriorContains(p);
}
inline bool S2R2Rect::Contains(const S2R2Rect& other) const {
  return rect_.Contains(other.rect_);
}
inline bool S2R2Rect::InteriorContains(const S2R2Rect& other) const {
  return rect_.InteriorContains(other.rect_);
}
inline bool S2R2Rect::Intersects(const S2R2Rect& other) const {
  return rect_.Intersects(other.rect_);
}
inline bool S2R2Rect::InteriorIntersects(const S2R2Rect& other) const {
  return rect_.InteriorIntersects(other.rect_);
}
inline void S2R2Rect::AddPoint(const R2Point& p) {
  rect_.AddPoint(p);
}
inline R2Point S2R2Rect::Project(const R2Point& p) const {
  return rect_.Project(p);
}
inline S2R2Rect S2R2Rect::Expanded(const R2Point& margin) const {
  return S2R2Rect(rect_.Expanded(margin));
}
inline S2R2Rect S2R2Rect::Expanded(double margin) const {
  return S2R2Rect(rect_.Expanded(margin));
}
inline S2R2Rect S2R2Rect::Union(const S2R2Rect& other) const {
  return S2R2Rect(rect_.Union(other.rect_));
}
inline S2R2Rect S2R2Rect::Intersection(const S2R2Rect& other) const {
  return S2R2Rect(rect_.Intersection(other.rect_));
}
inline bool S2R2Rect::operator==(const S2R2Rect& other) const {
  return rect_ == other.rect_;
}
inline bool S2R2Rect::ApproxEquals(const S2R2Rect& other,
                                   S1Angle max_error) const {
  return rect_.ApproxEquals(other.rect_, max_error.radians());
}

#endif  // S2_S2R2RECT_H_
