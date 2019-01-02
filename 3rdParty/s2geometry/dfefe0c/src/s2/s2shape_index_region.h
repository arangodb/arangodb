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
#ifndef S2_S2SHAPE_INDEX_REGION_H_
#define S2_S2SHAPE_INDEX_REGION_H_

#include <vector>
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_union.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2edge_clipping.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2region.h"
#include "s2/s2shape_index.h"

// This class wraps an S2ShapeIndex object with the additional methods needed
// to implement the S2Region API, in order to allow S2RegionCoverer to compute
// S2CellId coverings of arbitrary collections of geometry.
//
// These methods could conceivably be made part of S2ShapeIndex itself, but
// there are several advantages to having a separate class:
//
//  - The class can be templated in order to avoid virtual calls and memory
//    allocation (for iterators) when the concrete S2ShapeIndex type is known.
//
//  - Implementing these methods efficiently requires an S2ShapeIndex iterator,
//    and this design allows a single iterator to be allocated and reused.
//
//  - S2Region::Clone() is not a good fit for the S2ShapeIndex API because
//    it can't be implemented for some subtypes (e.g., EncodedS2ShapeIndex).
//
// Example usage:
//
// S2CellUnion GetCovering(const S2ShapeIndex& index) {
//   S2RegionCoverer coverer;
//   coverer.mutable_options()->set_max_cells(20);
//   S2CellUnion covering;
//   coverer.GetCovering(MakeS2ShapeIndexRegion(&index), &covering);
//   return covering;
// }
//
// This class is not thread-safe.  To use it in parallel, each thread should
// construct its own instance (this is not expensive).
template <class IndexType>
class S2ShapeIndexRegion final : public S2Region {
 public:
  // Rather than calling this constructor, which requires specifying the
  // S2ShapeIndex type explicitly, the preferred idiom is to call
  // MakeS2ShapeIndexRegion(&index) instead.  For example:
  //
  //   coverer.GetCovering(MakeS2ShapeIndexRegion(&index), &covering);
  explicit S2ShapeIndexRegion(const IndexType* index);

  const IndexType& index() const;

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  // Clone() returns a *shallow* copy; it does not make a copy of the
  // underlying S2ShapeIndex.
  S2ShapeIndexRegion<IndexType>* Clone() const override;

  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;

  // This method currently returns at most 4 cells, unless the index spans
  // multiple faces in which case it may return up to 6 cells.
  void GetCellUnionBound(std::vector<S2CellId> *cell_ids) const override;

  // Returns true if "target" is contained by any single shape.  If the cell
  // is covered by a union of different shapes then it may return false.
  //
  // The implementation is conservative but not exact; if a shape just barely
  // contains the given cell then it may return false.  The maximum error is
  // less than 10 * DBL_EPSILON radians (or about 15 nanometers).
  bool Contains(const S2Cell& target) const override;

  // Returns true if any shape intersects "target".
  //
  // The implementation is conservative but not exact; if a shape is just
  // barely disjoint from the given cell then it may return true.  The maximum
  // error is less than 10 * DBL_EPSILON radians (or about 15 nanometers).
  bool MayIntersect(const S2Cell& target) const override;

  // Returns true if the given point is contained by any two-dimensional shape
  // (i.e., polygon).  Boundaries are treated as being semi-open (i.e., the
  // same rules as S2Polygon).  Zero and one-dimensional shapes are ignored by
  // this method (if you need more flexibility, see S2BooleanOperation).
  bool Contains(const S2Point& p) const override;

 private:
  using Iterator = typename IndexType::Iterator;

  static void CoverRange(S2CellId first, S2CellId last,
                         std::vector<S2CellId> *cell_ids);

  // Returns true if the indexed shape "clipped" in the indexed cell "id"
  // contains the point "p".
  //
  // REQUIRES: id.contains(S2CellId(p))
  bool Contains(S2CellId id, const S2ClippedShape& clipped,
                const S2Point& p) const;

  // Returns true if any edge of the indexed shape "clipped" intersects the
  // cell "target".  It may also return true if an edge is very close to
  // "target"; the maximum error is less than 10 * DBL_EPSILON radians (about
  // 15 nanometers).
  bool AnyEdgeIntersects(const S2ClippedShape& clipped,
                         const S2Cell& target) const;

  // This class is not thread-safe!
  mutable S2ContainsPointQuery<IndexType> contains_query_;

  // Optimization: rather than declaring our own iterator, instead we reuse
  // the iterator declared by S2ContainsPointQuery.  (This improves benchmark
  // times significantly for classes that create a new S2ShapeIndexRegion
  // object on every call to Contains/MayIntersect(S2Cell).
  Iterator& iter_ = *contains_query_.mutable_iter();
};

// Returns an S2ShapeIndexRegion that wraps the given S2ShapeIndex.  Note that
// it is efficient to return S2ShapeIndexRegion objects by value.
template <class IndexType>
S2ShapeIndexRegion<IndexType> MakeS2ShapeIndexRegion(const IndexType* index);


//////////////////   Implementation details follow   ////////////////////


template <class IndexType>
S2ShapeIndexRegion<IndexType>::S2ShapeIndexRegion(const IndexType* index)
    : contains_query_(index) {
}

template <class IndexType>
inline const IndexType& S2ShapeIndexRegion<IndexType>::index() const {
  return contains_query_.index();
}

template <class IndexType>
S2ShapeIndexRegion<IndexType>* S2ShapeIndexRegion<IndexType>::Clone() const {
  return new S2ShapeIndexRegion<IndexType>(&index());
}

template <class IndexType>
S2Cap S2ShapeIndexRegion<IndexType>::GetCapBound() const {
  std::vector<S2CellId> covering;
  GetCellUnionBound(&covering);
  return S2CellUnion(std::move(covering)).GetCapBound();
}

template <class IndexType>
S2LatLngRect S2ShapeIndexRegion<IndexType>::GetRectBound() const {
  std::vector<S2CellId> covering;
  GetCellUnionBound(&covering);
  return S2CellUnion(std::move(covering)).GetRectBound();
}

template <class IndexType>
void S2ShapeIndexRegion<IndexType>::GetCellUnionBound(
    std::vector<S2CellId> *cell_ids) const {
  // We find the range of S2Cells spanned by the index and choose a level such
  // that the entire index can be covered with just a few cells.  There are
  // two cases:
  //
  //  - If the index intersects two or more faces, then for each intersected
  //    face we add one cell to the covering.  Rather than adding the entire
  //    face, instead we add the smallest S2Cell that covers the S2ShapeIndex
  //    cells within that face.
  //
  //  - If the index intersects only one face, then we first find the smallest
  //    cell S that contains the index cells (just like the case above).
  //    However rather than using the cell S itself, instead we repeat this
  //    process for each of its child cells.  In other words, for each
  //    child cell C we add the smallest S2Cell C' that covers the index cells
  //    within C.  This extra step is relatively cheap and produces much
  //    tighter coverings when the S2ShapeIndex consists of a small region
  //    near the center of a large S2Cell.
  //
  // The following code uses only a single Iterator object because creating an
  // Iterator may be relatively expensive for some S2ShapeIndex types (e.g.,
  // it may involve memory allocation).
  cell_ids->clear();
  cell_ids->reserve(6);

  // Find the last S2CellId in the index.
  iter_.Finish();
  if (!iter_.Prev()) return;  // Empty index.
  const S2CellId last_index_id = iter_.id();
  iter_.Begin();
  if (iter_.id() != last_index_id) {
    // The index has at least two cells.  Choose an S2CellId level such that
    // the entire index can be spanned with at most 6 cells (if the index
    // spans multiple faces) or 4 cells (it the index spans a single face).
    int level = iter_.id().GetCommonAncestorLevel(last_index_id) + 1;

    // For each cell C at the chosen level, we compute the smallest S2Cell
    // that covers the S2ShapeIndex cells within C.
    const S2CellId last_id = last_index_id.parent(level);
    for (auto id = iter_.id().parent(level); id != last_id; id = id.next()) {
      // If the cell C does not contain any index cells, then skip it.
      if (id.range_max() < iter_.id()) continue;

      // Find the range of index cells contained by C and then shrink C so
      // that it just covers those cells.
      S2CellId first = iter_.id();
      iter_.Seek(id.range_max().next());
      iter_.Prev();
      CoverRange(first, iter_.id(), cell_ids);
      iter_.Next();
    }
  }
  CoverRange(iter_.id(), last_index_id, cell_ids);
}

// Computes the smallest S2Cell that covers the S2Cell range (first, last) and
// adds this cell to "cell_ids".
//
// REQUIRES: "first" and "last" have a common ancestor.
template <class IndexType>
inline void S2ShapeIndexRegion<IndexType>::CoverRange(
    S2CellId first, S2CellId last, std::vector<S2CellId> *cell_ids) {
  if (first == last) {
    // The range consists of a single index cell.
    cell_ids->push_back(first);
  } else {
    // Add the lowest common ancestor of the given range.
    int level = first.GetCommonAncestorLevel(last);
    S2_DCHECK_GE(level, 0);
    cell_ids->push_back(first.parent(level));
  }
}

template <class IndexType>
bool S2ShapeIndexRegion<IndexType>::Contains(const S2Cell& target) const {
  S2ShapeIndex::CellRelation relation = iter_.Locate(target.id());

  // If the relation is DISJOINT, then "target" is not contained.  Similarly if
  // the relation is SUBDIVIDED then "target" is not contained, since index
  // cells are subdivided only if they (nearly) intersect too many edges.
  if (relation != S2ShapeIndex::INDEXED) return false;

  // Otherwise, the iterator points to an index cell containing "target".
  // If any shape contains the target cell, we return true.
  S2_DCHECK(iter_.id().contains(target.id()));
  const S2ShapeIndexCell& cell = iter_.cell();
  for (int s = 0; s < cell.num_clipped(); ++s) {
    const S2ClippedShape& clipped = cell.clipped(s);
    // The shape contains the target cell iff the shape contains the cell
    // center and none of its edges intersects the (padded) cell interior.
    if (iter_.id() == target.id()) {
      if (clipped.num_edges() == 0 && clipped.contains_center()) return true;
    } else {
      // It is faster to call AnyEdgeIntersects() before Contains().
      if (index().shape(clipped.shape_id())->dimension() == 2 &&
          !AnyEdgeIntersects(clipped, target) &&
          contains_query_.ShapeContains(iter_, clipped, target.GetCenter())) {
        return true;
      }
    }
  }
  return false;
}

template <class IndexType>
bool S2ShapeIndexRegion<IndexType>::MayIntersect(const S2Cell& target) const {
  S2ShapeIndex::CellRelation relation = iter_.Locate(target.id());

  // If "target" does not overlap any index cell, there is no intersection.
  if (relation == S2ShapeIndex::DISJOINT) return false;

  // If "target" is subdivided into one or more index cells, then there is an
  // intersection to within the S2ShapeIndex error bound.
  if (relation == S2ShapeIndex::SUBDIVIDED) return true;

  // Otherwise, the iterator points to an index cell containing "target".
  //
  // If "target" is an index cell itself, there is an intersection because index
  // cells are created only if they have at least one edge or they are
  // entirely contained by the loop.
  S2_DCHECK(iter_.id().contains(target.id()));
  if (iter_.id() == target.id()) return true;

  // Test whether any shape intersects the target cell or contains its center.
  const S2ShapeIndexCell& cell = iter_.cell();
  for (int s = 0; s < cell.num_clipped(); ++s) {
    const S2ClippedShape& clipped = cell.clipped(s);
    if (AnyEdgeIntersects(clipped, target)) return true;
    if (contains_query_.ShapeContains(iter_, clipped, target.GetCenter())) {
      return true;
    }
  }
  return false;
}

template <class IndexType>
bool S2ShapeIndexRegion<IndexType>::Contains(const S2Point& p) const {
  if (iter_.Locate(p)) {
    const S2ShapeIndexCell& cell = iter_.cell();
    for (int s = 0; s < cell.num_clipped(); ++s) {
      if (contains_query_.ShapeContains(iter_, cell.clipped(s), p)) {
        return true;
      }
    }
  }
  return false;
}

template <class IndexType>
bool S2ShapeIndexRegion<IndexType>::AnyEdgeIntersects(
    const S2ClippedShape& clipped, const S2Cell& target) const {
  static const double kMaxError = (S2::kFaceClipErrorUVCoord +
                                   S2::kIntersectsRectErrorUVDist);
  const R2Rect bound = target.GetBoundUV().Expanded(kMaxError);
  const int face = target.face();
  const S2Shape& shape = *index().shape(clipped.shape_id());
  const int num_edges = clipped.num_edges();
  for (int i = 0; i < num_edges; ++i) {
    const auto edge = shape.edge(clipped.edge(i));
    R2Point p0, p1;
    if (S2::ClipToPaddedFace(edge.v0, edge.v1, face, kMaxError, &p0, &p1) &&
        S2::IntersectsRect(p0, p1, bound)) {
      return true;
    }
  }
  return false;
}

template <class IndexType>
inline S2ShapeIndexRegion<IndexType> MakeS2ShapeIndexRegion(
    const IndexType* index) {
  return S2ShapeIndexRegion<IndexType>(index);
}

#endif  // S2_S2SHAPE_INDEX_REGION_H_
