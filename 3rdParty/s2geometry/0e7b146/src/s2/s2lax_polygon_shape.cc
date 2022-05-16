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

#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"

#include "s2/util/coding/varint.h"
#include "s2/s2shapeutil_get_reference_point.h"

using absl::make_unique;
using absl::MakeSpan;
using absl::Span;
using std::vector;
using ChainPosition = S2Shape::ChainPosition;

namespace {
template <typename T>
std::unique_ptr<T> make_unique_for_overwrite(size_t n) {
  // We only need to support this one variant.
  static_assert(std::is_array<T>::value);
  return std::unique_ptr<T>(new typename absl::remove_extent_t<T>[n]);
}
}  // namespace


// When adding a new encoding, be aware that old binaries will not be able
// to decode it.
static const unsigned char kCurrentEncodingVersionNumber = 1;

S2LaxPolygonShape::S2LaxPolygonShape(
    const vector<S2LaxPolygonShape::Loop>& loops) {
  Init(loops);
}

S2LaxPolygonShape::S2LaxPolygonShape(Span<const Span<const S2Point>> loops) {
  Init(loops);
}

S2LaxPolygonShape::S2LaxPolygonShape(const S2Polygon& polygon) {
  Init(polygon);
}

void S2LaxPolygonShape::Init(const vector<S2LaxPolygonShape::Loop>& loops) {
  vector<Span<const S2Point>> spans;
  spans.reserve(loops.size());
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
      S2Point* v0 = &vertices_[loop_starts_[i]];
      std::reverse(v0, v0 + num_loop_vertices(i));
    }
  }
}

void S2LaxPolygonShape::Init(Span<const Span<const S2Point>> loops) {
  num_loops_ = loops.size();
  if (num_loops_ == 0) {
    num_vertices_ = 0;
  } else if (num_loops_ == 1) {
    num_vertices_ = loops[0].size();
    // TODO(ericv): Use std::allocator to obtain uninitialized memory instead.
    // This would avoid default-constructing all the elements before we
    // overwrite them, and it would also save 8 bytes of memory allocation
    // since "new T[]" stores its own copy of the array size.
    //
    // Note that even absl::make_unique_for_overwrite<> and c++20's
    // std::make_unique_for_overwrite<T[]> default-construct all elements when
    // T is a class type.
    vertices_ = make_unique<S2Point[]>(num_vertices_);
    std::copy(loops[0].begin(), loops[0].end(), vertices_.get());
  } else {
    // Don't use make_unique<> here in order to avoid zero initialization.
    loop_starts_ = make_unique_for_overwrite<uint32[]>(num_loops_ + 1);
    num_vertices_ = 0;
    for (int i = 0; i < num_loops_; ++i) {
      loop_starts_[i] = num_vertices_;
      num_vertices_ += loops[i].size();
    }
    loop_starts_[num_loops_] = num_vertices_;
    vertices_ = make_unique<S2Point[]>(num_vertices_);  // TODO(see above)
    for (int i = 0; i < num_loops_; ++i) {
      std::copy(loops[i].begin(), loops[i].end(),
                vertices_.get() + loop_starts_[i]);
    }
  }
}

S2LaxPolygonShape::~S2LaxPolygonShape() {
}

int S2LaxPolygonShape::num_loop_vertices(int i) const {
  S2_DCHECK_LT(i, num_loops());
  if (num_loops() == 1) {
    return num_vertices_;
  } else {
    return loop_starts_[i + 1] - loop_starts_[i];
  }
}

const S2Point& S2LaxPolygonShape::loop_vertex(int i, int j) const {
  S2_DCHECK_LT(i, num_loops());
  S2_DCHECK_LT(j, num_loop_vertices(i));
  if (i == 0) {
    return vertices_[j];
  } else {
    return vertices_[loop_starts_[i] + j];
  }
}

void S2LaxPolygonShape::Encode(Encoder* encoder,
                               s2coding::CodingHint hint) const {
  encoder->Ensure(1 + Varint::kMax32);
  encoder->put8(kCurrentEncodingVersionNumber);
  encoder->put_varint32(num_loops());
  s2coding::EncodeS2PointVector(MakeSpan(vertices_.get(), num_vertices()),
                                hint, encoder);
  if (num_loops() > 1) {
    s2coding::EncodeUintVector<uint32>(
        MakeSpan(loop_starts_.get(), num_loops() + 1), encoder);
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
  } else {
    num_vertices_ = vertices.size();
    vertices_ = make_unique<S2Point[]>(num_vertices_);  // TODO(see above)
    for (int i = 0; i < num_vertices_; ++i) {
      vertices_[i] = vertices[i];
    }
    if (num_loops_ > 1) {
      s2coding::EncodedUintVector<uint32> loop_starts;
      if (!loop_starts.Init(decoder)) return false;
      loop_starts_ = make_unique_for_overwrite<uint32[]>(loop_starts.size());
      for (int i = 0; i < loop_starts.size(); ++i) {
        loop_starts_[i] = loop_starts[i];
      }
    }
  }
  return true;
}

S2Shape::Edge S2LaxPolygonShape::edge(int e) const {
  // Method names are fully specified to enable inlining.
  ChainPosition pos = S2LaxPolygonShape::chain_position(e);
  return S2LaxPolygonShape::chain_edge(pos.chain_id, pos.offset);
}

S2Shape::ReferencePoint S2LaxPolygonShape::GetReferencePoint() const {
  return s2shapeutil::GetReferencePoint(*this);
}

S2Shape::Chain S2LaxPolygonShape::chain(int i) const {
  S2_DCHECK_LT(i, num_loops());
  if (num_loops() == 1) {
    return Chain(0, num_vertices_);
  } else {
    int start = loop_starts_[i];
    return Chain(start, loop_starts_[i + 1] - start);
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
    if (!loop_starts_.Init(decoder)) return false;
  }
  return true;
}

// The encoding must be identical to S2LaxPolygonShape::Encode().
void EncodedS2LaxPolygonShape::Encode(Encoder* encoder,
                                      s2coding::CodingHint) const {
  encoder->Ensure(1 + Varint::kMax32);
  encoder->put8(kCurrentEncodingVersionNumber);
  encoder->put_varint32(num_loops_);
  vertices_.Encode(encoder);
  if (num_loops_ > 1) {
    loop_starts_.Encode(encoder);
  }
}

int EncodedS2LaxPolygonShape::num_vertices() const {
  if (num_loops() <= 1) {
    return vertices_.size();
  } else {
    return loop_starts_[num_loops()];
  }
}

int EncodedS2LaxPolygonShape::num_loop_vertices(int i) const {
  S2_DCHECK_LT(i, num_loops());
  if (num_loops() == 1) {
    return vertices_.size();
  } else {
    return loop_starts_[i + 1] - loop_starts_[i];
  }
}

S2Point EncodedS2LaxPolygonShape::loop_vertex(int i, int j) const {
  S2_DCHECK_LT(i, num_loops());
  S2_DCHECK_LT(j, num_loop_vertices(i));
  if (num_loops() == 1) {
    return vertices_[j];
  } else {
    return vertices_[loop_starts_[i] + j];
  }
}

S2Shape::Edge EncodedS2LaxPolygonShape::edge(int e) const {
  S2_DCHECK_LT(e, num_edges());
  int e1 = e + 1;
  if (num_loops() == 1) {
    if (e1 == vertices_.size()) { e1 = 0; }
    return Edge(vertices_[e], vertices_[e1]);
  } else {
    // Method names are fully specified to enable inlining.
    ChainPosition pos = EncodedS2LaxPolygonShape::chain_position(e);
    return EncodedS2LaxPolygonShape::chain_edge(pos.chain_id, pos.offset);
  }
}

S2Shape::ReferencePoint EncodedS2LaxPolygonShape::GetReferencePoint() const {
  return s2shapeutil::GetReferencePoint(*this);
}

S2Shape::Chain EncodedS2LaxPolygonShape::chain(int i) const {
  S2_DCHECK_LT(i, num_loops());
  if (num_loops() == 1) {
    return Chain(0, vertices_.size());
  } else {
    int start = loop_starts_[i];
    return Chain(start, loop_starts_[i + 1] - start);
  }
}
