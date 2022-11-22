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

#include "s2/s2lax_loop_shape.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "s2/s2loop.h"
#include "s2/s2shapeutil_get_reference_point.h"

using absl::make_unique;
using absl::Span;
using ReferencePoint = S2Shape::ReferencePoint;

S2LaxLoopShape::S2LaxLoopShape(S2LaxLoopShape&& other)
    : S2Shape(std::move(other)),
      num_vertices_(absl::exchange(other.num_vertices_, 0)),
      vertices_(std::move(other.vertices_)) {}

S2LaxLoopShape& S2LaxLoopShape::operator=(S2LaxLoopShape&& other) {
  S2Shape::operator=(static_cast<S2Shape&&>(other));
  num_vertices_ = absl::exchange(other.num_vertices_, 0);
  vertices_ = std::move(other.vertices_);
  return *this;
}

S2LaxLoopShape::S2LaxLoopShape(Span<const S2Point> vertices) {
  Init(vertices);
}

S2LaxLoopShape::S2LaxLoopShape(const S2Loop& loop) {
  Init(loop);
}

void S2LaxLoopShape::Init(Span<const S2Point> vertices) {
  num_vertices_ = vertices.size();
  vertices_ = make_unique<S2Point[]>(num_vertices_);
  std::copy(vertices.begin(), vertices.end(), vertices_.get());
}

void S2LaxLoopShape::Init(const S2Loop& loop) {
  S2_DCHECK(!loop.is_full()) << "Full loops not supported; use S2LaxPolygonShape";
  if (loop.is_empty()) {
    num_vertices_ = 0;
    vertices_ = nullptr;
  } else {
    num_vertices_ = loop.num_vertices();
    vertices_ = make_unique<S2Point[]>(num_vertices_);
    std::copy(&loop.vertex(0), &loop.vertex(0) + num_vertices_,
              vertices_.get());
  }
}

S2Shape::Edge S2LaxLoopShape::edge(int e0) const {
  S2_DCHECK_LT(e0, num_edges());
  int e1 = e0 + 1;
  if (e1 == num_vertices()) e1 = 0;
  return Edge(vertices_[e0], vertices_[e1]);
}

S2Shape::Edge S2LaxLoopShape::chain_edge(int i, int j) const {
  S2_DCHECK_EQ(i, 0);
  S2_DCHECK_LT(j, num_edges());
  int k = (j + 1 == num_vertices()) ? 0 : j + 1;
  return Edge(vertices_[j], vertices_[k]);
}

S2Shape::ReferencePoint S2LaxLoopShape::GetReferencePoint() const {
  return s2shapeutil::GetReferencePoint(*this);
}

S2VertexIdLaxLoopShape::S2VertexIdLaxLoopShape(S2VertexIdLaxLoopShape&& other)
    : S2Shape(std::move(other)),
      num_vertices_(absl::exchange(other.num_vertices_, 0)),
      vertex_ids_(std::move(other.vertex_ids_)),
      vertex_array_(std::move(other.vertex_array_)) {}

S2VertexIdLaxLoopShape& S2VertexIdLaxLoopShape::operator=(
    S2VertexIdLaxLoopShape&& other) {
  S2Shape::operator=(static_cast<S2Shape&&>(other));
  num_vertices_ = absl::exchange(other.num_vertices_, 0);
  vertex_ids_ = std::move(other.vertex_ids_);
  vertex_array_ = std::move(other.vertex_array_);
  return *this;
}

S2VertexIdLaxLoopShape::S2VertexIdLaxLoopShape(Span<const int32> vertex_ids,
                                               const S2Point* vertex_array) {
  Init(vertex_ids, vertex_array);
}

void S2VertexIdLaxLoopShape::Init(Span<const int32> vertex_ids,
                                  const S2Point* vertex_array) {
  num_vertices_ = vertex_ids.size();
  vertex_ids_.reset(new int32[num_vertices_]);
  std::copy(vertex_ids.begin(), vertex_ids.end(), vertex_ids_.get());
  vertex_array_ = vertex_array;
}

S2Shape::Edge S2VertexIdLaxLoopShape::edge(int e0) const {
  S2_DCHECK_LT(e0, num_edges());
  int e1 = e0 + 1;
  if (e1 == num_vertices()) e1 = 0;
  return Edge(vertex(e0), vertex(e1));
}

S2Shape::Edge S2VertexIdLaxLoopShape::chain_edge(int i, int j) const {
  S2_DCHECK_EQ(i, 0);
  S2_DCHECK_LT(j, num_edges());
  int k = (j + 1 == num_vertices()) ? 0 : j + 1;
  return Edge(vertex(j), vertex(k));
}

S2Shape::ReferencePoint S2VertexIdLaxLoopShape::GetReferencePoint() const {
  // GetReferencePoint interprets a loop with no vertices as "full".
  if (num_vertices() == 0) return ReferencePoint::Contained(false);
  return s2shapeutil::GetReferencePoint(*this);
}
