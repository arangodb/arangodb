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

#ifndef S2_S2LOOP_H_
#define S2_S2LOOP_H_

#include <atomic>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <map>
#include <vector>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2debug.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2loop_measures.h"
#include "s2/s2pointutil.h"
#include "s2/s2region.h"
#include "s2/s2shape_index.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/util/math/matrix3x3.h"
#include "s2/util/math/vector.h"

class Decoder;
class Encoder;
class LoopCrosser;
class LoopRelation;
class MergingIterator;
class S2Cap;
class S2Cell;
class S2CrossingEdgeQuery;
class S2Error;
class S2Loop;
struct S2XYZFaceSiTi;
namespace s2builderutil { class S2PolygonLayer; }

// An S2Loop represents a simple spherical polygon.  It consists of a single
// chain of vertices where the first vertex is implicitly connected to the
// last. All loops are defined to have a CCW orientation, i.e. the interior of
// the loop is on the left side of the edges.  This implies that a clockwise
// loop enclosing a small area is interpreted to be a CCW loop enclosing a
// very large area.
//
// Loops are not allowed to have any duplicate vertices (whether adjacent or
// not).  Non-adjacent edges are not allowed to intersect, and furthermore edges
// of length 180 degrees are not allowed (i.e., adjacent vertices cannot be
// antipodal).  Loops must have at least 3 vertices (except for the empty and
// full loops discussed below).  Although these restrictions are not enforced
// in optimized code, you may get unexpected results if they are violated.
//
// There are two special loops: the "empty loop" contains no points, while the
// "full loop" contains all points.  These loops do not have any edges, but to
// preserve the invariant that every loop can be represented as a vertex
// chain, they are defined as having exactly one vertex each (see kEmpty and
// kFull).
//
// Point containment of loops is defined such that if the sphere is subdivided
// into faces (loops), every point is contained by exactly one face.  This
// implies that loops do not necessarily contain their vertices.
//
// Note: The reason that duplicate vertices and intersecting edges are not
// allowed is that they make it harder to define and implement loop
// relationships, e.g. whether one loop contains another.  If your data does
// not satisfy these restrictions, you can use S2Builder to normalize it.
class S2Loop final : public S2Region {
 public:
  // Default constructor.  The loop must be initialized by calling Init() or
  // Decode() before it is used.
  S2Loop();

  // Convenience constructor that calls Init() with the given vertices.
  explicit S2Loop(const std::vector<S2Point>& vertices);

  // Convenience constructor to disable the automatic validity checking
  // controlled by the --s2debug flag.  Example:
  //
  //   S2Loop* loop = new S2Loop(vertices, S2Debug::DISABLE);
  //
  // This is equivalent to:
  //
  //   S2Loop* loop = new S2Loop;
  //   loop->set_s2debug_override(S2Debug::DISABLE);
  //   loop->Init(vertices);
  //
  // The main reason to use this constructor is if you intend to call
  // IsValid() explicitly.  See set_s2debug_override() for details.
  S2Loop(const std::vector<S2Point>& vertices, S2Debug override);

  // Initialize a loop with given vertices.  The last vertex is implicitly
  // connected to the first.  All points should be unit length.  Loops must
  // have at least 3 vertices (except for the empty and full loops, see
  // kEmpty and kFull).  This method may be called multiple times.
  void Init(const std::vector<S2Point>& vertices);

  // A special vertex chain of length 1 that creates an empty loop (i.e., a
  // loop with no edges that contains no points).  Example usage:
  //
  //    S2Loop empty(S2Loop::kEmpty());
  //
  // The loop may be safely encoded lossily (e.g. by snapping it to an S2Cell
  // center) as long as its position does not move by 90 degrees or more.
  static std::vector<S2Point> kEmpty();

  // A special vertex chain of length 1 that creates a full loop (i.e., a loop
  // with no edges that contains all points).  See kEmpty() for details.
  static std::vector<S2Point> kFull();

  // Construct a loop corresponding to the given cell.
  //
  // Note that the loop and cell *do not* contain exactly the same set of
  // points, because S2Loop and S2Cell have slightly different definitions of
  // point containment.  For example, an S2Cell vertex is contained by all
  // four neighboring S2Cells, but it is contained by exactly one of four
  // S2Loops constructed from those cells.  As another example, the S2Cell
  // coverings of "cell" and "S2Loop(cell)" will be different, because the
  // loop contains points on its boundary that actually belong to other cells
  // (i.e., the covering will include a layer of neighboring cells).
  explicit S2Loop(const S2Cell& cell);

  ~S2Loop() override;

  // Allows overriding the automatic validity checks controlled by the
  // --s2debug flag.  If this flag is true, then loops are automatically
  // checked for validity as they are initialized.  The main reason to disable
  // this flag is if you intend to call IsValid() explicitly, like this:
  //
  //   S2Loop loop;
  //   loop.set_s2debug_override(S2Debug::DISABLE);
  //   loop.Init(...);
  //   if (!loop.IsValid()) { ... }
  //
  // Without the call to set_s2debug_override(), invalid data would cause a
  // fatal error in Init() whenever the --s2debug flag is enabled.
  //
  // This setting is preserved across calls to Init() and Decode().
  void set_s2debug_override(S2Debug override);
  S2Debug s2debug_override() const;

  // Returns true if this is a valid loop.  Note that validity is checked
  // automatically during initialization when --s2debug is enabled (true by
  // default in debug binaries).
  bool IsValid() const;

  // Returns true if this is *not* a valid loop and sets "error"
  // appropriately.  Otherwise returns false and leaves "error" unchanged.
  //
  // REQUIRES: error != nullptr
  bool FindValidationError(S2Error* error) const;

  int num_vertices() const { return num_vertices_; }

  // For convenience, we make two entire copies of the vertex list available:
  // vertex(n..2*n-1) is mapped to vertex(0..n-1), where n == num_vertices().
  //
  // REQUIRES: 0 <= i < 2 * num_vertices()
  const S2Point& vertex(int i) const {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 2 * num_vertices());
    int j = i - num_vertices();
    return vertices_[j < 0 ? i : j];
  }

  // Like vertex(), but this method returns vertices in reverse order if the
  // loop represents a polygon hole.  For example, arguments 0, 1, 2 are
  // mapped to vertices n-1, n-2, n-3, where n == num_vertices().  This
  // ensures that the interior of the polygon is always to the left of the
  // vertex chain.
  //
  // REQUIRES: 0 <= i < 2 * num_vertices()
  const S2Point& oriented_vertex(int i) const {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 2 * num_vertices());
    int j = i - num_vertices();
    if (j < 0) j = i;
    if (is_hole()) j = num_vertices() - 1 - j;
    return vertices_[j];
  }

  // Returns true if this is the special empty loop that contains no points.
  bool is_empty() const;

  // Returns true if this is the special full loop that contains all points.
  bool is_full() const;

  // Returns true if this loop is either empty or full.
  bool is_empty_or_full() const;

  // The depth of a loop is defined as its nesting level within its containing
  // polygon.  "Outer shell" loops have depth 0, holes within those loops have
  // depth 1, shells within those holes have depth 2, etc.  This field is only
  // used by the S2Polygon implementation.
  int depth() const { return depth_; }
  void set_depth(int depth) { depth_ = depth; }

  // Returns true if this loop represents a hole in its containing polygon.
  bool is_hole() const { return (depth_ & 1) != 0; }

  // The sign of a loop is -1 if the loop represents a hole in its containing
  // polygon, and +1 otherwise.
  int sign() const { return is_hole() ? -1 : 1; }

  // Returns true if the loop area is at most 2*Pi.  Degenerate loops are
  // handled consistently with s2pred::Sign(), i.e., if a loop can be
  // expressed as the union of degenerate or nearly-degenerate CCW triangles,
  // then it will always be considered normalized.
  bool IsNormalized() const;

  // Invert the loop if necessary so that the area enclosed by the loop is at
  // most 2*Pi.
  void Normalize();

  // Reverse the order of the loop vertices, effectively complementing the
  // region represented by the loop.  For example, the loop ABCD (with edges
  // AB, BC, CD, DA) becomes the loop DCBA (with edges DC, CB, BA, AD).
  // Notice that the last edge is the same in both cases except that its
  // direction has been reversed.
  void Invert();

  // Returns the area of the loop interior, i.e. the region on the left side of
  // the loop.  The return value is between 0 and 4*Pi.  (Note that the return
  // value is not affected by whether this loop is a "hole" or a "shell".)
  double GetArea() const;

  // Returns the true centroid of the loop multiplied by the area of the loop
  // (see s2centroids.h for details on centroids).  The result is not unit
  // length, so you may want to normalize it.  Also note that in general, the
  // centroid may not be contained by the loop.
  //
  // We prescale by the loop area for two reasons: (1) it is cheaper to
  // compute this way, and (2) it makes it easier to compute the centroid of
  // more complicated shapes (by splitting them into disjoint regions and
  // adding their centroids).
  //
  // Note that the return value is not affected by whether this loop is a
  // "hole" or a "shell".
  S2Point GetCentroid() const;

  // Returns the geodesic curvature of the loop, defined as the sum of the turn
  // angles at each vertex (see S2::TurnAngle).  The result is positive if the
  // loop is counter-clockwise, negative if the loop is clockwise, and zero if
  // the loop is a great circle.  The geodesic curvature is equal to 2*Pi minus
  // the area of the loop.
  //
  // Degenerate and nearly-degenerate loops are handled consistently with
  // s2pred::Sign().  So for example, if a loop has zero area (i.e., it is a
  // very small CCW loop) then its geodesic curvature will always be positive.
  double GetCurvature() const;

  // Returns the maximum error in GetCurvature().  The return value is not
  // constant; it depends on the loop.
  double GetCurvatureMaxError() const;

  // Returns the distance from the given point to the loop interior.  If the
  // loop is empty, return S1Angle::Infinity().  "x" should be unit length.
  S1Angle GetDistance(const S2Point& x) const;

  // Returns the distance from the given point to the loop boundary.  If the
  // loop is empty or full, return S1Angle::Infinity() (since the loop has no
  // boundary).  "x" should be unit length.
  S1Angle GetDistanceToBoundary(const S2Point& x) const;

  // If the given point is contained by the loop, return it.  Otherwise return
  // the closest point on the loop boundary.  If the loop is empty, return the
  // input argument.  Note that the result may or may not be contained by the
  // loop.  "x" should be unit length.
  S2Point Project(const S2Point& x) const;

  // Returns the closest point on the loop boundary to the given point.  If the
  // loop is empty or full, return the input argument (since the loop has no
  // boundary).  "x" should be unit length.
  S2Point ProjectToBoundary(const S2Point& x) const;

  // Returns true if the region contained by this loop is a superset of the
  // region contained by the given other loop.
  bool Contains(const S2Loop* b) const;

  // Returns true if the region contained by this loop intersects the region
  // contained by the given other loop.
  bool Intersects(const S2Loop* b) const;

  // Returns true if two loops have the same vertices in the same linear order
  // (i.e., cyclic rotations are not allowed).
  bool Equals(const S2Loop* b) const;

  // Returns true if two loops have the same boundary.  This is true if and
  // only if the loops have the same vertices in the same cyclic order (i.e.,
  // the vertices may be cyclically rotated).  The empty and full loops are
  // considered to have different boundaries.
  bool BoundaryEquals(const S2Loop* b) const;

  // Returns true if two loops have the same boundary except for vertex
  // perturbations.  More precisely, the vertices in the two loops must be in
  // the same cyclic order, and corresponding vertex pairs must be separated
  // by no more than "max_error".
  bool BoundaryApproxEquals(const S2Loop& b,
                            S1Angle max_error = S1Angle::Radians(1e-15)) const;

  // Returns true if the two loop boundaries are within "max_error" of each
  // other along their entire lengths.  The two loops may have different
  // numbers of vertices.  More precisely, this method returns true if the two
  // loops have parameterizations a:[0,1] -> S^2, b:[0,1] -> S^2 such that
  // distance(a(t), b(t)) <= max_error for all t.  You can think of this as
  // testing whether it is possible to drive two cars all the way around the
  // two loops such that no car ever goes backward and the cars are always
  // within "max_error" of each other.
  bool BoundaryNear(const S2Loop& b,
                    S1Angle max_error = S1Angle::Radians(1e-15)) const;

  // This method computes the oriented surface integral of some quantity f(x)
  // over the loop interior, given a function f_tri(A,B,C) that returns the
  // corresponding integral over the spherical triangle ABC.  Here "oriented
  // surface integral" means:
  //
  // (1) f_tri(A,B,C) must be the integral of f if ABC is counterclockwise,
  //     and the integral of -f if ABC is clockwise.
  //
  // (2) The result of this function is *either* the integral of f over the
  //     loop interior, or the integral of (-f) over the loop exterior.
  //
  // Note that there are at least two common situations where it easy to work
  // around property (2) above:
  //
  //  - If the integral of f over the entire sphere is zero, then it doesn't
  //    matter which case is returned because they are always equal.
  //
  //  - If f is non-negative, then it is easy to detect when the integral over
  //    the loop exterior has been returned, and the integral over the loop
  //    interior can be obtained by adding the integral of f over the entire
  //    unit sphere (a constant) to the result.
  //
  // Also requires that the default constructor for T must initialize the
  // value to zero.  (This is true for built-in types such as "double".)
  template <class T>
  T GetSurfaceIntegral(T f_tri(const S2Point&, const S2Point&, const S2Point&))
      const;

  // Constructs a regular polygon with the given number of vertices, all
  // located on a circle of the specified radius around "center".  The radius
  // is the actual distance from "center" to each vertex.
  static std::unique_ptr<S2Loop> MakeRegularLoop(const S2Point& center,
                                                 S1Angle radius,
                                                 int num_vertices);

  // Like the function above, but this version constructs a loop centered
  // around the z-axis of the given coordinate frame, with the first vertex in
  // the direction of the positive x-axis.  (This allows the loop to be
  // rotated for testing purposes.)
  static std::unique_ptr<S2Loop> MakeRegularLoop(const Matrix3x3_d& frame,
                                                 S1Angle radius,
                                                 int num_vertices);

  // Returnss the total number of bytes used by the loop.
  size_t SpaceUsed() const;

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  S2Loop* Clone() const override;

  // GetRectBound() returns essentially tight results, while GetCapBound()
  // might have a lot of extra padding.  Both bounds are conservative in that
  // if the loop contains a point P, then the bound contains P also.
  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override { return bound_; }

  bool Contains(const S2Cell& cell) const override;
  bool MayIntersect(const S2Cell& cell) const override;

  // The point 'p' does not need to be normalized.
  bool Contains(const S2Point& p) const override;

  // Appends a serialized representation of the S2Loop to "encoder".
  //
  // Generally clients should not use S2Loop::Encode().  Instead they should
  // encode an S2Polygon, which unlike this method supports (lossless)
  // compression.
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void Encode(Encoder* const encoder) const;

  // Decodes a loop encoded with Encode() or the private method
  // EncodeCompressed() (used by the S2Polygon encoder).  Returns true on
  // success.
  //
  // This method may be called with loops that have already been initialized.
  bool Decode(Decoder* const decoder);

  // Provides the same functionality as Decode, except that decoded regions
  // are allowed to point directly into the Decoder's memory buffer rather
  // than copying the data.  This can be much faster, but the decoded loop is
  // only valid within the scope (lifetime) of the Decoder's memory buffer.
  bool DecodeWithinScope(Decoder* const decoder);

  ////////////////////////////////////////////////////////////////////////
  // Methods intended primarily for use by the S2Polygon implementation:

  // Given two loops of a polygon, return true if A contains B.  This version
  // of Contains() is cheap because it does not test for edge intersections.
  // The loops must meet all the S2Polygon requirements; for example this
  // implies that their boundaries may not cross or have any shared edges
  // (although they may have shared vertices).
  bool ContainsNested(const S2Loop* b) const;

  // Returns +1 if A contains the boundary of B, -1 if A excludes the boundary
  // of B, and 0 if the boundaries of A and B cross.  Shared edges are handled
  // as follows: If XY is a shared edge, define Reversed(XY) to be true if XY
  // appears in opposite directions in A and B.  Then A contains XY if and
  // only if Reversed(XY) == B->is_hole().  (Intuitively, this checks whether
  // A contains a vanishingly small region extending from the boundary of B
  // toward the interior of the polygon to which loop B belongs.)
  //
  // This method is used for testing containment and intersection of
  // multi-loop polygons.  Note that this method is not symmetric, since the
  // result depends on the direction of loop A but not on the direction of
  // loop B (in the absence of shared edges).
  //
  // REQUIRES: neither loop is empty.
  // REQUIRES: if b->is_full(), then !b->is_hole().
  int CompareBoundary(const S2Loop* b) const;

  // Given two loops whose boundaries do not cross (see CompareBoundary),
  // return true if A contains the boundary of B.  If "reverse_b" is true, the
  // boundary of B is reversed first (which only affects the result when there
  // are shared edges).  This method is cheaper than CompareBoundary() because
  // it does not test for edge intersections.
  //
  // REQUIRES: neither loop is empty.
  // REQUIRES: if b->is_full(), then reverse_b == false.
  bool ContainsNonCrossingBoundary(const S2Loop* b, bool reverse_b) const;

  // Wrapper class for indexing a loop (see S2ShapeIndex).  Once this object
  // is inserted into an S2ShapeIndex it is owned by that index, and will be
  // automatically deleted when no longer needed by the index.  Note that this
  // class does not take ownership of the loop itself (see OwningShape below).
  // You can also subtype this class to store additional data (see S2Shape for
  // details).
#ifndef SWIG
  class Shape : public S2Shape {
   public:
    Shape() : loop_(nullptr) {}  // Must call Init().

    // Initialize the shape.  Does not take ownership of "loop".
    explicit Shape(const S2Loop* loop) { Init(loop); }
    void Init(const S2Loop* loop) { loop_ = loop; }

    const S2Loop* loop() const { return loop_; }

    // S2Shape interface:
    int num_edges() const final {
      return loop_->is_empty_or_full() ? 0 : loop_->num_vertices();
    }
    Edge edge(int e) const final {
      return Edge(loop_->vertex(e), loop_->vertex(e + 1));
    }
    int dimension() const final { return 2; }
    ReferencePoint GetReferencePoint() const final {
      return ReferencePoint(S2::Origin(), loop_->contains_origin());
    }
    int num_chains() const final;
    Chain chain(int i) const final;
    Edge chain_edge(int i, int j) const final {
      S2_DCHECK_EQ(i, 0);
      return Edge(loop_->vertex(j), loop_->vertex(j + 1));
    }
    ChainPosition chain_position(int e) const final {
      return ChainPosition(0, e);
    }

   private:
    const S2Loop* loop_;
  };

  // Like Shape, except that the S2Loop is automatically deleted when this
  // object is deleted by the S2ShapeIndex.  This is useful when an S2Loop
  // is constructed solely for the purpose of indexing it.
  class OwningShape : public Shape {
   public:
    OwningShape() {}  // Must call Init().
    explicit OwningShape(std::unique_ptr<const S2Loop> loop)
        : Shape(loop.release()) {
    }
    void Init(std::unique_ptr<const S2Loop> loop) {
      Shape::Init(loop.release());
    }
    ~OwningShape() override { delete loop(); }
  };
#endif  // SWIG

 private:
  // All of the following need access to contains_origin().  Possibly this
  // method should be public.
  friend class Shape;
  friend class S2Polygon;
  friend class S2Stats;
  friend class S2LoopTestBase;
  friend class LoopCrosser;
  friend class s2builderutil::S2PolygonLayer;

  // Internal copy constructor used only by Clone() that makes a deep copy of
  // its argument.
  S2Loop(const S2Loop& src);

  // Returns an S2PointLoopSpan containing the loop vertices, for use with the
  // functions defined in s2loop_measures.h.
  S2PointLoopSpan vertices_span() const {
    return S2PointLoopSpan(vertices_, num_vertices());
  }

  // Returns true if this loop contains S2::Origin().
  bool contains_origin() const { return origin_inside_; }

  // The single vertex in the "empty loop" vertex chain.
  static S2Point kEmptyVertex();

  // The single vertex in the "full loop" vertex chain.
  static S2Point kFullVertex();

  void InitOriginAndBound();
  void InitBound();
  void InitIndex();

  // A version of Contains(S2Point) that does not use the S2ShapeIndex.
  // Used by the S2Polygon implementation.
  bool BruteForceContains(const S2Point& p) const;

  // Like FindValidationError(), but skips any checks that would require
  // building the S2ShapeIndex (i.e., self-intersection tests).  This is used
  // by the S2Polygon implementation, which uses its own index to check for
  // loop self-intersections.
  bool FindValidationErrorNoIndex(S2Error* error) const;

  // Internal implementation of the Decode and DecodeWithinScope methods above.
  // If within_scope is true, memory is allocated for vertices_ and data
  // is copied from the decoder using std::copy. If it is false, vertices_
  // will point to the memory area inside the decoder, and the field
  // owns_vertices_ is set to false.
  bool DecodeInternal(Decoder* const decoder, bool within_scope);

  // Converts the loop vertices to the S2XYZFaceSiTi format and store the result
  // in the given array, which must be large enough to store all the vertices.
  void GetXYZFaceSiTiVertices(S2XYZFaceSiTi* vertices) const;

  // Encode the loop's vertices using S2EncodePointsCompressed.  Uses
  // approximately 8 bytes for the first vertex, going down to less than 4 bytes
  // per vertex on Google's geographic repository, plus 24 bytes per vertex that
  // does not correspond to the center of a cell at level 'snap_level'. The loop
  // vertices must first be converted to the S2XYZFaceSiTi format with
  // GetXYZFaceSiTiVertices.
  //
  // REQUIRES: the loop is initialized and valid.
  void EncodeCompressed(Encoder* encoder, const S2XYZFaceSiTi* vertices,
                        int snap_level) const;

  // Decode a loop encoded with EncodeCompressed. The parameters must be the
  // same as the one used when EncodeCompressed was called.
  bool DecodeCompressed(Decoder* decoder, int snap_level);

  // Returns a bitset of properties used by EncodeCompressed
  // to efficiently encode boolean values.  Properties are
  // origin_inside and whether the bound was encoded.
  std::bitset<2> GetCompressedEncodingProperties() const;

  // Given an iterator that is already positioned at the S2ShapeIndexCell
  // containing "p", returns Contains(p).
  bool Contains(const MutableS2ShapeIndex::Iterator& it,
                const S2Point& p) const;

  // Returns true if the loop boundary intersects "target".  It may also
  // return true when the loop boundary does not intersect "target" but
  // some edge comes within the worst-case error tolerance.
  //
  // REQUIRES: it.id().contains(target.id())
  // [This condition is true whenever it.Locate(target) returns INDEXED.]
  bool BoundaryApproxIntersects(const MutableS2ShapeIndex::Iterator& it,
                                const S2Cell& target) const;

  // Returns an index "first" and a direction "dir" such that the vertex
  // sequence (first, first + dir, ..., first + (n - 1) * dir) does not change
  // when the loop vertex order is rotated or reversed.  This allows the loop
  // vertices to be traversed in a canonical order.
  S2::LoopOrder GetCanonicalLoopOrder() const;

  // Returns the index of a vertex at point "p", or -1 if not found.
  // The return value is in the range 1..num_vertices_ if found.
  int FindVertex(const S2Point& p) const;

  // This method checks all edges of loop A for intersection against all edges
  // of loop B.  If there is any shared vertex, the wedges centered at this
  // vertex are sent to "relation".
  //
  // If the two loop boundaries cross, this method is guaranteed to return
  // true.  It also returns true in certain cases if the loop relationship is
  // equivalent to crossing.  For example, if the relation is Contains() and a
  // point P is found such that B contains P but A does not contain P, this
  // method will return true to indicate that the result is the same as though
  // a pair of crossing edges were found (since Contains() returns false in
  // both cases).
  //
  // See Contains(), Intersects() and CompareBoundary() for the three uses of
  // this function.
  static bool HasCrossingRelation(const S2Loop& a, const S2Loop& b,
                                  LoopRelation* relation);

  // When the loop is modified (Invert(), or Init() called again) then the
  // indexing structures need to be cleared since they become invalid.
  void ClearIndex();

  // The nesting depth, if this field belongs to an S2Polygon.  We define it
  // here to optimize field packing.
  int depth_ = 0;

  // We store the vertices in an array rather than a vector because we don't
  // need any STL methods, and computing the number of vertices using size()
  // would be relatively expensive (due to division by sizeof(S2Point) == 24).
  // When DecodeWithinScope is used to initialize the loop, we do not
  // take ownership of the memory for vertices_, and the owns_vertices_ field
  // is used to prevent deallocation and overwriting.
  int num_vertices_ = 0;
  S2Point* vertices_ = nullptr;
  bool owns_vertices_ = false;

  S2Debug s2debug_override_ = S2Debug::ALLOW;
  bool origin_inside_ = false;  // Does the loop contain S2::Origin()?

  // In general we build the index the first time it is needed, but we make an
  // exception for Contains(S2Point) because this method has a simple brute
  // force implementation that is also relatively cheap.  For this one method
  // we keep track of the number of calls made and only build the index once
  // enough calls have been made that we think an index would be worthwhile.
  mutable std::atomic<int32> unindexed_contains_calls_;

  // "bound_" is a conservative bound on all points contained by this loop:
  // if A.Contains(P), then A.bound_.Contains(S2LatLng(P)).
  S2LatLngRect bound_;

  // Since "bound_" is not exact, it is possible that a loop A contains
  // another loop B whose bounds are slightly larger.  "subregion_bound_"
  // has been expanded sufficiently to account for this error, i.e.
  // if A.Contains(B), then A.subregion_bound_.Contains(B.bound_).
  S2LatLngRect subregion_bound_;

  // Spatial index for this loop.
  MutableS2ShapeIndex index_;

  // SWIG doesn't understand "= delete".
#ifndef SWIG
  void operator=(const S2Loop&) = delete;
#endif  // SWIG
};


//////////////////// Implementation Details Follow ////////////////////////


// Any single-vertex loop is interpreted as being either the empty loop or the
// full loop, depending on whether the vertex is in the northern or southern
// hemisphere respectively.
inline S2Point S2Loop::kEmptyVertex() { return S2Point(0, 0, 1); }
inline S2Point S2Loop::kFullVertex() { return S2Point(0, 0, -1); }

inline std::vector<S2Point> S2Loop::kEmpty() {
  return std::vector<S2Point>(1, kEmptyVertex());
}

inline std::vector<S2Point> S2Loop::kFull() {
  return std::vector<S2Point>(1, kFullVertex());
}

inline bool S2Loop::is_empty() const {
  return is_empty_or_full() && !contains_origin();
}

inline bool S2Loop::is_full() const {
  return is_empty_or_full() && contains_origin();
}

inline bool S2Loop::is_empty_or_full() const {
  return num_vertices() == 1;
}

// Since this method is templatized and public, the implementation needs to be
// in the .h file.

template <class T>
T S2Loop::GetSurfaceIntegral(T f_tri(const S2Point&, const S2Point&,
                                     const S2Point&)) const {
  return S2::GetSurfaceIntegral(vertices_span(), f_tri);
}

#endif  // S2_S2LOOP_H_
