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

#ifndef S2_S2LAX_POLYGON_SHAPE_H_
#define S2_S2LAX_POLYGON_SHAPE_H_

#include <memory>
#include <vector>

#include "s2/third_party/absl/types/span.h"
#include "s2/encoded_uint_vector.h"
#include "s2/encoded_s2point_vector.h"
#include "s2/s2polygon.h"
#include "s2/s2shape.h"

// S2LaxPolygonShape represents a region defined by a collection of zero or
// more closed loops.  The interior is the region to the left of all loops.
// This is similar to S2Polygon::Shape except that this class supports
// polygons with degeneracies.  Degeneracies are of two types: degenerate
// edges (from a vertex to itself) and sibling edge pairs (consisting of two
// oppositely oriented edges).  Degeneracies can represent either "shells" or
// "holes" depending on the loop they are contained by.  For example, a
// degenerate edge or sibling pair contained by a "shell" would be interpreted
// as a degenerate hole.  Such edges form part of the boundary of the polygon.
//
// Loops with fewer than three vertices are interpreted as follows:
//  - A loop with two vertices defines two edges (in opposite directions).
//  - A loop with one vertex defines a single degenerate edge.
//  - A loop with no vertices is interpreted as the "full loop" containing
//    all points on the sphere.  If this loop is present, then all other loops
//    must form degeneracies (i.e., degenerate edges or sibling pairs).  For
//    example, two loops {} and {X} would be interpreted as the full polygon
//    with a degenerate single-point hole at X.
//
// S2LaxPolygonShape does not have any error checking, and it is perfectly
// fine to create S2LaxPolygonShape objects that do not meet the requirements
// below (e.g., in order to analyze or fix those problems).  However,
// S2LaxPolygonShapes must satisfy some additional conditions in order to
// perform certain operations:
//
//  - In order to be valid for point containment tests, the polygon must
//    satisfy the "interior is on the left" rule.  This means that there must
//    not be any crossing edges, and if there are duplicate edges then all but
//    at most one of thm must belong to a sibling pair (i.e., the number of
//    edges in opposite directions must differ by at most one).
//
//  - To be valid for boolean operations (S2BooleanOperation), degenerate
//    edges and sibling pairs cannot coincide with any other edges.  For
//    example, the following situations are not allowed:
//
//      {AA, AA}      // degenerate edge coincides with another edge
//      {AA, AB}      // degenerate edge coincides with another edge
//      {AB, BA, AB}  // sibling pair coincides with another edge
//
// Note that S2LaxPolygonShape is must faster to initialize and is more
// compact than S2Polygon, but unlike S2Polygon it does not have any built-in
// operations.  Instead you should use S2ShapeIndex operations
// (S2BooleanOperation, S2ClosestEdgeQuery, etc).
class S2LaxPolygonShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 5;

  // Constructs an empty polygon.
  S2LaxPolygonShape() : num_loops_(0), num_vertices_(0) {}

  // Constructs an S2LaxPolygonShape from the given vertex loops.
  using Loop = std::vector<S2Point>;
  explicit S2LaxPolygonShape(const std::vector<Loop>& loops);

  // Constructs an S2LaxPolygonShape from an S2Polygon, by copying its data.
  // Full and empty S2Polygons are supported.
  explicit S2LaxPolygonShape(const S2Polygon& polygon);

  ~S2LaxPolygonShape() override;

  // Initializes an S2LaxPolygonShape from the given vertex loops.
  void Init(const std::vector<Loop>& loops);

  // Initializes an S2LaxPolygonShape from an S2Polygon, by copying its data.
  // Full and empty S2Polygons are supported.
  void Init(const S2Polygon& polygon);

  // Returns the number of loops.
  int num_loops() const { return num_loops_; }

  // Returns the total number of vertices in all loops.
  int num_vertices() const;

  // Returns the number of vertices in the given loop.
  int num_loop_vertices(int i) const;

  // Returns the vertex from loop "i" at index "j".
  // REQUIRES: 0 <= i < num_loops()
  // REQUIRES: 0 <= j < num_loop_vertices(i)
  const S2Point& loop_vertex(int i, int j) const;

  // Appends an encoded representation of the S2LaxPolygonShape to "encoder".
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void Encode(Encoder* encoder,
              s2coding::CodingHint hint = s2coding::CodingHint::COMPACT) const;

  // Decodes an S2LaxPolygonShape, returning true on success.  (The method
  // name is chosen for compatibility with EncodedS2LaxPolygonShape below.)
  bool Init(Decoder* decoder);

  // S2Shape interface:
  int num_edges() const final { return num_vertices(); }
  Edge edge(int e) const final;
  int dimension() const final { return 2; }
  ReferencePoint GetReferencePoint() const final;
  int num_chains() const final { return num_loops(); }
  Chain chain(int i) const final;
  Edge chain_edge(int i, int j) const final;
  ChainPosition chain_position(int e) const final;
  TypeTag type_tag() const override { return kTypeTag; }

 private:
  void Init(const std::vector<absl::Span<const S2Point>>& loops);

  int32 num_loops_;
  std::unique_ptr<S2Point[]> vertices_;
  // If num_loops_ <= 1, this union stores the number of vertices.
  // Otherwise it points to an array of size (num_loops + 1) where element "i"
  // is the total number of vertices in loops 0..i-1.
  union {
    int32 num_vertices_;
    uint32* cumulative_vertices_;  // Don't use unique_ptr in unions.
  };
};

// Exactly like S2LaxPolygonShape, except that the vertices are kept in an
// encoded form and are decoded only as they are accessed.  This allows for
// very fast initialization and no additional memory use beyond the encoded
// data.  The encoded data is not owned by this class; typically it points
// into a large contiguous buffer that contains other encoded data as well.
class EncodedS2LaxPolygonShape : public S2Shape {
 public:
  // Constructs an uninitialized object; requires Init() to be called.
  EncodedS2LaxPolygonShape() {}

  // Initializes an EncodedS2LaxPolygonShape.
  //
  // REQUIRES: The Decoder data buffer must outlive this object.
  bool Init(Decoder* decoder);

  int num_loops() const { return num_loops_; }
  int num_vertices() const;
  int num_loop_vertices(int i) const;
  S2Point loop_vertex(int i, int j) const;

  // S2Shape interface:
  int num_edges() const final { return num_vertices(); }
  Edge edge(int e) const final;
  int dimension() const final { return 2; }
  ReferencePoint GetReferencePoint() const final;
  int num_chains() const final { return num_loops(); }
  Chain chain(int i) const final;
  Edge chain_edge(int i, int j) const final;
  ChainPosition chain_position(int e) const final;

 private:
  int32 num_loops_;
  s2coding::EncodedS2PointVector vertices_;
  s2coding::EncodedUintVector<uint32> cumulative_vertices_;
};

#endif  // S2_S2LAX_POLYGON_SHAPE_H_
