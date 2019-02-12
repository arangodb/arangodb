// Copyright 2017 Google Inc. All Rights Reserved.
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
// This file defines a collection of classes that are useful for computing
// minimum distances on the sphere.  Their purpose is to allow code to be
// shared among the various query classes that find nearby geometry, such as
// S2ClosestEdgeQuery, S2ClosestPointQuery, and S2ClosestCellQuery.

#ifndef S2_S2MIN_DISTANCE_TARGETS_H_
#define S2_S2MIN_DISTANCE_TARGETS_H_

#include <memory>

#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"
#include "s2/s2cell_index.h"
#include "s2/s1chord_angle.h"
#include "s2/s2cell.h"
#include "s2/s2distance_target.h"
#include "s2/s2edge_distances.h"
#include "s2/s2shape_index.h"

// Forward references because these classes depend on the types defined here.
class S2ClosestCellQuery;
class S2ClosestEdgeQuery;

// S2MinDistance is a thin wrapper around S1ChordAngle that is used by classes
// such as S2ClosestEdgeQuery to compute minimum distances on the sphere (as
// opposed to maximum distances, ellipsoidal distances, etc).
//
// It implements the Distance concept defined by S2DistanceTarget (see
// s2distance_target.h for details).
class S2MinDistance : public S1ChordAngle {
 public:
  using Delta = S1ChordAngle;

  S2MinDistance() : S1ChordAngle() {}
  explicit S2MinDistance(S1Angle x) : S1ChordAngle(x) {}
  explicit S2MinDistance(S1ChordAngle x) : S1ChordAngle(x) {}
  static S2MinDistance Zero();
  static S2MinDistance Infinity();
  static S2MinDistance Negative();
  friend S2MinDistance operator-(S2MinDistance x, S1ChordAngle delta);
  S1ChordAngle GetChordAngleBound() const;

  // If (dist < *this), updates *this and returns true (used internally).
  bool UpdateMin(const S2MinDistance& dist);
};

// S2MinDistanceTarget represents a geometric object to which distances are
// measured.  Specifically, it is used to compute minimum distances on the
// sphere (as opposed to maximum distances, ellipsoidal distances, etc).
//
// Subtypes are defined below for measuring the distance to a point, an edge,
// an S2Cell, or an S2ShapeIndex (an arbitrary collection of geometry).
using S2MinDistanceTarget = S2DistanceTarget<S2MinDistance>;

// An S2DistanceTarget subtype for computing the minimum distance to a point.
class S2MinDistancePointTarget : public S2MinDistanceTarget {
 public:
  explicit S2MinDistancePointTarget(const S2Point& point);
  S2Cap GetCapBound() final;
  bool UpdateMinDistance(const S2Point& p, S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Cell& cell,
                         S2MinDistance* min_dist) final;
  bool VisitContainingShapes(const S2ShapeIndex& index,
                             const ShapeVisitor& visitor) final;

 private:
  S2Point point_;
};

// An S2DistanceTarget subtype for computing the minimum distance to a edge.
class S2MinDistanceEdgeTarget : public S2MinDistanceTarget {
 public:
  S2MinDistanceEdgeTarget(const S2Point& a, const S2Point& b);
  S2Cap GetCapBound() final;
  bool UpdateMinDistance(const S2Point& p, S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Cell& cell,
                         S2MinDistance* min_dist) final;
  bool VisitContainingShapes(const S2ShapeIndex& index,
                             const ShapeVisitor& visitor) final;

 private:
  S2Point a_, b_;
};

// An S2DistanceTarget subtype for computing the minimum distance to an S2Cell
// (including the interior of the cell).
class S2MinDistanceCellTarget : public S2MinDistanceTarget {
 public:
  explicit S2MinDistanceCellTarget(const S2Cell& cell);
  S2Cap GetCapBound() final;
  bool UpdateMinDistance(const S2Point& p, S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Cell& cell,
                         S2MinDistance* min_dist) final;
  bool VisitContainingShapes(const S2ShapeIndex& index,
                             const ShapeVisitor& visitor) final;

 private:
  S2Cell cell_;
};

// An S2DistanceTarget subtype for computing the minimum distance to an
// S2CellUnion (including the interior of all cells).
class S2MinDistanceCellUnionTarget : public S2MinDistanceTarget {
 public:
  explicit S2MinDistanceCellUnionTarget(S2CellUnion cell_union);
  ~S2MinDistanceCellUnionTarget() override;

  // Specifies that the distances should be computed by examining every cell
  // in the S2CellIndex (for testing and debugging purposes).
  //
  // DEFAULT: false
  bool use_brute_force() const;
  void set_use_brute_force(bool use_brute_force);

  // Note that set_max_error() should not be called directly by clients; it is
  // used internally by the S2Closest*Query implementations.
  bool set_max_error(const S1ChordAngle& max_error) override;

  S2Cap GetCapBound() final;
  bool UpdateMinDistance(const S2Point& p, S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Cell& cell,
                         S2MinDistance* min_dist) final;
  bool VisitContainingShapes(const S2ShapeIndex& query_index,
                             const ShapeVisitor& visitor) final;

 private:
  bool UpdateMinDistance(S2MinDistanceTarget* target, S2MinDistance* min_dist);

  S2CellUnion cell_union_;
  S2CellIndex index_;
  std::unique_ptr<S2ClosestCellQuery> query_;
};

// An S2DistanceTarget subtype for computing the minimum distance to an
// S2ShapeIndex (a collection of points, polylines, and/or polygons).
//
// Note that ShapeIndexTarget has its own options:
//
//   include_interiors()
//     - specifies that distances are measured to the boundary and interior
//       of polygons in the S2ShapeIndex.  (If set to false, distance is
//       measured to the polygon boundary only.)
//       DEFAULT: true.
//
//   brute_force()
//     - specifies that the distances should be computed by examining every
//       edge in the S2ShapeIndex (for testing and debugging purposes).
//       DEFAULT: false.
//
// These options are specified independently of the corresponding
// S2ClosestEdgeQuery options.  For example, if include_interiors is true for
// a ShapeIndexTarget but false for the S2ClosestEdgeQuery where the target
// is used, then distances will be measured from the boundary of one
// S2ShapeIndex to the boundary and interior of the other.
//
// Note that when the distance to a ShapeIndexTarget is zero because the
// target intersects the interior of the query index, you can find a point
// that achieves this zero distance by calling the VisitContainingShapes()
// method directly.  For example:
//
//   S2ClosestEdgeQuery::ShapeIndexTarget target(&target_index);
//   target.VisitContainingShapes(
//       query_index, [](S2Shape* containing_shape,
//                       const S2Point& target_point) {
//         ... do something with "target_point" ...
//         return false;  // Terminate search
//       }));
class S2MinDistanceShapeIndexTarget : public S2MinDistanceTarget {
 public:
  explicit S2MinDistanceShapeIndexTarget(const S2ShapeIndex* index);
  ~S2MinDistanceShapeIndexTarget() override;

  // Specifies that distance will be measured to the boundary and interior
  // of polygons in the S2ShapeIndex rather than to polygon boundaries only.
  //
  // DEFAULT: true
  bool include_interiors() const;
  void set_include_interiors(bool include_interiors);

  // Specifies that the distances should be computed by examining every edge
  // in the S2ShapeIndex (for testing and debugging purposes).
  //
  // DEFAULT: false
  bool use_brute_force() const;
  void set_use_brute_force(bool use_brute_force);

  // Note that set_max_error() should not be called directly by clients; it is
  // used internally by the S2Closest*Query implementations.
  bool set_max_error(const S1ChordAngle& max_error) override;

  S2Cap GetCapBound() final;
  bool UpdateMinDistance(const S2Point& p, S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         S2MinDistance* min_dist) final;
  bool UpdateMinDistance(const S2Cell& cell,
                         S2MinDistance* min_dist) final;
  bool VisitContainingShapes(const S2ShapeIndex& query_index,
                             const ShapeVisitor& visitor) final;

 private:
  bool UpdateMinDistance(S2MinDistanceTarget* target, S2MinDistance* min_dist);

  const S2ShapeIndex* index_;
  std::unique_ptr<S2ClosestEdgeQuery> query_;
};


//////////////////   Implementation details follow   ////////////////////


inline S2MinDistance S2MinDistance::Zero() {
  return S2MinDistance(S1ChordAngle::Zero());
}

inline S2MinDistance S2MinDistance::Infinity() {
  return S2MinDistance(S1ChordAngle::Infinity());
}

inline S2MinDistance S2MinDistance::Negative() {
  return S2MinDistance(S1ChordAngle::Negative());
}

inline S2MinDistance operator-(S2MinDistance x, S1ChordAngle delta) {
  return S2MinDistance(S1ChordAngle(x) - delta);
}

inline S1ChordAngle S2MinDistance::GetChordAngleBound() const {
  return PlusError(GetS1AngleConstructorMaxError());
}

inline bool S2MinDistance::UpdateMin(const S2MinDistance& dist) {
  if (dist < *this) {
    *this = dist;
    return true;
  }
  return false;
}

inline S2MinDistancePointTarget::S2MinDistancePointTarget(const S2Point& point)
    : point_(point) {
}

inline S2MinDistanceEdgeTarget::S2MinDistanceEdgeTarget(const S2Point& a,
                                                        const S2Point& b)
    : a_(a), b_(b) {
}

#endif  // S2_S2MIN_DISTANCE_TARGETS_H_
