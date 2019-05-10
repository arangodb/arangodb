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

#ifndef S2_S2POLYGON_H_
#define S2_S2POLYGON_H_

#include <atomic>
#include <cstddef>
#include <map>
#include <vector>

#include "s2/base/integral_types.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/_fp_contract_off.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s2boolean_operation.h"
#include "s2/s2builder.h"
#include "s2/s2cell_id.h"
#include "s2/s2debug.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2loop.h"
#include "s2/s2polyline.h"
#include "s2/s2region.h"
#include "s2/s2shape_index.h"

class Decoder;
class Encoder;
class S1Angle;
class S2Cap;
class S2Cell;
class S2CellUnion;
class S2Error;
class S2Loop;
class S2PolygonBuilder;
class S2Polyline;
struct S2XYZFaceSiTi;

// An S2Polygon is an S2Region object that represents a polygon.  A polygon is
// defined by zero or more loops; recall that the interior of a loop is
// defined to be its left-hand side (see S2Loop).  There are two different
// conventions for creating an S2Polygon:
//
//   - InitNested() expects the input loops to be nested hierarchically.  The
//     polygon interior then consists of the set of points contained by an odd
//     number of loops.  So for example, a circular region with a hole in it
//     would be defined as two CCW loops, with one loop containing the other.
//     The loops can be provided in any order.
//
//     When the orientation of the input loops is unknown, the nesting
//     requirement is typically met by calling S2Loop::Normalize() on each
//     loop (which inverts the loop if necessary so that it encloses at most
//     half the sphere).  But in fact any set of loops can be used as long as
//     (1) there is no pair of loops that cross, and (2) there is no pair of
//     loops whose union is the entire sphere.
//
//   - InitOriented() expects the input loops to be oriented such that the
//     polygon interior is on the left-hand side of every loop.  So for
//     example, a circular region with a hole in it would be defined using a
//     CCW outer loop and a CW inner loop.  The loop orientations must all be
//     consistent; for example, it is not valid to have one CCW loop nested
//     inside another CCW loop, because the region between the two loops is on
//     the left-hand side of one loop and the right-hand side of the other.
//
// Most clients will not call these methods directly; instead they should use
// S2Builder, which has better support for dealing with imperfect data.
//
// When the polygon is initialized, the given loops are automatically
// converted into a canonical form consisting of "shells" and "holes".  Shells
// and holes are both oriented CCW, and are nested hierarchically.  The loops
// are reordered to correspond to a preorder traversal of the nesting
// hierarchy; InitOriented may also invert some loops. The set of input S2Loop
// pointers is always preserved; the caller can use this to determine how the
// loops were reordered if desired.
//
// Polygons may represent any region of the sphere with a polygonal boundary,
// including the entire sphere (known as the "full" polygon).  The full
// polygon consists of a single full loop (see S2Loop), whereas the empty
// polygon has no loops at all.
//
// Polygons have the following restrictions:
//
//  - Loops may not cross, i.e. the boundary of a loop may not intersect
//    both the interior and exterior of any other loop.
//
//  - Loops may not share edges, i.e. if a loop contains an edge AB, then
//    no other loop may contain AB or BA.
//
//  - Loops may share vertices, however no vertex may appear twice in a
//    single loop (see S2Loop).
//
//  - No loop may be empty.  The full loop may appear only in the full polygon.

class S2Polygon final : public S2Region {
 public:
  // The default constructor creates an empty polygon.  It can be made
  // non-empty by calling Init(), Decode(), etc.
  S2Polygon();

  // Hide these overloads from SWIG to prevent compilation errors.
  // TODO(user): Fix the SWIG wrapping in a better way.
#ifndef SWIG
  // Convenience constructor that calls InitNested() with the given loops.
  //
  // When called with override == S2Debug::ALLOW, the automatic validity
  // checking is controlled by --s2debug (which is true by default in
  // non-optimized builds).  When this flag is enabled, a fatal error is
  // generated whenever an invalid polygon is constructed.
  //
  // With override == S2Debug::DISABLE, the automatic validity checking
  // is disabled.  The main reason to do this is if you intend to call
  // IsValid() explicitly.  (See set_s2debug_override() for details.)
  // Example:
  //
  //   S2Polygon* polygon = new S2Polygon(loops, S2Debug::DISABLE);
  //
  // This is equivalent to:
  //
  //   S2Polygon* polygon = new S2Polygon;
  //   polygon->set_s2debug_override(S2Debug::DISABLE);
  //   polygon->InitNested(loops);
  explicit S2Polygon(std::vector<std::unique_ptr<S2Loop> > loops,
                     S2Debug override = S2Debug::ALLOW);
#endif

  // Convenience constructor that creates a polygon with a single loop
  // corresponding to the given cell.
  explicit S2Polygon(const S2Cell& cell);

#ifndef SWIG
  // Convenience constructor that calls Init(S2Loop*).  Note that this method
  // automatically converts the special empty loop (see S2Loop) into an empty
  // polygon, unlike the vector-of-loops constructor which does not allow
  // empty loops at all.
  explicit S2Polygon(std::unique_ptr<S2Loop> loop,
                     S2Debug override = S2Debug::ALLOW);

  // Create a polygon from a set of hierarchically nested loops.  The polygon
  // interior consists of the points contained by an odd number of loops.
  // (Recall that a loop contains the set of points on its left-hand side.)
  //
  // This method figures out the loop nesting hierarchy and assigns every
  // loop a depth.  Shells have even depths, and holes have odd depths.  Note
  // that the loops are reordered so the hierarchy can be traversed more
  // easily (see GetParent(), GetLastDescendant(), and S2Loop::depth()).
  //
  // This method may be called more than once, in which case any existing
  // loops are deleted before being replaced by the input loops.
  void InitNested(std::vector<std::unique_ptr<S2Loop> > loops);

  // Like InitNested(), but expects loops to be oriented such that the polygon
  // interior is on the left-hand side of all loops.  This implies that shells
  // and holes should have opposite orientations in the input to this method.
  // (During initialization, loops representing holes will automatically be
  // inverted.)
  void InitOriented(std::vector<std::unique_ptr<S2Loop> > loops);

  // Initialize a polygon from a single loop.  Note that this method
  // automatically converts the special empty loop (see S2Loop) into an empty
  // polygon, unlike the vector-of-loops InitNested() method which does not
  // allow empty loops at all.
  void Init(std::unique_ptr<S2Loop> loop);
#endif  // !defined(SWIG)

  // Releases ownership of and returns the loops of this polygon, and resets
  // the polygon to be empty.
  std::vector<std::unique_ptr<S2Loop> > Release();

  // Makes a deep copy of the given source polygon.  The destination polygon
  // will be cleared if necessary.
  void Copy(const S2Polygon* src);

  // Destroys the polygon and frees its loops.
  ~S2Polygon() override;

  // Allows overriding the automatic validity checks controlled by
  // --s2debug (which is true by default in non-optimized builds).
  // When this flag is enabled, a fatal error is generated whenever
  // an invalid polygon is constructed.  The main reason to disable
  // this flag is if you intend to call IsValid() explicitly, like this:
  //
  //   S2Polygon polygon;
  //   polygon.set_s2debug_override(S2Debug::DISABLE);
  //   polygon.Init(...);
  //   if (!polygon.IsValid()) { ... }
  //
  // This setting is preserved across calls to Init() and Decode().
  void set_s2debug_override(S2Debug override);
  S2Debug s2debug_override() const;

  // Returns true if this is a valid polygon (including checking whether all
  // the loops are themselves valid).  Note that validity is checked
  // automatically during initialization when --s2debug is enabled (true by
  // default in debug binaries).
  bool IsValid() const;

  // Returns true if this is *not* a valid polygon and sets "error"
  // appropriately.  Otherwise returns false and leaves "error" unchanged.
  //
  // Note that in error messages, loops that represent holes have their edges
  // numbered in reverse order, starting from the last vertex of the loop.
  //
  // REQUIRES: error != nullptr
  bool FindValidationError(S2Error* error) const;

  // Return true if this is the empty polygon (consisting of no loops).
  bool is_empty() const { return loops_.empty(); }

  // Return true if this is the full polygon (consisting of a single loop that
  // encompasses the entire sphere).
  bool is_full() const { return num_loops() == 1 && loop(0)->is_full(); }

  // Return the number of loops in this polygon.
  int num_loops() const { return static_cast<int>(loops_.size()); }

  // Total number of vertices in all loops.
  int num_vertices() const { return num_vertices_; }

  // Return the loop at the given index.  Note that during initialization, the
  // given loops are reordered according to a preorder traversal of the loop
  // nesting hierarchy.  This implies that every loop is immediately followed
  // by its descendants.  This hierarchy can be traversed using the methods
  // GetParent(), GetLastDescendant(), and S2Loop::depth().
  const S2Loop* loop(int k) const { return loops_[k].get(); }
  S2Loop* loop(int k) { return loops_[k].get(); }

  // Return the index of the parent of loop k, or -1 if it has no parent.
  int GetParent(int k) const;

  // Return the index of the last loop that is contained within loop k.
  // Returns num_loops() - 1 if k < 0.  Note that loops are indexed according
  // to a preorder traversal of the nesting hierarchy, so the immediate
  // children of loop k can be found by iterating over loops
  // (k+1)..GetLastDescendant(k) and selecting those whose depth is equal to
  // (loop(k)->depth() + 1).
  int GetLastDescendant(int k) const;

  // Return the area of the polygon interior, i.e. the region on the left side
  // of an odd number of loops.  The return value is between 0 and 4*Pi.
  double GetArea() const;

  // Return the true centroid of the polygon multiplied by the area of the
  // polygon (see s2centroids.h for details on centroids).  The result is not
  // unit length, so you may want to normalize it.  Also note that in general,
  // the centroid may not be contained by the polygon.
  //
  // We prescale by the polygon area for two reasons: (1) it is cheaper to
  // compute this way, and (2) it makes it easier to compute the centroid of
  // more complicated shapes (by splitting them into disjoint regions and
  // adding their centroids).
  S2Point GetCentroid() const;

  // If all of the polygon's vertices happen to be the centers of S2Cells at
  // some level, then return that level, otherwise return -1.  See also
  // InitToSnapped() and s2builderutil::S2CellIdSnapFunction.
  // Returns -1 if the polygon has no vertices.
  int GetSnapLevel() const;

  // Return the distance from the given point to the polygon interior.  If the
  // polygon is empty, return S1Angle::Infinity().  "x" should be unit length.
  S1Angle GetDistance(const S2Point& x) const;

  // Return the distance from the given point to the polygon boundary.  If the
  // polygon is empty or full, return S1Angle::Infinity() (since the polygon
  // has no boundary).  "x" should be unit length.
  S1Angle GetDistanceToBoundary(const S2Point& x) const;

  // Return the overlap fractions between two polygons, i.e. the ratios of the
  // area of intersection to the area of each polygon.
  static std::pair<double, double> GetOverlapFractions(const S2Polygon* a,
                                                       const S2Polygon* b);

  // If the given point is contained by the polygon, return it.  Otherwise
  // return the closest point on the polygon boundary.  If the polygon is
  // empty, return the input argument.  Note that the result may or may not be
  // contained by the polygon.  "x" should be unit length.
  S2Point Project(const S2Point& x) const;

  // Return the closest point on the polygon boundary to the given point.  If
  // the polygon is empty or full, return the input argument (since the
  // polygon has no boundary).  "x" should be unit length.
  S2Point ProjectToBoundary(const S2Point& x) const;

  // Return true if this polygon contains the given other polygon, i.e.
  // if polygon A contains all points contained by polygon B.
  bool Contains(const S2Polygon* b) const;

  // Returns true if this polgyon (A) approximately contains the given other
  // polygon (B). This is true if it is possible to move the vertices of B
  // no further than "tolerance" such that A contains the modified B.
  //
  // For example, the empty polygon will contain any polygon whose maximum
  // width is no more than "tolerance".
  bool ApproxContains(const S2Polygon* b, S1Angle tolerance) const;

  // Return true if this polygon intersects the given other polygon, i.e.
  // if there is a point that is contained by both polygons.
  bool Intersects(const S2Polygon* b) const;

  // Returns true if this polgyon (A) and the given polygon (B) are
  // approximately disjoint.  This is true if it is possible to ensure that A
  // and B do not intersect by moving their vertices no further than
  // "tolerance".  This implies that in borderline cases where A and B overlap
  // slightly, this method returns true (A and B are approximately disjoint).
  //
  // For example, any polygon is approximately disjoint from a polygon whose
  // maximum width is no more than "tolerance".
  bool ApproxDisjoint(const S2Polygon* b, S1Angle tolerance) const;

  // Initialize this polygon to the intersection, union, difference (A - B),
  // or symmetric difference (XOR) of the given two polygons.
  //
  // "snap_function" allows you to specify a minimum spacing between output
  // vertices, and/or that the vertices should be snapped to a discrete set of
  // points (e.g. S2CellId centers or E7 lat/lng coordinates).  Any snap
  // function can be used, including the IdentitySnapFunction with a
  // snap_radius of zero (which preserves the input vertices exactly).
  //
  // The boundary of the output polygon before snapping is guaranteed to be
  // accurate to within S2::kIntersectionError of the exact result.
  // Snapping can move the boundary by an additional distance that depends on
  // the snap function.  Finally, any degenerate portions of the output
  // polygon are automatically removed (i.e., regions that do not contain any
  // points) since S2Polygon does not allow such regions.
  //
  // See S2Builder and s2builderutil for more details on snap functions.  For
  // example, you can snap to E7 coordinates by setting "snap_function" to
  // s2builderutil::IntLatLngSnapFunction(7).
  //
  // The default snap function is the IdentitySnapFunction with a snap radius
  // of S2::kIntersectionMergeRadius (equal to about 1.8e-15 radians
  // or 11 nanometers on the Earth's surface).  This means that vertices may
  // be positioned arbitrarily, but vertices that are extremely close together
  // can be merged together.  The reason for a non-zero default snap radius is
  // that it helps to eliminate narrow cracks and slivers when T-vertices are
  // present.  For example, adjacent S2Cells at different levels do not share
  // exactly the same boundary, so there can be a narrow crack between them.
  // If a polygon is intersected with those cells and the pieces are unioned
  // together, the result would have a narrow crack unless the snap radius is
  // set to a non-zero value.
  //
  // Note that if you want to encode the vertices in a lower-precision
  // representation (such as S2CellIds or E7), it is much better to use a
  // suitable SnapFunction rather than rounding the vertices yourself, because
  // this will create self-intersections unless you ensure that the vertices
  // and edges are sufficiently well-separated first.  In particular you need
  // to use a snap function whose min_edge_vertex_separation() is at least
  // twice the maximum distance that a vertex can move when rounded.
  void InitToIntersection(const S2Polygon* a, const S2Polygon* b);
  void InitToIntersection(const S2Polygon& a, const S2Polygon& b,
                          const S2Builder::SnapFunction& snap_function);

  void InitToUnion(const S2Polygon* a, const S2Polygon* b);
  void InitToUnion(const S2Polygon& a, const S2Polygon& b,
                   const S2Builder::SnapFunction& snap_function);

  void InitToDifference(const S2Polygon* a, const S2Polygon* b);
  void InitToDifference(const S2Polygon& a, const S2Polygon& b,
                        const S2Builder::SnapFunction& snap_function);

  void InitToSymmetricDifference(const S2Polygon* a, const S2Polygon* b);
  void InitToSymmetricDifference(const S2Polygon& a, const S2Polygon& b,
                                 const S2Builder::SnapFunction& snap_function);

  // Convenience functions that use the IdentitySnapFunction with the given
  // snap radius.  TODO(ericv): Consider deprecating these and require the
  // snap function to be specified explcitly?
  void InitToApproxIntersection(const S2Polygon* a, const S2Polygon* b,
                                S1Angle snap_radius);
  void InitToApproxUnion(const S2Polygon* a, const S2Polygon* b,
                         S1Angle snap_radius);
  void InitToApproxDifference(const S2Polygon* a, const S2Polygon* b,
                              S1Angle snap_radius);
  void InitToApproxSymmetricDifference(const S2Polygon* a, const S2Polygon* b,
                                       S1Angle snap_radius);

  // Snaps the vertices of the given polygon using the given SnapFunction
  // (e.g., s2builderutil::IntLatLngSnapFunction(6) snaps to E6 coordinates).
  // This can change the polygon topology (merging loops, for example), but
  // the resulting polygon is guaranteed to be valid, and no vertex will move
  // by more than snap_function.snap_radius().  See S2Builder for other
  // guarantees (e.g., minimum edge-vertex separation).
  //
  // Note that this method is a thin wrapper over S2Builder, so if you are
  // starting with data that is not in S2Polygon format (e.g., integer E7
  // coordinates) then it is faster to just use S2Builder directly.
  void InitToSnapped(const S2Polygon& polygon,
                     const S2Builder::SnapFunction& snap_function);

  // Convenience function that snaps the vertices to S2CellId centers at the
  // given level (default level 30, which has S2CellId centers spaced about 1
  // centimeter apart).  Polygons can be efficiently encoded by Encode() after
  // they have been snapped.
  void InitToSnapped(const S2Polygon* polygon,
                     int snap_level = S2CellId::kMaxLevel);

  // Snaps the input polygon according to the given "snap_function" and
  // reduces the number of vertices if possible, while ensuring that no vertex
  // moves further than snap_function.snap_radius().
  //
  // Simplification works by replacing nearly straight chains of short edges
  // with longer edges, in a way that preserves the topology of the input
  // polygon up to the creation of degeneracies.  This means that loops or
  // portions of loops may become degenerate, in which case they are removed.
  // For example, if there is a very small island in the original polygon, it
  // may disappear completely.  (Even if there are dense islands, they could
  // all be removed rather than being replaced by a larger simplified island
  // if more area is covered by water than land.)
  void InitToSimplified(const S2Polygon& a,
                        const S2Builder::SnapFunction& snap_function);

  // Like InitToSimplified, except that any vertices or edges on the boundary
  // of the given S2Cell are preserved if possible.  This method requires that
  // the polygon has already been clipped so that it does not extend outside
  // the cell by more than "boundary_tolerance".  In other words, it operates
  // on polygons that have already been intersected with a cell.
  //
  // Typically this method is used in geometry-processing pipelines that
  // intersect polygons with a collection of S2Cells and then process those
  // cells in parallel, where each cell generates some geometry that needs to
  // be simplified.  In contrast, if you just need to simplify the *input*
  // geometry then it is easier and faster to do the simplification before
  // computing the intersection with any S2Cells.
  //
  // "boundary_tolerance" specifies how close a vertex must be to the cell
  // boundary to be kept.  The default tolerance is large enough to handle any
  // reasonable way of interpolating points along the cell boundary, such as
  // S2::GetIntersection(), S2::Interpolate(), or direct (u,v)
  // interpolation using S2::FaceUVtoXYZ().  However, if the vertices have
  // been snapped to a lower-precision representation (e.g., S2CellId centers
  // or E7 coordinates) then you will need to set this tolerance explicitly.
  // For example, if the vertices were snapped to E7 coordinates then
  // "boundary_tolerance" should be set to
  //
  //   s2builderutil::IntLatLngSnapFunction::MinSnapRadiusForExponent(7)
  //
  // Degenerate portions of loops are always removed, so if a vertex on the
  // cell boundary belongs only to degenerate regions then it will not be
  // kept.  For example, if the input polygon is a narrow strip of width less
  // than "snap_radius" along one side of the cell, then the entire loop may
  // become degenerate and be removed.
  //
  // REQUIRES: all vertices of "a" are within "boundary_tolerance" of "cell".
  void InitToSimplifiedInCell(
      const S2Polygon* a, const S2Cell& cell, S1Angle snap_radius,
      S1Angle boundary_tolerance = S1Angle::Radians(1e-15));

  // Initialize this polygon to the complement of the given polygon.
  void InitToComplement(const S2Polygon* a);

  // Invert the polygon (replace it by its complement).
  void Invert();

  // Return true if this polygon contains the given polyline.  This method
  // returns an exact result, according to the following model:
  //
  //  - All edges are geodesics (of course).
  //
  //  - Vertices are ignored for the purposes of defining containment.
  //    (This is because polygons often do not contain their vertices, in
  //    order to that when a set of polygons tiles the sphere then every point
  //    is contained by exactly one polygon.)
  //
  //  - Points that lie exactly on geodesic edges are resolved using symbolic
  //    perturbations (i.e., they are considered to be infinitesmally offset
  //    from the edge).
  //
  //  - If the polygon and polyline share an edge, it is handled as follows.
  //    First, the polygon edges are oriented so that the interior is always
  //    on the left.  Then the shared polyline edge is contained if and only
  //    if it is in the same direction as the corresponding polygon edge.
  //    (This model ensures that when a polyline is intersected with a polygon
  //    and its complement, the edge only appears in one of the two results.)
  //
  // TODO(ericv): Update the implementation to correspond to the model above.
  bool Contains(const S2Polyline& b) const;

  // Returns true if this polgyon approximately contains the given polyline
  // This is true if it is possible to move the polyline vertices no further
  // than "tolerance" such that the polyline is now contained.
  bool ApproxContains(const S2Polyline& b, S1Angle tolerance) const;

  // Return true if this polygon intersects the given polyline.  This method
  // returns an exact result; see Contains(S2Polyline) for details.
  bool Intersects(const S2Polyline& b) const;

  // Returns true if this polgyon is approximately disjoint from the given
  // polyline.  This is true if it is possible to avoid intersection by moving
  // their vertices no further than "tolerance".
  //
  // This implies that in borderline cases where there is a small overlap,
  // this method returns true (i.e., they are approximately disjoint).
  bool ApproxDisjoint(const S2Polyline& b, S1Angle tolerance) const;

#ifndef SWIG
  // Intersect this polygon with the polyline "in" and return the resulting
  // zero or more polylines.  The polylines are returned in the order they
  // would be encountered by traversing "in" from beginning to end.
  // Note that the output may include polylines with only one vertex,
  // but there will not be any zero-vertex polylines.
  //
  // This is equivalent to calling ApproxIntersectWithPolyline() with the
  // "snap_radius" set to S2::kIntersectionMergeRadius.
  std::vector<std::unique_ptr<S2Polyline> > IntersectWithPolyline(
      const S2Polyline& in) const;

  // Similar to IntersectWithPolyline(), except that vertices will be
  // dropped as necessary to ensure that all adjacent vertices in the
  // sequence obtained by concatenating the output polylines will be
  // farther than "snap_radius" apart.  Note that this can change
  // the number of output polylines and/or yield single-vertex polylines.
  std::vector<std::unique_ptr<S2Polyline> > ApproxIntersectWithPolyline(
      const S2Polyline& in, S1Angle snap_radius) const;

  // TODO(ericv): Update documentation.
  std::vector<std::unique_ptr<S2Polyline>> IntersectWithPolyline(
      const S2Polyline& in, const S2Builder::SnapFunction& snap_function) const;

  // Same as IntersectWithPolyline, but subtracts this polygon from
  // the given polyline.
  std::vector<std::unique_ptr<S2Polyline> > SubtractFromPolyline(
      const S2Polyline& in) const;

  // Same as ApproxIntersectWithPolyline, but subtracts this polygon
  // from the given polyline.
  std::vector<std::unique_ptr<S2Polyline> > ApproxSubtractFromPolyline(
      const S2Polyline& in, S1Angle snap_radius) const;

  std::vector<std::unique_ptr<S2Polyline>> SubtractFromPolyline(
      const S2Polyline& in, const S2Builder::SnapFunction& snap_function) const;

  // Return a polygon which is the union of the given polygons.
  static std::unique_ptr<S2Polygon> DestructiveUnion(
      std::vector<std::unique_ptr<S2Polygon> > polygons);
  static std::unique_ptr<S2Polygon> DestructiveApproxUnion(
      std::vector<std::unique_ptr<S2Polygon> > polygons,
      S1Angle snap_radius);
#endif  // !defined(SWIG)

  // Initialize this polygon to the outline of the given cell union.
  // In principle this polygon should exactly contain the cell union and
  // this polygon's inverse should not intersect the cell union, but rounding
  // issues may cause this not to be the case.
  void InitToCellUnionBorder(const S2CellUnion& cells);

  // Return true if every loop of this polygon shares at most one vertex with
  // its parent loop.  Every polygon has a unique normalized form.  A polygon
  // can be normalized by passing it through S2Builder (with no snapping) in
  // order to reconstruct the polygon from its edges.
  //
  // Generally there is no reason to convert polygons to normalized form.  It
  // is mainly useful for testing in order to compare whether two polygons
  // have exactly the same interior, even when they have a different loop
  // structure.  For example, a diamond nested within a square (touching at
  // four points) could be represented as a square with a diamond-shaped hole,
  // or as four triangles.  Methods such as BoundaryApproxEquals() will report
  // these polygons as being different (because they have different
  // boundaries) even though they contain the same points.  However if they
  // are both converted to normalized form (the "four triangles" version) then
  // they can be compared more easily.
  //
  // Also see ApproxEquals(), which can determine whether two polygons contain
  // approximately the same set of points without any need for normalization.
  bool IsNormalized() const;

  // Return true if two polygons have exactly the same loops.  The loops must
  // appear in the same order, and corresponding loops must have the same
  // linear vertex ordering (i.e., cyclic rotations are not allowed).
  bool Equals(const S2Polygon* b) const;

  // Return true if two polygons are approximately equal to within the given
  // tolerance.  This is true if it is possible to move the vertices of the
  // two polygons so that they contain the same set of points.
  //
  // Note that according to this model, small regions less than "tolerance" in
  // width do not need to be considered, since these regions can be collapsed
  // into degenerate loops (which contain no points) by moving their vertices.
  //
  // This model is not as strict as using the Hausdorff distance would be, and
  // it is also not as strict as BoundaryNear (defined below).  However, it is
  // a good choice for comparing polygons that have been snapped, simplified,
  // unioned, etc, since these operations use a model similar to this one
  // (i.e., degenerate loops or portions of loops are automatically removed).
  bool ApproxEquals(const S2Polygon* b, S1Angle tolerance) const;

  // Returns true if two polygons have the same boundary.  More precisely,
  // this method requires that both polygons have loops with the same cyclic
  // vertex order and the same nesting hierarchy.  (This implies that vertices
  // may be cyclically rotated between corresponding loops, and the loop
  // ordering may be different between the two polygons as long as the nesting
  // hierarchy is the same.)
  bool BoundaryEquals(const S2Polygon* b) const;

  // Return true if two polygons have the same boundary except for vertex
  // perturbations.  Both polygons must have loops with the same cyclic vertex
  // order and the same nesting hierarchy, but the vertex locations are
  // allowed to differ by up to "max_error".
  bool BoundaryApproxEquals(const S2Polygon& b,
                            S1Angle max_error = S1Angle::Radians(1e-15)) const;

  // Return true if two polygons have boundaries that are within "max_error"
  // of each other along their entire lengths.  More precisely, there must be
  // a bijection between the two sets of loops such that for each pair of
  // loops, "a_loop->BoundaryNear(b_loop)" is true.
  bool BoundaryNear(const S2Polygon& b,
                    S1Angle max_error = S1Angle::Radians(1e-15)) const;

  // Returns the total number of bytes used by the polygon.
  size_t SpaceUsed() const;

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  // GetRectBound() returns essentially tight results, while GetCapBound()
  // might have a lot of extra padding.  Both bounds are conservative in that
  // if the loop contains a point P, then the bound contains P also.
  S2Polygon* Clone() const override;
  S2Cap GetCapBound() const override;  // Cap surrounding rect bound.
  S2LatLngRect GetRectBound() const override { return bound_; }
  void GetCellUnionBound(std::vector<S2CellId> *cell_ids) const override;

  bool Contains(const S2Cell& cell) const override;
  bool MayIntersect(const S2Cell& cell) const override;

  // The point 'p' does not need to be normalized.
  bool Contains(const S2Point& p) const override;

  // Appends a serialized representation of the S2Polygon to "encoder".
  //
  // The encoding uses about 4 bytes per vertex for typical polygons in
  // Google's geographic repository, assuming that most vertices have been
  // snapped to the centers of S2Cells at some fixed level (typically using
  // InitToSnapped). The remaining vertices are stored using 24 bytes.
  // Decoding a polygon encoded this way always returns the original polygon,
  // without any loss of precision.
  //
  // The snap level is chosen to be the one that has the most vertices snapped
  // to S2Cells at that level.  If most vertices need 24 bytes, then all
  // vertices are encoded this way (this method automatically chooses the
  // encoding that has the best chance of giving the smaller output size).
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void Encode(Encoder* const encoder) const;

  // Encodes the polygon's S2Points directly as three doubles using
  // (40 + 43 * num_loops + 24 * num_vertices) bytes.
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void EncodeUncompressed(Encoder* encoder) const;

  // Decodes a polygon encoded with Encode().  Returns true on success.
  bool Decode(Decoder* const decoder);

  // Decodes a polygon by pointing the S2Loop vertices directly into the
  // decoder's memory buffer (which needs to persist for the lifetime of the
  // decoded S2Polygon).  It is much faster than Decode(), but requires that
  // all the polygon vertices were encoded exactly using 24 bytes per vertex.
  // This essentially requires that the polygon was not snapped beforehand to
  // a given S2Cell level; otherwise this method falls back to Decode().
  //
  // Returns true on success.
  bool DecodeWithinScope(Decoder* const decoder);

#ifndef SWIG
  // Wrapper class for indexing a polygon (see S2ShapeIndex).  Once this
  // object is inserted into an S2ShapeIndex it is owned by that index, and
  // will be automatically deleted when no longer needed by the index.  Note
  // that this class does not take ownership of the polygon itself (see
  // OwningShape below).  You can also subtype this class to store additional
  // data (see S2Shape for details).
  //
  // Note that unlike S2Polygon, the edges of S2Polygon::Shape are directed
  // such that the polygon interior is always on the left.
  class Shape : public S2Shape {
   public:
    static constexpr TypeTag kTypeTag = 1;

    Shape() : polygon_(nullptr), cumulative_edges_(nullptr) {}
    ~Shape() override;

    // Initialization.  Does not take ownership of "polygon".  May be called
    // more than once.
    // TODO(ericv/jrosenstock): Make "polygon" a const reference.
    explicit Shape(const S2Polygon* polygon);
    void Init(const S2Polygon* polygon);

    const S2Polygon* polygon() const { return polygon_; }

    // Encodes the polygon using S2Polygon::Encode().
    void Encode(Encoder* encoder) const {
      polygon_->Encode(encoder);
    }

    // Encodes the polygon using S2Polygon::EncodeUncompressed().
    void EncodeUncompressed(Encoder* encoder) const {
      polygon_->EncodeUncompressed(encoder);
    }

    // Decoding is defined only for S2Polyline::OwningShape below.

    // S2Shape interface:
    int num_edges() const final { return num_edges_; }
    Edge edge(int e) const final;
    int dimension() const final { return 2; }
    ReferencePoint GetReferencePoint() const final;
    int num_chains() const final;
    Chain chain(int i) const final;
    Edge chain_edge(int i, int j) const final;
    ChainPosition chain_position(int e) const final;
    TypeTag type_tag() const override { return kTypeTag; }

   private:
    // The total number of edges in the polygon.  This is the same as
    // polygon_->num_vertices() except in one case (polygon_->is_full()).  On
    // the other hand this field doesn't take up any extra space due to field
    // packing with S2Shape::id_.
    //
    // TODO(ericv): Consider using this field instead as an atomic<int> hint to
    // speed up edge location when there are a large number of loops.  Also
    // consider changing S2Polygon::num_vertices to num_edges instead.
    int num_edges_;

    const S2Polygon* polygon_;

    // An array where element "i" is the total number of edges in loops 0..i-1.
    // This field is only used for polygons that have a large number of loops.
    int* cumulative_edges_;
  };

  // Like Shape, except that the S2Polygon is automatically deleted when this
  // object is deleted by the S2ShapeIndex.  This is useful when an S2Polygon
  // is constructed solely for the purpose of indexing it.
  class OwningShape : public Shape {
   public:
    OwningShape() {}  // Must call Init().

    explicit OwningShape(std::unique_ptr<const S2Polygon> polygon)
        : Shape(polygon.release()) {
    }

    void Init(std::unique_ptr<const S2Polygon> polygon) {
      Shape::Init(polygon.release());
    }

    bool Init(Decoder* decoder) {
      auto polygon = absl::make_unique<S2Polygon>();
      if (!polygon->Decode(decoder)) return false;
      Shape::Init(polygon.release());
      return true;
    }

    ~OwningShape() override { delete polygon(); }
  };
#endif  // SWIG

  // Returns the built-in S2ShapeIndex associated with every S2Polygon.  This
  // can be used in conjunction with the various S2ShapeIndex query classes
  // (S2ClosestEdgeQuery, S2BooleanOperation, etc) to do things beyond what is
  // possible with S2Polygon built-in convenience methods.
  //
  // For example, to measure the distance from one S2Polygon to another, you
  // can write:
  //   S2ClosestEdgeQuery query(&polygon1.index());
  //   S2ClosestEdgeQuery::ShapeIndexTarget target(&polygon2.index());
  //   S1ChordAngle distance = query.GetDistance(&target);
  //
  // The index contains a single S2Polygon::Shape object.
  const MutableS2ShapeIndex& index() const { return index_; }

 private:
  friend class S2Stats;
  friend class PolygonOperation;

  // Given that loops_ contains a single loop, initialize all other fields.
  void InitOneLoop();

  // Compute num_vertices_, bound_, subregion_bound_.
  void InitLoopProperties();

  // Deletes the contents of the loops_ vector and resets the polygon state.
  void ClearLoops();

  // Return true if there is an error in the loop nesting hierarchy.
  bool FindLoopNestingError(S2Error* error) const;

  // A map from each loop to its immediate children with respect to nesting.
  // This map is built during initialization of multi-loop polygons to
  // determine which are shells and which are holes, and then discarded.
  typedef std::map<S2Loop*, std::vector<S2Loop*> > LoopMap;

  void InsertLoop(S2Loop* new_loop, S2Loop* parent, LoopMap* loop_map);
  void InitLoops(LoopMap* loop_map);

  // Add the polygon's loops to the S2ShapeIndex.  (The actual work of
  // building the index only happens when the index is first used.)
  void InitIndex();

  // When the loop is modified (Invert(), or Init() called again) then the
  // indexing structures need to be cleared since they become invalid.
  void ClearIndex();

  // Initializes the polygon to the result of the given boolean operation.
  bool InitToOperation(S2BooleanOperation::OpType op_type,
                       const S2Builder::SnapFunction& snap_function,
                       const S2Polygon& a, const S2Polygon& b);

  // Initializes the polygon from input polygon "a" using the given S2Builder.
  // If the result has an empty boundary (no loops), also decides whether the
  // result should be the full polygon rather than the empty one based on the
  // area of the input polygon.  (See comments in InitToApproxIntersection.)
  void InitFromBuilder(const S2Polygon& a, S2Builder* builder);

  std::vector<std::unique_ptr<S2Polyline> > OperationWithPolyline(
      S2BooleanOperation::OpType op_type,
      const S2Builder::SnapFunction& snap_function,
      const S2Polyline& a) const;

  // Decode a polygon encoded with EncodeUncompressed().  Used by the Decode
  // and DecodeWithinScope methods above.  The within_scope parameter
  // specifies whether to call DecodeWithinScope on the loops.
  bool DecodeUncompressed(Decoder* const decoder, bool within_scope);

  // Encode the polygon's vertices using about 4 bytes / vertex plus 24 bytes /
  // unsnapped vertex. All the loop vertices must be converted first to the
  // S2XYZFaceSiTi format using S2Loop::GetXYZFaceSiTiVertices, and concatenated
  // in the all_vertices array.
  //
  // REQUIRES: snap_level >= 0.
  void EncodeCompressed(Encoder* encoder, const S2XYZFaceSiTi* all_vertices,
                        int snap_level) const;

  // Decode a polygon encoded with EncodeCompressed().
  bool DecodeCompressed(Decoder* decoder);

  static std::vector<std::unique_ptr<S2Polyline> > SimplifyEdgesInCell(
      const S2Polygon& a, const S2Cell& cell,
      double tolerance_uv, S1Angle snap_radius);

  // Internal implementation of intersect/subtract polyline functions above.
  std::vector<std::unique_ptr<S2Polyline> > InternalClipPolyline(
      bool invert, const S2Polyline& a, S1Angle snap_radius) const;

  // Defines a total ordering on S2Loops that does not depend on the cyclic
  // order of loop vertices.  This function is used to choose which loop to
  // invert in the case where several loops have exactly the same area.
  static int CompareLoops(const S2Loop* a, const S2Loop* b);

  std::vector<std::unique_ptr<S2Loop> > loops_;

  // Allows overriding the automatic validity checking controlled by the
  // --s2debug flag.
  S2Debug s2debug_override_;

  // True if InitOriented() was called and the given loops had inconsistent
  // orientations (i.e., it is not possible to construct a polygon such that
  // the interior is on the left-hand side of all loops).  We need to remember
  // this error so that it can be returned later by FindValidationError(),
  // since it is not possible to detect this error once the polygon has been
  // initialized.  This field is not preserved by Encode/Decode.
  uint8 error_inconsistent_loop_orientations_;

  // Cache for num_vertices().
  int num_vertices_;

  // In general we build the index the first time it is needed, but we make an
  // exception for Contains(S2Point) because this method has a simple brute
  // force implementation that is also relatively cheap.  For this one method
  // we keep track of the number of calls made and only build the index once
  // enough calls have been made that we think an index would be worthwhile.
  mutable std::atomic<int32> unindexed_contains_calls_;

  // "bound_" is a conservative bound on all points contained by this polygon:
  // if A.Contains(P), then A.bound_.Contains(S2LatLng(P)).
  S2LatLngRect bound_;

  // Since "bound_" is not exact, it is possible that a polygon A contains
  // another polygon B whose bounds are slightly larger.  "subregion_bound_"
  // has been expanded sufficiently to account for this error, i.e.
  // if A.Contains(B), then A.subregion_bound_.Contains(B.bound_).
  S2LatLngRect subregion_bound_;

  // Spatial index containing this polygon.
  MutableS2ShapeIndex index_;

#ifndef SWIG
  S2Polygon(const S2Polygon&) = delete;
  void operator=(const S2Polygon&) = delete;
#endif
};

#endif  // S2_S2POLYGON_H_
