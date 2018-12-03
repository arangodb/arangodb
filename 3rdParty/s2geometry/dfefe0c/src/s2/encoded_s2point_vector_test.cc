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

#include "s2/encoded_s2point_vector.h"

#include <vector>
#include <gtest/gtest.h>

using std::vector;

namespace s2coding {

void TestEncodedS2PointVector(const vector<S2Point>& expected,
                              size_t expected_bytes) {
  Encoder encoder;
  EncodeS2PointVector(expected, CodingHint::FAST, &encoder);
  EXPECT_EQ(expected_bytes, encoder.length());
  Decoder decoder(encoder.base(), encoder.length());
  EncodedS2PointVector actual;
  ASSERT_TRUE(actual.Init(&decoder));
  EXPECT_EQ(actual.Decode(), expected);
}

TEST(EncodedS2PointVectorTest, Empty) {
  TestEncodedS2PointVector({}, 1);
}

TEST(EncodedS2PointVectorTest, OnePoint) {
  TestEncodedS2PointVector({S2Point(1, 0, 0)}, 25);
}

TEST(EncodedS2PointVectorTest, TenPoints) {
  vector<S2Point> points;
  for (int i = 0; i < 10; ++i) {
    points.push_back(S2Point(1, i, 0).Normalize());
  }
  TestEncodedS2PointVector(points, 241);
}

}  // namespace s2coding
