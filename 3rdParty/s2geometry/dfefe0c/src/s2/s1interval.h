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

#ifndef S2_S1INTERVAL_H_
#define S2_S1INTERVAL_H_

#include <cmath>
#include <iosfwd>
#include <iostream>

#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/util/math/vector.h"  // IWYU pragma: export

// An S1Interval represents a closed interval on a unit circle (also known
// as a 1-dimensional sphere).  It is capable of representing the empty
// interval (containing no points), the full interval (containing all
// points), and zero-length intervals (containing a single point).
//
// Points are represented by the angle they make with the positive x-axis in
// the range [-Pi, Pi].  An interval is represented by its lower and upper
// bounds (both inclusive, since the interval is closed).  The lower bound may
// be greater than the upper bound, in which case the interval is "inverted"
// (i.e. it passes through the point (-1, 0)).
//
// Note that the point (-1, 0) has two valid representations, Pi and -Pi.
// The normalized representation of this point internally is Pi, so that
// endpoints of normal intervals are in the range (-Pi, Pi].  However, we
// take advantage of the point -Pi to construct two special intervals:
// the Full() interval is [-Pi, Pi], and the Empty() interval is [Pi, -Pi].
//
// This class is intended to be copied by value as desired.  It uses
// the default copy constructor and assignment operator.
class S1Interval {
 public:
  // Constructor.  Both endpoints must be in the range -Pi to Pi inclusive.
  // The value -Pi is converted internally to Pi except for the Full()
  // and Empty() intervals.
  S1Interval(double lo, double hi);

  // The default constructor creates an empty interval.
  //
  // Note: Don't construct an interval using the default constructor and
  // set_lo()/set_hi().  If you need to set both endpoints, use the
  // constructor above:
  //
  //   lng_bounds_ = S1Interval(lng_lo, lng_hi);
  S1Interval();

  // Returns the empty interval.
  static S1Interval Empty();

  // Returns the full interval.
  static S1Interval Full();

  // Convenience method to construct an interval containing a single point.
  static S1Interval FromPoint(double p);

  // Convenience method to construct the minimal interval containing
  // the two given points.  This is equivalent to starting with an empty
  // interval and calling AddPoint() twice, but it is more efficient.
  static S1Interval FromPointPair(double p1, double p2);

  // Accessors methods.
  double lo() const { return bounds_[0]; }
  double hi() const { return bounds_[1]; }

  // Methods that allow the S1Interval to be accessed as a vector.  (The
  // recommended style is to use lo() and hi() whenever possible, but these
  // methods are useful when the endpoint to be selected is not constant.)
  //
  // Only const versions of these methods are provided, since S1Interval
  // has invariants that must be maintained after each update.
  double operator[](int i) const { return bounds_[i]; }
  const Vector2_d& bounds() const { return bounds_; }

  // An interval is valid if neither bound exceeds Pi in absolute value,
  // and the value -Pi appears only in the Empty() and Full() intervals.
  bool is_valid() const;

  // Return true if the interval contains all points on the unit circle.
  bool is_full() const { return lo() == -M_PI && hi() == M_PI; }

  // Return true if the interval is empty, i.e. it contains no points.
  bool is_empty() const { return lo() == M_PI && hi() == -M_PI; }

  // Return true if lo() > hi().  (This is true for empty intervals.)
  bool is_inverted() const { return lo() > hi(); }

  // Return the midpoint of the interval.  For full and empty intervals,
  // the result is arbitrary.
  double GetCenter() const;

  // Return the length of the interval.  The length of an empty interval
  // is negative.
  double GetLength() const;

  // Return the complement of the interior of the interval.  An interval and
  // its complement have the same boundary but do not share any interior
  // values.  The complement operator is not a bijection, since the complement
  // of a singleton interval (containing a single value) is the same as the
  // complement of an empty interval.
  S1Interval Complement() const;

  // Return the midpoint of the complement of the interval. For full and empty
  // intervals, the result is arbitrary. For a singleton interval (containing a
  // single point), the result is its antipodal point on S1.
  double GetComplementCenter() const;

  // Return true if the interval (which is closed) contains the point 'p'.
  bool Contains(double p) const;

  // Return true if the interior of the interval contains the point 'p'.
  bool InteriorContains(double p) const;

  // Return true if the interval contains the given interval 'y'.
  // Works for empty, full, and singleton intervals.
  bool Contains(const S1Interval& y) const;

  // Returns true if the interior of this interval contains the entire
  // interval 'y'.  Note that x.InteriorContains(x) is true only when
  // x is the empty or full interval, and x.InteriorContains(S1Interval(p,p))
  // is equivalent to x.InteriorContains(p).
  bool InteriorContains(const S1Interval& y) const;

  // Return true if the two intervals contain any points in common.
  // Note that the point +/-Pi has two representations, so the intervals
  // [-Pi,-3] and [2,Pi] intersect, for example.
  bool Intersects(const S1Interval& y) const;

  // Return true if the interior of this interval contains any point of the
  // interval 'y' (including its boundary).  Works for empty, full, and
  // singleton intervals.
  bool InteriorIntersects(const S1Interval& y) const;

  // Return the Hausdorff distance to the given interval 'y'. For two
  // S1Intervals x and y, this distance is defined by
  //     h(x, y) = max_{p in x} min_{q in y} d(p, q),
  // where d(.,.) is measured along S1.
  double GetDirectedHausdorffDistance(const S1Interval& y) const;

  // Expand the interval by the minimum amount necessary so that it
  // contains the given point "p" (an angle in the range [-Pi, Pi]).
  void AddPoint(double p);

  // Return the closest point in the interval to the given point "p".
  // The interval must be non-empty.
  double Project(double p) const;

  // Return an interval that has been expanded on each side by the given
  // distance "margin".  If "margin" is negative, then shrink the interval on
  // each side by "margin" instead.  The resulting interval may be empty or
  // full.  Any expansion (positive or negative) of a full interval remains
  // full, and any expansion of an empty interval remains empty.
  S1Interval Expanded(double margin) const;

  // Return the smallest interval that contains this interval and the
  // given interval "y".
  S1Interval Union(const S1Interval& y) const;

  // Return the smallest interval that contains the intersection of this
  // interval with "y".  Note that the region of intersection may
  // consist of two disjoint intervals.
  S1Interval Intersection(const S1Interval& y) const;

  // Return true if two intervals contains the same set of points.
  bool operator==(const S1Interval& y) const;

  // Return true if this interval can be transformed into the given interval by
  // moving each endpoint by at most "max_error" (and without the endpoints
  // crossing, which would invert the interval).  Empty and full intervals are
  // considered to start at an arbitrary point on the unit circle, thus any
  // interval with (length <= 2*max_error) matches the empty interval, and any
  // interval with (length >= 2*Pi - 2*max_error) matches the full interval.
  bool ApproxEquals(const S1Interval& y, double max_error = 1e-15) const;

  // Low-level methods to modify one endpoint of an existing S1Interval.
  // These methods should really be private because setting just one endpoint
  // can violate the invariants maintained by S1Interval.  In particular:
  //
  //  - It is not valid to call these methods on an Empty() or Full()
  //    interval, since these intervals do not have any endpoints.
  //
  //  - It is not allowed to set an endpoint to -Pi.  (When these methods are
  //    used internally, values of -Pi have already been normalized to Pi.)
  //
  // The preferred way to modify both endpoints of an interval is to use a
  // constructor, e.g. lng = S1Interval(lng_lo, lng_hi).
  void set_lo(double p);
  void set_hi(double p);

 private:
  enum ArgsChecked { ARGS_CHECKED };

  // Internal constructor that assumes that both arguments are in the
  // correct range, i.e. normalization from -Pi to Pi is already done.
  S1Interval(double lo, double hi, ArgsChecked dummy);

  // Return true if the interval (which is closed) contains the point 'p'.
  // Skips the normalization of 'p' from -Pi to Pi.
  bool FastContains(double p) const;

  Vector2_d bounds_;
};

inline S1Interval::S1Interval(double lo, double hi) : bounds_(lo, hi) {
  if (lo == -M_PI && hi != M_PI) set_lo(M_PI);
  if (hi == -M_PI && lo != M_PI) set_hi(M_PI);
  S2_DCHECK(is_valid());
}

inline S1Interval::S1Interval(double lo, double hi, ArgsChecked dummy)
  : bounds_(lo, hi) {
  S2_DCHECK(is_valid());
}

inline S1Interval::S1Interval() : bounds_(M_PI, -M_PI) {
}

inline S1Interval S1Interval::Empty() {
  return S1Interval();
}

inline S1Interval S1Interval::Full() {
  return S1Interval(-M_PI, M_PI, ARGS_CHECKED);
}

inline bool S1Interval::is_valid() const {
  return (std::fabs(lo()) <= M_PI && std::fabs(hi()) <= M_PI &&
          !(lo() == -M_PI && hi() != M_PI) &&
          !(hi() == -M_PI && lo() != M_PI));
}

inline bool S1Interval::operator==(const S1Interval& y) const {
  return lo() == y.lo() && hi() == y.hi();
}

inline void S1Interval::set_lo(double p) {
  bounds_[0] = p;
  S2_DCHECK(is_valid());
}

inline void S1Interval::set_hi(double p) {
  bounds_[1] = p;
  S2_DCHECK(is_valid());
}

inline std::ostream& operator<<(std::ostream& os, const S1Interval& x) {
  return os << "[" << x.lo() << ", " << x.hi() << "]";
}

#endif  // S2_S1INTERVAL_H_
