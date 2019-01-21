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

#ifndef S2_S2CONTAINS_POINT_QUERY_H_
#define S2_S2CONTAINS_POINT_QUERY_H_

#include <vector>

#include "s2/s2edge_crosser.h"
#include "s2/s2shape_index.h"
#include "s2/s2shapeutil_shape_edge.h"

// Defines whether shapes are considered to contain their vertices.  Note that
// these definitions differ from the ones used by S2BooleanOperation.
//
//  - In the OPEN model, no shapes contain their vertices (not even points).
//    Therefore Contains(S2Point) returns true if and only if the point is
//    in the interior of some polygon.
//
//  - In the SEMI_OPEN model, polygon point containment is defined such that
//    if several polygons tile the region around a vertex, then exactly one of
//    those polygons contains that vertex.  Points and polylines still do not
//    contain any vertices.
//
//  - In the CLOSED model, all shapes contain their vertices (including points
//    and polylines).
//
// Note that points other than vertices are never contained by polylines.
// If you want need this behavior, use S2ClosestEdgeQuery::IsDistanceLess()
// with a suitable distance threshold instead.
enum class S2VertexModel { OPEN, SEMI_OPEN, CLOSED };

// This class defines the options supported by S2ContainsPointQuery.
class S2ContainsPointQueryOptions {
 public:
  S2ContainsPointQueryOptions() {}

  // Convenience constructor that sets the vertex_model() option.
  explicit S2ContainsPointQueryOptions(S2VertexModel vertex_model);

  // Controls whether shapes are considered to contain their vertices (see
  // definitions above).  By default the SEMI_OPEN model is used.
  //
  // DEFAULT: S2VertexModel::SEMI_OPEN
  S2VertexModel vertex_model() const;
  void set_vertex_model(S2VertexModel model);

 private:
  S2VertexModel vertex_model_ = S2VertexModel::SEMI_OPEN;
};

// S2ContainsPointQuery determines whether one or more shapes in an
// S2ShapeIndex contain a given S2Point.  The S2ShapeIndex may contain any
// number of points, polylines, and/or polygons (possibly overlapping).
// Shape boundaries may be modeled as OPEN, SEMI_OPEN, or CLOSED (this affects
// whether or not shapes are considered to contain their vertices).
//
// Example usage:
//   auto query = MakeS2ContainsPointQuery(&index, S2VertexModel::CLOSED);
//   return query.Contains(point);
//
// This class is not thread-safe.  To use it in parallel, each thread should
// construct its own instance (this is not expensive).
//
// However, note that if you need to do a large number of point containment
// tests, it is more efficient to re-use the S2ContainsPointQuery object
// rather than constructing a new one each time.
template <class IndexType>
class S2ContainsPointQuery {
 private:
  using Iterator = typename IndexType::Iterator;

 public:
  // Default constructor; requires Init() to be called.
  S2ContainsPointQuery();

  // Rather than calling this constructor, which requires specifying the
  // IndexType template argument explicitly, the preferred idiom is to call
  // MakeS2ContainsPointQuery() instead.  For example:
  //
  //   return MakeS2ContainsPointQuery(&index).Contains(p);
  using Options = S2ContainsPointQueryOptions;
  explicit S2ContainsPointQuery(const IndexType* index,
                                const Options& options = Options());

  // Convenience constructor that accepts the S2VertexModel directly.
  S2ContainsPointQuery(const IndexType* index, S2VertexModel vertex_model);

  const IndexType& index() const { return *index_; }
  const Options& options() const { return options_; }

  // Equivalent to the two-argument constructor above.
  void Init(const IndexType* index, const Options& options = Options());

  // Returns true if any shape in the given index() contains the point "p"
  // under the vertex model specified (OPEN, SEMI_OPEN, or CLOSED).
  bool Contains(const S2Point& p);

  // Returns true if the given shape contains the point "p" under the vertex
  // model specified (OPEN, SEMI_OPEN, or CLOSED).
  //
  // REQUIRES: "shape" belongs to index().
  bool ShapeContains(const S2Shape& shape, const S2Point& p);

  // Visits all shapes in the given index() that contain the given point "p",
  // terminating early if the given ShapeVisitor function returns false (in
  // which case VisitContainingShapes returns false as well).  Each shape is
  // visited at most once.
  //
  // Note that the API allows non-const access to the visited shapes.
  using ShapeVisitor = std::function<bool (S2Shape* shape)>;
  bool VisitContainingShapes(const S2Point& p, const ShapeVisitor& visitor);

  // Convenience function that returns all the shapes that contain the given
  // point "p".
  std::vector<S2Shape*> GetContainingShapes(const S2Point& p);

  // Visits all edges in the given index() that are incident to the point "p"
  // (i.e., "p" is one of the edge endpoints), terminating early if the given
  // EdgeVisitor function returns false (in which case VisitIncidentEdges
  // returns false as well).  Each edge is visited at most once.
  using EdgeVisitor = std::function<bool (const s2shapeutil::ShapeEdge&)>;
  bool VisitIncidentEdges(const S2Point& p, const EdgeVisitor& visitor);

  /////////////////////////// Low-Level Methods ////////////////////////////
  //
  // Most clients will not need the following methods.  They can be slightly
  // more efficient but are harder to use.

  // Returns a pointer to the iterator used internally by this class, in order
  // to avoid the need for clients to create their own iterator.  Clients are
  // allowed to reposition this iterator arbitrarily between method calls.
  Iterator* mutable_iter() { return &it_; }

  // Low-level helper method that returns true if the given S2ClippedShape
  // referred to by an S2ShapeIndex::Iterator contains the point "p".
  bool ShapeContains(const Iterator& it, const S2ClippedShape& clipped,
                     const S2Point& p) const;

 private:
  const IndexType* index_;
  Options options_;
  Iterator it_;
};

// Returns an S2ContainsPointQuery for the given S2ShapeIndex.  Note that
// it is efficient to return S2ContainsPointQuery objects by value.
template <class IndexType>
inline S2ContainsPointQuery<IndexType> MakeS2ContainsPointQuery(
    const IndexType* index,
    const S2ContainsPointQueryOptions& options =
    S2ContainsPointQueryOptions()) {
  return S2ContainsPointQuery<IndexType>(index, options);
}


//////////////////   Implementation details follow   ////////////////////


inline S2ContainsPointQueryOptions::S2ContainsPointQueryOptions(
    S2VertexModel vertex_model)
    : vertex_model_(vertex_model) {
}

inline S2VertexModel S2ContainsPointQueryOptions::vertex_model() const {
  return vertex_model_;
}

inline void S2ContainsPointQueryOptions::set_vertex_model(S2VertexModel model) {
  vertex_model_ = model;
}

template <class IndexType>
inline S2ContainsPointQuery<IndexType>::S2ContainsPointQuery()
    : index_(nullptr) {
}

template <class IndexType>
inline S2ContainsPointQuery<IndexType>::S2ContainsPointQuery(
    const IndexType* index, const Options& options)
    : index_(index), options_(options), it_(index_) {
}

template <class IndexType>
inline S2ContainsPointQuery<IndexType>::S2ContainsPointQuery(
    const IndexType* index, S2VertexModel vertex_model)
    : S2ContainsPointQuery(index, Options(vertex_model)) {
}

template <class IndexType>
void S2ContainsPointQuery<IndexType>::Init(const IndexType* index,
                                           const Options& options) {
  index_ = index;
  options_ = options;
  it_.Init(index);
}

template <class IndexType>
bool S2ContainsPointQuery<IndexType>::Contains(const S2Point& p) {
  if (!it_.Locate(p)) return false;

  const S2ShapeIndexCell& cell = it_.cell();
  int num_clipped = cell.num_clipped();
  for (int s = 0; s < num_clipped; ++s) {
    if (ShapeContains(it_, cell.clipped(s), p)) return true;
  }
  return false;
}

template <class IndexType>
bool S2ContainsPointQuery<IndexType>::ShapeContains(const S2Shape& shape,
                                                    const S2Point& p) {
  if (!it_.Locate(p)) return false;
  const S2ClippedShape* clipped = it_.cell().find_clipped(shape.id());
  if (clipped == nullptr) return false;
  return ShapeContains(it_, *clipped, p);
}

template <class IndexType>
bool S2ContainsPointQuery<IndexType>::VisitContainingShapes(
    const S2Point& p, const ShapeVisitor& visitor) {
  // This function returns "false" only if the algorithm terminates early
  // because the "visitor" function returned false.
  if (!it_.Locate(p)) return true;

  const S2ShapeIndexCell& cell = it_.cell();
  int num_clipped = cell.num_clipped();
  for (int s = 0; s < num_clipped; ++s) {
    const S2ClippedShape& clipped = cell.clipped(s);
    if (ShapeContains(it_, clipped, p) &&
        !visitor(index_->shape(clipped.shape_id()))) {
      return false;
    }
  }
  return true;
}

template <class IndexType>
bool S2ContainsPointQuery<IndexType>::VisitIncidentEdges(
    const S2Point& p, const EdgeVisitor& visitor) {
  // This function returns "false" only if the algorithm terminates early
  // because the "visitor" function returned false.
  if (!it_.Locate(p)) return true;

  const S2ShapeIndexCell& cell = it_.cell();
  int num_clipped = cell.num_clipped();
  for (int s = 0; s < num_clipped; ++s) {
    const S2ClippedShape& clipped = cell.clipped(s);
    int num_edges = clipped.num_edges();
    if (num_edges == 0) continue;
    const S2Shape& shape = *index_->shape(clipped.shape_id());
    for (int i = 0; i < num_edges; ++i) {
      int edge_id = clipped.edge(i);
      auto edge = shape.edge(edge_id);
      if ((edge.v0 == p || edge.v1 == p) &&
          !visitor(s2shapeutil::ShapeEdge(shape.id(), edge_id, edge))) {
        return false;
      }
    }
  }
  return true;
}

template <class IndexType>
std::vector<S2Shape*> S2ContainsPointQuery<IndexType>::GetContainingShapes(
    const S2Point& p) {
  std::vector<S2Shape*> results;
  VisitContainingShapes(p, [&results](S2Shape* shape) {
      results.push_back(shape);
      return true;
    });
  return results;
}

template <class IndexType>
bool S2ContainsPointQuery<IndexType>::ShapeContains(
    const Iterator& it, const S2ClippedShape& clipped, const S2Point& p) const {
  bool inside = clipped.contains_center();
  const int num_edges = clipped.num_edges();
  if (num_edges > 0) {
    const S2Shape& shape = *index_->shape(clipped.shape_id());
    if (shape.dimension() < 2) {
      // Points and polylines can be ignored unless the vertex model is CLOSED.
      if (options_.vertex_model() != S2VertexModel::CLOSED) return false;

      // Otherwise, the point is contained if and only if it matches a vertex.
      for (int i = 0; i < num_edges; ++i) {
        auto edge = shape.edge(clipped.edge(i));
        if (edge.v0 == p || edge.v1 == p) return true;
      }
      return false;
    }
    // Test containment by drawing a line segment from the cell center to the
    // given point and counting edge crossings.
    S2CopyingEdgeCrosser crosser(it.center(), p);
    for (int i = 0; i < num_edges; ++i) {
      auto edge = shape.edge(clipped.edge(i));
      int sign = crosser.CrossingSign(edge.v0, edge.v1);
      if (sign < 0) continue;
      if (sign == 0) {
        // For the OPEN and CLOSED models, check whether "p" is a vertex.
        if (options_.vertex_model() != S2VertexModel::SEMI_OPEN &&
            (edge.v0 == p || edge.v1 == p)) {
          return (options_.vertex_model() == S2VertexModel::CLOSED);
        }
        sign = S2::VertexCrossing(crosser.a(), crosser.b(), edge.v0, edge.v1);
      }
      inside ^= sign;
    }
  }
  return inside;
}

#endif  // S2_S2CONTAINS_POINT_QUERY_H_
