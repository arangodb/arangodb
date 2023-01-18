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
//
// See S2ClosestPointQuery (defined below) for an overview.

#ifndef S2_S2CLOSEST_POINT_QUERY_H_
#define S2_S2CLOSEST_POINT_QUERY_H_

#include <vector>

#include "s2/base/logging.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2closest_point_query_base.h"
#include "s2/s2min_distance_targets.h"
#include "s2/s2point_index.h"

// Options that control the set of points returned.  Note that by default
// *all* points are returned, so you will always want to set either the
// max_results() option or the max_distance() option (or both).
//
// This class is also available as S2ClosestPointQuery<Data>::Options.
// (It is defined here to avoid depending on the "Data" template argument.)
class S2ClosestPointQueryOptions :
    public S2ClosestPointQueryBaseOptions<S2MinDistance> {
 public:
  using Distance = S2MinDistance;
  using Base = S2ClosestPointQueryBaseOptions<Distance>;

  // See S2ClosestPointQueryBaseOptions for the full set of options.

  // Specifies that only points whose distance to the target is less than
  // "max_distance" should be returned.
  //
  // Note that points whose distance is exactly equal to "max_distance" are
  // not returned.  Normally this doesn't matter, because distances are not
  // computed exactly in the first place, but if such points are needed then
  // see set_inclusive_max_distance() below.
  //
  // DEFAULT: Distance::Infinity()
  void set_max_distance(S1ChordAngle max_distance);

  // Like set_max_distance(), except that points whose distance is exactly
  // equal to "max_distance" are also returned.  Equivalent to calling
  // set_max_distance(max_distance.Successor()).
  void set_inclusive_max_distance(S1ChordAngle max_distance);

  // Like set_inclusive_max_distance(), except that "max_distance" is also
  // increased by the maximum error in the distance calculation.  This ensures
  // that all points whose true distance is less than or equal to
  // "max_distance" will be returned (along with some points whose true
  // distance is slightly greater).
  //
  // Algorithms that need to do exact distance comparisons can use this
  // option to find a set of candidate points that can then be filtered
  // further (e.g., using s2pred::CompareDistance).
  void set_conservative_max_distance(S1ChordAngle max_distance);

  // Versions of set_max_distance that take an S1Angle argument.  (Note that
  // these functions require a conversion, and that the S1ChordAngle versions
  // are preferred.)
  void set_max_distance(S1Angle max_distance);
  void set_inclusive_max_distance(S1Angle max_distance);
  void set_conservative_max_distance(S1Angle max_distance);

  // See S2ClosestPointQueryBaseOptions for documentation.
  using Base::set_max_error;              // S1Chordangle version
  void set_max_error(S1Angle max_error);  // S1Angle version

  // Inherited options (see s2closest_point_query_base.h for details):
  using Base::set_max_results;
  using Base::set_region;
  using Base::set_use_brute_force;
};

// S2ClosestPointQueryTarget represents the geometry to which the distance is
// measured.  There are subtypes for measuring the distance to a point, an
// edge, an S2Cell, or an S2ShapeIndex (an arbitrary collection of geometry).
using S2ClosestPointQueryTarget = S2MinDistanceTarget;

// Target subtype that computes the closest distance to a point.
//
// This class is also available as S2ClosestPointQuery<Data>::PointTarget.
// (It is defined here to avoid depending on the "Data" template argument.)
class S2ClosestPointQueryPointTarget final : public S2MinDistancePointTarget {
 public:
  explicit S2ClosestPointQueryPointTarget(const S2Point& point);
  int max_brute_force_index_size() const override;
};

// Target subtype that computes the closest distance to an edge.
//
// This class is also available as S2ClosestPointQuery<Data>::EdgeTarget.
// (It is defined here to avoid depending on the "Data" template argument.)
class S2ClosestPointQueryEdgeTarget final : public S2MinDistanceEdgeTarget {
 public:
  explicit S2ClosestPointQueryEdgeTarget(const S2Point& a, const S2Point& b);
  int max_brute_force_index_size() const override;
};

// Target subtype that computes the closest distance to an S2Cell
// (including the interior of the cell).
//
// This class is also available as S2ClosestPointQuery<Data>::CellTarget.
// (It is defined here to avoid depending on the "Data" template argument.)
class S2ClosestPointQueryCellTarget final : public S2MinDistanceCellTarget {
 public:
  explicit S2ClosestPointQueryCellTarget(const S2Cell& cell);
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
//
// This class is also available as S2ClosestPointQuery<Data>::ShapeIndexTarget.
// (It is defined here to avoid depending on the "Data" template argument.)
class S2ClosestPointQueryShapeIndexTarget final :
    public S2MinDistanceShapeIndexTarget {
 public:
  explicit S2ClosestPointQueryShapeIndexTarget(const S2ShapeIndex* index);
  int max_brute_force_index_size() const override;
};

// Given a set of points stored in an S2PointIndex, S2ClosestPointQuery
// provides methods that find the closest point(s) to a given query point
// or query edge.  Example usage:
//
// void Test(const vector<S2Point>& index_points,
//           const vector<S2Point>& target_points) {
//   // The template argument allows auxiliary data to be attached to each
//   // point (in this case, the array index).
//   S2PointIndex<int> index;
//   for (int i = 0; i < index_points.size(); ++i) {
//     index.Add(index_points[i], i);
//   }
//   S2ClosestPointQuery<int> query(&index);
//   query.mutable_options()->set_max_results(5);
//   for (const S2Point& target_point : target_points) {
//     S2ClosestPointQueryPointTarget target(target_point);
//     for (const auto& result : query.FindClosestPoints(&target)) {
//       // The Result class contains the following methods:
//       //   distance() is the distance to the target.
//       //   point() is the indexed point.
//       //   data() is the auxiliary data.
//       DoSomething(target_point, result);
//     }
//   }
// }
//
// You can find either the k closest points, or all points within a given
// radius, or both (i.e., the k closest points up to a given maximum radius).
// E.g. to find all the points within 5 kilometers, call
//
//   query.mutable_options()->set_max_distance(
//       S2Earth::ToAngle(util::units::Kilometers(5)));
//
// By default *all* points are returned, so you should always specify either
// max_results() or max_distance() or both.  There is also a FindClosestPoint()
// convenience method that returns only the closest point.
//
// You can restrict the results to an arbitrary S2Region, for example:
//
//   S2LatLngRect rect(...);
//   query.mutable_options()->set_region(&rect);  // Does *not* take ownership.
//
// To find the closest points to a query edge rather than a point, use:
//
//   S2ClosestPointQueryEdgeTarget target(v0, v1);
//   query.FindClosestPoints(&target);
//
// Similarly you can find the closest points to an S2Cell by using an
// S2ClosestPointQuery::CellTarget, and you can find the closest points to an
// arbitrary collection of points, polylines, and polygons by using an
// S2ClosestPointQuery::ShapeIndexTarget.
//
// The implementation is designed to be fast for both small and large
// point sets.
template <class Data>
class S2ClosestPointQuery {
 public:
  // See S2ClosestPointQueryBase for full documentation.

  using Index = S2PointIndex<Data>;
  using PointData = typename Index::PointData;

  // S2MinDistance is a thin wrapper around S1ChordAngle that implements the
  // Distance concept required by S2ClosestPointQueryBase.
  using Distance = S2MinDistance;
  using Base = S2ClosestPointQueryBase<Distance, Data>;

  // Each "Result" object represents a closest point.  Here are its main
  // methods (see S2ClosestPointQueryBase::Result for details):
  //
  //   // The distance from the target to this point.
  //   S1ChordAngle distance() const;
  //
  //   // The point itself.
  //   const S2Point& point() const;
  //
  //   // The client-specified data associated with this point.
  //   const Data& data() const;
  using Result = typename Base::Result;

  using Options = S2ClosestPointQueryOptions;

  // The available target types (see definitions above).
  using Target = S2ClosestPointQueryTarget;
  using PointTarget = S2ClosestPointQueryPointTarget;
  using EdgeTarget = S2ClosestPointQueryEdgeTarget;
  using CellTarget = S2ClosestPointQueryCellTarget;
  using ShapeIndexTarget = S2ClosestPointQueryShapeIndexTarget;

  // Convenience constructor that calls Init().  Options may be specified here
  // or changed at any time using the mutable_options() accessor method.
  explicit S2ClosestPointQuery(const Index* index,
                               const Options& options = Options());

  // Default constructor; requires Init() to be called.
  S2ClosestPointQuery();
  ~S2ClosestPointQuery();

  // Initializes the query.  Options may be specified here or changed at any
  // time using the mutable_options() accessor method.
  //
  // REQUIRES: "index" must persist for the lifetime of this object.
  // REQUIRES: ReInit() must be called if "index" is modified.
  void Init(const Index* index, const Options& options = Options());

  // Reinitializes the query.  This method must be called whenever the
  // underlying index is modified.
  void ReInit();

  // Returns a reference to the underlying S2PointIndex.
  const Index& index() const;

  // Returns the query options.  Options can be modifed between queries.
  const Options& options() const;
  Options* mutable_options();

  // Returns the closest points to the given target that satisfy the current
  // options.  This method may be called multiple times.
  std::vector<Result> FindClosestPoints(Target* target);

  // This version can be more efficient when this method is called many times,
  // since it does not require allocating a new vector on each call.
  void FindClosestPoints(Target* target, std::vector<Result>* results);

  //////////////////////// Convenience Methods ////////////////////////

  // Returns the closest point to the target.  If no point satisfies the search
  // criteria, then a Result object with distance() == Infinity() and
  // is_empty() == true is returned.
  Result FindClosestPoint(Target* target);

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

 private:
  Options options_;
  Base base_;
};


//////////////////   Implementation details follow   ////////////////////


inline void S2ClosestPointQueryOptions::set_max_distance(
    S1ChordAngle max_distance) {
  Base::set_max_distance(Distance(max_distance));
}

inline void S2ClosestPointQueryOptions::set_max_distance(S1Angle max_distance) {
  Base::set_max_distance(Distance(max_distance));
}

inline void S2ClosestPointQueryOptions::set_inclusive_max_distance(
    S1ChordAngle max_distance) {
  set_max_distance(max_distance.Successor());
}

inline void S2ClosestPointQueryOptions::set_inclusive_max_distance(
    S1Angle max_distance) {
  set_inclusive_max_distance(S1ChordAngle(max_distance));
}

inline void S2ClosestPointQueryOptions::set_max_error(S1Angle max_error) {
  Base::set_max_error(S1ChordAngle(max_error));
}

inline S2ClosestPointQueryPointTarget::S2ClosestPointQueryPointTarget(
    const S2Point& point)
    : S2MinDistancePointTarget(point) {
}

inline S2ClosestPointQueryEdgeTarget::S2ClosestPointQueryEdgeTarget(
    const S2Point& a, const S2Point& b)
    : S2MinDistanceEdgeTarget(a, b) {
}

inline S2ClosestPointQueryCellTarget::S2ClosestPointQueryCellTarget(
    const S2Cell& cell)
    : S2MinDistanceCellTarget(cell) {
}

inline S2ClosestPointQueryShapeIndexTarget::S2ClosestPointQueryShapeIndexTarget(
    const S2ShapeIndex* index)
    : S2MinDistanceShapeIndexTarget(index) {
}

template <class Data>
inline S2ClosestPointQuery<Data>::S2ClosestPointQuery(const Index* index,
                                                      const Options& options) {
  Init(index, options);
}

template <class Data>
S2ClosestPointQuery<Data>::S2ClosestPointQuery() {
  // Prevent inline constructor bloat by defining here.
}

template <class Data>
S2ClosestPointQuery<Data>::~S2ClosestPointQuery() {
  // Prevent inline destructor bloat by defining here.
}

template <class Data>
void S2ClosestPointQuery<Data>::Init(const Index* index,
                                     const Options& options) {
  options_ = options;
  base_.Init(index);
}

template <class Data>
inline void S2ClosestPointQuery<Data>::ReInit() {
  base_.ReInit();
}

template <class Data>
inline const S2PointIndex<Data>& S2ClosestPointQuery<Data>::index() const {
  return base_.index();
}

template <class Data>
inline const S2ClosestPointQueryOptions& S2ClosestPointQuery<Data>::options()
    const {
  return options_;
}

template <class Data>
inline S2ClosestPointQueryOptions*
S2ClosestPointQuery<Data>::mutable_options() {
  return &options_;
}

template <class Data>
inline std::vector<typename S2ClosestPointQuery<Data>::Result>
S2ClosestPointQuery<Data>::FindClosestPoints(Target* target) {
  return base_.FindClosestPoints(target, options_);
}

template <class Data>
inline void S2ClosestPointQuery<Data>::FindClosestPoints(
    Target* target, std::vector<Result>* results) {
  base_.FindClosestPoints(target, options_, results);
}

template <class Data>
inline typename S2ClosestPointQuery<Data>::Result
S2ClosestPointQuery<Data>::FindClosestPoint(Target* target) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  return base_.FindClosestPoint(target, tmp_options);
}

template <class Data>
inline S1ChordAngle S2ClosestPointQuery<Data>::GetDistance(Target* target) {
  return FindClosestPoint(target).distance();
}

template <class Data>
bool S2ClosestPointQuery<Data>::IsDistanceLess(
    Target* target, S1ChordAngle limit) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  tmp_options.set_max_distance(limit);
  tmp_options.set_max_error(S1ChordAngle::Straight());
  return !base_.FindClosestPoint(target, tmp_options).is_empty();
}

template <class Data>
bool S2ClosestPointQuery<Data>::IsDistanceLessOrEqual(
    Target* target, S1ChordAngle limit) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  tmp_options.set_inclusive_max_distance(limit);
  tmp_options.set_max_error(S1ChordAngle::Straight());
  return !base_.FindClosestPoint(target, tmp_options).is_empty();
}

template <class Data>
bool S2ClosestPointQuery<Data>::IsConservativeDistanceLessOrEqual(
    Target* target, S1ChordAngle limit) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  tmp_options.set_conservative_max_distance(limit);
  tmp_options.set_max_error(S1ChordAngle::Straight());
  return !base_.FindClosestPoint(target, tmp_options).is_empty();
}

#endif  // S2_S2CLOSEST_POINT_QUERY_H_
