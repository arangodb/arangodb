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

#ifndef S2_S2EDGE_CROSSER_H_
#define S2_S2EDGE_CROSSER_H_

#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"

class S2CopyingEdgeCrosser;  // Forward declaration

// This class allows edges to be efficiently tested for intersection with a
// given fixed edge AB.  It is especially efficient when testing for
// intersection with an edge chain connecting vertices v0, v1, v2, ...
//
// Example usage:
//
//   void CountIntersections(const S2Point& a, const S2Point& b,
//                           const vector<pair<S2Point, S2Point>>& edges) {
//     int count = 0;
//     S2EdgeCrosser crosser(&a, &b);
//     for (const auto& edge : edges) {
//       if (crosser.CrossingSign(&edge.first, &edge.second) >= 0) {
//         ++count;
//       }
//     }
//     return count;
//   }
//
// This class expects that the client already has all the necessary vertices
// stored in memory, so that this class can refer to them with pointers and
// does not need to make its own copies.  If this is not the case (e.g., you
// want to pass temporary objects as vertices), see S2CopyingEdgeCrosser.
class S2EdgeCrosser {
 public:
  // Default constructor; must be followed by a call to Init().
  S2EdgeCrosser() {}

  // Convenience constructor that calls Init() with the given fixed edge AB.
  // The arguments "a" and "b" must point to values that persist for the
  // lifetime of the S2EdgeCrosser object (or until the next Init() call).
  S2EdgeCrosser(const S2Point* a, const S2Point* b);

  // Accessors for the constructor arguments.
  const S2Point* a() { return a_; }
  const S2Point* b() { return b_; }

  // Initialize the S2EdgeCrosser with the given fixed edge AB.  The arguments
  // "a" and "b" must point to values that persist for the lifetime of the
  // S2EdgeCrosser object (or until the next Init() call).
  void Init(const S2Point* a, const S2Point* b);

  // This function determines whether the edge AB intersects the edge CD.
  // Returns +1 if AB crosses CD at a point that is interior to both edges.
  // Returns  0 if any two vertices from different edges are the same.
  // Returns -1 otherwise.
  //
  // Note that if an edge is degenerate (A == B or C == D), the return value
  // is 0 if two vertices from different edges are the same and -1 otherwise.
  //
  // Properties of CrossingSign:
  //
  //  (1) CrossingSign(b,a,c,d) == CrossingSign(a,b,c,d)
  //  (2) CrossingSign(c,d,a,b) == CrossingSign(a,b,c,d)
  //  (3) CrossingSign(a,b,c,d) == 0 if a==c, a==d, b==c, b==d
  //  (3) CrossingSign(a,b,c,d) <= 0 if a==b or c==d (see above)
  //
  // This function implements an exact, consistent perturbation model such
  // that no three points are ever considered to be collinear.  This means
  // that even if you have 4 points A, B, C, D that lie exactly in a line
  // (say, around the equator), C and D will be treated as being slightly to
  // one side or the other of AB.  This is done in a way such that the
  // results are always consistent (see s2pred::Sign).
  //
  // Note that if you want to check an edge against a chain of other edges,
  // it is slightly more efficient to use the single-argument version of
  // CrossingSign below.
  //
  // The arguments must point to values that persist until the next call.
  int CrossingSign(const S2Point* c, const S2Point* d);

  // This method extends the concept of a "crossing" to the case where AB
  // and CD have a vertex in common.  The two edges may or may not cross,
  // according to the rules defined in VertexCrossing() below.  The rules
  // are designed so that point containment tests can be implemented simply
  // by counting edge crossings.  Similarly, determining whether one edge
  // chain crosses another edge chain can be implemented by counting.
  //
  // Returns true if CrossingSign(c, d) > 0, or AB and CD share a vertex
  // and VertexCrossing(a, b, c, d) returns true.
  //
  // The arguments must point to values that persist until the next call.
  bool EdgeOrVertexCrossing(const S2Point* c, const S2Point* d);

  ///////////////////////// Edge Chain Methods ///////////////////////////
  //
  // You don't need to use these unless you're trying to squeeze out every
  // last drop of performance.  Essentially all you are saving is a test
  // whether the first vertex of the current edge is the same as the second
  // vertex of the previous edge.  Example usage:
  //
  //   vector<S2Point> chain;
  //   crosser.RestartAt(&chain[0]);
  //   for (int i = 1; i < chain.size(); ++i) {
  //     if (crosser.EdgeOrVertexCrossing(&chain[i])) { ++count; }
  //   }

  // Convenience constructor that uses AB as the fixed edge, and C as the
  // first vertex of the vertex chain (equivalent to calling RestartAt(c)).
  //
  // The arguments must point to values that persist until the next call.
  S2EdgeCrosser(const S2Point* a, const S2Point* b, const S2Point* c);

  // Call this method when your chain 'jumps' to a new place.
  // The argument must point to a value that persists until the next call.
  void RestartAt(const S2Point* c);

  // Like CrossingSign above, but uses the last vertex passed to one of
  // the crossing methods (or RestartAt) as the first vertex of the current
  // edge.
  //
  // The argument must point to a value that persists until the next call.
  int CrossingSign(const S2Point* d);

  // Like EdgeOrVertexCrossing above, but uses the last vertex passed to one
  // of the crossing methods (or RestartAt) as the first vertex of the
  // current edge.
  //
  // The argument must point to a value that persists until the next call.
  bool EdgeOrVertexCrossing(const S2Point* d);

  // Returns the last vertex of the current edge chain being tested, i.e. the
  // C vertex that will be used to construct the edge CD when one of the
  // methods above is called.
  const S2Point* c() { return c_; }

 private:
  friend class S2CopyingEdgeCrosser;

  // These functions handle the "slow path" of CrossingSign().
  int CrossingSignInternal(const S2Point* d);
  int CrossingSignInternal2(const S2Point& d);

  // Used internally by S2CopyingEdgeCrosser.  Updates "c_" only.
  void set_c(const S2Point* c) { c_ = c; }

  // The fields below are constant after the call to Init().
  const S2Point* a_;
  const S2Point* b_;
  Vector3_d a_cross_b_;

  // To reduce the number of calls to s2pred::ExpensiveSign(), we compute an
  // outward-facing tangent at A and B if necessary.  If the plane
  // perpendicular to one of these tangents separates AB from CD (i.e., one
  // edge on each side) then there is no intersection.
  bool have_tangents_;  // True if the tangents have been computed.
  S2Point a_tangent_;   // Outward-facing tangent at A.
  S2Point b_tangent_;   // Outward-facing tangent at B.

  // The fields below are updated for each vertex in the chain.
  const S2Point* c_;       // Previous vertex in the vertex chain.
  int acb_;                // The orientation of triangle ACB.

  // The field below is a temporary used by CrossingSignInternal().
  int bda_;                // The orientation of triangle BDA.

  S2EdgeCrosser(const S2EdgeCrosser&) = delete;
  void operator=(const S2EdgeCrosser&) = delete;
};

// S2CopyingEdgeCrosser is exactly like S2EdgeCrosser, except that it makes its
// own copy of all arguments so that they do not need to persist between
// calls.  This is less efficient, but makes it possible to use points that
// are generated on demand and cannot conveniently be stored by the client.
class S2CopyingEdgeCrosser {
 public:
  // These methods are all exactly like S2EdgeCrosser, except that the
  // arguments can be temporaries.
  S2CopyingEdgeCrosser() {}
  S2CopyingEdgeCrosser(const S2Point& a, const S2Point& b);
  const S2Point& a() { return a_; }
  const S2Point& b() { return b_; }
  const S2Point& c() { return c_; }
  void Init(const S2Point& a, const S2Point& b);
  int CrossingSign(const S2Point& c, const S2Point& d);
  bool EdgeOrVertexCrossing(const S2Point& c, const S2Point& d);
  S2CopyingEdgeCrosser(const S2Point& a, const S2Point& b, const S2Point& c);
  void RestartAt(const S2Point& c);
  int CrossingSign(const S2Point& d);
  bool EdgeOrVertexCrossing(const S2Point& d);

 private:
  S2Point a_, b_, c_;
  // TODO(ericv): It would be more efficient to implement S2CopyingEdgeCrosser
  // directly rather than as a wrapper around S2EdgeCrosser.
  S2EdgeCrosser crosser_;

  S2CopyingEdgeCrosser(const S2CopyingEdgeCrosser&) = delete;
  void operator=(const S2CopyingEdgeCrosser&) = delete;
};


//////////////////   Implementation details follow   ////////////////////


inline S2EdgeCrosser::S2EdgeCrosser(const S2Point* a, const S2Point* b)
    : a_(a), b_(b), a_cross_b_(a_->CrossProd(*b_)), have_tangents_(false),
      c_(nullptr) {
  S2_DCHECK(S2::IsUnitLength(*a));
  S2_DCHECK(S2::IsUnitLength(*b));
}

inline void S2EdgeCrosser::Init(const S2Point* a, const S2Point* b) {
  a_ = a;
  b_ = b;
  a_cross_b_ = a->CrossProd(*b_);
  have_tangents_ = false;
  c_ = nullptr;
}

inline int S2EdgeCrosser::CrossingSign(const S2Point* c, const S2Point* d) {
  if (c != c_) RestartAt(c);
  return CrossingSign(d);
}

inline bool S2EdgeCrosser::EdgeOrVertexCrossing(const S2Point* c,
                                                const S2Point* d) {
  if (c != c_) RestartAt(c);
  return EdgeOrVertexCrossing(d);
}

inline S2EdgeCrosser::S2EdgeCrosser(
    const S2Point* a, const S2Point* b, const S2Point* c)
    : a_(a), b_(b), a_cross_b_(a_->CrossProd(*b_)), have_tangents_(false) {
  S2_DCHECK(S2::IsUnitLength(*a));
  S2_DCHECK(S2::IsUnitLength(*b));
  RestartAt(c);
}

inline void S2EdgeCrosser::RestartAt(const S2Point* c) {
  S2_DCHECK(S2::IsUnitLength(*c));
  c_ = c;
  acb_ = -s2pred::TriageSign(*a_, *b_, *c_, a_cross_b_);
}

inline int S2EdgeCrosser::CrossingSign(const S2Point* d) {
  S2_DCHECK(S2::IsUnitLength(*d));
  // For there to be an edge crossing, the triangles ACB, CBD, BDA, DAC must
  // all be oriented the same way (CW or CCW).  We keep the orientation of ACB
  // as part of our state.  When each new point D arrives, we compute the
  // orientation of BDA and check whether it matches ACB.  This checks whether
  // the points C and D are on opposite sides of the great circle through AB.

  // Recall that TriageSign is invariant with respect to rotating its
  // arguments, i.e. ABD has the same orientation as BDA.
  int bda = s2pred::TriageSign(*a_, *b_, *d, a_cross_b_);
  if (acb_ == -bda && bda != 0) {
    // The most common case -- triangles have opposite orientations.  Save the
    // current vertex D as the next vertex C, and also save the orientation of
    // the new triangle ACB (which is opposite to the current triangle BDA).
    c_ = d;
    acb_ = -bda;
    return -1;
  }
  bda_ = bda;
  return CrossingSignInternal(d);
}

inline bool S2EdgeCrosser::EdgeOrVertexCrossing(const S2Point* d) {
  // We need to copy c_ since it is clobbered by CrossingSign().
  const S2Point* c = c_;
  int crossing = CrossingSign(d);
  if (crossing < 0) return false;
  if (crossing > 0) return true;
  return S2::VertexCrossing(*a_, *b_, *c, *d);
}

inline S2CopyingEdgeCrosser::S2CopyingEdgeCrosser(const S2Point& a,
                                                  const S2Point& b)
    : a_(a), b_(b), c_(S2Point()), crosser_(&a_, &b_) {
}

inline void S2CopyingEdgeCrosser::Init(const S2Point& a, const S2Point& b) {
  a_ = a;
  b_ = b;
  c_ = S2Point();
  crosser_.Init(&a_, &b_);
}

inline int S2CopyingEdgeCrosser::CrossingSign(const S2Point& c,
                                              const S2Point& d) {
  if (c != c_ || crosser_.c_ == nullptr) RestartAt(c);
  return CrossingSign(d);
}

inline bool S2CopyingEdgeCrosser::EdgeOrVertexCrossing(
    const S2Point& c, const S2Point& d) {
  if (c != c_ || crosser_.c_ == nullptr) RestartAt(c);
  return EdgeOrVertexCrossing(d);
}

inline S2CopyingEdgeCrosser::S2CopyingEdgeCrosser(
    const S2Point& a, const S2Point& b, const S2Point& c)
    : a_(a), b_(b), c_(c), crosser_(&a_, &b_, &c) {
}

inline void S2CopyingEdgeCrosser::RestartAt(const S2Point& c) {
  c_ = c;
  crosser_.RestartAt(&c_);
}

inline int S2CopyingEdgeCrosser::CrossingSign(const S2Point& d) {
  int result = crosser_.CrossingSign(&d);
  c_ = d;
  crosser_.set_c(&c_);
  return result;
}

inline bool S2CopyingEdgeCrosser::EdgeOrVertexCrossing(const S2Point& d) {
  bool result = crosser_.EdgeOrVertexCrossing(&d);
  c_ = d;
  crosser_.set_c(&c_);
  return result;
}

#endif  // S2_S2EDGE_CROSSER_H_
