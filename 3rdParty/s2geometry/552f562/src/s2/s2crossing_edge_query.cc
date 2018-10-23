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

#include "s2/s2crossing_edge_query.h"

#include <algorithm>
#include <vector>

#include "s2/base/logging.h"
#include "s2/r1interval.h"
#include "s2/s2cell_id.h"
#include "s2/s2edge_clipping.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2shapeutil_count_edges.h"

using s2shapeutil::ShapeEdge;
using s2shapeutil::ShapeEdgeId;
using std::vector;

// For small loops it is faster to use brute force.  The threshold below was
// determined using the benchmarks in the unit test.
static const int kMaxBruteForceEdges = 27;

S2CrossingEdgeQuery::S2CrossingEdgeQuery() {
}

S2CrossingEdgeQuery::~S2CrossingEdgeQuery() {
}

void S2CrossingEdgeQuery::Init(const S2ShapeIndex* index) {
  index_ = index;
  iter_.Init(index);
}

vector<s2shapeutil::ShapeEdge> S2CrossingEdgeQuery::GetCrossingEdges(
    const S2Point& a0, const S2Point& a1, CrossingType type) {
  vector<s2shapeutil::ShapeEdge> edges;
  GetCrossingEdges(a0, a1, type, &edges);
  return edges;
}

vector<s2shapeutil::ShapeEdge> S2CrossingEdgeQuery::GetCrossingEdges(
    const S2Point& a0, const S2Point& a1, const S2Shape& shape,
    CrossingType type) {
  vector<s2shapeutil::ShapeEdge> edges;
  GetCrossingEdges(a0, a1, shape, type, &edges);
  return edges;
}

void S2CrossingEdgeQuery::GetCrossingEdges(
    const S2Point& a0, const S2Point& a1, CrossingType type,
    vector<ShapeEdge>* edges) {
  edges->clear();
  GetCandidates(a0, a1, &tmp_candidates_);
  int min_sign = (type == CrossingType::ALL) ? 0 : 1;
  S2CopyingEdgeCrosser crosser(a0, a1);
  int shape_id = -1;
  const S2Shape* shape = nullptr;
  for (ShapeEdgeId candidate : tmp_candidates_) {
    if (candidate.shape_id != shape_id) {
      shape_id = candidate.shape_id;
      shape = index_->shape(shape_id);
    }
    int edge_id = candidate.edge_id;
    S2Shape::Edge b = shape->edge(edge_id);
    if (crosser.CrossingSign(b.v0, b.v1) >= min_sign) {
      edges->push_back(ShapeEdge(shape_id, edge_id, b));
    }
  }
}

void S2CrossingEdgeQuery::GetCrossingEdges(
    const S2Point& a0, const S2Point& a1, const S2Shape& shape,
    CrossingType type, vector<ShapeEdge>* edges) {
  edges->clear();
  GetCandidates(a0, a1, shape, &tmp_candidates_);
  int min_sign = (type == CrossingType::ALL) ? 0 : 1;
  S2CopyingEdgeCrosser crosser(a0, a1);
  for (ShapeEdgeId candidate : tmp_candidates_) {
    int edge_id = candidate.edge_id;
    S2Shape::Edge b = shape.edge(edge_id);
    if (crosser.CrossingSign(b.v0, b.v1) >= min_sign) {
      edges->push_back(ShapeEdge(shape.id(), edge_id, b));
    }
  }
}

vector<ShapeEdgeId> S2CrossingEdgeQuery::GetCandidates(
    const S2Point& a0, const S2Point& a1) {
  vector<ShapeEdgeId> edges;
  GetCandidates(a0, a1, &edges);
  return edges;
}

vector<ShapeEdgeId> S2CrossingEdgeQuery::GetCandidates(
    const S2Point& a0, const S2Point& a1, const S2Shape& shape) {
  vector<ShapeEdgeId> edges;
  GetCandidates(a0, a1, shape, &edges);
  return edges;
}

void S2CrossingEdgeQuery::GetCandidates(const S2Point& a0, const S2Point& a1,
                                        vector<ShapeEdgeId>* edges) {
  edges->clear();
  int num_edges = s2shapeutil::CountEdgesUpTo(*index_, kMaxBruteForceEdges + 1);
  if (num_edges <= kMaxBruteForceEdges) {
    edges->reserve(num_edges);
  }
  VisitRawCandidates(a0, a1, [edges](ShapeEdgeId id) {
      edges->push_back(id);
      return true;
    });
  if (edges->size() > 1) {
    std::sort(edges->begin(), edges->end());
    edges->erase(std::unique(edges->begin(), edges->end()), edges->end());
  }
}

void S2CrossingEdgeQuery::GetCandidates(const S2Point& a0, const S2Point& a1,
                                        const S2Shape& shape,
                                        vector<ShapeEdgeId>* edges) {
  edges->clear();
  int num_edges = shape.num_edges();
  if (num_edges <= kMaxBruteForceEdges) {
    edges->reserve(num_edges);
  }
  VisitRawCandidates(a0, a1, shape, [edges](ShapeEdgeId id) {
      edges->push_back(id);
      return true;
    });
  if (edges->size() > 1) {
    std::sort(edges->begin(), edges->end());
    edges->erase(std::unique(edges->begin(), edges->end()), edges->end());
  }
}

bool S2CrossingEdgeQuery::VisitRawCandidates(
    const S2Point& a0, const S2Point& a1, const ShapeEdgeIdVisitor& visitor) {
  int num_edges = s2shapeutil::CountEdgesUpTo(*index_, kMaxBruteForceEdges + 1);
  if (num_edges <= kMaxBruteForceEdges) {
    int num_shape_ids = index_->num_shape_ids();
    for (int s = 0; s < num_shape_ids; ++s) {
      const S2Shape* shape = index_->shape(s);
      if (shape == nullptr) continue;
      int num_shape_edges = shape->num_edges();
      for (int e = 0; e < num_shape_edges; ++e) {
        if (!visitor(ShapeEdgeId(s, e))) return false;
      }
    }
    return true;
  }
  return VisitCells(a0, a1, [&visitor](const S2ShapeIndexCell& cell) {
      for (int s = 0; s < cell.num_clipped(); ++s) {
        const S2ClippedShape& clipped = cell.clipped(s);
        for (int j = 0; j < clipped.num_edges(); ++j) {
          if (!visitor(ShapeEdgeId(clipped.shape_id(), clipped.edge(j)))) {
            return false;
          }
        }
      }
      return true;
    });
}

bool S2CrossingEdgeQuery::VisitRawCandidates(
    const S2Point& a0, const S2Point& a1, const S2Shape& shape,
    const ShapeEdgeIdVisitor& visitor) {
  int num_edges = shape.num_edges();
  if (num_edges <= kMaxBruteForceEdges) {
    for (int e = 0; e < num_edges; ++e) {
      if (!visitor(ShapeEdgeId(shape.id(), e))) return false;
    }
    return true;
  }
  return VisitCells(a0, a1, [&shape, &visitor](const S2ShapeIndexCell& cell) {
      const S2ClippedShape* clipped = cell.find_clipped(shape.id());
      if (clipped == nullptr) return true;
      for (int j = 0; j < clipped->num_edges(); ++j) {
        if (!visitor(ShapeEdgeId(shape.id(), clipped->edge(j)))) return false;
      }
      return true;
    });
}

bool S2CrossingEdgeQuery::VisitCells(const S2Point& a0, const S2Point& a1,
                                     const CellVisitor& visitor) {
  visitor_ = &visitor;
  S2::FaceSegmentVector segments;
  S2::GetFaceSegments(a0, a1, &segments);
  for (const auto& segment : segments) {
    a0_ = segment.a;
    a1_ = segment.b;

    // Optimization: rather than always starting the recursive subdivision at
    // the top level face cell, instead we start at the smallest S2CellId that
    // contains the edge (the "edge root cell").  This typically lets us skip
    // quite a few levels of recursion since most edges are short.
    R2Rect edge_bound = R2Rect::FromPointPair(a0_, a1_);
    S2PaddedCell pcell(S2CellId::FromFace(segment.face), 0);
    S2CellId edge_root = pcell.ShrinkToFit(edge_bound);

    // Now we need to determine how the edge root cell is related to the cells
    // in the spatial index (cell_map_).  There are three cases:
    //
    //  1. edge_root is an index cell or is contained within an index cell.
    //     In this case we only need to look at the contents of that cell.
    //  2. edge_root is subdivided into one or more index cells.  In this case
    //     we recursively subdivide to find the cells intersected by a0a1.
    //  3. edge_root does not intersect any index cells.  In this case there
    //     is nothing to do.
    S2ShapeIndex::CellRelation relation = iter_.Locate(edge_root);
    if (relation == S2ShapeIndex::INDEXED) {
      // edge_root is an index cell or is contained by an index cell (case 1).
      S2_DCHECK(iter_.id().contains(edge_root));
      if (!visitor(iter_.cell())) return false;
    } else if (relation == S2ShapeIndex::SUBDIVIDED) {
      // edge_root is subdivided into one or more index cells (case 2).  We
      // find the cells intersected by a0a1 using recursive subdivision.
      if (!edge_root.is_face()) pcell = S2PaddedCell(edge_root, 0);
      if (!VisitCells(pcell, edge_bound)) return false;
    }
  }
  return true;
}

bool S2CrossingEdgeQuery::VisitCells(
    const S2Point& a0, const S2Point& a1, const S2PaddedCell& root,
    const CellVisitor& visitor) {
  visitor_ = &visitor;
  if (S2::ClipToFace(a0, a1, root.id().face(), &a0_, &a1_)) {
    R2Rect edge_bound = R2Rect::FromPointPair(a0_, a1_);
    if (root.bound().Intersects(edge_bound)) {
      return VisitCells(root, edge_bound);
    }
  }
  return true;
}

// Computes the index cells intersected by the current edge that are
// descendants of "pcell" and calls visitor_ for each one.
//
// WARNING: This function is recursive with a maximum depth of 30.  The frame
// size is about 2K in versions of GCC prior to 4.7 due to poor overlapping
// of storage for temporaries.  This is fixed in GCC 4.7, reducing the frame
// size to about 350 bytes (i.e., worst-case total stack usage of about 10K).
bool S2CrossingEdgeQuery::VisitCells(const S2PaddedCell& pcell,
                                     const R2Rect& edge_bound) {
  iter_.Seek(pcell.id().range_min());
  if (iter_.done() || iter_.id() > pcell.id().range_max()) {
    // The index does not contain "pcell" or any of its descendants.
    return true;
  }
  if (iter_.id() == pcell.id()) {
    return (*visitor_)(iter_.cell());
  }

  // Otherwise, split the edge among the four children of "pcell".
  R2Point center = pcell.middle().lo();
  if (edge_bound[0].hi() < center[0]) {
    // Edge is entirely contained in the two left children.
    return ClipVAxis(edge_bound, center[1], 0, pcell);
  } else if (edge_bound[0].lo() >= center[0]) {
    // Edge is entirely contained in the two right children.
    return ClipVAxis(edge_bound, center[1], 1, pcell);
  } else {
    R2Rect child_bounds[2];
    SplitUBound(edge_bound, center[0], child_bounds);
    if (edge_bound[1].hi() < center[1]) {
      // Edge is entirely contained in the two lower children.
      return (VisitCells(S2PaddedCell(pcell, 0, 0), child_bounds[0]) &&
              VisitCells(S2PaddedCell(pcell, 1, 0), child_bounds[1]));
    } else if (edge_bound[1].lo() >= center[1]) {
      // Edge is entirely contained in the two upper children.
      return (VisitCells(S2PaddedCell(pcell, 0, 1), child_bounds[0]) &&
              VisitCells(S2PaddedCell(pcell, 1, 1), child_bounds[1]));
    } else {
      // The edge bound spans all four children.  The edge itself intersects
      // at most three children (since no padding is being used).
      return (ClipVAxis(child_bounds[0], center[1], 0, pcell) &&
              ClipVAxis(child_bounds[1], center[1], 1, pcell));
    }
  }
}

// Given either the left (i=0) or right (i=1) side of a padded cell "pcell",
// determine whether the current edge intersects the lower child, upper child,
// or both children, and call VisitCells() recursively on those children.
// "center" is the v-coordinate at the center of "pcell".
inline bool S2CrossingEdgeQuery::ClipVAxis(const R2Rect& edge_bound,
                                           double center, int i,
                                           const S2PaddedCell& pcell) {
  if (edge_bound[1].hi() < center) {
    // Edge is entirely contained in the lower child.
    return VisitCells(S2PaddedCell(pcell, i, 0), edge_bound);
  } else if (edge_bound[1].lo() >= center) {
    // Edge is entirely contained in the upper child.
    return VisitCells(S2PaddedCell(pcell, i, 1), edge_bound);
  } else {
    // The edge intersects both children.
    R2Rect child_bounds[2];
    SplitVBound(edge_bound, center, child_bounds);
    return (VisitCells(S2PaddedCell(pcell, i, 0), child_bounds[0]) &&
            VisitCells(S2PaddedCell(pcell, i, 1), child_bounds[1]));
  }
}

// Split the current edge into two child edges at the given u-value "u" and
// return the bound for each child.
void S2CrossingEdgeQuery::SplitUBound(const R2Rect& edge_bound, double u,
                                      R2Rect child_bounds[2]) const {
  // See comments in MutableS2ShapeIndex::ClipUBound.
  double v = edge_bound[1].Project(
      S2::InterpolateDouble(u, a0_[0], a1_[0], a0_[1], a1_[1]));

  // "diag_" indicates which diagonal of the bounding box is spanned by a0a1:
  // it is 0 if a0a1 has positive slope, and 1 if a0a1 has negative slope.
  int diag = (a0_[0] > a1_[0]) != (a0_[1] > a1_[1]);
  SplitBound(edge_bound, 0, u, diag, v, child_bounds);
}

// Split the current edge into two child edges at the given v-value "v" and
// return the bound for each child.
void S2CrossingEdgeQuery::SplitVBound(const R2Rect& edge_bound, double v,
                                      R2Rect child_bounds[2]) const {
  double u = edge_bound[0].Project(
      S2::InterpolateDouble(v, a0_[1], a1_[1], a0_[0], a1_[0]));
  int diag = (a0_[0] > a1_[0]) != (a0_[1] > a1_[1]);
  SplitBound(edge_bound, diag, u, 0, v, child_bounds);
}

// Split the current edge into two child edges at the given point (u,v) and
// return the bound for each child.  "u_end" and "v_end" indicate which bound
// endpoints of child 1 will be updated.
inline void S2CrossingEdgeQuery::SplitBound(const R2Rect& edge_bound, int u_end,
                                            double u, int v_end, double v,
                                            R2Rect child_bounds[2]) {
  child_bounds[0] = edge_bound;
  child_bounds[0][0][1 - u_end] = u;
  child_bounds[0][1][1 - v_end] = v;
  S2_DCHECK(!child_bounds[0].is_empty());
  S2_DCHECK(edge_bound.Contains(child_bounds[0]));

  child_bounds[1] = edge_bound;
  child_bounds[1][0][u_end] = u;
  child_bounds[1][1][v_end] = v;
  S2_DCHECK(!child_bounds[1].is_empty());
  S2_DCHECK(edge_bound.Contains(child_bounds[1]));
}

void S2CrossingEdgeQuery::GetCells(const S2Point& a0, const S2Point& a1,
                                   const S2PaddedCell& root,
                                   vector<const S2ShapeIndexCell*>* cells) {
  cells->clear();
  VisitCells(a0, a1, root, [cells](const S2ShapeIndexCell& cell) {
      cells->push_back(&cell);
      return true;
    });
}
