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
//
// This is a helper class for simplifying polylines.  It allows you to compute
// a maximal edge that intersects a sequence of discs, and that optionally
// avoids a different sequence of discs.  The results are conservative in that
// the edge is guaranteed to intersect or avoid the specified discs using
// exact arithmetic (see s2predicates.h).
//
// Note that S2Builder can also simplify polylines and supports more features
// (e.g., snapping to S2CellId centers), so it is only recommended to use this
// class if S2Builder does not meet your needs.
//
// Here is a simple example showing how to simplify a polyline into a sequence
// of edges that stay within "max_error" of the original edges:
//
//   vector<S2Point> v = { ... };
//   S2PolylineSimplifier simplifier;
//   simplifier.Init(v[0]);
//   for (int i = 1; i < v.size(); ++i) {
//     if (!simplifier.Extend(v[i])) {
//       OutputEdge(simplifier.src(), v[i-1]);
//       simplifier.Init(v[i-1]);
//     }
//     simplifier.TargetDisc(v[i], max_error);
//   }
//   OutputEdge(simplifer.src(), v.back());
//
// Note that the points targeted by TargetDisc do not need to be the same as
// the candidate endpoints passed to Extend.  So for example, you could target
// the original vertices of a polyline, but only consider endpoints that are
// snapped to E7 coordinates or S2CellId centers.
//
// Please be aware that this class works by maintaining a range of acceptable
// angles (bearings) from the start vertex to the hypothetical destination
// vertex.  It does not keep track of distances to any of the discs to be
// targeted or avoided.  Therefore to use this class correctly, constraints
// should be added in increasing order of distance.  (The actual requirement
// is slightly weaker than this, which is why it is not enforced, but
// basically you should only call TargetDisc() and AvoidDisc() with arguments
// that you want to constrain the immediately following call to Extend().)

#ifndef S2_S2POLYLINE_SIMPLIFIER_H_
#define S2_S2POLYLINE_SIMPLIFIER_H_

#include <vector>

#include "s2/_fp_contract_off.h"
#include "s2/s1chord_angle.h"
#include "s2/s1interval.h"

class S2PolylineSimplifier {
 public:
  S2PolylineSimplifier() {}

  // Starts a new simplified edge at "src".
  void Init(const S2Point& src);

  // Returns the source vertex of the output edge.
  S2Point src() const;

  // Returns true if the edge (src, dst) satisfies all of the targeting
  // requirements so far.  Returns false if the edge would be longer than
  // 90 degrees (such edges are not supported).
  bool Extend(const S2Point& dst) const;

  // Requires that the output edge must pass through the given disc.
  bool TargetDisc(const S2Point& point, S1ChordAngle radius);

  // Requires that the output edge must avoid the given disc.  "disc_on_left"
  // specifies whether the disc must be to the left or right of the output
  // edge AB.  (This feature allows the simplified edge to preserve the
  // topology of the original polyline with respect to other nearby points.)
  //
  // More precisely, let AB be the output edge, P be the center of the disc,
  // and r be its radius.  Then this method ensures that
  //
  //   (1) Distance(AB, P) > r, and
  //   (2) if DotProd(AB, AP) > 0, then Sign(ABP) > 0 iff disc_on_left is true.
  //
  // The second condition says that "disc_on_left" has an effect if and only
  // if P is not behind the source vertex A with respect to the direction AB.
  //
  // If your input is a polyline, you can compute "disc_on_left" as follows.
  // Let the polyline be ABCDE and assume that it already avoids a set of
  // points X_i.  Suppose that you have aleady added ABC to the simplifier, and
  // now want to extend the edge chain to D.  First find the X_i that are near
  // the edge CD, then discard the ones such that AX_i <= AC or AX_i >= AD
  // (since these points have either already been considered or aren't
  // relevant yet).  Now X_i is to the left of the polyline if and only if
  // s2pred::OrderedCCW(A, D, X_i, C) (in other words, if X_i is to the left of
  // the angle wedge ACD).  Note that simply testing s2pred::Sign(C, D, X_i)
  // or s2pred::Sign(A, D, X_i) does not handle all cases correctly.
  bool AvoidDisc(const S2Point& point, S1ChordAngle radius, bool disc_on_left);

 private:
  // Unfortunately, the discs to avoid cannot be processed until the direction
  // of the output edge is constrained to lie within an S1Interval of at most
  // 180 degrees.  This happens only when the first target disc is added that
  // does not contain the source vertex.  Until that time we simply store all
  // the discs as ranges of directions to avoid.
  struct RangeToAvoid {
    S1Interval interval;  // Range of directions to avoid.
    bool on_left;         // Is this disc to the left of the output edge?
  };

  double GetDirection(const S2Point& p) const;
  double GetSemiwidth(const S2Point& p, S1ChordAngle r,
                      int round_direction) const;
  void AvoidRange(const S1Interval& avoid_interval, bool disc_on_left);

  S2Point src_;             // Output edge source vertex.
  S2Point x_dir_, y_dir_;   // Orthonormal frame for mapping vectors to angles.
  S1Interval window_;       // Allowable range of angles for the output edge.

  // We store the discs to avoid individually until TargetDisc() is first
  // called with a disc that does not contain the source vertex.  At that time
  // all such discs are processed by using them to constrain "window_", and
  // this vector is cleared.
  std::vector<RangeToAvoid> ranges_to_avoid_;
};

#endif  // S2_S2POLYLINE_SIMPLIFIER_H_
