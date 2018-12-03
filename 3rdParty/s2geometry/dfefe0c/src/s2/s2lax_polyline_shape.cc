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

#include "s2/s2lax_polyline_shape.h"

#include <algorithm>
#include "s2/base/logging.h"
#include "s2/s2polyline.h"

using absl::make_unique;
using absl::MakeSpan;
using std::vector;

S2LaxPolylineShape::S2LaxPolylineShape(const vector<S2Point>& vertices) {
  Init(vertices);
}

S2LaxPolylineShape::S2LaxPolylineShape(const S2Polyline& polyline) {
  Init(polyline);
}

void S2LaxPolylineShape::Init(const vector<S2Point>& vertices) {
  num_vertices_ = vertices.size();
  S2_LOG_IF(WARNING, num_vertices_ == 1)
      << "s2shapeutil::S2LaxPolylineShape with one vertex has no edges";
  vertices_.reset(new S2Point[num_vertices_]);
  std::copy(vertices.begin(), vertices.end(), vertices_.get());
}

void S2LaxPolylineShape::Init(const S2Polyline& polyline) {
  num_vertices_ = polyline.num_vertices();
  S2_LOG_IF(WARNING, num_vertices_ == 1)
      << "s2shapeutil::S2LaxPolylineShape with one vertex has no edges";
  vertices_.reset(new S2Point[num_vertices_]);
  std::copy(&polyline.vertex(0), &polyline.vertex(0) + num_vertices_,
            vertices_.get());
}

void S2LaxPolylineShape::Encode(Encoder* encoder,
                                s2coding::CodingHint hint) const {
  s2coding::EncodeS2PointVector(MakeSpan(vertices_.get(), num_vertices_),
                                hint, encoder);
}

bool S2LaxPolylineShape::Init(Decoder* decoder) {
  s2coding::EncodedS2PointVector vertices;
  if (!vertices.Init(decoder)) return false;
  num_vertices_ = vertices.size();
  vertices_ = make_unique<S2Point[]>(vertices.size());
  for (int i = 0; i < num_vertices_; ++i) {
    vertices_[i] = vertices[i];
  }
  return true;
}

S2Shape::Edge S2LaxPolylineShape::edge(int e) const {
  S2_DCHECK_LT(e, num_edges());
  return Edge(vertex(e), vertex(e + 1));
}

int S2LaxPolylineShape::num_chains() const {
  return std::min(1, S2LaxPolylineShape::num_edges());  // Avoid virtual call.
}

S2Shape::Chain S2LaxPolylineShape::chain(int i) const {
  return Chain(0, S2LaxPolylineShape::num_edges());  // Avoid virtual call.
}

S2Shape::Edge S2LaxPolylineShape::chain_edge(int i, int j) const {
  S2_DCHECK_EQ(i, 0);
  S2_DCHECK_LT(j, num_edges());
  return Edge(vertex(j), vertex(j + 1));
}

S2Shape::ChainPosition S2LaxPolylineShape::chain_position(int e) const {
  return S2Shape::ChainPosition(0, e);
}

bool EncodedS2LaxPolylineShape::Init(Decoder* decoder) {
  return vertices_.Init(decoder);
}

S2Shape::Edge EncodedS2LaxPolylineShape::edge(int e) const {
  S2_DCHECK_LT(e, num_edges());
  return Edge(vertex(e), vertex(e + 1));
}

int EncodedS2LaxPolylineShape::num_chains() const {
  return std::min(1, EncodedS2LaxPolylineShape::num_edges());
}

S2Shape::Chain EncodedS2LaxPolylineShape::chain(int i) const {
  return Chain(0, EncodedS2LaxPolylineShape::num_edges());
}

S2Shape::Edge EncodedS2LaxPolylineShape::chain_edge(int i, int j) const {
  S2_DCHECK_EQ(i, 0);
  S2_DCHECK_LT(j, num_edges());
  return Edge(vertex(j), vertex(j + 1));
}

S2Shape::ChainPosition EncodedS2LaxPolylineShape::chain_position(int e) const {
  return S2Shape::ChainPosition(0, e);
}
