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

#ifndef S2_S2EDGE_VECTOR_SHAPE_H_
#define S2_S2EDGE_VECTOR_SHAPE_H_

#include <utility>
#include <vector>
#include "s2/s2shape.h"

// S2EdgeVectorShape is an S2Shape representing an arbitrary set of edges.  It
// is mainly used for testing, but it can also be useful if you have, say, a
// collection of polylines and don't care about memory efficiency (since this
// class would store most of the vertices twice).
//
// Note that if you already have data stored in an S2Loop, S2Polyline, or
// S2Polygon, then you would be better off using the "Shape" class defined
// within those classes (e.g., S2Loop::Shape).  Similarly, if the vertex data
// is stored in your own data structures, you can easily write your own
// subclass of S2Shape that points to the existing vertex data rather than
// copying it.
class S2EdgeVectorShape : public S2Shape {
 public:
  // Constructs an empty edge vector.
  S2EdgeVectorShape() {}

  // Constructs an S2EdgeVectorShape from a vector of edges.
  explicit S2EdgeVectorShape(std::vector<std::pair<S2Point, S2Point>> edges) {
    edges_ = std::move(edges);
  }

  // Creates an S2EdgeVectorShape containing a single edge.
  S2EdgeVectorShape(const S2Point& a, const S2Point& b) {
    edges_.push_back(std::make_pair(a, b));
  }

  ~S2EdgeVectorShape() override = default;

  // Adds an edge to the vector.
  //
  // IMPORTANT: This method should only be called *before* adding the
  // S2EdgeVectorShape to an S2ShapeIndex.  S2Shapes can only be modified by
  // removing them from the index, making changes, and adding them back again.
  void Add(const S2Point& a, const S2Point& b) {
    edges_.push_back(std::make_pair(a, b));
  }

  // S2Shape interface:
  int num_edges() const final { return static_cast<int>(edges_.size()); }
  Edge edge(int e) const final {
    return Edge(edges_[e].first, edges_[e].second);
  }
  int dimension() const final { return 1; }
  ReferencePoint GetReferencePoint() const final {
    return ReferencePoint::Contained(false);
  }
  int num_chains() const final { return static_cast<int>(edges_.size()); }
  Chain chain(int i) const final { return Chain(i, 1); }
  Edge chain_edge(int i, int j) const final {
    S2_DCHECK_EQ(j, 0);
    return Edge(edges_[i].first, edges_[i].second);
  }
  ChainPosition chain_position(int e) const final {
    return ChainPosition(e, 0);
  }

 private:
  std::vector<std::pair<S2Point, S2Point>> edges_;
};

#endif  // S2_S2EDGE_VECTOR_SHAPE_H_
