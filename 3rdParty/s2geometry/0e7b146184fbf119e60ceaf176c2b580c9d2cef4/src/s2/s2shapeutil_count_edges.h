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

#ifndef S2_S2SHAPEUTIL_COUNT_EDGES_H_
#define S2_S2SHAPEUTIL_COUNT_EDGES_H_

#include "s2/s2shape_index.h"

namespace s2shapeutil {

// Returns the total number of edges in all indexed shapes.  This method takes
// time linear in the number of shapes.
template <class S2ShapeIndexType>
int CountEdges(const S2ShapeIndexType& index);

// Like CountEdges(), but stops once "max_edges" edges have been found (in
// which case the current running total is returned).
template <class S2ShapeIndexType>
int CountEdgesUpTo(const S2ShapeIndexType& index, int max_edges);


//////////////////   Implementation details follow   ////////////////////


template <class S2ShapeIndexType>
inline int CountEdges(const S2ShapeIndexType& index) {
  return CountEdgesUpTo(index, std::numeric_limits<int>::max());
}

template <class S2ShapeIndexType>
int CountEdgesUpTo(const S2ShapeIndexType& index, int max_edges) {
  int num_edges = 0;
  for (S2Shape* shape : index) {
    if (shape == nullptr) continue;
    num_edges += shape->num_edges();
    if (num_edges >= max_edges) break;
  }
  return num_edges;
}

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_COUNT_EDGES_H_
