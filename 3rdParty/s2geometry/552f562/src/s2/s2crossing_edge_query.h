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

#ifndef S2_S2CROSSING_EDGE_QUERY_H_
#define S2_S2CROSSING_EDGE_QUERY_H_

#include <type_traits>
#include <vector>

#include "s2/third_party/absl/base/macros.h"
#include "s2/third_party/absl/container/inlined_vector.h"
#include "s2/_fp_contract_off.h"
#include "s2/r2.h"
#include "s2/r2rect.h"
#include "s2/s2padded_cell.h"
#include "s2/s2shape_index.h"
#include "s2/s2shapeutil_shape_edge.h"
#include "s2/s2shapeutil_shape_edge_id.h"

// A parameter that controls the reporting of edge intersections.
//
//  - CrossingType::INTERIOR reports intersections that occur at a point
//    interior to both edges (i.e., not at a vertex).
//
//  - CrossingType::ALL reports all intersections, even those where two edges
//    intersect only because they share a common vertex.
namespace s2shapeutil {
enum class CrossingType { INTERIOR, ALL };
}  // namespace s2shapeutil

// S2CrossingEdgeQuery is used to find edges or shapes that are crossed by
// an edge.  Here is an example showing how to index a set of polylines,
// and then find the polylines that are crossed by a given edge AB:
//
// void Test(const vector<S2Polyline*>& polylines,`
//           const S2Point& a0, const S2Point &a1) {
//   MutableS2ShapeIndex index;
//   for (S2Polyline* polyline : polylines) {
//     index.Add(absl::make_unique<S2Polyline::Shape>(polyline));
//   }
//   S2CrossingEdgeQuery query(&index);
//   for (const auto& edge : query.GetCrossingEdges(a, b, CrossingType::ALL)) {
//     S2_CHECK_GE(S2::CrossingSign(a0, a1, edge.v0(), edge.v1()), 0);
//   }
// }
//
// Note that if you need to query many edges, it is more efficient to declare
// a single S2CrossingEdgeQuery object and reuse it so that temporary storage
// does not need to be reallocated each time.
//
// If you want to find *all* pairs of crossing edges, use
// s2shapeutil::VisitCrossingEdgePairs() instead.
class S2CrossingEdgeQuery {
 public:
  using CrossingType = s2shapeutil::CrossingType;  // Defined above.

  // Convenience constructor that calls Init().
  explicit S2CrossingEdgeQuery(const S2ShapeIndex* index);

  // Default constructor; requires Init() to be called.
  S2CrossingEdgeQuery();
  ~S2CrossingEdgeQuery();

  S2CrossingEdgeQuery(const S2CrossingEdgeQuery&) = delete;
  void operator=(const S2CrossingEdgeQuery&) = delete;

  const S2ShapeIndex& index() const { return *index_; }

  // REQUIRES: "index" is not modified after this method is called.
  void Init(const S2ShapeIndex* index);

  // Returns all edges that intersect the given query edge (a0,a1) and that
  // have the given CrossingType (ALL or INTERIOR).  Edges are sorted and
  // unique.
  std::vector<s2shapeutil::ShapeEdge> GetCrossingEdges(
      const S2Point& a0, const S2Point& a1, CrossingType type);

  // A specialized version of GetCrossingEdges() that only returns the edges
  // that belong to a particular S2Shape.
  std::vector<s2shapeutil::ShapeEdge> GetCrossingEdges(
      const S2Point& a0, const S2Point& a1,
      const S2Shape& shape, CrossingType type);

  // These versions can be more efficient when they are called many times,
  // since they do not require allocating a new vector on each call.
  void GetCrossingEdges(const S2Point& a0, const S2Point& a1, CrossingType type,
                        std::vector<s2shapeutil::ShapeEdge>* edges);

  void GetCrossingEdges(const S2Point& a0, const S2Point& a1,
                        const S2Shape& shape, CrossingType type,
                        std::vector<s2shapeutil::ShapeEdge>* edges);


  /////////////////////////// Low-Level Methods ////////////////////////////
  //
  // Most clients will not need the following methods.  They can be slightly
  // more efficient but are harder to use, since they require the client to do
  // all the actual crossing tests.

  // Returns a superset of the edges that intersect a query edge (a0, a1).
  // This method is useful for clients that want to test intersections in some
  // other way, e.g. using S2::EdgeOrVertexCrossing().
  std::vector<s2shapeutil::ShapeEdgeId> GetCandidates(const S2Point& a0,
                                                      const S2Point& a1);

  // A specialized version of GetCandidates() that only returns the edges that
  // belong to a particular S2Shape.
  std::vector<s2shapeutil::ShapeEdgeId> GetCandidates(const S2Point& a0,
                                                      const S2Point& a1,
                                                      const S2Shape& shape);

  // These versions can be more efficient when they are called many times,
  // since they do not require allocating a new vector on each call.
  void GetCandidates(const S2Point& a0, const S2Point& a1,
                     std::vector<s2shapeutil::ShapeEdgeId>* edges);

  void GetCandidates(const S2Point& a0, const S2Point& a1, const S2Shape& shape,
                     std::vector<s2shapeutil::ShapeEdgeId>* edges);

  // A function that is called with each candidate intersecting edge.  The
  // function may return false in order to request that the algorithm should
  // be terminated, i.e. no further crossings are needed.
  using ShapeEdgeIdVisitor =
      std::function<bool (const s2shapeutil::ShapeEdgeId& id)>;

  // Visits a superset of the edges that intersect the query edge (a0, a1),
  // terminating early if the given ShapeEdgeIdVisitor returns false (in which
  // case this function returns false as well).
  //
  // CAVEAT: Edges may be visited more than once.
  bool VisitRawCandidates(const S2Point& a0, const S2Point& a1,
                          const ShapeEdgeIdVisitor& visitor);

  bool VisitRawCandidates(const S2Point& a0, const S2Point& a1,
                          const S2Shape& shape,
                          const ShapeEdgeIdVisitor& visitor);

  // A function that is called with each S2ShapeIndexCell that might contain
  // edges intersecting the given query edge.  The function may return false
  // in order to request that the algorithm should be terminated, i.e. no
  // further crossings are needed.
  using CellVisitor = std::function<bool (const S2ShapeIndexCell& cell)>;

  // Visits all S2ShapeIndexCells that might contain edges intersecting the
  // given query edge (a0, a1), terminating early if the given CellVisitor
  // returns false (in which case this function returns false as well).
  //
  // NOTE: Each candidate cell is visited exactly once.
  bool VisitCells(const S2Point& a0, const S2Point& a1,
                  const CellVisitor& visitor);

  // Visits all S2ShapeIndexCells within "root" that might contain edges
  // intersecting the given query edge (a0, a1), terminating early if the
  // given CellVisitor returns false (in which case this function returns
  // false as well).
  //
  // NOTE: Each candidate cell is visited exactly once.
  bool VisitCells(const S2Point& a0, const S2Point& a1,
                  const S2PaddedCell& root, const CellVisitor& visitor);


  // Given a query edge AB and a cell "root", returns all S2ShapeIndex cells
  // within "root" that might contain edges intersecting AB.
  void GetCells(const S2Point& a0, const S2Point& a1, const S2PaddedCell& root,
                std::vector<const S2ShapeIndexCell*>* cells);

 private:
  // Internal methods are documented with their definitions.
  bool VisitCells(const S2PaddedCell& pcell, const R2Rect& edge_bound);
  bool ClipVAxis(const R2Rect& edge_bound, double center, int i,
                 const S2PaddedCell& pcell);
  void SplitUBound(const R2Rect& edge_bound, double u,
                   R2Rect child_bounds[2]) const;
  void SplitVBound(const R2Rect& edge_bound, double v,
                   R2Rect child_bounds[2]) const;
  static void SplitBound(const R2Rect& edge_bound, int u_end, double u,
                         int v_end, double v, R2Rect child_bounds[2]);

  const S2ShapeIndex* index_ = nullptr;

  //////////// Temporary storage used while processing a query ///////////

  R2Point a0_, a1_;
  S2ShapeIndex::Iterator iter_;
  const CellVisitor* visitor_;

  // Avoids repeated allocation when methods are called many times.
  std::vector<s2shapeutil::ShapeEdgeId> tmp_candidates_;
};


//////////////////   Implementation details follow   ////////////////////


inline S2CrossingEdgeQuery::S2CrossingEdgeQuery(const S2ShapeIndex* index) {
  Init(index);
}

#endif  // S2_S2CROSSING_EDGE_QUERY_H_
