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

#ifndef S2_S2SHAPE_INDEX_BUFFERED_REGION_H_
#define S2_S2SHAPE_INDEX_BUFFERED_REGION_H_

#include <vector>
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_union.h"
#include "s2/s2closest_edge_query.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2region.h"
#include "s2/s2shape_index.h"

// This class provides a way to expand an arbitrary collection of geometry by
// a fixed radius (an operation variously known as "buffering", "offsetting",
// or "Minkowski sum with a disc") in order to compute an S2CellId covering
// (see S2RegionCoverer).  The resulting covering contains all points within
// the given radius of any point in the original geometry.
//
// This class does not actually buffer the geometry; instead it implements the
// S2Region API by computing the distance from candidate S2CellIds to the
// original geometry.  If this distance is below the given radius then the
// S2CellId intersects the buffered geometry.  For example, if the original
// geometry consists of a single S2Point then the buffered geometry is exactly
// equivalent to an S2Cap with the given radius.  (Note that the region is not
// approximated as a polygonal loop.)
//
// Example usage:
//
// S2CellUnion GetBufferedCovering(const S2ShapeIndex& index, S1Angle radius) {
//   S2RegionCoverer coverer;
//   coverer.mutable_options()->set_max_cells(20);
//   S2CellUnion covering;
//   S2ShapeIndexBufferedRegion region(&index, radius);
//   coverer.GetCovering(region, &covering);
//   return covering;
// }
//
// This class is not thread-safe.  To use it in parallel, each thread should
// construct its own instance (this is not expensive).
class S2ShapeIndexBufferedRegion final : public S2Region {
 public:
  // Default constructor; requires Init() to be called.
  S2ShapeIndexBufferedRegion();

  // Constructs a region representing all points within the given radius of
  // any point in the given S2ShapeIndex.
  S2ShapeIndexBufferedRegion(const S2ShapeIndex* index,
                             S1ChordAngle radius);

  // Convenience constructor that accepts an S1Angle for the radius.
  // REQUIRES: radius >= S1Angle::Zero()
  S2ShapeIndexBufferedRegion(const S2ShapeIndex* index, S1Angle radius)
      : S2ShapeIndexBufferedRegion(index, S1ChordAngle(radius)) {}

  // Equivalent to the constructor above.
  void Init(const S2ShapeIndex* index, S1ChordAngle radius);

  const S2ShapeIndex& index() const;
  S1ChordAngle radius() const;

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  // Clone() returns a *shallow* copy; it does not make a copy of the
  // underlying S2ShapeIndex.
  S2ShapeIndexBufferedRegion* Clone() const override;

  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;

  // This method returns a small non-optimal covering that may include
  // duplicate or overlapping cells.  It should not be used directly.
  // Instead, use S2RegionCoverer::GetCovering or GetFastCovering.
  void GetCellUnionBound(std::vector<S2CellId> *cellids) const override;

  // The implementation is approximate but conservative; it always returns
  // "false" if the cell is not contained by the buffered region, but it may
  // also return false in some cases where "cell" is in fact contained.
  bool Contains(const S2Cell& cell) const override;

  // Returns true if any buffered shape intersects "cell" (to within a very
  // small error margin).
  bool MayIntersect(const S2Cell& cell) const override;

  // Returns true if the given point is contained by the buffered region,
  // i.e. if it is within the given radius of any original shape.
  bool Contains(const S2Point& p) const override;

 private:
  S1ChordAngle radius_;

  // In order to handle (radius_ == 0) corectly, we need to test whether
  // distances are less than or equal to "radius_".  This is done by testing
  // whether distances are less than radius_.Successor().
  S1ChordAngle radius_successor_;

  mutable S2ClosestEdgeQuery query_;  // This class is not thread-safe!
};


//////////////////   Implementation details follow   ////////////////////


inline S2ShapeIndexBufferedRegion::S2ShapeIndexBufferedRegion(
    const S2ShapeIndex* index, S1ChordAngle radius)
    : radius_(radius), radius_successor_(radius.Successor()), query_(index) {
  query_.mutable_options()->set_include_interiors(true);
}

inline const S2ShapeIndex& S2ShapeIndexBufferedRegion::index() const {
  return query_.index();
}

inline S1ChordAngle S2ShapeIndexBufferedRegion::radius() const {
  return radius_;
}

#endif  // S2_S2SHAPE_INDEX_BUFFERED_REGION_H_
