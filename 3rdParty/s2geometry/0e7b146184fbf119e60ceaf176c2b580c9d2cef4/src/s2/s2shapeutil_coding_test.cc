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

#include <gtest/gtest.h>
#include "s2/util/coding/coder.h"
#include "s2/s2polygon.h"
#include "s2/s2text_format.h"

using absl::make_unique;

namespace s2shapeutil {

TEST(FastEncodeShape, S2Polygon) {
  auto polygon = s2textformat::MakePolygonOrDie("0:0, 0:1, 1:0");
  auto shape = make_unique<S2Polygon::Shape>(polygon.get());
  Encoder encoder;
  FastEncodeShape(*shape, &encoder);
  Decoder decoder(encoder.base(), encoder.length());
  auto shape2 = FullDecodeShape(S2Polygon::Shape::kTypeTag, &decoder);
  EXPECT_TRUE(shape->polygon()->Equals(
      down_cast<S2Polygon::Shape*>(shape2.get())->polygon()));
}

TEST(FastEncodeTaggedShapes, MixedShapes) {
  // Tests encoding/decoding a collection of points, polylines, and polygons.
  auto index = s2textformat::MakeIndexOrDie(
      "0:0 | 0:1 # 1:1, 1:2, 1:3 # 2:2; 2:3, 2:4, 3:3");
  Encoder encoder;
  s2shapeutil::FastEncodeTaggedShapes(*index, &encoder);
  index->Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  MutableS2ShapeIndex decoded_index;
  decoded_index.Init(&decoder, s2shapeutil::FullDecodeShapeFactory(&decoder));
  EXPECT_EQ(s2textformat::ToString(*index),
            s2textformat::ToString(decoded_index));
}

}  // namespace s2shapeutil
