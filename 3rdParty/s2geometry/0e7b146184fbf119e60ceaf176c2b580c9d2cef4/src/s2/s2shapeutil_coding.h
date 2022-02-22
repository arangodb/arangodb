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
//
// Helper functions for encoding/decoding S2Shapes and S2Shape vectors.
// Design goals:
//
//  - Allow control over encoding tradeoffs (i.e., speed vs encoding size).
//
//  - Allow control over decoding tradeoffs (e.g., whether to decode data
//    immediately or lazily).
//
//  - Don't force all S2Shape types to have the same Encode() method.  Some
//    implementations may want extra parameters.
//
//  - Support custom encodings of shape vectors; e.g., if all shapes are
//    of a known type, then there is no need to tag them individually.
//
//  - Support client-defined S2Shape types.
//
//  - Support client-defined encodings of standard S2Shape types.

#ifndef S2_S2SHAPEUTIL_CODING_H_
#define S2_S2SHAPEUTIL_CODING_H_

#include <functional>
#include <memory>
#include "s2/util/coding/coder.h"
#include "s2/encoded_string_vector.h"
#include "s2/s2shape.h"
#include "s2/s2shape_index.h"

namespace s2shapeutil {

// A function that appends a serialized representation of the given shape to
// the given Encoder.  The encoding should *not* include any type information
// (e.g., shape->type_tag()); the caller is responsible for encoding this
// separately if necessary.
//
// Note that you can add your own encodings and/or shape types by wrapping one
// of the standard functions and adding exceptions:
//
// void MyShapeEncoder(const S2Shape& shape, Encoder* encoder) {
//   if (shape.type_tag() == MyShape::kTypeTag) {
//     down_cast<const MyShape*>(&shape)->Encode(encoder);
//     return true;
//   } else {
//     return CompactEncodeShape(shape, encoder);
//   }
// }
//
// REQUIRES: "encoder" uses the default constructor, so that its buffer
//           can be enlarged as necessary by calling Ensure(int).
using ShapeEncoder =
    std::function<bool (const S2Shape& shape, Encoder* encoder)>;

// A ShapeEncoder that can encode all standard S2Shape types, preferring fast
// (but larger) encodings.  For example, points are typically represented as
// uncompressed S2Point values (24 bytes each).
//
// REQUIRES: "encoder" uses the default constructor, so that its buffer
//           can be enlarged as necessary by calling Ensure(int).
bool FastEncodeShape(const S2Shape& shape, Encoder* encoder);

// A ShapeEncoder that encode all standard S2Shape types, preferring
// compact (but slower) encodings.  For example, points that have been snapped
// to S2CellId centers will be encoded using at most 8 bytes.
//
// REQUIRES: "encoder" uses the default constructor, so that its buffer
//           can be enlarged as necessary by calling Ensure(int).
bool CompactEncodeShape(const S2Shape& shape, Encoder* encoder);

// A function that decodes an S2Shape of the given type, consuming data from
// the given Decoder.  Returns nullptr on errors.
using ShapeDecoder =
    std::function<std::unique_ptr<S2Shape>(S2Shape::TypeTag tag,
                                           Decoder* decoder)>;

// A ShapeDecoder that fully decodes an S2Shape of the given type.  After this
// function returns, the underlying Decoder data is no longer needed.
std::unique_ptr<S2Shape> FullDecodeShape(S2Shape::TypeTag tag,
                                         Decoder* decoder);

// A ShapeDecoder that prefers to decode the given S2Shape lazily (as data is
// accessed).  This is only possible when the given shape type (e.g.,
// LaxPolygonShape) has an alternate implementation that can work directly
// with encoded data (e.g., EncodedLaxPolygonShape).  All other shape types
// are handled by decoding them fully (e.g., S2Polygon::Shape).
std::unique_ptr<S2Shape> LazyDecodeShape(S2Shape::TypeTag tag,
                                         Decoder* decoder);

// Encodes the shapes in the given S2ShapeIndex.  Each shape is encoded with a
// type tag allows it to be decoded into an S2Shape of the appropriate type.
// "shape_encoder" allows control over the encoding strategy.  Note that when
// an S2ShapeIndex is also being encoded, it should be encoded *after* the
// shape vector, like this:
//
//   s2shapeutil::CompactEncodeTaggedShapes(index, encoder);
//   index.Encode(encoder);
//
// This is because when the index is decoded, the shape vector is required as
// a parameter.
//
// REQUIRES: "encoder" uses the default constructor, so that its buffer
//           can be enlarged as necessary by calling Ensure(int).
bool EncodeTaggedShapes(const S2ShapeIndex& index,
                        const ShapeEncoder& shape_encoder,
                        Encoder* encoder);

// Convenience function that calls EncodeTaggedShapes using FastEncodeShape as
// the ShapeEncoder.
//
// REQUIRES: "encoder" uses the default constructor, so that its buffer
//           can be enlarged as necessary by calling Ensure(int).
bool FastEncodeTaggedShapes(const S2ShapeIndex& index, Encoder* encoder);

// Convenience function that calls EncodeTaggedShapes using CompactEncodeShape
// as the ShapeEncoder.
//
// REQUIRES: "encoder" uses the default constructor, so that its buffer
//           can be enlarged as necessary by calling Ensure(int).
bool CompactEncodeTaggedShapes(const S2ShapeIndex& index, Encoder* encoder);

// A ShapeFactory that decodes a vector generated by EncodeTaggedShapes()
// above.  Example usage:
//
//   index.Init(decoder, s2shapeutil::FullDecodeShapeFactory(decoder));
//
// Note that the S2Shape vector must be encoded *before* the S2ShapeIndex
// (like the example code for EncodeTaggedShapes), since the shapes need to be
// decoded before the index.
//
// REQUIRES: The Decoder data buffer must outlive all calls to the given
//           ShapeFactory (not including its destructor).
class TaggedShapeFactory : public S2ShapeIndex::ShapeFactory {
 public:
  // Returns an empty vector and/or null S2Shapes on decoding errors.
  TaggedShapeFactory(const ShapeDecoder& shape_decoder, Decoder* decoder);

  int size() const override { return encoded_shapes_.size(); }

  std::unique_ptr<S2Shape> operator[](int shape_id) const override;

  std::unique_ptr<ShapeFactory> Clone() const override {
    return absl::make_unique<TaggedShapeFactory>(*this);
  }

 private:
  ShapeDecoder shape_decoder_;
  s2coding::EncodedStringVector encoded_shapes_;
};

// Convenience function that calls TaggedShapeFactory using FullDecodeShape
// as the ShapeDecoder.
TaggedShapeFactory FullDecodeShapeFactory(Decoder* decoder);

// Convenience function that calls TaggedShapeFactory using LazyDecodeShape
// as the ShapeDecoder.
TaggedShapeFactory LazyDecodeShapeFactory(Decoder* decoder);

// A ShapeFactory that simply returns shapes from the given vector.
//
// REQUIRES: Each shape is requested at most once.  (This implies that when
// the ShapeFactory is passed to an S2ShapeIndex, S2ShapeIndex::Minimize must
// not be called.)  Additional requests for the same shape return nullptr.
class VectorShapeFactory : public S2ShapeIndex::ShapeFactory {
 public:
  explicit VectorShapeFactory(std::vector<std::unique_ptr<S2Shape>> shapes);

  int size() const override { return shared_shapes_->size(); }

  std::unique_ptr<S2Shape> operator[](int shape_id) const override;

  std::unique_ptr<ShapeFactory> Clone() const override {
    return absl::make_unique<VectorShapeFactory>(*this);
  }

 private:
  // Since this class is copyable, we need to access the shape vector through
  // a shared pointer.
  std::shared_ptr<std::vector<std::unique_ptr<S2Shape>>> shared_shapes_;
};

// A ShapeFactory that returns the single given S2Shape.  Useful for testing.
VectorShapeFactory SingletonShapeFactory(std::unique_ptr<S2Shape> shape);

// A ShapeFactory that wraps the shapes from the given index.  Used for testing.
class WrappedShapeFactory : public S2ShapeIndex::ShapeFactory {
 public:
  // REQUIRES: The given index must persist for the lifetime of this object.
  explicit WrappedShapeFactory(const S2ShapeIndex* index) : index_(*index) {}

  int size() const override { return index_.num_shape_ids(); }

  std::unique_ptr<S2Shape> operator[](int shape_id) const override;

  std::unique_ptr<ShapeFactory> Clone() const override {
    return absl::make_unique<WrappedShapeFactory>(*this);
  }

 private:
  const S2ShapeIndex& index_;
};


// Encodes the shapes in the given index, which must all have the same type.
// The given Shape type does *not* need to have a "type tag" assigned.
// This is useful for encoding experimental or locally defined types, or when
// the S2Shape type in a given index is known in advance.
//
// REQUIRES: The Shape class must have an Encode(Encoder*) method.
// REQUIRES: "encoder" uses the default constructor, so that its buffer
//           can be enlarged as necessary by calling Ensure(int).
template <class Shape>
void EncodeHomogeneousShapes(const S2ShapeIndex& index, Encoder* encoder);

// A ShapeFactory that decodes shapes of a given fixed type (e.g.,
// EncodedS2LaxPolylineShape).  Example usage:
//
// REQUIRES: The Shape type must have an Init(Decoder*) method.
// REQUIRES: The Decoder data buffer must outlive the returned ShapeFactory
//           and all shapes returned by that factory.
template <class Shape>
class HomogeneousShapeFactory : public S2ShapeIndex::ShapeFactory {
 public:
  // Returns an empty vector and/or null S2Shapes on decoding errors.
  explicit HomogeneousShapeFactory(Decoder* decoder);

  int size() const override { return encoded_shapes_.size(); }

  std::unique_ptr<S2Shape> operator[](int shape_id) const override;

  std::unique_ptr<ShapeFactory> Clone() const override {
    return absl::make_unique<HomogeneousShapeFactory>(*this);
  }

 private:
  s2coding::EncodedStringVector encoded_shapes_;
};

//////////////////   Implementation details follow   ////////////////////


template <class Shape>
void EncodeHomogeneousShapes(const S2ShapeIndex& index, Encoder* encoder) {
  s2coding::StringVectorEncoder shape_vector;
  for (S2Shape* shape : index) {
    S2_DCHECK(shape != nullptr);
    down_cast<Shape*>(shape)->Encode(shape_vector.AddViaEncoder());
  }
  shape_vector.Encode(encoder);
}

template <class Shape>
HomogeneousShapeFactory<Shape>::HomogeneousShapeFactory(Decoder* decoder) {
  if (!encoded_shapes_.Init(decoder)) encoded_shapes_.Clear();
}

template <class Shape>
std::unique_ptr<S2Shape> HomogeneousShapeFactory<Shape>::operator[](
    int shape_id) const {
  Decoder decoder = encoded_shapes_.GetDecoder(shape_id);
  auto shape = absl::make_unique<Shape>();
  if (!shape->Init(&decoder)) return nullptr;
  return std::move(shape);  // Converts from Shape to S2Shape.
}

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_CODING_H_
