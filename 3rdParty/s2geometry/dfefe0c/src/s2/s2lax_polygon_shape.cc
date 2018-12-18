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

#include "s2/s2lax_polygon_shape.h"

#include "s2/s2shapeutil_get_reference_point.h"

using absl::make_unique;
using absl::MakeSpan;
using absl::Span;
using std::vector;
using ChainPosition = S2Shape::ChainPosition;

// When adding a new encoding, be aware that old binaries will not be able
// to decode it.
static const unsigned char kCurrentEncodingVersionNumber = 1;

S2LaxPolygonShape::S2LaxPolygonShape(
    const vector<S2LaxPolygonShape::Loop>& loops) {
  Init(loops);
}

S2LaxPolygonShape::S2LaxPolygonShape(const S2Polygon& polygon) {
  Init(polygon);
}

void S2LaxPolygonShape::Init(const vector<S2LaxPolygonShape::Loop>& loops) {
  vector<Span<const S2Point>> spans;
  for (const S2LaxPolygonShape::Loop& loop : loops) {
    spans.emplace_back(loop);
  }
  Init(spans);
}

void S2LaxPolygonShape::Init(const S2Polygon& polygon) {
  vector<Span<const S2Point>> spans;
  for (int i = 0; i < polygon.num_loops(); ++i) {
    const S2Loop* loop = polygon.loop(i);
    if (loop->is_full()) {
      spans.emplace_back();  // Empty span.
    } else {
      spans.emplace_back(&loop->vertex(0), loop->num_vertices());
    }
  }
  Init(spans);

  // S2Polygon and S2LaxPolygonShape holes are oriented oppositely, so we need
  // to reverse the orientation of any loops representing holes.
  for (int i = 0; i < polygon.num_loops(); ++i) {
    if (polygon.loop(i)->is_hole()) {
      S2Point* v0 = &vertices_[cumulative_vertices_[i]];
      std::reverse(v0, v0 + num_loop_vertices(i));
    }
  }
}

void S2LaxPolygonShape::Init(const vector<Span<const S2Point>>& loops) {
  num_loops_ = loops.size();
  if (num_loops_ == 0) {
    num_vertices_ = 0;
    vertices_ = nullptr;
  } else if (num_loops_ == 1) {
    num_vertices_ = loops[0].size();
    vertices_.reset(new S2Point[num_vertices_]);
    std::copy(loops[0].begin(), loops[0].end(), vertices_.get());
  } else {
    cumulative_vertices_ = new uint32[num_loops_ + 1];
    int32 num_vertices = 0;
    for (int i = 0; i < num_loops_; ++i) {
      cumulative_vertices_[i] = num_vertices;
      num_vertices += loops[i].size();
    }
    cumulative_vertices_[num_loops_] = num_vertices;
    vertices_.reset(new S2Point[num_vertices]);
    for (int i = 0; i < num_loops_; ++i) {
      std::copy(loops[i].begin(), loops[i].end(),
                vertices_.get() + cumulative_vertices_[i]);
    }
  }
}

S2LaxPolygonShape::~S2LaxPolygonShape() {
  if (num_loops() > 1) {
    delete[] cumulative_vertices_;
  }
}

int S2LaxPolygonShape::num_vertices() const {
  if (num_loops() <= 1) {
    return num_vertices_;
  } else {
    return cumulative_vertices_[num_loops()];
  }
}

int S2LaxPolygonShape::num_loop_vertices(int i) const {
  S2_DCHECK_LT(i, num_loops());
  if (num_loops() == 1) {
    return num_vertices_;
  } else {
    return cumulative_vertices_[i + 1] - cumulative_vertices_[i];
  }
}

const S2Point& S2LaxPolygonShape::loop_vertex(int i, int j) const {
  S2_DCHECK_LT(i, num_loops());
  S2_DCHECK_LT(j, num_loop_vertices(i));
  if (num_loops() == 1) {
    return vertices_[j];
  } else {
    return vertices_[cumulative_vertices_[i] + j];
  }
}

void S2LaxPolygonShape::Encode(Encoder* encoder,
                               s2coding::CodingHint hint) const {
  encoder->Ensure(1 + Varint::kMax32);
  encoder->put8(kCurrentEncodingVersionNumber);
  encoder->put_varint32(num_loops_);
  s2coding::EncodeS2PointVector(MakeSpan(vertices_.get(), num_vertices()),
                                hint, encoder);
  if (num_loops() > 1) {
    s2coding::EncodeUintVector<uint32>(MakeSpan(cumulative_vertices_,
                                                num_loops() + 1), encoder);
  }
}

bool S2LaxPolygonShape::Init(Decoder* decoder) {
  if (decoder->avail() < 1) return false;
  uint8 version = decoder->get8();
  if (version != kCurrentEncodingVersionNumber) return false;

  uint32 num_loops;
  if (!decoder->get_varint32(&num_loops)) return false;
  num_loops_ = num_loops;
  s2coding::EncodedS2PointVector vertices;
  if (!vertices.Init(decoder)) return false;

  if (num_loops_ == 0) {
    num_vertices_ = 0;
    vertices_ = nullptr;
  } else {
    vertices_ = make_unique<S2Point[]>(vertices.size());
    for (int i = 0; i < vertices.size(); ++i) {
      vertices_[i] = vertices[i];
    }
    if (num_loops_ == 1) {
      num_vertices_ = vertices.size();
    } else {
      s2coding::EncodedUintVector<uint32> cumulative_vertices;
      if (!cumulative_vertices.Init(decoder)) return false;
      cumulative_vertices_ = new uint32[cumulative_vertices.size()];
      for (int i = 0; i < cumulative_vertices.size(); ++i) {
        cumulative_vertices_[i] = cumulative_vertices[i];
      }
    }
  }
  return true;
}

S2Shape::Edge S2LaxPolygonShape::edge(int e0) const {
  S2_DCHECK_LT(e0, num_edges());
  int e1 = e0 + 1;
  if (num_loops() == 1) {
    if (e1 == num_vertices_) { e1 = 0; }
  } else {
    // Find the index of the first vertex of the loop following this one.
    const int kMaxLinearSearchLoops = 12;  // From benchmarks.
    uint32* next = cumulative_vertices_ + 1;
    if (num_loops() <= kMaxLinearSearchLoops) {
      while (*next <= e0) ++next;
    } else {
      next = std::lower_bound(next, next + num_loops(), e1);
    }
    // Wrap around to the first vertex of the loop if necessary.
    if (e1 == *next) { e1 = next[-1]; }
  }
  return Edge(vertices_[e0], vertices_[e1]);
}

S2Shape::ReferencePoint S2LaxPolygonShape::GetReferencePoint() const {
  return s2shapeutil::GetReferencePoint(*this);
}

S2Shape::Chain S2LaxPolygonShape::chain(int i) const {
  S2_DCHECK_LT(i, num_loops());
  if (num_loops() == 1) {
    return Chain(0, num_vertices_);
  } else {
    int start = cumulative_vertices_[i];
    return Chain(start, cumulative_vertices_[i + 1] - start);
  }
}

S2Shape::Edge S2LaxPolygonShape::chain_edge(int i, int j) const {
  S2_DCHECK_LT(i, num_loops());
  S2_DCHECK_LT(j, num_loop_vertices(i));
  int n = num_loop_vertices(i);
  int k = (j + 1 == n) ? 0 : j + 1;
  if (num_loops() == 1) {
    return Edge(vertices_[j], vertices_[k]);
  } else {
    int base = cumulative_vertices_[i];
    return Edge(vertices_[base + j], vertices_[base + k]);
  }
}

S2Shape::ChainPosition S2LaxPolygonShape::chain_position(int e) const {
  S2_DCHECK_LT(e, num_edges());
  const int kMaxLinearSearchLoops = 12;  // From benchmarks.
  if (num_loops() == 1) {
    return ChainPosition(0, e);
  } else {
    // Find the index of the first vertex of the loop following this one.
    uint32* next = cumulative_vertices_ + 1;
    if (num_loops() <= kMaxLinearSearchLoops) {
      while (*next <= e) ++next;
    } else {
      next = std::lower_bound(next, next + num_loops(), e + 1);
    }
    return ChainPosition(next - (cumulative_vertices_ + 1), e - next[-1]);
  }
}

bool EncodedS2LaxPolygonShape::Init(Decoder* decoder) {
  if (decoder->avail() < 1) return false;
  uint8 version = decoder->get8();
  if (version != kCurrentEncodingVersionNumber) return false;

  uint32 num_loops;
  if (!decoder->get_varint32(&num_loops)) return false;
  num_loops_ = num_loops;

  if (!vertices_.Init(decoder)) return false;

  if (num_loops_ > 1) {
    if (!cumulative_vertices_.Init(decoder)) return false;
  }
  return true;
}

int EncodedS2LaxPolygonShape::num_vertices() const {
  if (num_loops() <= 1) {
    return vertices_.size();
  } else {
    return cumulative_vertices_[num_loops()];
  }
}

int EncodedS2LaxPolygonShape::num_loop_vertices(int i) const {
  S2_DCHECK_LT(i, num_loops());
  if (num_loops() == 1) {
    return vertices_.size();
  } else {
    return cumulative_vertices_[i + 1] - cumulative_vertices_[i];
  }
}

S2Point EncodedS2LaxPolygonShape::loop_vertex(int i, int j) const {
  S2_DCHECK_LT(i, num_loops());
  S2_DCHECK_LT(j, num_loop_vertices(i));
  if (num_loops() == 1) {
    return vertices_[j];
  } else {
    return vertices_[cumulative_vertices_[i] + j];
  }
}

S2Shape::Edge EncodedS2LaxPolygonShape::edge(int e) const {
  S2_DCHECK_LT(e, num_edges());
  int e1 = e + 1;
  if (num_loops() == 1) {
    if (e1 == vertices_.size()) { e1 = 0; }
  } else {
    // Find the index of the first vertex of the loop following this one.
    const int kMaxLinearSearchLoops = 12;  // From benchmarks.
    int next = 1;
    if (num_loops() <= kMaxLinearSearchLoops) {
      while (cumulative_vertices_[next] <= e) ++next;
    } else {
      next = cumulative_vertices_.lower_bound(e1);
    }
    // Wrap around to the first vertex of the loop if necessary.
    if (e1 == cumulative_vertices_[next]) {
      e1 = cumulative_vertices_[next - 1];
    }
  }
  return Edge(vertices_[e], vertices_[e1]);
}

S2Shape::ReferencePoint EncodedS2LaxPolygonShape::GetReferencePoint() const {
  return s2shapeutil::GetReferencePoint(*this);
}

S2Shape::Chain EncodedS2LaxPolygonShape::chain(int i) const {
  S2_DCHECK_LT(i, num_loops());
  if (num_loops() == 1) {
    return Chain(0, vertices_.size());
  } else {
    int start = cumulative_vertices_[i];
    return Chain(start, cumulative_vertices_[i + 1] - start);
  }
}

S2Shape::Edge EncodedS2LaxPolygonShape::chain_edge(int i, int j) const {
  S2_DCHECK_LT(i, num_loops());
  S2_DCHECK_LT(j, num_loop_vertices(i));
  int n = num_loop_vertices(i);
  int k = (j + 1 == n) ? 0 : j + 1;
  if (num_loops() == 1) {
    return Edge(vertices_[j], vertices_[k]);
  } else {
    int base = cumulative_vertices_[i];
    return Edge(vertices_[base + j], vertices_[base + k]);
  }
}

S2Shape::ChainPosition EncodedS2LaxPolygonShape::chain_position(int e) const {
  S2_DCHECK_LT(e, num_edges());
  const int kMaxLinearSearchLoops = 12;  // From benchmarks.
  if (num_loops() == 1) {
    return ChainPosition(0, e);
  } else {
    // Find the index of the first vertex of the loop following this one.
    int next = 1;
    if (num_loops() <= kMaxLinearSearchLoops) {
      while (cumulative_vertices_[next] <= e) ++next;
    } else {
      next = cumulative_vertices_.lower_bound(e + 1);
    }
    return ChainPosition(next - 1, e - cumulative_vertices_[next - 1]);
  }
}
