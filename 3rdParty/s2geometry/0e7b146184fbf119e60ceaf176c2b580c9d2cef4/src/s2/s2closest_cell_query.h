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

// Author: ericv@google.com (Eric Veach)

#ifndef S2_S2CLOSEST_CELL_QUERY_H_
#define S2_S2CLOSEST_CELL_QUERY_H_

#include <vector>

#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2closest_cell_query_base.h"
#include "s2/s2min_distance_targets.h"

// S2ClosestCellQuery is a helper class for finding the closest cell(s) to a
// given point, edge, S2Cell, S2CellUnion, or geometry collection.  A typical
// use case would be to add a collection of S2Cell coverings to an S2CellIndex
// (representing a collection of original geometry), and then use
// S2ClosestCellQuery to find all coverings that are within a given distance
// of some target geometry (which could be represented exactly, or could also
// be a covering).  The distance to the original geometry corresponding to
// each covering could then be measured more precisely if desired.
//
// For example, here is how to find all cells that are closer than
// "distance_limit" to a given target point:
//
//   S2ClosestCellQuery query(&cell_index);
//   query.mutable_options()->set_max_distance(distance_limit);
//   S2ClosestCellQuery::PointTarget target(target_point);
//   for (const auto& result : query.FindClosestCells(&target)) {
//     // result.distance() is the distance to the target.
//     // result.cell_id() is the indexed S2CellId.
//     // result.label() is the integer label associated with the S2CellId.
//     DoSomething(target_point, result);
//   }
//
// You can find either the k closest cells, or all cells within a given
// radius, or both (i.e., the k closest cells up to a given maximum radius).
// By default *all* cells are returned, so you should always specify either
// max_results() or max_distance() or both.  You can also restrict the results
// to cells that intersect a given S2Region; for example:
//
//   S2LatLngRect rect(...);
//   query.mutable_options()->set_region(&rect);  // Does *not* take ownership.
//
// There is a FindClosestCell() convenience method that returns the closest
// cell.  However, if you only need to test whether the distance is above or
// below a given threshold (e.g., 10 km), it is typically much faster to use
// the IsDistanceLess() method instead.  Unlike FindClosestCell(), this method
// stops as soon as it can prove that the minimum distance is either above or
// below the threshold.  Example usage:
//
//   if (query.IsDistanceLess(&target, limit_distance)) ...
//
// To find the closest cells to a query edge rather than a point, use:
//
//   S2ClosestCellQuery::EdgeTarget target(v0, v1);
//   query.FindClosestCells(&target);
//
// Similarly you can find the closest cells to an S2Cell using an
// S2ClosestCellQuery::CellTarget, you can find the closest cells to an
// S2CellUnion using an S2ClosestCellQuery::CellUnionTarget, and you can find
// the closest cells to an arbitrary collection of points, polylines, and
// polygons by using an S2ClosestCellQuery::ShapeIndexTarget.
//
// The implementation is designed to be fast for both simple and complex
// geometric objects.
class S2ClosestCellQuery {
 public:
  // See S2ClosestCellQueryBase for full documentation.

  // S2MinDistance is a thin wrapper around S1ChordAngle that implements the
  // Distance concept required by S2ClosestCellQueryBase.
  using Distance = S2MinDistance;
  using Base = S2ClosestCellQueryBase<Distance>;

  // Each "Result" object represents a closest (s2cell_id, label) pair.  Here
  // are its main methods (see S2ClosestCellQueryBase::Result for details):
  //
  //   // The distance from the target to this point.
  //   S1ChordAngle distance() const;
  //
  //   // The S2CellId itself.
  //   S2CellId cell_id() const;
  //
  //   // The label associated with this S2CellId.
  //   const Label& label() const;
  using Result = Base::Result;

  // Options that control the set of cells returned.  Note that by default
  // *all* cells are returned, so you will always want to set either the
  // max_results() option or the max_distance() option (or both).
  class Options : public Base::Options {
   public:
    // See S2ClosestCellQueryBase::Options for the full set of options.

    // Specifies that only cells whose distance to the target is less than
    // "max_distance" should be returned.
    //
    // Note that cells whose distance is exactly equal to "max_distance" are
    // not returned.  Normally this doesn't matter, because distances are not
    // computed exactly in the first place, but if such cells are needed then
    // see set_inclusive_max_distance() below.
    //
    // DEFAULT: Distance::Infinity()
    void set_max_distance(S1ChordAngle max_distance);

    // Like set_max_distance(), except that cells whose distance is exactly
    // equal to "max_distance" are also returned.  Equivalent to calling
    // set_max_distance(max_distance.Successor()).
    void set_inclusive_max_distance(S1ChordAngle max_distance);

    // Like set_inclusive_max_distance(), except that "max_distance" is also
    // increased by the maximum error in the distance calculation.  This
    // ensures that all cells whose true distance is less than or equal to
    // "max_distance" will be returned (along with some cells whose true
    // distance is slightly greater).
    //
    // Algorithms that need to do exact distance comparisons can use this
    // option to find a set of candidate cells that can then be filtered
    // further (e.g., using s2pred::CompareDistance).
    void set_conservative_max_distance(S1ChordAngle max_distance);

    // Versions of set_max_distance that take an S1Angle argument.  (Note that
    // these functions require a conversion, and that the S1ChordAngle versions
    // are preferred.)
    void set_max_distance(S1Angle max_distance);
    void set_inclusive_max_distance(S1Angle max_distance);
    void set_conservative_max_distance(S1Angle max_distance);

    // See S2ClosestCellQueryBase::Options for documentation.
    using Base::Options::set_max_error;     // S1Chordangle version
    void set_max_error(S1Angle max_error);  // S1Angle version

    // Inherited options (see s2closest_cell_query_base.h for details):
    using Base::Options::set_max_results;
    using Base::Options::set_region;
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

  // Target subtype that computes the closest distance to an S2CellUnion.
  class CellUnionTarget final : public S2MinDistanceCellUnionTarget {
   public:
    explicit CellUnionTarget(S2CellUnion cell_union);
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
  //
  // REQUIRES: "index" must persist for the lifetime of this object.
  // REQUIRES: ReInit() must be called if "index" is modified.
  explicit S2ClosestCellQuery(const S2CellIndex* index,
                              const Options& options = Options());

  // Default constructor; requires Init() to be called.
  S2ClosestCellQuery();
  ~S2ClosestCellQuery();

  // Initializes the query.  Options may be specified here or changed at any
  // time using the mutable_options() accessor method.
  //
  // REQUIRES: "index" must persist for the lifetime of this object.
  // REQUIRES: ReInit() must be called if "index" is modified.
  void Init(const S2CellIndex* index, const Options& options = Options());

  // Reinitializes the query.  This method must be called if the underlying
  // S2CellIndex is modified (by calling Clear() and Build() again).
  void ReInit();

  // Returns a reference to the underlying S2CellIndex.
  const S2CellIndex& index() const;

  // Returns the query options.  Options can be modified between queries.
  const Options& options() const;
  Options* mutable_options();

  // Returns the closest cells to the given target that satisfy the current
  // options.  This method may be called multiple times.
  std::vector<Result> FindClosestCells(Target* target);

  // This version can be more efficient when this method is called many times,
  // since it does not require allocating a new vector on each call.
  void FindClosestCells(Target* target, std::vector<Result>* results);

  //////////////////////// Convenience Methods ////////////////////////

  // Returns the closest cell to the target.  If no cell satisfies the search
  // criteria, then the Result object will have distance == Infinity() and
  // is_empty() == true.
  Result FindClosestCell(Target* target);

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
  bool IsConservativeDistanceLessOrEqual(Target* target, S1ChordAngle limit);

 private:
  Options options_;
  Base base_;

  S2ClosestCellQuery(const S2ClosestCellQuery&) = delete;
  void operator=(const S2ClosestCellQuery&) = delete;
};


//////////////////   Implementation details follow   ////////////////////


inline void S2ClosestCellQuery::Options::set_max_distance(
    S1ChordAngle max_distance) {
  Base::Options::set_max_distance(Distance(max_distance));
}

inline void S2ClosestCellQuery::Options::set_max_distance(
    S1Angle max_distance) {
  Base::Options::set_max_distance(Distance(max_distance));
}

inline void S2ClosestCellQuery::Options::set_inclusive_max_distance(
    S1ChordAngle max_distance) {
  set_max_distance(max_distance.Successor());
}

inline void S2ClosestCellQuery::Options::set_inclusive_max_distance(
    S1Angle max_distance) {
  set_inclusive_max_distance(S1ChordAngle(max_distance));
}

inline void S2ClosestCellQuery::Options::set_max_error(S1Angle max_error) {
  Base::Options::set_max_error(S1ChordAngle(max_error));
}

inline S2ClosestCellQuery::PointTarget::PointTarget(const S2Point& point)
    : S2MinDistancePointTarget(point) {
}

inline S2ClosestCellQuery::EdgeTarget::EdgeTarget(const S2Point& a,
                                                  const S2Point& b)
    : S2MinDistanceEdgeTarget(a, b) {
}

inline S2ClosestCellQuery::CellTarget::CellTarget(const S2Cell& cell)
    : S2MinDistanceCellTarget(cell) {
}

inline S2ClosestCellQuery::CellUnionTarget::CellUnionTarget(
    S2CellUnion cell_union)
    : S2MinDistanceCellUnionTarget(std::move(cell_union)) {
}

inline S2ClosestCellQuery::ShapeIndexTarget::ShapeIndexTarget(
    const S2ShapeIndex* index)
    : S2MinDistanceShapeIndexTarget(index) {
}

inline S2ClosestCellQuery::S2ClosestCellQuery(const S2CellIndex* index,
                                              const Options& options) {
  Init(index, options);
}

inline void S2ClosestCellQuery::Init(const S2CellIndex* index,
                                     const Options& options) {
  options_ = options;
  base_.Init(index);
}

inline void S2ClosestCellQuery::ReInit() {
  base_.ReInit();
}

inline const S2CellIndex& S2ClosestCellQuery::index() const {
  return base_.index();
}

inline const S2ClosestCellQuery::Options& S2ClosestCellQuery::options() const {
  return options_;
}

inline S2ClosestCellQuery::Options* S2ClosestCellQuery::mutable_options() {
  return &options_;
}

inline std::vector<S2ClosestCellQuery::Result>
S2ClosestCellQuery::FindClosestCells(Target* target) {
  return base_.FindClosestCells(target, options_);
}

inline void S2ClosestCellQuery::FindClosestCells(Target* target,
                                                 std::vector<Result>* results) {
  base_.FindClosestCells(target, options_, results);
}

inline S2ClosestCellQuery::Result S2ClosestCellQuery::FindClosestCell(
    Target* target) {
  static_assert(sizeof(Options) <= 32, "Consider not copying Options here");
  Options tmp_options = options_;
  tmp_options.set_max_results(1);
  return base_.FindClosestCell(target, tmp_options);
}

inline S1ChordAngle S2ClosestCellQuery::GetDistance(Target* target) {
  return FindClosestCell(target).distance();
}

#endif  // S2_S2CLOSEST_CELL_QUERY_H_
