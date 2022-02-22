// Copyright 2013 Google Inc. All Rights Reserved.
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

#ifndef S2_S2CLOSEST_EDGE_QUERY_H_
#define S2_S2CLOSEST_EDGE_QUERY_H_

#include <memory>
#include <queue>
#include <type_traits>
#include <vector>

#include "s2/base/logging.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/third_party/absl/container/inlined_vector.h"
#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2closest_edge_query_base.h"
#include "s2/s2edge_distances.h"
#include "s2/s2min_distance_targets.h"
#include "s2/s2shape_index.h"

// S2ClosestEdgeQuery is a helper class for finding the closest edge(s) to a
// given point, edge, S2Cell, or geometry collection.  For example, given a
// set of polylines, the following code efficiently finds the closest 5 edges
// to a query point:
//
// void Test(const vector<S2Polyline*>& polylines, const S2Point& point) {
//   MutableS2ShapeIndex index;
//   for (S2Polyline* polyline : polylines) {
//     index.Add(new S2Polyline::Shape(polyline));
//   }
//   S2ClosestEdgeQuery query(&index);
//   query.mutable_options()->set_max_results(5);
//   S2ClosestEdgeQuery::PointTarget target(point);
//   for (const auto& result : query.FindClosestEdges(&target)) {
//     // The Result struct contains the following fields:
//     //   "distance" is the distance to the edge.
//     //   "shape_id" identifies the S2Shape containing the edge.
//     //   "edge_id" identifies the edge with the given shape.
//     // The following convenience methods may also be useful:
//     //   query.GetEdge(result) returns the endpoints of the edge.
//     //   query.Project(point, result) computes the closest point on the
//     //       result edge to the given target point.
//     int polyline_index = result.shape_id;
//     int edge_index = result.edge_id;
//     S1ChordAngle distance = result.distance;  // Use ToAngle() for S1Angle.
//     S2Shape::Edge edge = query.GetEdge(result);
//     S2Point closest_point = query.Project(point, result);
//   }
// }
//
// You can find either the k closest edges, or all edges within a given
// radius, or both (i.e., the k closest edges up to a given maximum radius).
// E.g. to find all the edges within 5 kilometers, call
//
//   query.mutable_options()->set_max_distance(
//       S2Earth::ToAngle(util::units::Kilometers(5)));
//
// By default *all* edges are returned, so you should always specify either
// max_results() or max_distance() or both.  There is also a FindClosestEdge()
// convenience method that returns only the closest edge.
//
// Note that by default, distances are measured to the boundary and interior
// of polygons.  For example, if a point is inside a polygon then its distance
// is zero.  To change this behavior, call set_include_interiors(false).
//
// If you only need to test whether the distance is above or below a given
// threshold (e.g., 10 km), you can use the IsDistanceLess() method.  This is
// much faster than actually calculating the distance with FindClosestEdge(),
// since the implementation can stop as soon as it can prove that the minimum
// distance is either above or below the threshold.
//
// To find the closest edges to a query edge rather than a point, use:
//
//   S2ClosestEdgeQuery::EdgeTarget target(v0, v1);
//   query.FindClosestEdges(&target);
//
// Similarly you can find the closest edges to an S2Cell by using an
// S2ClosestEdgeQuery::CellTarget, and you can find the closest edges to an
// arbitrary collection of points, polylines, and polygons by using an
// S2ClosestEdgeQuery::ShapeIndexTarget.
//
// The implementation is designed to be fast for both simple and complex
// geometric objects.
class S2ClosestEdgeQuery {
 public:
  // See S2ClosestEdgeQueryBase for full documentation.

  // S2MinDistance is a thin wrapper around S1ChordAngle that implements the
  // Distance concept required by S2ClosestPointQueryBase.
  using Distance = S2MinDistance;
  using Base = S2ClosestEdgeQueryBase<Distance>;

  // Each "Result" object represents a closest edge.  It has the following
  // fields:
  //
  //   S1ChordAngle distance;  // The distance from the target to this edge.
  //   int32 shape_id;         // Identifies an indexed shape.
  //   int32 edge_id;          // Identifies an edge within the shape.
  using Result = Base::Result;

  // Options that control the set of edges returned.  Note that by default
  // *all* edges are returned, so you will always want to set either the
  // max_results() option or the max_distance() option (or both).
  class Options : public Base::Options {
   public:
    // See S2ClosestEdgeQueryBase::Options for the full set of options.

    // Specifies that only edges whose distance to the target is less than
    // "max_distance" should be returned.
    //
    // Note that edges whose distance is exactly equal to "max_distance" are
    // not returned.  Normally this doesn't matter, because distances are not
    // computed exactly in the first place, but if such edges are needed then
    // see set_inclusive_max_distance() below.
    //
    // DEFAULT: Distance::Infinity()
    void set_max_distance(S1ChordAngle max_distance);

    // Like set_max_distance(), except that edges whose distance is exactly
    // equal to "max_distance" are also returned.  Equivalent to calling
    // set_max_distance(max_distance.Successor()).
    void set_inclusive_max_distance(S1ChordAngle max_distance);

    // Like set_inclusive_max_distance(), except that "max_distance" is also
    // increased by the maximum error in the distance calculation.  This
    // ensures that all edges whose true distance is less than or equal to
    // "max_distance" will be returned (along with some edges whose true
    // distance is slightly greater).
    //
    // Algorithms that need to do exact distance comparisons can use this
    // option to find a set of candidate edges that can then be filtered
    // further (e.g., using s2pred::CompareDistance).
    void set_conservative_max_distance(S1ChordAngle max_distance);

    // Versions of set_max_distance that take an S1Angle argument.  (Note that
    // these functions require a conversion, and that the S1ChordAngle versions
    // are preferred.)
    void set_max_distance(S1Angle max_distance);
    void set_inclusive_max_distance(S1Angle max_distance);
    void set_conservative_max_distance(S1Angle max_distance);

    // See S2ClosestEdgeQueryBase::Options for documentation.
    using Base::Options::set_max_error;     // S1Chordangle version
    void set_max_error(S1Angle max_error);  // S1Angle version

    // Inherited options (see s2closest_edge_query_base.h for details):
    using Base::Options::set_max_results;
    using Base::Options::set_include_interiors;
    using Base::Options::set_use_brute_force;
  };

  // "Target" represents the geometry to which the distance is measured.
  // There are subtypes for measuring the distance to a point, an edge, an
  // S2Cell, or an S2ShapeIndex (an arbitrary collection of geometry).
  using Target = S2MinDistanceTarget;

  // Target subtype that computes the closest distance to a point.
  class PointTarget final : public S2MinDistancePointTarget {
   public:
    explicit PointTarget(const S2Point& point);
    int max_brute_force_index_size() const override;
  };

  // Target subtype that computes the closest distance to an edge.
  class EdgeTarget final : public S2MinDistanceEdgeTarget {
   public:
    explicit EdgeTarget(const S2Point& a, const S2Point& b);
    int max_brute_force_index_size() const override;
  };

  // Target subtype that computes the closest distance to an S2Cell
  // (including the interior of the cell).
  class CellTarget final : public S2MinDistanceCellTarget {
   public:
    explicit CellTarget(const S2Cell& cell);
    int max_brute_force_index_size() const override;
  };

  // Target subtype that computes the closest distance to an S2ShapeIndex
  // (an arbitrary collection of points, polylines, and/or polygons).
  //
  // By default, distances are measured to the boundary and interior of
  // polygons in the S2ShapeIndex rather than to polygon boundaries only.
  // If you wish to change this behavior, you may call
  //
  //   target.set_include_interiors(false);
  //
  // (see S2MinDistanceShapeIndexTarget for details).
  class ShapeIndexTarget final : public S2MinDistanceShapeIndexTarget {
   public:
    explicit ShapeIndexTarget(const S2ShapeIndex* index);
    int max_brute_force_index_size() const override;
  };

  // Convenience constructor that calls Init().  Options may be specified here
  // or changed at any time using the mutable_options() accessor method.
  explicit S2ClosestEdgeQuery(const S2ShapeIndex* index,
                              const Options& options = Options());

  // Default constructor; requires Init() to be called.
  S2ClosestEdgeQuery();
  ~S2ClosestEdgeQuery();

  // Initializes the query.  Options may be specified here or changed at any
  // time using the mutable_options() accessor method.
  //
  // REQUIRES: "index" must persist for the lifetime of this object.
  // REQUIRES: ReInit() must be called if "index" is modified.
  void Init(const S2ShapeIndex* index, const Options& options = Options());

  // Reinitializes the query.  This method must be called whenever the
  // underlying S2ShapeIndex is modified.
  void ReInit();

  // Returns a reference to the underlying S2ShapeIndex.
  const S2ShapeIndex& index() const;

  // Returns the query options.  Options can be modified between queries.
  const Options& options() const;
  Options* mutable_options();

  // Returns the closest edges to the given target that satisfy the current
  // options.  This method may be called multiple times.
  //
  // Note that if options().include_interiors() is true, the result vector may
  // include some entries with edge_id == -1.  This indicates that the target
  // intersects the indexed polygon with the given shape_id.
  std::vector<Result> FindClosestEdges(Target* target);

  // This version can be more efficient when this method is called many times,
  // since it does not require allocating a new vector on each call.
  void FindClosestEdges(Target* target, std::vector<Result>* results);

  //////////////////////// Convenience Methods ////////////////////////

  // Returns the closest edge to the target.  If no edge satisfies the search
  // criteria, then the Result object will have distance == Infinity(),
  // is_empty() == true, and shape_id == edge_id == -1.
  //
  // Note that if options.include_interiors() is true, edge_id == -1 is also
  // used to indicate that the target intersects an indexed polygon (but in
  // that case distance == Zero() and shape_id >= 0).
  Result FindClosestEdge(Target* target);

  // Returns the minimum distance to the target.  If the index or target is
  // empty, returns S1ChordAngle::Infinity().
  //
  // Use IsDistanceLess() if you only want to compare the distance against a
  // threshold value, since it is often much faster.
  S1ChordAngle GetDistance(Target* target);

  // Returns true if the distance to "target" is less than "limit".
  //
  // This method is usually much faster than GetDistance(), since it is much
  // less work to determine whether the minimum distance is above or below a
  // threshold than it is to calculate the actual minimum distance.
  bool IsDistanceLess(Target* target, S1ChordAngle limit);

  // Like IsDistanceLess(), but also returns true if the distance to "target"
  // is exactly equal to "limit".
  bool IsDistanceLessOrEqual(Target* target, S1ChordAngle limit);

  // Like IsDistanceLessOrEqual(), except that "limit" is increased by the
  // maximum error in the distance calculation.  This ensures that this
  // function returns true whenever the true, exact distance is less than
  // or equal to "limit".
  //
  // For example, suppose that we want to test whether two geometries might
  // intersect each other after they are snapped together using S2Builder
  // (using the IdentitySnapFunction with a given "snap_radius").  Since
  // S2Builder uses exact distance predicates (s2predicates.h), we need to
  // measure the distance between the two geometries conservatively.  If the
  // distance is definitely greater than "snap_radius", then the geometries
  // are guaranteed to not intersect after snapping.
  bool IsConservativeDistanceLessOrEqual(Target* target, S1ChordAngle limit);

  // Returns the endpoints of the given result edge.
  //
  // CAVEAT: If options().include_interiors() is true, then clients must not
  // pass this method any Result objects that correspond to shape interiors,
  // i.e. those where result.edge_id < 0.
  //
  // REQUIRES: result.edge_id >= 0
  S2Shape::Edge GetEdge(const Result& result) const;

  // Returns the point on given result edge that is closest to "point".
  S2Point Project(const S2Point& point, const Result& result) const;

 private:
  Options options_;
  Base base_;

  S2ClosestEdgeQuery(const S2ClosestEdgeQuery&) = delete;
  void operator=(const S2ClosestEdgeQuery&) = delete;
};


//////////////////   Implementation details follow   ////////////////////


inline void S2ClosestEdgeQuery::Options::set_max_distance(
    S1ChordAngle max_distance) {
  Base::Options::set_max_distance(Distance(max_distance));
}

inline void S2ClosestEdgeQuery::Options::set_max_distance(
    S1Angle max_distance) {
  Base::Options::set_max_distance(Distance(max_distance));
}

inline void S2ClosestEdgeQuery::Options::set_inclusive_max_distance(
    S1ChordAngle max_distance) {
  set_max_distance(max_distance.Successor());
}

inline void S2ClosestEdgeQuery::Options::set_inclusive_max_distance(
    S1Angle max_distance) {
  set_inclusive_max_distance(S1ChordAngle(max_distance));
}

inline void S2ClosestEdgeQuery::Options::set_max_error(S1Angle max_error) {
  Base::Options::set_max_error(S1ChordAngle(max_error));
}

inline S2ClosestEdgeQuery::PointTarget::PointTarget(const S2Point& point)
    : S2MinDistancePointTarget(point) {
}

inline S2ClosestEdgeQuery::EdgeTarget::EdgeTarget(const S2Point& a,
                                                  const S2Point& b)
    : S2MinDistanceEdgeTarget(a, b) {
}

inline S2ClosestEdgeQuery::CellTarget::CellTarget(const S2Cell& cell)
    : S2MinDistanceCellTarget(cell) {
}

inline S2ClosestEdgeQuery::ShapeIndexTarget::ShapeIndexTarget(
    const S2ShapeIndex* index)
    : S2MinDistanceShapeIndexTarget(index) {
}

inline S2ClosestEdgeQuery::S2ClosestEdgeQuery(const S2ShapeIndex* index,
                                              const Options& options) {
  Init(index, options);
}

inline void S2ClosestEdgeQuery::Init(const S2ShapeIndex* index,
                                     const Options& options) {
  options_ = options;
  base_.Init(index);
}

inline void S2ClosestEdgeQuery::ReInit() {
  base_.ReInit();
}

inline const S2ShapeIndex& S2ClosestEdgeQuery::index() const {
  return base_.index();
}

inline const S2ClosestEdgeQuery::Options& S2ClosestEdgeQuery::options() const {
  return options_;
}

inline S2ClosestEdgeQuery::Options* S2ClosestEdgeQuery::mutable_options() {
  return &options_;
}

inline std::vector<S2ClosestEdgeQuery::Result>
S2ClosestEdgeQuery::FindClosestEdges(Target* target) {
  return base_.FindClosestEdges(target, options_);
}

inline void S2ClosestEdgeQuery::FindClosestEdges(Target* target,
                                                 std::vector<Result>* results) {
  base_.FindClosestEdges(target, options_, results);
}

inline S2ClosestEdgeQuery::Result S2ClosestEdgeQuery::FindClosestEdge(
    Target* target) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  return base_.FindClosestEdge(target, tmp_options);
}

inline S1ChordAngle S2ClosestEdgeQuery::GetDistance(Target* target) {
  return FindClosestEdge(target).distance();
}

inline S2Shape::Edge S2ClosestEdgeQuery::GetEdge(const Result& result) const {
  return index().shape(result.shape_id())->edge(result.edge_id());
}

inline S2Point S2ClosestEdgeQuery::Project(const S2Point& point,
                                           const Result& result) const {
  if (result.edge_id() < 0) return point;
  auto edge = GetEdge(result);
  return S2::Project(point, edge.v0, edge.v1);
}

#endif  // S2_S2CLOSEST_EDGE_QUERY_H_
