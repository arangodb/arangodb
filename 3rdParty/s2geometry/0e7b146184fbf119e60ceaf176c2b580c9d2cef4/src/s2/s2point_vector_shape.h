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

#ifndef S2_S2POINT_VECTOR_SHAPE_H_
#define S2_S2POINT_VECTOR_SHAPE_H_

#include <vector>
#include "s2/encoded_s2point_vector.h"
#include "s2/s2shape.h"

// S2PointVectorShape is an S2Shape representing a set of S2Points. Each point
// is reprsented as a degenerate edge with the same starting and ending
// vertices.
//
// This class is useful for adding a collection of points to an S2ShapeIndex.
class S2PointVectorShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 3;

  // Constructs an empty point vector.
  S2PointVectorShape() {}

  // Constructs an S2PointVectorShape from a vector of points.
  explicit S2PointVectorShape(std::vector<S2Point> points) {
    points_ = std::move(points);
  }

  ~S2PointVectorShape() override = default;

  int num_points() const { return static_cast<int>(points_.size()); }
  const S2Point& point(int i) const { return points_[i]; }

  // Appends an encoded representation of the S2PointVectorShape to "encoder".
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void Encode(Encoder* encoder,
              s2coding::CodingHint hint = s2coding::CodingHint::COMPACT) const {
    s2coding::EncodeS2PointVector(points_, hint, encoder);
  }

  // Decodes an S2PointVectorShape, returning true on success.  (The method
  // name is chosen for compatibility with EncodedS2PointVectorShape below.)
  bool Init(Decoder* decoder) {
    s2coding::EncodedS2PointVector points;
    if (!points.Init(decoder)) return false;
    points_ = points.Decode();
    return true;
  }

  // S2Shape interface:
  int num_edges() const final { return num_points(); }
  Edge edge(int e) const final { return Edge(points_[e], points_[e]); }
  int dimension() const final { return 0; }
  ReferencePoint GetReferencePoint() const final {
    return ReferencePoint::Contained(false);
  }
  int num_chains() const final { return num_points(); }
  Chain chain(int i) const final { return Chain(i, 1); }
  Edge chain_edge(int i, int j) const final {
    S2_DCHECK_EQ(j, 0);
    return Edge(points_[i], points_[i]);
  }
  ChainPosition chain_position(int e) const final {
    return ChainPosition(e, 0);
  }
  TypeTag type_tag() const override { return kTypeTag; }

 private:
  std::vector<S2Point> points_;
};

// Exactly like S2PointVectorShape, except that the points are kept in an
// encoded form and are decoded only as they are accessed.  This allows for
// very fast initialization and no additional memory use beyond the encoded
// data.  The encoded data is not owned by this class; typically it points
// into a large contiguous buffer that contains other encoded data as well.
class EncodedS2PointVectorShape : public S2Shape {
 public:
  // Constructs an uninitialized object; requires Init() to be called.
  EncodedS2PointVectorShape() {}

  // Initializes an EncodedS2PointVectorShape.
  //
  // REQUIRES: The Decoder data buffer must outlive this object.
  bool Init(Decoder* decoder) { return points_.Init(decoder); }

  int num_points() const { return static_cast<int>(points_.size()); }
  S2Point point(int i) const { return points_[i]; }

  // S2Shape interface:
  int num_edges() const final { return num_points(); }
  Edge edge(int e) const final { return Edge(points_[e], points_[e]); }
  int dimension() const final { return 0; }
  ReferencePoint GetReferencePoint() const final {
    return ReferencePoint::Contained(false);
  }
  int num_chains() const final { return num_points(); }
  Chain chain(int i) const final { return Chain(i, 1); }
  Edge chain_edge(int i, int j) const final {
    S2_DCHECK_EQ(j, 0);
    return Edge(points_[i], points_[i]);
  }
  ChainPosition chain_position(int e) const final {
    return ChainPosition(e, 0);
  }

 private:
  s2coding::EncodedS2PointVector points_;
};


#endif  // S2_S2POINT_VECTOR_SHAPE_H_
