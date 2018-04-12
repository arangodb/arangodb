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
// This file defines various S2Shape types representing loops:
//
// S2LaxLoopShape
//   - like S2Loop::Shape but allows duplicate vertices & edges, more compact
//     representation, and faster to initialize.
//
// S2LaxClosedPolylineShape
//   - like S2LaxLoopShape, but defines a loop that does not have an interior
//     (a closed polyline).
//
// S2VertexIdLaxLoopShape
//   - like S2LaxLoopShape, but vertices are specified as indices into an
//     existing vertex array.

#ifndef S2_S2LAX_LOOP_SHAPE_H_
#define S2_S2LAX_LOOP_SHAPE_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "s2/s2loop.h"
#include "s2/s2shape.h"

// S2LaxLoopShape represents a closed loop of edges surrounding an interior
// region.  It is similar to S2Loop::Shape except that this class allows
// duplicate vertices and edges.  Loops may have any number of vertices,
// including 0, 1, or 2.  (A one-vertex loop defines a degenerate edge
// consisting of a single point.)
//
// Note that S2LaxLoopShape is faster to initialize and more compact than
// S2Loop::Shape, but does not support the same operations as S2Loop.
class S2LaxLoopShape : public S2Shape {
 public:
  // Constructs an empty loop.
  S2LaxLoopShape() : num_vertices_(0) {}

  // Constructs an S2LaxLoopShape with the given vertices.
  explicit S2LaxLoopShape(const std::vector<S2Point>& vertices);

  // Constructs an S2LaxLoopShape from the given S2Loop, by copying its data.
  explicit S2LaxLoopShape(const S2Loop& loop);

  // Initializes an S2LaxLoopShape with the given vertices.
  void Init(const std::vector<S2Point>& vertices);

  // Initializes an S2LaxLoopShape from the given S2Loop, by copying its data.
  //
  // REQUIRES: !loop->is_full()
  //           [Use S2LaxPolygonShape if you need to represent a full loop.]
  void Init(const S2Loop& loop);

  int num_vertices() const { return num_vertices_; }
  const S2Point& vertex(int i) const { return vertices_[i]; }

  // S2Shape interface:
  int num_edges() const final { return num_vertices(); }
  Edge edge(int e) const final;
  // Not final; overridden by S2LaxClosedPolylineShape.
  int dimension() const override { return 2; }
  // Not final; overridden by S2LaxClosedPolylineShape.
  ReferencePoint GetReferencePoint() const override;
  int num_chains() const final { return std::min(1, num_vertices_); }
  Chain chain(int i) const final { return Chain(0, num_vertices_); }
  Edge chain_edge(int i, int j) const final;
  ChainPosition chain_position(int e) const final {
    return ChainPosition(0, e);
  }

 private:
  // For clients that have many small loops, we save some memory by
  // representing the vertices as an array rather than using std::vector.
  int32 num_vertices_;
  std::unique_ptr<S2Point[]> vertices_;
};

// S2LaxClosedPolylineShape is like S2LaxPolylineShape except that the last
// vertex is implicitly joined to the first.  It is also like S2LaxLoopShape
// except that it does not have an interior (which makes it more efficient to
// index).
class S2LaxClosedPolylineShape : public S2LaxLoopShape {
 public:
  // See S2LaxLoopShape for constructors.
  using S2LaxLoopShape::S2LaxLoopShape;

  int dimension() const final { return 1; }
  ReferencePoint GetReferencePoint() const final {
    return ReferencePoint::Contained(false);
  }
};

// S2VertexIdLaxLoopShape is just like S2LaxLoopShape, except that vertices are
// specified as indices into a vertex array.  This representation can be more
// compact when many loops are arranged in a mesh structure.
class S2VertexIdLaxLoopShape : public S2Shape {
 public:
  // Constructs an empty loop.
  S2VertexIdLaxLoopShape() : num_vertices_(0) {}

  // Constructs the shape from the given vertex array and indices.
  // "vertex_ids" is a vector of indices into "vertex_array".
  //
  // ENSURES:  loop->vertex(i) == (*vertex_array)[vertex_ids[i]]
  // REQUIRES: "vertex_array" persists for the lifetime of this object.
  explicit S2VertexIdLaxLoopShape(const std::vector<int32>& vertex_ids,
                                  const S2Point* vertex_array);

  // Initializes the shape from the given vertex array and indices.
  // "vertex_ids" is a vector of indices into "vertex_array".
  void Init(const std::vector<int32>& vertex_ids,
            const S2Point* vertex_array);

  // Returns the number of vertices in the loop.
  int num_vertices() const { return num_vertices_; }
  int32 vertex_id(int i) const { return vertex_ids_[i]; }
  const S2Point& vertex(int i) const { return vertex_array_[vertex_id(i)]; }

  // S2Shape interface:
  int num_edges() const final { return num_vertices(); }
  Edge edge(int e) const final;
  int dimension() const final { return 2; }
  ReferencePoint GetReferencePoint() const final;
  int num_chains() const final { return std::min(1, num_vertices_); }
  Chain chain(int i) const final { return Chain(0, num_vertices_); }
  Edge chain_edge(int i, int j) const final;
  ChainPosition chain_position(int e) const final {
    return ChainPosition(0, e);
  }

 private:
  int32 num_vertices_;
  std::unique_ptr<int32[]> vertex_ids_;
  const S2Point* vertex_array_;
};

#endif  // S2_S2LAX_LOOP_SHAPE_H_
