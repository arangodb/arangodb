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
#include "s2/s2edge_crossings_internal.h"
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"

// This file defines two classes S2EdgeCrosser and S2CopyingEdgeCrosser that
// allow edges to be efficiently tested for intersection with a given fixed
// edge AB.  They are especially efficient when testing for intersection with
// an edge chain connecting vertices v0, v1, v2, ...
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
// The example above uses S2EdgeCrosser, which requires that the client
// already has all the necessary vertices stored in memory so that this class
// can refer to them with pointers and does not need to make its own copies.
// If this is not the case (e.g., you want to pass temporary objects as
// vertices) then you should use S2CopyingEdgeCrosser, which has exactly the
// same API except that vertices are passed by const reference and do not need
// to persist.
//
// The class below is instantiated twice:
//
//  - For S2EdgeCrosser, ArgType is (const S2Point*) and all points must be
//    stored persistently by the client.
//
//  - For S2CopyingEdgeCrosser, ArgType is (const S2Point&) and points may
//    be temporary objects (since this class makes its own copies).
//
// Note that S2EdgeCrosser is 5-10% faster in real applications when its
// requirements can be met.  (Also note that simple benchmarks are not
// sufficient to measure this performance difference; it seems to have
// something to do with cache performance.)
template <class PointRep>
class S2EdgeCrosserBase {
 private:
  using ArgType = typename PointRep::T;

 public:
  // Default constructor; must be followed by a call to Init().
  S2EdgeCrosserBase() {}

  // Convenience constructor that calls Init() with the given fixed edge AB.
  //
  // For S2EdgeCrosser (only), the arguments "a" and "b" must point to values
  // that persist for the lifetime of the S2EdgeCrosser object.
  //
  // S2EdgeCrosser(const S2Point* a, const S2Point* b);
  // S2CopyingEdgeCrosser(const S2Point& a, const S2Point& b);
  S2EdgeCrosserBase(ArgType a, ArgType b);

  // Accessors for the constructor arguments.
  //
  // const S2Point* S2EdgeCrosser::a();
  // const S2Point* S2EdgeCrosser::b();
  // const S2Point& S2CopyingEdgeCrosser::a();
  // const S2Point& S2CopyingEdgeCrosser::b();
  ArgType a() { return a_; }
  ArgType b() { return b_; }

  // Initialize the object with the given fixed edge AB.
  //
  // For S2EdgeCrosser (only), the arguments "a" and "b" must point to values
  // that persist for the lifetime of the S2EdgeCrosser object.
  //
  // void S2EdgeCrosser::Init(const S2Point* a, const S2Point* b);
  // void S2CopyingEdgeCrosser::Init(const S2Point& a, const S2Point& b);
  void Init(ArgType a, ArgType b);

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
  // For S2EdgeCrosser (only), the arguments must point to values that persist
  // until the next call.
  //
  // int S2EdgeCrosser::CrossingSign(const S2Point* c, const S2Point* d);
  // int S2CopyingEdgeCrosser::CrossingSign(const S2Point& c, const S2Point& d);
  int CrossingSign(ArgType c, ArgType d);

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
  // For S2EdgeCrosser (only), the arguments must point to values that persist
  // until the next call.
  //
  // bool S2EdgeCrosser::EdgeOrVertexCrossing(const S2Point* c,
  //                                          const S2Point* d);
  // bool S2CopyingEdgeCrosser::EdgeOrVertexCrossing(const S2Point& c,
  //                                                 const S2Point& d);
  bool EdgeOrVertexCrossing(ArgType c, ArgType d);

  // Like EdgeOrVertexCrossing() but returns -1 if AB crosses CD from left to
  // right, +1 if AB crosses CD from right to left, and 0 otherwise.  This
  // implies that if CD bounds some region according to the "interior is on
  // the left" rule, this function returns -1 when AB exits the region and +1
  // when AB enters.
  //
  // This method allows computing the change in winding number from point A to
  // point B by summing the signed edge crossings of AB with the edges of the
  // loop(s) used to define the winding number.
  //
  // The arguments must point to values that persist until the next call.
  //
  // int S2EdgeCrosser::SignedEdgeOrVertexCrossing(const S2Point* c,
  //                                               const S2Point* d);
  // int S2CopyingEdgeCrosser::SignedEdgeOrVertexCrossing(const S2Point& c,
  //                                                      const S2Point& d);
  int SignedEdgeOrVertexCrossing(ArgType c, ArgType d);

  // If the preceding call to CrossingSign() returned +1 (indicating that the
  // edge crosses the edge CD), this method returns -1 if AB crossed CD from
  // left to right and +1 if AB crossed CD from right to left.  Otherwise its
  // return value is undefined.
  int last_interior_crossing_sign() const;

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
  // For S2EdgeCrosser (only), the arguments must point to values that persist
  // until the next call.
  //
  // S2EdgeCrosser(S2Point const* a, S2Point const* b, S2Point const* c);
  // S2CopyingEdgeCrosser(S2Point const& a, S2Point const& b, S2Point const& c);
  S2EdgeCrosserBase(ArgType a, ArgType b, ArgType c);

  // Call this method when your chain 'jumps' to a new place.
  //
  // For S2EdgeCrosser (only), the argument must point to a value that
  // persists until the next call.
  //
  // void S2EdgeCrosser::RestartAt(S2Point const* c);
  // void S2CopyingEdgeCrosser::RestartAt(S2Point const& c);
  void RestartAt(ArgType c);

  // Like CrossingSign above, but uses the last vertex passed to one of the
  // crossing methods (or RestartAt) as the first vertex of the current edge.
  //
  // For S2EdgeCrosser (only), the argument must point to a value that
  // persists until the next call.
  //
  // int S2EdgeCrosser::CrossingSign(S2Point const* d);
  // int S2CopyingEdgeCrosser::CrossingSign(S2Point const& d);
  int CrossingSign(ArgType d);

  // Like EdgeOrVertexCrossing above, but uses the last vertex passed to one
  // of the crossing methods (or RestartAt) as the first vertex of the
  // current edge.
  //
  // For S2EdgeCrosser (only), the argument must point to a value that
  // persists until the next call.
  //
  // bool S2EdgeCrosser::EdgeOrVertexCrossing(S2Point const* d);
  // bool S2CopyingEdgeCrosser::EdgeOrVertexCrossing(S2Point const& d);
  bool EdgeOrVertexCrossing(ArgType d);

  // Like EdgeOrVertexCrossing above, but uses the last vertex passed to one
  // of the crossing methods (or RestartAt) as the first vertex of the
  // current edge.
  //
  // For S2EdgeCrosser (only), the argument must point to a value that
  // persists until the next call.
  //
  // int S2EdgeCrosser::SignedEdgeOrVertexCrossing(S2Point const* d);
  // int S2CopyingEdgeCrosser::SignedEdgeOrVertexCrossing(S2Point const& d);
  int SignedEdgeOrVertexCrossing(ArgType d);

  // Returns the last vertex of the current edge chain being tested, i.e.
  // the C vertex that will be used to construct the edge CD when one of the
  // methods above is called.
  //
  // const S2Point* S2EdgeCrosser::c();
  // const S2Point& S2CopyingEdgeCrosser::c();
  ArgType c() { return c_; }

 private:
  // These functions handle the "slow path" of CrossingSign().
  int CrossingSignInternal(PointRep d);
  int CrossingSignInternal2(const S2Point& d);

  // The fields below are constant after the call to Init().
  PointRep a_;
  PointRep b_;
  Vector3_d a_cross_b_;

  // To reduce the number of calls to s2pred::ExpensiveSign(), we compute an
  // outward-facing tangent at A and B if necessary.  If the plane
  // perpendicular to one of these tangents separates AB from CD (i.e., one
  // edge on each side) then there is no intersection.
  bool have_tangents_;  // True if the tangents have been computed.
  S2Point a_tangent_;   // Outward-facing tangent at A.
  S2Point b_tangent_;   // Outward-facing tangent at B.

  // The fields below are updated for each vertex in the chain.  acb_ is
  // initialized to avoid undefined behavior in the case where the edge chain
  // starts with the invalid point (0, 0, 0).
  PointRep c_;          // Previous vertex in the vertex chain.
  int acb_ = 0;         // The orientation of triangle ACB.

  // The field below is a temporary used by CrossingSignInternal().
  int bda_;             // The orientation of triangle BDA.

  S2EdgeCrosserBase(const S2EdgeCrosserBase&) = delete;
  void operator=(const S2EdgeCrosserBase&) = delete;
};

// S2EdgeCrosser implements the API above by using (const S2Point *) as the
// argument type and requiring that all points are stored persistently by the
// client.  If this is not the case, use S2CopyingEdgeCrosser (below).
using S2EdgeCrosser = S2EdgeCrosserBase<S2::internal::S2Point_PointerRep>;

// S2CopyingEdgeCrosser is exactly like S2EdgeCrosser except that it makes its
// own copy of all arguments so that they do not need to persist between
// calls.  This is less efficient, but makes it possible to use points that
// are generated on demand and cannot conveniently be stored by the client.
using S2CopyingEdgeCrosser = S2EdgeCrosserBase<S2::internal::S2Point_ValueRep>;


//////////////////   Implementation details follow   ////////////////////


// Let the compiler know that these classes are explicitly instantiated in the
// .cc file; this helps to reduce compilation time.
extern template class S2EdgeCrosserBase<S2::internal::S2Point_PointerRep>;
extern template class S2EdgeCrosserBase<S2::internal::S2Point_ValueRep>;

template <class PointRep>
inline S2EdgeCrosserBase<PointRep>::S2EdgeCrosserBase(ArgType a, ArgType b)
    : a_(a), b_(b), a_cross_b_(a_->CrossProd(*b_)), have_tangents_(false),
      c_() {
  S2_DCHECK(S2::IsUnitLength(*a_));
  S2_DCHECK(S2::IsUnitLength(*b_));
}

template <class PointRep>
inline void S2EdgeCrosserBase<PointRep>::Init(ArgType a, ArgType b) {
  a_ = a;
  b_ = b;
  a_cross_b_ = a_->CrossProd(*b_);
  have_tangents_ = false;
  c_ = PointRep();
}

template <class PointRep>
inline int S2EdgeCrosserBase<PointRep>::CrossingSign(ArgType c, ArgType d) {
  if (c != c_) RestartAt(c);
  return CrossingSign(d);
}

template <class PointRep>
inline bool S2EdgeCrosserBase<PointRep>::EdgeOrVertexCrossing(ArgType c,
                                                              ArgType d) {
  if (c != c_) RestartAt(c);
  return EdgeOrVertexCrossing(d);
}

template <class PointRep>
inline int S2EdgeCrosserBase<PointRep>::SignedEdgeOrVertexCrossing(ArgType c,
                                                                   ArgType d) {
  if (c != c_) RestartAt(c);
  return SignedEdgeOrVertexCrossing(d);
}

template <class PointRep>
inline int S2EdgeCrosserBase<PointRep>::last_interior_crossing_sign() const {
  // When AB crosses CD, the crossing sign is Sign(ABC).  S2EdgeCrosser doesn't
  // store this, but it does store the sign of the *next* triangle ACB.  These
  // two values happen to be the same.
  return acb_;
}

template <class PointRep>
inline S2EdgeCrosserBase<PointRep>::S2EdgeCrosserBase(
    ArgType a, ArgType b, ArgType c)
    : a_(a), b_(b), a_cross_b_(a_->CrossProd(*b_)), have_tangents_(false) {
  S2_DCHECK(S2::IsUnitLength(*a_));
  S2_DCHECK(S2::IsUnitLength(*b_));
  RestartAt(c);
}

template <class PointRep>
inline void S2EdgeCrosserBase<PointRep>::RestartAt(ArgType c) {
  c_ = c;
  S2_DCHECK(S2::IsUnitLength(*c_));
  acb_ = -s2pred::TriageSign(*a_, *b_, *c_, a_cross_b_);
}

template <class PointRep>
inline int S2EdgeCrosserBase<PointRep>::CrossingSign(ArgType d_arg) {
  PointRep d(d_arg);
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

template <class PointRep>
inline bool S2EdgeCrosserBase<PointRep>::EdgeOrVertexCrossing(ArgType d) {
  // We need to copy c_ since it is clobbered by CrossingSign().
  PointRep c = c_;
  int crossing = CrossingSign(d);
  if (crossing < 0) return false;
  if (crossing > 0) return true;
  return S2::VertexCrossing(*a_, *b_, *c, *PointRep(d));
}

template <class PointRep>
inline int S2EdgeCrosserBase<PointRep>::SignedEdgeOrVertexCrossing(ArgType d) {
  // We need to copy c_ since it is clobbered by CrossingSign().
  PointRep c = c_;
  int crossing = CrossingSign(d);
  if (crossing < 0) return 0;
  if (crossing > 0) return last_interior_crossing_sign();
  return S2::SignedVertexCrossing(*a_, *b_, *c, *PointRep(d));
}

#endif  // S2_S2EDGE_CROSSER_H_
