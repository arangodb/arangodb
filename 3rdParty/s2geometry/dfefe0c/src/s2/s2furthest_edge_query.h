// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef S2_S2FURTHEST_EDGE_QUERY_H_
#define S2_S2FURTHEST_EDGE_QUERY_H_

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
#include "s2/s2max_distance_targets.h"
#include "s2/s2shape_index.h"

// S2FurthestEdgeQuery is a helper class for searching within an S2ShapeIndex
// for the furthest edge(s) to a given query point, edge, S2Cell, or geometry
// collection.  The furthest edge is defined as the one which maximizes the
// distance from any point on that edge to any point on the target geometry.

// As an example, given a set of polylines, the following code efficiently
// finds the furthest 5 edges to a query point:
//
// void Test(const vector<S2Polyline*>& polylines, const S2Point& point) {
//   MutableS2ShapeIndex index;
//   for (S2Polyline* polyline : polylines) {
//     index.Add(new S2Polyline::Shape(polyline));
//   }
//   S2FurthestEdgeQuery query(&index);
//   query.mutable_options()->set_max_results(5);
//   S2FurthestEdgeQuery::PointTarget target(point);
//   for (const auto& result : query.FindFurthestEdges(&target)) {
//     // The Result struct contains the following accessors:
//     //   "distance()" is the distance to the edge.
//     //   "shape_id()" identifies the S2Shape containing the edge.
//     //   "edge_id()" identifies the edge with the given shape.
//     // The following convenience methods may also be useful:
//     //   query.GetEdge(result) returns the endpoints of the edge.
//     int polyline_index = result.shape_id();
//     int edge_index = result.edge_id();
//     S1ChordAngle distance = result.distance();
//     S2Shape::Edge edge = query.GetEdge(result);
//   }
// }
//
// You can find either the k furthest edges, or all edges no closer than a
// given radius, or both (i.e., the k furthest edges no closer than a given
// minimum radius).
// E.g. to find all the edges further than 5 kilometers, call
//
//   query.mutable_options()->set_min_distance(
//       S2Earth::ToAngle(util::units::Kilometers(5)));
//
// By default *all* edges are returned, so you should always specify either
// max_results() or min_distance() or both.  Setting min distance may not be
// very restrictive, so strongly consider using max_results().  There is also a
// FindFurthestEdge() convenience method that returns only the single furthest
// edge.
//
// Note that by default, distances are measured to the boundary and interior
// of polygons.  To change this behavior, call set_include_interiors(false).
//
// If you only need to test whether the distance is above or below a given
// threshold (e.g., 10 km), you can use the IsDistanceGreater() method.  This
// is much faster than actually calculating the distance with
// FindFurthestEdge(), since the implementation can stop as soon as it can
// prove that the maximum distance is either above or below the threshold.
//
// To find the furthest edges to a query edge rather than a point, use:
//
//   S2FurthestEdgeQuery::EdgeTarget target(v0, v1);
//   query.FindFurthestEdges(&target);
//
// Similarly you can find the furthest edges to an S2Cell by using an
// S2FurthestEdgeQuery::CellTarget, and you can find the furthest edges to an
// arbitrary collection of points, polylines, and polygons by using an
// S2FurthestEdgeQuery::ShapeIndexTarget.
//
// The implementation is designed to be fast for both simple and complex
// geometric objects.
class S2FurthestEdgeQuery {
 public:
  // See S2FurthestEdgeQueryBase for full documentation.

  // S2MaxDistance is a thin wrapper around S1ChordAngle that implements the
  // Distance concept required by S2ClosestEdgeQueryBase.  It inverts the sense
  // of some methods to enable the closest edge query to return furthest
  // edges.
  // Distance and Base are made private to prevent leakage outside of this
  // class.  The private and public sections are interleaved since the public
  // options class needs the private Base class.
 private:
  using Distance = S2MaxDistance;
  using Base = S2ClosestEdgeQueryBase<Distance>;

 public:
  // Options that control the set of edges returned.  Note that by default
  // *all* edges are returned, so you will always want to set either the
  // max_results() option or the min_distance() option (or both).
  class Options : public Base::Options {
   public:
    // See S2ClosestEdgeQueryBase::Options for the full set of options.
    Options();

    // The following methods are like the corresponding max_distance() methods
    // in S2ClosestEdgeQuery, except that they specify the minimum distance to
    // the target.
    S1ChordAngle min_distance() const;
    void set_min_distance(S1ChordAngle min_distance);
    void set_inclusive_min_distance(S1ChordAngle min_distance);
    void set_conservative_min_distance(S1ChordAngle min_distance);

    void set_min_distance(S1Angle min_distance);
    void set_inclusive_min_distance(S1Angle min_distance);
    void set_conservative_min_distance(S1Angle min_distance);

    // Versions of set_max_error() that accept S1ChordAngle / S1Angle.
    // (See S2ClosestEdgeQueryBaseOptions for details.)
    S1ChordAngle max_error() const;
    void set_max_error(S1ChordAngle max_error);
    void set_max_error(S1Angle max_error);

    // Inherited options (see s2closestedgequerybase.h for details):
    using Base::Options::set_max_results;
    using Base::Options::set_include_interiors;
    using Base::Options::set_use_brute_force;

   private:
    // The S2MaxDistance versions of these methods are not intended for
    // public use.
    using Base::Options::set_max_distance;
    using Base::Options::set_max_error;
    using Base::Options::max_distance;
    using Base::Options::max_error;
  };

  // "Target" represents the geometry to which the distance is measured.
  // There are subtypes for measuring the distance to a point, an edge, an
  // S2Cell, or an S2ShapeIndex (an arbitrary collection of geometry).  Note
  // that S2DistanceTarget<Distance> is equivalent to S2MaxDistanceTarget in
  // s2max_distance_targets, which the following subtypes
  // (e.g. S2MaxDistancePointTarget) extend.
  using Target = S2DistanceTarget<Distance>;

  // Target subtype that computes the furthest distance to a point.
  class PointTarget final : public S2MaxDistancePointTarget {
   public:
    explicit PointTarget(const S2Point& point);
    int max_brute_force_index_size() const override;
  };

  // Target subtype that computes the furthest distance to an edge.
  class EdgeTarget final : public S2MaxDistanceEdgeTarget {
   public:
    explicit EdgeTarget(const S2Point& a, const S2Point& b);
    int max_brute_force_index_size() const override;
  };

  // Target subtype that computes the furthest distance to an S2Cell
  // (including the interior of the cell).
  class CellTarget final : public S2MaxDistanceCellTarget {
   public:
    explicit CellTarget(const S2Cell& cell);
    int max_brute_force_index_size() const override;
  };

  // Target subtype that computes the furthest distance to an S2ShapeIndex
  // (an arbitrary collection of points, polylines, and/or polygons).
  //
  // By default, distances are measured to the boundary and interior of
  // polygons in the S2ShapeIndex rather than to polygon boundaries only.
  // If you wish to change this behavior, you may call
  //
  //   target.set_include_interiors(false);
  //
  // (see S2MaxDistanceShapeIndexTarget for details).
  class ShapeIndexTarget final : public S2MaxDistanceShapeIndexTarget {
   public:
    explicit ShapeIndexTarget(const S2ShapeIndex* index);
    int max_brute_force_index_size() const override;
  };

  // Result class to pass back to user.  We choose to pass back this result
  // type, which has an S1ChordAngle as its distance, rather than the
  // Base::Result returned from the query which uses S2MaxDistance.
  class Result {
   public:
    // The default constructor, which yields an invalid result.
    Result() : Result(S1ChordAngle::Negative(), -1, -1) {}

    // Construct a Result from a Base::Result.
    explicit Result(const Base::Result& base)
        : Result(S1ChordAngle(base.distance()), base.shape_id(),
                 base.edge_id()) {}

    // Constructs a Result object for the given edge with the given distance.
    Result(S1ChordAngle distance, int32 _shape_id, int32 _edge_id)
        : distance_(distance), shape_id_(_shape_id), edge_id_(_edge_id) {}

    // Returns true if this Result object does not refer to any edge.
    // (The only case where an empty Result is returned is when the
    // FindFurthestEdges() method does not find any edges that meet the
    // specified criteria.)
    bool is_empty() const { return edge_id_ < 0; }

    // The distance from the target to this point.
    S1ChordAngle distance() const { return distance_; }

    // The edge identifiers.
    int32 shape_id() const { return shape_id_; }
    int32 edge_id() const { return edge_id_; }

    // Returns true if two Result objects are identical.
    friend bool operator==(const Result& x, const Result& y) {
      return (x.distance_ == y.distance_ &&
              x.shape_id_ == y.shape_id_ &&
              x.edge_id_ == y.edge_id_);
    }

    // Compares edges first by distance, then by (shape_id, edge_id).
    friend bool operator<(const Result& x, const Result& y) {
      if (x.distance_ < y.distance_) return true;
      if (y.distance_ < x.distance_) return false;
      if (x.shape_id_ < y.shape_id_) return true;
      if (y.shape_id_ < x.shape_id_) return false;
      return x.edge_id_ < y.edge_id_;
    }

   private:
    S1ChordAngle distance_;
    int32 shape_id_;     // Identifies an indexed shape.
    int32 edge_id_;      // Identifies an edge within the shape.
  };

  // Convenience constructor that calls Init().  Options may be specified here
  // or changed at any time using the mutable_options() accessor method.
  explicit S2FurthestEdgeQuery(const S2ShapeIndex* index,
                               const Options& options = Options());

  // Default constructor; requires Init() to be called.
  S2FurthestEdgeQuery();
  ~S2FurthestEdgeQuery();

  S2FurthestEdgeQuery(const S2FurthestEdgeQuery&) = delete;
  void operator=(const S2FurthestEdgeQuery&) = delete;

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

  // Returns the furthest edges to the given target that satisfy the given
  // options.  This method may be called multiple times.
  std::vector<Result> FindFurthestEdges(Target* target);

  // This version can be more efficient when this method is called many times,
  // since it does not require allocating a new vector on each call.
  void FindFurthestEdges(Target* target, std::vector<Result>* results);

  //////////////////////// Convenience Methods ////////////////////////

  // Returns the furthest edge to the target.  If no edge satisfies the search
  // criteria, then the Result object will have
  // distance == S1ChordAngle::Negative() and shape_id == edge_id == -1.
  Result FindFurthestEdge(Target* target);

  // Returns the maximum distance to the target.  If the index or target is
  // empty, returns S1ChordAngle::Negative().
  //
  // Use IsDistanceGreater() if you only want to compare the distance against a
  // threshold value, since it is often much faster.
  S1ChordAngle GetDistance(Target* target);

  // Returns true if the distance to "target" is greater than "limit".
  //
  // This method is usually much faster than GetDistance(), since it is much
  // less work to determine whether the maximum distance is above or below a
  // threshold than it is to calculate the actual maximum distance.
  bool IsDistanceGreater(Target* target, S1ChordAngle limit);

  // Like IsDistanceGreater(), but also returns true if the distance to
  // "target" is exactly equal to "limit".
  bool IsDistanceGreaterOrEqual(Target* target, S1ChordAngle limit);

  // Like IsDistanceGreaterOrEqual(), except that "limit" is decreased by the
  // maximum error in the distance calculation.  This ensures that this
  // function returns true whenever the true, exact distance is greater than
  // or equal to "limit".
  bool IsConservativeDistanceGreaterOrEqual(Target* target,
                                            S1ChordAngle limit);

  // Returns the endpoints of the given result edge.
  // REQUIRES: result.edge_id >= 0
  S2Shape::Edge GetEdge(const Result& result) const;

 private:
  Options options_;
  Base base_;
};


//////////////////   Implementation details follow   ////////////////////

inline S2FurthestEdgeQuery::Options::Options() {
}

inline S1ChordAngle S2FurthestEdgeQuery::Options::min_distance() const {
  return S1ChordAngle(max_distance());
}

inline void S2FurthestEdgeQuery::Options::set_min_distance(
    S1ChordAngle min_distance) {
  Base::Options::set_max_distance(Distance(min_distance));
}

inline void S2FurthestEdgeQuery::Options::set_min_distance(
    S1Angle min_distance) {
  Base::Options::set_max_distance(Distance(S1ChordAngle(min_distance)));
}

inline void S2FurthestEdgeQuery::Options::set_inclusive_min_distance(
    S1ChordAngle min_distance) {
  set_min_distance(min_distance.Predecessor());
}

inline void S2FurthestEdgeQuery::Options::set_inclusive_min_distance(
    S1Angle min_distance) {
  set_inclusive_min_distance(S1ChordAngle(min_distance));
}

inline S1ChordAngle S2FurthestEdgeQuery::Options::max_error() const {
  return S1ChordAngle(Base::Options::max_error());
}

inline void S2FurthestEdgeQuery::Options::set_max_error(
    S1ChordAngle max_error) {
  Base::Options::set_max_error(max_error);
}

inline void S2FurthestEdgeQuery::Options::set_max_error(S1Angle max_error) {
  Base::Options::set_max_error(S1ChordAngle(max_error));
}

inline S2FurthestEdgeQuery::PointTarget::PointTarget(const S2Point& point)
    : S2MaxDistancePointTarget(point) {
}

inline S2FurthestEdgeQuery::EdgeTarget::EdgeTarget(const S2Point& a,
                                                  const S2Point& b)
    : S2MaxDistanceEdgeTarget(a, b) {
}

inline S2FurthestEdgeQuery::CellTarget::CellTarget(const S2Cell& cell)
    : S2MaxDistanceCellTarget(cell) {
}

inline S2FurthestEdgeQuery::ShapeIndexTarget::ShapeIndexTarget(
    const S2ShapeIndex* index)
    : S2MaxDistanceShapeIndexTarget(index) {
}

inline S2FurthestEdgeQuery::S2FurthestEdgeQuery(const S2ShapeIndex* index,
                                                const Options& options) {
  Init(index, options);
}

inline void S2FurthestEdgeQuery::Init(const S2ShapeIndex* index,
                                      const Options& options) {
  options_ = options;
  base_.Init(index);
}

inline void S2FurthestEdgeQuery::ReInit() {
  base_.ReInit();
}

inline const S2ShapeIndex& S2FurthestEdgeQuery::index() const {
  return base_.index();
}

inline const S2FurthestEdgeQuery::Options&
    S2FurthestEdgeQuery::options() const {
  return options_;
}

inline S2FurthestEdgeQuery::Options* S2FurthestEdgeQuery::mutable_options() {
  return &options_;
}

inline std::vector<S2FurthestEdgeQuery::Result>
S2FurthestEdgeQuery::FindFurthestEdges(Target* target) {
  std::vector<S2FurthestEdgeQuery::Result> results;
  FindFurthestEdges(target, &results);
  return results;
}

inline S1ChordAngle S2FurthestEdgeQuery::GetDistance(Target* target) {
  return FindFurthestEdge(target).distance();
}

inline S2Shape::Edge S2FurthestEdgeQuery::GetEdge(const Result& result) const {
  return index().shape(result.shape_id())->edge(result.edge_id());
}

#endif  // S2_S2FURTHEST_EDGE_QUERY_H_
