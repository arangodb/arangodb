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

// Author: ericv@google.com (Eric Veach)

#ifndef S2_S2POINT_SPAN_H_
#define S2_S2POINT_SPAN_H_

#include "s2/base/logging.h"
#include "s2/third_party/absl/types/span.h"
#include "s2/s2point.h"

// S2PointSpan represents a view of an S2Point array.  It is used to pass
// vertex arrays to functions that don't care about the actual array type
// (e.g. std::vector<S2Point> or S2Point[]).
//
// NOTE: S2PointSpan has an implicit constructor from any container type with
// data() and size() methods (such as std::vector and std::array).  Therefore
// you can use such containers as arguments for any S2PointSpan parameter.
using S2PointSpan = absl::Span<const S2Point>;

// Like S2PointSpan, except that operator[] maps index values in the range
// [n, 2*n-1] to the range [0, n-1] by subtracting n (where n == size()).
// In other words, two full copies of the vertex array are available.  (This
// is a compromise between convenience and efficiency, since computing the
// index modulo "n" is surprisingly expensive.)
//
// This property is useful for implementing algorithms where the elements of
// the span represent the vertices of a loop.
class S2PointLoopSpan : public S2PointSpan {
 public:
  // Inherit all constructors.
  using absl::Span<const S2Point>::Span;

  // Like operator[], but allows index values in the range [0, 2*size()-1]
  // where each index i >= size() is mapped to i - size().
  reference operator[](int i) const noexcept {
    S2_DCHECK_GE(i, 0);
    S2_DCHECK_LT(i, 2 * size());
    int j = i - size();
    return S2PointSpan::operator[](j < 0 ? i : j);
  }
};

#endif  // S2_S2POINT_SPAN_H_
