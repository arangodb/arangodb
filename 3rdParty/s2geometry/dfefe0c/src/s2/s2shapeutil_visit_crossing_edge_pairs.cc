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

#include "s2/s2shapeutil_visit_crossing_edge_pairs.h"

#include "s2/s2crossing_edge_query.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2error.h"
#include "s2/s2shapeutil_range_iterator.h"
#include "s2/s2wedge_relations.h"

using std::vector;
using ChainPosition = S2Shape::ChainPosition;

namespace s2shapeutil {

// Ensure that we don't usually need to allocate memory when collecting the
// edges in an S2ShapeIndex cell (which by default have about 10 edges).
using ShapeEdgeVector = absl::InlinedVector<ShapeEdge, 16>;

// Appends all edges in the given S2ShapeIndexCell to the given vector.
static void AppendShapeEdges(const S2ShapeIndex& index,
                             const S2ShapeIndexCell& cell,
                             ShapeEdgeVector* shape_edges) {
  for (int s = 0; s < cell.num_clipped(); ++s) {
    const S2ClippedShape& clipped = cell.clipped(s);
    const S2Shape& shape = *index.shape(clipped.shape_id());
    int num_edges = clipped.num_edges();
    for (int i = 0; i < num_edges; ++i) {
      shape_edges->push_back(ShapeEdge(shape, clipped.edge(i)));
    }
  }
}

// Returns a vector containing all edges in the given S2ShapeIndexCell.
// (The result is returned as an output parameter so that the same storage can
// be reused, rather than allocating a new temporary vector each time.)
inline static void GetShapeEdges(const S2ShapeIndex& index,
                                 const S2ShapeIndexCell& cell,
                                 ShapeEdgeVector* shape_edges) {
  shape_edges->clear();
  AppendShapeEdges(index, cell, shape_edges);
}

// Returns a vector containing all edges in the given S2ShapeIndexCell vector.
// (The result is returned as an output parameter so that the same storage can
// be reused, rather than allocating a new temporary vector each time.)
inline static void GetShapeEdges(const S2ShapeIndex& index,
                                 const vector<const S2ShapeIndexCell*>& cells,
                                 ShapeEdgeVector* shape_edges) {
  shape_edges->clear();
  for (auto cell : cells) {
    AppendShapeEdges(index, *cell, shape_edges);
  }
}

// Given a vector of edges within an S2ShapeIndexCell, visit all pairs of
// crossing edges (of the given CrossingType).
static bool VisitCrossings(const ShapeEdgeVector& shape_edges,
                           CrossingType type, bool need_adjacent,
                           const EdgePairVisitor& visitor) {
  const int min_crossing_sign = (type == CrossingType::INTERIOR) ? 1 : 0;
  int num_edges = shape_edges.size();
  for (int i = 0; i + 1 < num_edges; ++i) {
    const ShapeEdge& a = shape_edges[i];
    int j = i + 1;
    // A common situation is that an edge AB is followed by an edge BC.  We
    // only need to visit such crossings if "need_adjacent" is true (even if
    // AB and BC belong to different edge chains).
    if (!need_adjacent && a.v1() == shape_edges[j].v0()) {
      if (++j >= num_edges) break;
    }
    S2EdgeCrosser crosser(&a.v0(), &a.v1());
    for (; j < num_edges; ++j) {
      const ShapeEdge& b = shape_edges[j];
      if (crosser.c() == nullptr || *crosser.c() != b.v0()) {
        crosser.RestartAt(&b.v0());
      }
      int sign = crosser.CrossingSign(&b.v1());
      if (sign >= min_crossing_sign) {
        if (!visitor(a, b, sign == 1)) return false;
      }
    }
  }
  return true;
}

// Visits all pairs of crossing edges in the given S2ShapeIndex, terminating
// early if the given EdgePairVisitor function returns false (in which case
// VisitCrossings returns false as well).  "type" indicates whether all
// crossings should be visited, or only interior crossings.
//
// If "need_adjacent" is false, then edge pairs of the form (AB, BC) may
// optionally be ignored (even if the two edges belong to different edge
// chains).  This option exists for the benefit of FindSelfIntersection(),
// which does not need such edge pairs (see below).
static bool VisitCrossings(
    const S2ShapeIndex& index, CrossingType type, bool need_adjacent,
    const EdgePairVisitor& visitor) {
  // TODO(ericv): Use brute force if the total number of edges is small enough
  // (using a larger threshold if the S2ShapeIndex is not constructed yet).
  ShapeEdgeVector shape_edges;
  for (S2ShapeIndex::Iterator it(&index, S2ShapeIndex::BEGIN);
       !it.done(); it.Next()) {
    GetShapeEdges(index, it.cell(), &shape_edges);
    if (!VisitCrossings(shape_edges, type, need_adjacent, visitor)) {
      return false;
    }
  }
  return true;
}

bool VisitCrossingEdgePairs(const S2ShapeIndex& index, CrossingType type,
                            const EdgePairVisitor& visitor) {
  const bool need_adjacent = (type == CrossingType::ALL);
  return VisitCrossings(index, type, need_adjacent, visitor);
}

//////////////////////////////////////////////////////////////////////

// IndexCrosser is a helper class for finding the edge crossings between a
// pair of S2ShapeIndexes.  It is instantiated twice, once for the index pair
// (A,B) and once for the index pair (B,A), in order to be able to test edge
// crossings in the most efficient order.
namespace {
class IndexCrosser {
 public:
  // If "swapped" is true, the loops A and B have been swapped.  This affects
  // how arguments are passed to the given loop relation, since for example
  // A.Contains(B) is not the same as B.Contains(A).
  IndexCrosser(const S2ShapeIndex& a_index, const S2ShapeIndex& b_index,
               CrossingType type, const EdgePairVisitor& visitor, bool swapped)
      : a_index_(a_index), b_index_(b_index), visitor_(visitor),
        min_crossing_sign_(type == CrossingType::INTERIOR ? 1 : 0),
        swapped_(swapped), b_query_(&b_index_) {
  }

  // Given two iterators positioned such that ai->id().Contains(bi->id()),
  // visits all crossings between edges of A and B that intersect a->id().
  // Terminates early and returns false if visitor_ returns false.
  // Advances both iterators past ai->id().
  bool VisitCrossings(RangeIterator* ai, RangeIterator* bi);

  // Given two index cells, visits all crossings between edges of those cells.
  // Terminates early and returns false if visitor_ returns false.
  bool VisitCellCellCrossings(const S2ShapeIndexCell& a_cell,
                              const S2ShapeIndexCell& b_cell);

 private:
  bool VisitEdgePair(const ShapeEdge& a, const ShapeEdge& b, bool is_interior);

  // Visits all crossings of the current edge with all edges of the given index
  // cell of B.  Terminates early and returns false if visitor_ returns false.
  bool VisitEdgeCellCrossings(const ShapeEdge& a,
                              const S2ShapeIndexCell& b_cell);

  // Visits all crossings of any edge in "a_cell" with any index cell of B that
  // is a descendant of "b_id".  Terminates early and returns false if
  // visitor_ returns false.
  bool VisitSubcellCrossings(const S2ShapeIndexCell& a_cell, S2CellId b_id);

  // Visits all crossings of any edge in "a_edges" with any edge in "b_edges".
  bool VisitEdgesEdgesCrossings(const ShapeEdgeVector& a_edges,
                                const ShapeEdgeVector& b_edges);

  const S2ShapeIndex& a_index_;
  const S2ShapeIndex& b_index_;
  const EdgePairVisitor& visitor_;
  const int min_crossing_sign_;
  const bool swapped_;

  // Temporary data declared here to avoid repeated memory allocations.
  S2CrossingEdgeQuery b_query_;
  vector<const S2ShapeIndexCell*> b_cells_;
  ShapeEdgeVector a_shape_edges_;
  ShapeEdgeVector b_shape_edges_;
};
}  // namespace

inline bool IndexCrosser::VisitEdgePair(const ShapeEdge& a, const ShapeEdge& b,
                                        bool is_interior) {
  if (swapped_) {
    return visitor_(b, a, is_interior);
  } else {
    return visitor_(a, b, is_interior);
  }
}

bool IndexCrosser::VisitEdgeCellCrossings(const ShapeEdge& a,
                                          const S2ShapeIndexCell& b_cell) {
  // Test the current edge of A against all edges of "b_cell".

  // Note that we need to use a new S2EdgeCrosser (or call Init) whenever we
  // replace the contents of b_shape_edges_, since S2EdgeCrosser requires that
  // its S2Point arguments point to values that persist between Init() calls.
  GetShapeEdges(b_index_, b_cell, &b_shape_edges_);
  S2EdgeCrosser crosser(&a.v0(), &a.v1());
  for (const ShapeEdge& b : b_shape_edges_) {
    if (crosser.c() == nullptr || *crosser.c() != b.v0()) {
      crosser.RestartAt(&b.v0());
    }
    int sign = crosser.CrossingSign(&b.v1());
    if (sign >= min_crossing_sign_) {
      if (!VisitEdgePair(a, b, sign == 1)) return false;
    }
  }
  return true;
}

bool IndexCrosser::VisitSubcellCrossings(const S2ShapeIndexCell& a_cell,
                                         S2CellId b_id) {
  // Test all edges of "a_cell" against the edges contained in B index cells
  // that are descendants of "b_id".
  GetShapeEdges(a_index_, a_cell, &a_shape_edges_);
  S2PaddedCell b_root(b_id, 0);
  for (const ShapeEdge& a : a_shape_edges_) {
    // Use an S2CrossingEdgeQuery starting at "b_root" to find the index cells
    // of B that might contain crossing edges.
    if (!b_query_.VisitCells(
            a.v0(), a.v1(), b_root, [&a, this](const S2ShapeIndexCell& cell) {
              return VisitEdgeCellCrossings(a, cell);
            })) {
      return false;
    }
  }
  return true;
}

bool IndexCrosser::VisitEdgesEdgesCrossings(const ShapeEdgeVector& a_edges,
                                            const ShapeEdgeVector& b_edges) {
  // Test all edges of "a_edges" against all edges of "b_edges".
  for (const ShapeEdge& a : a_edges) {
    S2EdgeCrosser crosser(&a.v0(), &a.v1());
    for (const ShapeEdge& b : b_edges) {
      if (crosser.c() == nullptr || *crosser.c() != b.v0()) {
        crosser.RestartAt(&b.v0());
      }
      int sign = crosser.CrossingSign(&b.v1());
      if (sign >= min_crossing_sign_) {
        if (!VisitEdgePair(a, b, sign == 1)) return false;
      }
    }
  }
  return true;
}

inline bool IndexCrosser::VisitCellCellCrossings(
    const S2ShapeIndexCell& a_cell, const S2ShapeIndexCell& b_cell) {
  // Test all edges of "a_cell" against all edges of "b_cell".
  GetShapeEdges(a_index_, a_cell, &a_shape_edges_);
  GetShapeEdges(b_index_, b_cell, &b_shape_edges_);
  return VisitEdgesEdgesCrossings(a_shape_edges_, b_shape_edges_);
}

bool IndexCrosser::VisitCrossings(RangeIterator* ai, RangeIterator* bi) {
  S2_DCHECK(ai->id().contains(bi->id()));
  if (ai->cell().num_edges() == 0) {
    // Skip over the cells of B using binary search.
    bi->SeekBeyond(*ai);
  } else {
    // If ai->id() intersects many edges of B, then it is faster to use
    // S2CrossingEdgeQuery to narrow down the candidates.  But if it
    // intersects only a few edges, it is faster to check all the crossings
    // directly.  We handle this by advancing "bi" and keeping track of how
    // many edges we would need to test.
    static const int kEdgeQueryMinEdges = 23;
    int b_edges = 0;
    b_cells_.clear();
    do {
      int cell_edges = bi->cell().num_edges();
      if (cell_edges > 0) {
        b_edges += cell_edges;
        if (b_edges >= kEdgeQueryMinEdges) {
          // There are too many edges, so use an S2CrossingEdgeQuery.
          if (!VisitSubcellCrossings(ai->cell(), ai->id())) return false;
          bi->SeekBeyond(*ai);
          return true;
        }
        b_cells_.push_back(&bi->cell());
      }
      bi->Next();
    } while (bi->id() <= ai->range_max());
    if (!b_cells_.empty()) {
      // Test all the edge crossings directly.
      GetShapeEdges(a_index_, ai->cell(), &a_shape_edges_);
      GetShapeEdges(b_index_, b_cells_, &b_shape_edges_);
      if (!VisitEdgesEdgesCrossings(a_shape_edges_, b_shape_edges_)) {
        return false;
      }
    }
  }
  ai->Next();
  return true;
}

bool VisitCrossingEdgePairs(const S2ShapeIndex& a_index,
                            const S2ShapeIndex& b_index,
                            CrossingType type, const EdgePairVisitor& visitor) {
  // We look for S2CellId ranges where the indexes of A and B overlap, and
  // then test those edges for crossings.

  // TODO(ericv): Use brute force if the total number of edges is small enough
  // (using a larger threshold if the S2ShapeIndex is not constructed yet).
  RangeIterator ai(a_index), bi(b_index);
  IndexCrosser ab(a_index, b_index, type, visitor, false);  // Tests A against B
  IndexCrosser ba(b_index, a_index, type, visitor, true);   // Tests B against A
  while (!ai.done() || !bi.done()) {
    if (ai.range_max() < bi.range_min()) {
      // The A and B cells don't overlap, and A precedes B.
      ai.SeekTo(bi);
    } else if (bi.range_max() < ai.range_min()) {
      // The A and B cells don't overlap, and B precedes A.
      bi.SeekTo(ai);
    } else {
      // One cell contains the other.  Determine which cell is larger.
      int64 ab_relation = ai.id().lsb() - bi.id().lsb();
      if (ab_relation > 0) {
        // A's index cell is larger.
        if (!ab.VisitCrossings(&ai, &bi)) return false;
      } else if (ab_relation < 0) {
        // B's index cell is larger.
        if (!ba.VisitCrossings(&bi, &ai)) return false;
      } else {
        // The A and B cells are the same.
        if (ai.cell().num_edges() > 0 && bi.cell().num_edges() > 0) {
          if (!ab.VisitCellCellCrossings(ai.cell(), bi.cell())) return false;
        }
        ai.Next();
        bi.Next();
      }
    }
  }
  return true;
}

//////////////////////////////////////////////////////////////////////

// Helper function that formats a loop error message.  If the loop belongs to
// a multi-loop polygon, adds a prefix indicating which loop is affected.
static void InitLoopError(S2Error::Code code, const char* format,
                          ChainPosition ap, ChainPosition bp,
                          bool is_polygon, S2Error* error) {
  error->Init(code, format, ap.offset, bp.offset);
  if (is_polygon) {
    error->Init(code, "Loop %d: %s", ap.chain_id, error->text().c_str());
  }
}

// Given two loop edges that cross (including at a shared vertex), return true
// if there is a crossing error and set "error" to a human-readable message.
static bool FindCrossingError(const S2Shape& shape,
                              const ShapeEdge& a, const ShapeEdge& b,
                              bool is_interior, S2Error* error) {
  bool is_polygon = shape.num_chains() > 1;
  S2Shape::ChainPosition ap = shape.chain_position(a.id().edge_id);
  S2Shape::ChainPosition bp = shape.chain_position(b.id().edge_id);
  if (is_interior) {
    if (ap.chain_id != bp.chain_id) {
      error->Init(S2Error::POLYGON_LOOPS_CROSS,
                  "Loop %d edge %d crosses loop %d edge %d",
                  ap.chain_id, ap.offset, bp.chain_id, bp.offset);
    } else {
      InitLoopError(S2Error::LOOP_SELF_INTERSECTION,
                    "Edge %d crosses edge %d", ap, bp, is_polygon, error);
    }
    return true;
  }
  // Loops are not allowed to have duplicate vertices, and separate loops
  // are not allowed to share edges or cross at vertices.  We only need to
  // check a given vertex once, so we also require that the two edges have
  // the same end vertex.
  if (a.v1() != b.v1()) return false;
  if (ap.chain_id == bp.chain_id) {
    InitLoopError(S2Error::DUPLICATE_VERTICES,
                  "Edge %d has duplicate vertex with edge %d",
                  ap, bp, is_polygon, error);
    return true;
  }
  int a_len = shape.chain(ap.chain_id).length;
  int b_len = shape.chain(bp.chain_id).length;
  int a_next = (ap.offset + 1 == a_len) ? 0 : ap.offset + 1;
  int b_next = (bp.offset + 1 == b_len) ? 0 : bp.offset + 1;
  S2Point a2 = shape.chain_edge(ap.chain_id, a_next).v1;
  S2Point b2 = shape.chain_edge(bp.chain_id, b_next).v1;
  if (a.v0() == b.v0() || a.v0() == b2) {
    // The second edge index is sometimes off by one, hence "near".
    error->Init(S2Error::POLYGON_LOOPS_SHARE_EDGE,
                "Loop %d edge %d has duplicate near loop %d edge %d",
                ap.chain_id, ap.offset, bp.chain_id, bp.offset);
    return true;
  }
  // Since S2ShapeIndex loops are oriented such that the polygon interior is
  // always on the left, we need to handle the case where one wedge contains
  // the complement of the other wedge.  This is not specifically detected by
  // GetWedgeRelation, so there are two cases to check for.
  //
  // Note that we don't need to maintain any state regarding loop crossings
  // because duplicate edges are detected and rejected above.
  if (S2::GetWedgeRelation(a.v0(), a.v1(), a2, b.v0(), b2) ==
      S2::WEDGE_PROPERLY_OVERLAPS &&
      S2::GetWedgeRelation(a.v0(), a.v1(), a2, b2, b.v0()) ==
      S2::WEDGE_PROPERLY_OVERLAPS) {
    error->Init(S2Error::POLYGON_LOOPS_CROSS,
                "Loop %d edge %d crosses loop %d edge %d",
                ap.chain_id, ap.offset, bp.chain_id, bp.offset);
    return true;
  }
  return false;
}

bool FindSelfIntersection(const S2ShapeIndex& index, S2Error* error) {
  if (index.num_shape_ids() == 0) return false;
  S2_DCHECK_EQ(1, index.num_shape_ids());
  const S2Shape& shape = *index.shape(0);

  // Visit all crossing pairs except possibly for ones of the form (AB, BC),
  // since such pairs are very common and FindCrossingError() only needs pairs
  // of the form (AB, AC).
  return !VisitCrossings(
      index, CrossingType::ALL, false /*need_adjacent*/,
      [&](const ShapeEdge& a, const ShapeEdge& b, bool is_interior) {
        return !FindCrossingError(shape, a, b, is_interior, error);
      });
}

}  // namespace s2shapeutil
