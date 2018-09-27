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

// This file defines a collection of classes that are useful for computing
// maximum distances on the sphere.  Their purpose is to allow code to be
// shared among the various query classes that find remote geometry, such as
// S2FurthestPointQuery and S2FurthestEdgeQuery.

#ifndef S2_S2MAX_DISTANCE_TARGETS_H_
#define S2_S2MAX_DISTANCE_TARGETS_H_

#include <memory>

#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2cell.h"
#include "s2/s2distance_target.h"
#include "s2/s2edge_distances.h"
#include "s2/s2shape_index.h"

class S2FurthestEdgeQuery;

// S2MaxDistance is a class that allows maximum distances to be computed using
// a minimum distance algorithm.  Specifically, S2MaxDistance(x) represents the
// supplementary distance (Pi - x).  This has the effect of inverting the sort
// order, i.e.
//
//  (S2MaxDistance(x) < S2MaxDistance(y))  <=>  (Pi - x < Pi - y)  <=>  (x > y)
//
// All other operations are implemented similarly (using the supplementary
// distance Pi - x).  For example, S2MaxDistance(x) - S2MaxDistance(y) ==
// S2MaxDistance(x + y).
class S2MaxDistance {
 public:
  using Delta = S1ChordAngle;

  S2MaxDistance() : distance_() {}
  explicit S2MaxDistance(S1ChordAngle x) : distance_(x) {}
  explicit operator S1ChordAngle() const { return distance_; }
  static S2MaxDistance Zero();
  static S2MaxDistance Infinity();
  static S2MaxDistance Negative();

  friend bool operator==(S2MaxDistance x, S2MaxDistance y);
  friend bool operator<(S2MaxDistance x, S2MaxDistance y);

  friend S2MaxDistance operator-(S2MaxDistance x, S1ChordAngle delta);
  S1ChordAngle GetChordAngleBound() const;

  // If (dist < *this), updates *this and returns true (used internally).
  bool UpdateMin(const S2MaxDistance& dist);

 private:
  S1ChordAngle distance_;
};

// S2MaxDistanceTarget represents a geometric object to which maximum distances
// on the sphere are measured.
//
// Subtypes are defined below for measuring the distance to a point, an edge,
// an S2Cell, or an S2ShapeIndex (an arbitrary collection of geometry).
using S2MaxDistanceTarget = S2DistanceTarget<S2MaxDistance>;

// An S2DistanceTarget subtype for computing the maximum distance to a point.
class S2MaxDistancePointTarget : public S2MaxDistanceTarget {
 public:
  explicit S2MaxDistancePointTarget(const S2Point& point);

  S2Cap GetCapBound() final;
  bool UpdateMinDistance(const S2Point& p, S2MaxDistance* min_dist) final;
  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         S2MaxDistance* min_dist) final;
  bool UpdateMinDistance(const S2Cell& cell,
                         S2MaxDistance* min_dist) final;
  bool VisitContainingShapes(const S2ShapeIndex& index,
                             const ShapeVisitor& visitor) final;

 private:
  S2Point point_;
};

// An S2DistanceTarget subtype for computing the maximum distance to an edge.
class S2MaxDistanceEdgeTarget : public S2MaxDistanceTarget {
 public:
  explicit S2MaxDistanceEdgeTarget(const S2Point& a, const S2Point& b);

  S2Cap GetCapBound() final;
  bool UpdateMinDistance(const S2Point& p, S2MaxDistance* min_dist) final;
  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         S2MaxDistance* min_dist) final;
  bool UpdateMinDistance(const S2Cell& cell,
                         S2MaxDistance* min_dist) final;
  bool VisitContainingShapes(const S2ShapeIndex& index,
                             const ShapeVisitor& visitor) final;

 private:
  S2Point a_, b_;
};

// An S2DistanceTarget subtype for computing the maximum distance to an S2Cell
// (including the interior of the cell).
class S2MaxDistanceCellTarget : public S2MaxDistanceTarget {
 public:
  explicit S2MaxDistanceCellTarget(const S2Cell& cell);
  S2Cap GetCapBound() final;
  bool UpdateMinDistance(const S2Point& p, S2MaxDistance* min_dist) final;
  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         S2MaxDistance* min_dist) final;
  bool UpdateMinDistance(const S2Cell& cell,
                         S2MaxDistance* min_dist) final;
  bool VisitContainingShapes(const S2ShapeIndex& index,
                             const ShapeVisitor& visitor) final;

 private:
  S2Cell cell_;
};

// An S2DistanceTarget subtype for computing the maximum distance to an
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
// S2FurthestEdgeQuery options.  For example, if include_interiors is true for
// a ShapeIndexTarget but false for the S2FurthestEdgeQuery where the target
// is used, then distances will be measured from the boundary of one
// S2ShapeIndex to the boundary and interior of the other.
//
class S2MaxDistanceShapeIndexTarget : public S2MaxDistanceTarget {
 public:
  explicit S2MaxDistanceShapeIndexTarget(const S2ShapeIndex* index);
  ~S2MaxDistanceShapeIndexTarget() override;

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

  bool set_max_error(const S1ChordAngle& max_error) override;
  S2Cap GetCapBound() final;
  bool UpdateMinDistance(const S2Point& p, S2MaxDistance* min_dist) final;
  bool UpdateMinDistance(const S2Point& v0, const S2Point& v1,
                         S2MaxDistance* min_dist) final;
  bool UpdateMinDistance(const S2Cell& cell,
                         S2MaxDistance* min_dist) final;
  bool VisitContainingShapes(const S2ShapeIndex& query_index,
                             const ShapeVisitor& visitor) final;

 private:
  const S2ShapeIndex* index_;
  std::unique_ptr<S2FurthestEdgeQuery> query_;
};


//////////////////   Implementation details follow   ////////////////////

inline S2MaxDistance S2MaxDistance::Zero() {
  return S2MaxDistance(S1ChordAngle::Straight());
}

inline S2MaxDistance S2MaxDistance::Infinity() {
  return S2MaxDistance(S1ChordAngle::Negative());
}

inline S2MaxDistance S2MaxDistance::Negative() {
  return S2MaxDistance(S1ChordAngle::Infinity());
}

inline bool operator==(S2MaxDistance x, S2MaxDistance y) {
  return x.distance_ == y.distance_;
}

inline bool operator<(S2MaxDistance x, S2MaxDistance y) {
  return x.distance_ > y.distance_;
}

inline S2MaxDistance operator-(S2MaxDistance x, S1ChordAngle delta) {
  return S2MaxDistance(x.distance_ + delta);
}

inline S1ChordAngle S2MaxDistance::GetChordAngleBound() const {
  return S1ChordAngle::Straight() - distance_;
}

inline bool S2MaxDistance::UpdateMin(const S2MaxDistance& dist) {
  if (dist < *this) {
    *this = dist;
    return true;
  }
  return false;
}

inline S2MaxDistancePointTarget::S2MaxDistancePointTarget(const S2Point& point)
    : point_(point) {
}

inline S2MaxDistanceEdgeTarget::S2MaxDistanceEdgeTarget(const S2Point& a,
                                                        const S2Point& b)
    : a_(a), b_(b) {
  a_.Normalize();
  b_.Normalize();
}

inline S2MaxDistanceCellTarget::S2MaxDistanceCellTarget(const S2Cell& cell)
    : cell_(cell) {
}

#endif  // S2_S2MAX_DISTANCE_TARGETS_H_
