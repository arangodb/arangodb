// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef S2_S2SHAPEUTIL_EDGE_ITERATOR_H_
#define S2_S2SHAPEUTIL_EDGE_ITERATOR_H_

#include "s2/s2shape_index.h"
#include "s2/s2shapeutil_shape_edge_id.h"

namespace s2shapeutil {

// An iterator that advances through all edges in an S2ShapeIndex.
//
// Example usage:
//
// for (EdgeIterator it(index); !it.Done(); it.Next()) {
//   auto edge = it.edge();
//   //...
// }
class EdgeIterator {
 public:
  explicit EdgeIterator(const S2ShapeIndex* index);

  // Returns the current shape id.
  int32 shape_id() const { return shape_id_; }

  // Returns the current edge id.
  int32 edge_id() const { return edge_id_; }

  // Returns the current (shape_id, edge_id).
  ShapeEdgeId shape_edge_id() const { return ShapeEdgeId(shape_id_, edge_id_); }

  // Returns the current edge.
  S2Shape::Edge edge() const;

  // Returns true if there are no more edges in the index.
  bool Done() const { return shape_id() >= index_->num_shape_ids(); }

  // Advances to the next edge.
  void Next();

  bool operator==(const EdgeIterator& other) const {
    return index_ == other.index_ && shape_id_ == other.shape_id_ &&
           edge_id_ == other.edge_id_;
  }

  bool operator!=(const EdgeIterator& other) const { return !(*this == other); }

  string DebugString() const;

 private:
  const S2ShapeIndex* index_;
  int32 shape_id_;
  int32 num_edges_;
  int32 edge_id_;
};

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_EDGE_ITERATOR_H_
