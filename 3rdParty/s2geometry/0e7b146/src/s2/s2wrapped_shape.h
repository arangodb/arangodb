// Copyright 2021 Google Inc. All Rights Reserved.
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

#ifndef S2_S2WRAPPED_SHAPE_H_
#define S2_S2WRAPPED_SHAPE_H_

#include "s2/s2shape.h"

// An S2Shape that simply some other shape.  This is useful for adding
// an existing S2Shape to a new S2ShapeIndex without needing to copy
// its underlying data.
//
// Also see s2shapeutil::WrappedShapeFactory in s2shapeutil_coding.h, which
// is useful for testing S2ShapeIndex coding.
class S2WrappedShape : public S2Shape {
 public:
  explicit S2WrappedShape(const S2Shape* shape) : shape_(*shape) {}

  // S2Shape interface:
  int num_edges() const final { return shape_.num_edges(); }
  Edge edge(int e) const final { return shape_.edge(e); }
  int dimension() const final { return shape_.dimension(); }
  ReferencePoint GetReferencePoint() const final {
    return shape_.GetReferencePoint();
  }
  int num_chains() const final { return shape_.num_chains(); }
  Chain chain(int i) const final { return shape_.chain(i); }
  Edge chain_edge(int i, int j) const final {
    return shape_.chain_edge(i, j);
  }
  ChainPosition chain_position(int e) const final {
    return shape_.chain_position(e);
  }

 private:
  const S2Shape& shape_;
};

#endif  // S2_S2WRAPPED_SHAPE_H_
