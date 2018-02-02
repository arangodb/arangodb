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
