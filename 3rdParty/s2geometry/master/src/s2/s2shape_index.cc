// Copyright 2012 Google Inc. All Rights Reserved.
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

#include "s2/s2shape_index.h"

bool S2ClippedShape::ContainsEdge(int id) const {
  // Linear search is fast because the number of edges per shape is typically
  // very small (less than 10).
  for (int e = 0; e < num_edges(); ++e) {
    if (edge(e) == id) return true;
  }
  return false;
}

S2ShapeIndexCell::~S2ShapeIndexCell() {
  // Free memory for all shapes owned by this cell.
  for (S2ClippedShape& s : shapes_)
    s.Destruct();
  shapes_.clear();
}

const S2ClippedShape*
S2ShapeIndexCell::find_clipped(int shape_id) const {
  // Linear search is fine because the number of shapes per cell is typically
  // very small (most often 1), and is large only for pathological inputs
  // (e.g. very deeply nested loops).
  for (const auto& s : shapes_) {
    if (s.shape_id() == shape_id) return &s;
  }
  return nullptr;
}

// Allocate room for "n" additional clipped shapes in the cell, and return a
// pointer to the first new clipped shape.  Expects that all new clipped
// shapes will have a larger shape id than any current shape, and that shapes
// will be added in increasing shape id order.
S2ClippedShape* S2ShapeIndexCell::add_shapes(int n) {
  int size = shapes_.size();
  shapes_.resize(size + n);
  return &shapes_[size];
}

void S2ShapeIndexCell::Encode(int num_shape_ids, Encoder* encoder) const {
  // The encoding is designed to be especially compact in certain common
  // situations:
  //
  // 1. The S2ShapeIndex contains exactly one shape.
  //
  // 2. The S2ShapeIndex contains more than one shape, but a particular index
  //    cell contains only one shape (num_clipped == 1).
  //
  // 3. The edge ids for a given shape in a cell form a contiguous range.
  //
  // The details were optimized by constructing an S2ShapeIndex for each
  // feature in Google's geographic repository and measuring their total
  // encoded size.  The MutableS2ShapeIndex encoding (of which this function
  // is just one part) uses an average of 1.88 bytes per vertex for features
  // consisting of polygons or polylines.
  //
  // Note that this code does not bother handling num_shapes >= 2**28 or
  // num_edges >= 2**29.  This could be fixed using varint64 in a few more
  // places, but if a single cell contains this many shapes or edges then we
  // have bigger problems than just the encoding format :)
  if (num_shape_ids == 1) {
    // If the entire S2ShapeIndex contains just one shape, then we don't need
    // to encode any shape ids.  This is a very important and common case.
    S2_DCHECK_EQ(num_clipped(), 1);  // Index invariant: no empty cells.
    const S2ClippedShape& clipped = this->clipped(0);
    S2_DCHECK_EQ(clipped.shape_id(), 0);
    int n = clipped.num_edges();
    encoder->Ensure(Varint::kMax64 + n * Varint::kMax32);
    if (n >= 2 && n <= 17 && clipped.edge(n - 1) - clipped.edge(0) == n - 1) {
      // The cell contains a contiguous range of edges (*most common case*).
      // If the starting edge id is small then we can encode the cell in one
      // byte.  (The n == 0 and n == 1 cases are encoded compactly below.)
      // This encoding uses a 1-bit tag because it is by far the most common.
      //
      // Encoding: bit 0: 0
      //           bit 1: contains_center
      //           bits 2-5: (num_edges - 2)
      //           bits 6+: edge_id
      encoder->put_varint64(static_cast<uint64>(clipped.edge(0)) << 6 |
                            (n - 2) << 2 | clipped.contains_center() << 1 | 0);
    } else if (n == 1) {
      // The cell contains only one edge.  For edge ids up to 15, we can
      // encode the cell in a single byte.
      //
      // Encoding: bits 0-1: 1
      //           bit 2: contains_center
      //           bits 3+: edge_id
      encoder->put_varint64(static_cast<uint64>(clipped.edge(0)) << 3 |
                            clipped.contains_center() << 2 | 1);
    } else {
      // General case (including n == 0, which is encoded compactly here).
      //
      // Encoding: bits 0-1: 3
      //           bit 2: contains_center
      //           bits 3+: num_edges
      encoder->put_varint64(static_cast<uint64>(n) << 3 |
                            clipped.contains_center() << 2 | 3);
      EncodeEdges(clipped, encoder);
    }
  } else {
    if (num_clipped() > 1) {
      // The cell contains more than one shape.  The tag for this encoding
      // must be distinguishable from the cases encoded below.  We can afford
      // to use a 3-bit tag because num_clipped is generally small.
      encoder->Ensure(Varint::kMax32);
      encoder->put_varint32((num_clipped() << 3) | 3);
    }
    // The shape ids are delta-encoded.
    int shape_id_base = 0;
    for (int j = 0; j < num_clipped(); ++j) {
      const S2ClippedShape& clipped = this->clipped(j);
      S2_DCHECK_GE(clipped.shape_id(), shape_id_base);
      int shape_delta = clipped.shape_id() - shape_id_base;
      shape_id_base = clipped.shape_id() + 1;

      // Like the code above except that we also need to encode shape_id(s).
      // Because of this some choices are slightly different.
      int n = clipped.num_edges();
      encoder->Ensure((n + 2) * Varint::kMax32);
      if (n >= 1 && n <= 16 && clipped.edge(n - 1) - clipped.edge(0) == n - 1) {
        // The clipped shape has a contiguous range of up to 16 edges.  This
        // encoding uses a 1-bit tag because it is by far the most common.
        //
        // Encoding: bit 0: 0
        //           bit 1: contains_center
        //           bits 2+: edge_id
        // Next value: bits 0-3: (num_edges - 1)
        //             bits 4+: shape_delta
        encoder->put_varint32(clipped.edge(0) << 2 |
                              clipped.contains_center() << 1 | 0);
        encoder->put_varint32(shape_delta << 4 | (n - 1));
      } else if (n == 0) {
        // Special encoding for clipped shapes with no edges.  Such shapes are
        // common in polygon interiors.  This encoding uses a 3-bit tag in
        // order to leave more bits available for the other encodings.
        //
        // NOTE(ericv): When num_clipped > 1, this tag could be 2 bits
        // (because the tag used to indicate num_clipped > 1 can't appear).
        // Alternatively, that tag can be considered reserved for future use.
        //
        // Encoding: bits 0-2: 7
        //           bit 3: contains_center
        //           bits 4+: shape_delta
        encoder->put_varint32(shape_delta << 4 |
                              clipped.contains_center() << 3 | 7);
      } else {
        // General case.  This encoding uses a 2-bit tag, and the first value
        // typically is encoded into one byte.
        //
        // Encoding: bits 0-1: 1
        //           bit 2: contains_center
        //           bits 3+: (num_edges - 1)
        // Next value: shape_delta
        encoder->put_varint32((n - 1) << 3 |
                              clipped.contains_center() << 2 | 1);
        encoder->put_varint32(shape_delta);
        EncodeEdges(clipped, encoder);
      }
    }
  }
}

bool S2ShapeIndexCell::Decode(int num_shape_ids, Decoder* decoder) {
  // This function inverts the encodings documented above.
  if (num_shape_ids == 1) {
    // Entire S2ShapeIndex contains only one shape.
    S2ClippedShape* clipped = add_shapes(1);
    uint64 header;
    if (!decoder->get_varint64(&header)) return false;
    if ((header & 1) == 0) {
      // The cell contains a contiguous range of edges.
      int num_edges = ((header >> 2) & 15) + 2;
      clipped->Init(0 /*shape_id*/, num_edges);
      clipped->set_contains_center((header & 2) != 0);
      for (int i = 0, edge_id = header >> 6; i < num_edges; ++i) {
        clipped->set_edge(i, edge_id + i);
      }
      return true;
    }
    if ((header & 2) == 0) {
      // The cell contains a single edge.
      clipped->Init(0 /*shape_id*/, 1 /*num_edges*/);
      clipped->set_contains_center((header & 4) != 0);
      clipped->set_edge(0, header >> 3);
      return true;
    }
    // The cell contains some other combination of edges.
    int num_edges = header >> 3;
    clipped->Init(0 /*shape_id*/, num_edges);
    clipped->set_contains_center((header & 4) != 0);
    return DecodeEdges(num_edges, clipped, decoder);
  }
  // S2ShapeIndex contains more than one shape.
  uint32 header;
  if (!decoder->get_varint32(&header)) return false;
  int num_clipped = 1;
  if ((header & 7) == 3) {
    // This cell contains more than one shape.
    num_clipped = header >> 3;
    if (!decoder->get_varint32(&header)) return false;
  }
  int shape_id = 0;
  S2ClippedShape* clipped = add_shapes(num_clipped);
  for (int j = 0; j < num_clipped; ++j, ++clipped, ++shape_id) {
    if (j > 0 && !decoder->get_varint32(&header)) return false;
    if ((header & 1) == 0) {
      // The clipped shape contains a contiguous range of edges.
      uint32 shape_id_count = 0;
      if (!decoder->get_varint32(&shape_id_count)) return false;
      shape_id += shape_id_count >> 4;
      int num_edges = (shape_id_count & 15) + 1;
      clipped->Init(shape_id, num_edges);
      clipped->set_contains_center((header & 2) != 0);
      for (int i = 0, edge_id = header >> 2; i < num_edges; ++i) {
        clipped->set_edge(i, edge_id + i);
      }
    } else if ((header & 7) == 7) {
      // The clipped shape has no edges.
      shape_id += header >> 4;
      clipped->Init(shape_id, 0);
      clipped->set_contains_center((header & 8) != 0);
    } else {
      // The clipped shape contains some other combination of edges.
      S2_DCHECK_EQ(header & 3, 1);
      uint32 shape_delta;
      if (!decoder->get_varint32(&shape_delta)) return false;
      shape_id += shape_delta;
      int num_edges = (header >> 3) + 1;
      clipped->Init(shape_id, num_edges);
      clipped->set_contains_center((header & 4) != 0);
      if (!DecodeEdges(num_edges, clipped, decoder)) return false;
    }
  }
  return true;
}

inline void S2ShapeIndexCell::EncodeEdges(const S2ClippedShape& clipped,
                                          Encoder* encoder) {
  // Each entry is an (edge_id, count) pair representing a contiguous range of
  // edges.  The edge ids are delta-encoded such that 0 represents the minimum
  // valid next edge id.
  //
  // Encoding: if bits 0-2 < 7: encodes (count - 1)
  //            - bits 3+: edge delta
  //           if bits 0-2 == 7:
  //            - bits 3+ encode (count - 8)
  //            - Next value is edge delta
  //
  // No count is encoded for the last edge (saving 3 bits).
  int edge_id_base = 0;
  int num_edges = clipped.num_edges();
  for (int i = 0; i < num_edges; ++i) {
    int edge_id = clipped.edge(i);
    S2_DCHECK_GE(edge_id, edge_id_base);
    int delta = edge_id - edge_id_base;
    if (i + 1 == num_edges) {
      // This is the last edge; no need to encode an edge count.
      encoder->put_varint32(delta);
    } else {
      // Count the edges in this contiguous range.
      int count = 1;
      for (; i + 1 < num_edges && clipped.edge(i + 1) == edge_id + count; ++i) {
        ++count;
      }
      if (count < 8) {
        // Count is encoded in low 3 bits of delta.
        encoder->put_varint32(delta << 3 | (count - 1));
      } else {
        // Count and delta are encoded separately.
        encoder->put_varint32((count - 8) << 3 | 7);
        encoder->put_varint32(delta);
      }
      edge_id_base = edge_id + count;
    }
  }
}

inline bool S2ShapeIndexCell::DecodeEdges(int num_edges,
                                          S2ClippedShape* clipped,
                                          Decoder* decoder) {
  // This function inverts the encodings documented above.
  int32 edge_id = 0;
  for (int i = 0; i < num_edges; ) {
    uint32 delta;
    if (!decoder->get_varint32(&delta)) return false;
    if (i + 1 == num_edges) {
      // The last edge is encoded without an edge count.
      clipped->set_edge(i++, edge_id + delta);
    } else {
      // Otherwise decode the count and edge delta.
      uint32 count = (delta & 7) + 1;
      delta >>= 3;
      if (count == 8) {
        count = delta + 8;
        if (!decoder->get_varint32(&delta)) return false;
      }
      edge_id += delta;
      for (; count > 0; --count, ++i, ++edge_id) {
        clipped->set_edge(i, edge_id);
      }
    }
  }
  return true;
}
