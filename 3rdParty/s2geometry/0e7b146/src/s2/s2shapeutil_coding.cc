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

#include "s2/s2shapeutil_coding.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "s2/encoded_s2point_vector.h"
#include "s2/encoded_string_vector.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2lax_polyline_shape.h"
#include "s2/s2point_vector_shape.h"
#include "s2/s2polygon.h"
#include "s2/s2polyline.h"
#include "s2/s2wrapped_shape.h"

using absl::make_unique;
using std::make_shared;
using std::unique_ptr;
using std::vector;

using CodingHint = s2coding::CodingHint;

namespace s2shapeutil {

bool FastEncodeShape(const S2Shape& shape, Encoder* encoder) {
  uint32 tag = shape.type_tag();
  if (tag == S2Shape::kNoTypeTag) {
    S2_LOG(DFATAL) << "Unsupported S2Shape type: " << tag;
    return false;
  }
  // Update the following constant when adding new S2Shape encodings.
  S2_DCHECK_LT(shape.type_tag(), S2Shape::kNextAvailableTypeTag);
  shape.Encode(encoder, CodingHint::FAST);
  return true;
}

bool CompactEncodeShape(const S2Shape& shape, Encoder* encoder) {
  uint32 tag = shape.type_tag();
  if (tag == S2Shape::kNoTypeTag) {
    S2_LOG(DFATAL) << "Unsupported S2Shape type: " << tag;
    return false;
  }
  // Update the following constant when adding new S2Shape encodings.
  S2_DCHECK_LT(shape.type_tag(), S2Shape::kNextAvailableTypeTag);
  shape.Encode(encoder, CodingHint::COMPACT);
  return true;
}

// A ShapeDecoder that fully decodes an S2Shape of the given type.  After this
// function returns, the underlying Decoder data is no longer needed.
unique_ptr<S2Shape> FullDecodeShape(S2Shape::TypeTag tag, Decoder* decoder) {
  switch (tag) {
    case S2Polygon::Shape::kTypeTag: {
      auto shape = make_unique<S2Polygon::OwningShape>();
      if (!shape->Init(decoder)) return nullptr;
      // Some platforms (e.g. NaCl) require the following conversion.
      return std::move(shape);  // Converts to S2Shape.
    }
    case S2Polyline::Shape::kTypeTag: {
      auto shape = make_unique<S2Polyline::OwningShape>();
      if (!shape->Init(decoder)) return nullptr;
      return std::move(shape);  // Converts to S2Shape.
    }
    case S2PointVectorShape::kTypeTag: {
      auto shape = make_unique<S2PointVectorShape>();
      if (!shape->Init(decoder)) return nullptr;
      return std::move(shape);  // Converts to S2Shape.
    }
    case S2LaxPolylineShape::kTypeTag: {
      auto shape = make_unique<S2LaxPolylineShape>();
      if (!shape->Init(decoder)) return nullptr;
      return std::move(shape);  // Converts to S2Shape.
    }
    case S2LaxPolygonShape::kTypeTag: {
      auto shape = make_unique<S2LaxPolygonShape>();
      if (!shape->Init(decoder)) return nullptr;
      return std::move(shape);  // Converts to S2Shape.
    }
    default: {
      S2_LOG(DFATAL) << "Unsupported S2Shape type: " << tag;
      return nullptr;
    }
  }
}

unique_ptr<S2Shape> LazyDecodeShape(S2Shape::TypeTag tag, Decoder* decoder) {
  switch (tag) {
    case S2PointVectorShape::kTypeTag: {
      auto shape = make_unique<EncodedS2PointVectorShape>();
      if (!shape->Init(decoder)) return nullptr;
      // Some platforms (e.g. NaCl) require the following conversion.
      return std::move(shape);  // Converts to S2Shape.
    }
    case S2LaxPolylineShape::kTypeTag: {
      auto shape = make_unique<EncodedS2LaxPolylineShape>();
      if (!shape->Init(decoder)) return nullptr;
      return std::move(shape);  // Converts to S2Shape.
    }
    case S2LaxPolygonShape::kTypeTag: {
      auto shape = make_unique<EncodedS2LaxPolygonShape>();
      if (!shape->Init(decoder)) return nullptr;
      return std::move(shape);  // Converts to S2Shape.
    }
    default: {
      return FullDecodeShape(tag, decoder);
    }
  }
}

bool EncodeTaggedShapes(const S2ShapeIndex& index,
                        const ShapeEncoder& shape_encoder,
                        Encoder* encoder) {
  s2coding::StringVectorEncoder shape_vector;
  for (S2Shape* shape : index) {
    Encoder* sub_encoder = shape_vector.AddViaEncoder();
    if (shape == nullptr) continue;  // Encode as zero bytes.

    sub_encoder->Ensure(Encoder::kVarintMax32);
    sub_encoder->put_varint32(shape->type_tag());
    if (!shape_encoder(*shape, sub_encoder)) return false;
  }
  shape_vector.Encode(encoder);
  return true;
}

bool FastEncodeTaggedShapes(const S2ShapeIndex& index, Encoder* encoder) {
  return EncodeTaggedShapes(index, FastEncodeShape, encoder);
}

bool CompactEncodeTaggedShapes(const S2ShapeIndex& index, Encoder* encoder) {
  return EncodeTaggedShapes(index, CompactEncodeShape, encoder);
}

TaggedShapeFactory::TaggedShapeFactory(const ShapeDecoder& shape_decoder,
                                       Decoder* decoder)
    : shape_decoder_(shape_decoder) {
  if (!encoded_shapes_.Init(decoder)) encoded_shapes_.Clear();
}

unique_ptr<S2Shape> TaggedShapeFactory::operator[](int shape_id) const {
  Decoder decoder = encoded_shapes_.GetDecoder(shape_id);
  S2Shape::TypeTag tag;
  if (!decoder.get_varint32(&tag)) return nullptr;
  return shape_decoder_(tag, &decoder);
}

TaggedShapeFactory FullDecodeShapeFactory(Decoder* decoder) {
  return TaggedShapeFactory(FullDecodeShape, decoder);
}

TaggedShapeFactory LazyDecodeShapeFactory(Decoder* decoder) {
  return TaggedShapeFactory(LazyDecodeShape, decoder);
}

VectorShapeFactory::VectorShapeFactory(vector<unique_ptr<S2Shape>> shapes)
    : shared_shapes_(
          make_shared<vector<unique_ptr<S2Shape>>>(std::move(shapes))) {
}

unique_ptr<S2Shape> VectorShapeFactory::operator[](int shape_id) const {
  return std::move((*shared_shapes_)[shape_id]);
}

VectorShapeFactory SingletonShapeFactory(std::unique_ptr<S2Shape> shape) {
  vector<unique_ptr<S2Shape>> shapes;
  shapes.push_back(std::move(shape));
  return VectorShapeFactory(std::move(shapes));
}

unique_ptr<S2Shape> WrappedShapeFactory::operator[](int shape_id) const {
  S2Shape* shape = index_.shape(shape_id);
  if (shape == nullptr) return nullptr;
  return make_unique<S2WrappedShape>(shape);
}

}  // namespace s2shapeutil
