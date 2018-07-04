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

#ifndef S2_S2LAX_POLYLINE_SHAPE_H_
#define S2_S2LAX_POLYLINE_SHAPE_H_

#include <memory>
#include <vector>

#include "s2/s2polyline.h"
#include "s2/s2shape.h"

// S2LaxPolylineShape represents a polyline.  It is similar to
// S2Polyline::Shape except that duplicate vertices are allowed, and the
// representation is slightly more compact.
//
// Polylines may have any number of vertices, but note that polylines with
// fewer than 2 vertices do not define any edges.  (To create a polyline
// consisting of a single degenerate edge, either repeat the same vertex twice
// or use S2LaxClosedPolylineShape defined in s2_lax_loop_shape.h.)
class S2LaxPolylineShape : public S2Shape {
 public:
  // Constructs an empty polyline.
  S2LaxPolylineShape() : num_vertices_(0) {}

  // Constructs an S2LaxPolylineShape with the given vertices.
  explicit S2LaxPolylineShape(const std::vector<S2Point>& vertices);

  // Constructs an S2LaxPolylineShape from the given S2Polyline, by copying
  // its data.
  explicit S2LaxPolylineShape(const S2Polyline& polyline);

  // Initializes an S2LaxPolylineShape with the given vertices.
  void Init(const std::vector<S2Point>& vertices);

  // Initializes an S2LaxPolylineShape from the given S2Polyline, by copying
  // its data.
  void Init(const S2Polyline& polyline);

  int num_vertices() const { return num_vertices_; }
  const S2Point& vertex(int i) const { return vertices_[i]; }

  // S2Shape interface:
  int num_edges() const final { return std::max(0, num_vertices() - 1); }
  Edge edge(int e) const final;
  int dimension() const final { return 1; }
  ReferencePoint GetReferencePoint() const final {
    return ReferencePoint::Contained(false);
  }
  int num_chains() const final;
  Chain chain(int i) const final;
  Edge chain_edge(int i, int j) const final;
  ChainPosition chain_position(int e) const final;

 private:
  // For clients that have many small polylines, we save some memory by
  // representing the vertices as an array rather than using std::vector.
  int32 num_vertices_;
  std::unique_ptr<S2Point[]> vertices_;
};

#endif  // S2_S2LAX_POLYLINE_SHAPE_H_
