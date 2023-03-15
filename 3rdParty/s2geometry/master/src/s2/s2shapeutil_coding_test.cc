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
#include <string>
#include <utility>

#include <gtest/gtest.h>
#include "absl/strings/escaping.h"
#include "s2/util/coding/coder.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2lax_polyline_shape.h"
#include "s2/s2point_vector_shape.h"
#include "s2/s2polygon.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using std::string;

namespace s2shapeutil {

TEST(FastEncodeShape, S2Polygon) {
  auto polygon = s2textformat::MakePolygonOrDie("0:0, 0:1, 1:0");
  auto shape = make_unique<S2Polygon::Shape>(polygon.get());
  Encoder encoder;
  ASSERT_TRUE(FastEncodeShape(*shape, &encoder));
  Decoder decoder(encoder.base(), encoder.length());
  auto shape2 = FullDecodeShape(S2Polygon::Shape::kTypeTag, &decoder);
  EXPECT_TRUE(shape->polygon()->Equals(
      *down_cast<S2Polygon::Shape*>(shape2.get())->polygon()));
}

TEST(FastEncodeTaggedShapes, MixedShapes) {
  // Tests encoding/decoding a collection of points, polylines, and polygons.
  auto index = s2textformat::MakeIndexOrDie(
      "0:0 | 0:1 # 1:1, 1:2, 1:3 # 2:2; 2:3, 2:4, 3:3");
  Encoder encoder;
  ASSERT_TRUE(s2shapeutil::FastEncodeTaggedShapes(*index, &encoder));
  index->Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  MutableS2ShapeIndex decoded_index;
  ASSERT_TRUE(decoded_index.Init(
      &decoder, s2shapeutil::FullDecodeShapeFactory(&decoder)));
  EXPECT_EQ(s2textformat::ToString(*index),
            s2textformat::ToString(decoded_index));
}

TEST(DecodeTaggedShapes, DecodeFromByteString) {
  auto index = s2textformat::MakeIndexOrDie(
      "0:0 | 0:1 # 1:1, 1:2, 1:3 # 2:2; 2:3, 2:4, 3:3");
  auto polygon = s2textformat::MakePolygonOrDie("0:0, 0:4, 4:4, 4:0");
  auto polyline = s2textformat::MakePolylineOrDie("1:1, 1:2, 1:3");
  index->Add(make_unique<S2LaxPolylineShape>(*polyline));
  index->Add(make_unique<S2LaxPolygonShape>(*polygon));
  string bytes = absl::HexStringToBytes(
      "2932007C00E4002E0192010310000000000000F03F000000000000000000000000000000"
      "008AAFF597C0FEEF3F1EDD892B0BDF913F00000000000000000418B4825F3C81FDEF3F27"
      "DCF7C958DE913F1EDD892B0BDF913FD44A8442C3F9EF3FCE5B5A6FA6DDA13F1EDD892B0B"
      "DF913FAE0218F586F3EF3F3C3F66D2BBCAAA3F1EDD892B0BDF913F05010220FC7FB8B805"
      "F6EF3F28516A6D8FDBA13F27DCF7C958DEA13F96E20626CAEFEF3F4BF8A48399C7AA3F27"
      "DCF7C958DEA13F96B6DB0611E7EF3FC0221C80C6D8B13F27DCF7C958DEA13FE2337CCA8F"
      "E9EF3F6C573C9B60C2AA3F0EC9EF48C7CBAA3F0C0001040418B4825F3C81FDEF3F27DCF7"
      "C958DE913F1EDD892B0BDF913FD44A8442C3F9EF3FCE5B5A6FA6DDA13F1EDD892B0BDF91"
      "3FAE0218F586F3EF3F3C3F66D2BBCAAA3F1EDD892B0BDF913F05010120000000000000F0"
      "3F00000000000000000000000000000000F6FF70710BECEF3F28516A6D8FDBB13F000000"
      "00000000003C4A985423D8EF3F199E8D966CD0B13F28516A6D8FDBB13FF6FF70710BECEF"
      "3F000000000000000028516A6D8FDBB13F28C83900010003010403040504070400073807"
      "0E1B24292B3213000009030002130000110300092B00010001000000010D000002230410"
      "04020400020113082106110A4113000111030101");
  Decoder decoder(bytes.data(), bytes.length());
  MutableS2ShapeIndex decoded_index;
  ASSERT_TRUE(decoded_index.Init(
      &decoder, s2shapeutil::FullDecodeShapeFactory(&decoder)));
  EXPECT_EQ(s2textformat::ToString(*index),
            s2textformat::ToString(decoded_index));
}

TEST(DecodeTaggedShapes, DecodeFromEncoded) {
  Encoder encoder;

  // Make an encoded shape.
  ASSERT_TRUE(s2shapeutil::FastEncodeShape(
      S2PointVectorShape(s2textformat::ParsePointsOrDie("0:0, 0:1")),
      &encoder));
  Decoder decoder(encoder.base(), encoder.length());
  EncodedS2PointVectorShape encoded_shape;
  ASSERT_TRUE(encoded_shape.Init(&decoder));

  // Encode the encoded form.
  Encoder reencoder;
  ASSERT_TRUE(s2shapeutil::FastEncodeShape(encoded_shape, &reencoder));
  Decoder encoded_decoder(reencoder.base(), reencoder.length());

  // We can decode the shape in either full or lazy form from the same bytes.
  auto full_shape = s2shapeutil::FullDecodeShape(S2PointVectorShape::kTypeTag,
                                                 &encoded_decoder);
  ASSERT_EQ(full_shape->type_tag(), S2PointVectorShape::kTypeTag);

  encoded_decoder.reset(reencoder.base(), reencoder.length());
  auto lazy_shape = s2shapeutil::LazyDecodeShape(S2PointVectorShape::kTypeTag,
                                                 &encoded_decoder);
  ASSERT_EQ(lazy_shape->type_tag(), S2PointVectorShape::kTypeTag);
}

TEST(SingletonShapeFactory, S2Polygon) {
  auto polygon = s2textformat::MakePolygonOrDie("0:0, 0:1, 1:0");
  auto shape = make_unique<S2Polygon::Shape>(polygon.get());
  VectorShapeFactory shape_factory = SingletonShapeFactory(std::move(shape));

  // Returns the shape the first time.
  auto shape2 = shape_factory[0];
  EXPECT_TRUE(
      polygon->Equals(*down_cast<S2Polygon::Shape*>(shape2.get())->polygon()));

  // And nullptr after that.
  auto shape3 = shape_factory[0];
  EXPECT_TRUE(shape3 == nullptr);
}

}  // namespace s2shapeutil
