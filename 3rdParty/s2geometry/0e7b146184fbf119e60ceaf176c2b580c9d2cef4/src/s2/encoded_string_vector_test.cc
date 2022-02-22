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

#include "s2/encoded_string_vector.h"

#include <vector>
#include <gtest/gtest.h>
#include "s2/third_party/absl/strings/string_view.h"

using absl::string_view;
using std::vector;

namespace s2coding {

void TestEncodedStringVector(const vector<string>& input,
                             size_t expected_bytes) {
  Encoder encoder;
  StringVectorEncoder::Encode(input, &encoder);
  EXPECT_EQ(expected_bytes, encoder.length());
  Decoder decoder(encoder.base(), encoder.length());
  EncodedStringVector actual;
  ASSERT_TRUE(actual.Init(&decoder));
  vector<string_view> expected;
  for (const auto& str : input) {
    expected.push_back(string_view(str));
  }
  EXPECT_EQ(actual.Decode(), expected);
}

TEST(EncodedStringVectorTest, Empty) {
  TestEncodedStringVector({}, 1);
}

TEST(EncodedStringVectorTest, EmptyString) {
  TestEncodedStringVector({""}, 2);
}

TEST(EncodedStringVectorTest, RepeatedEmptyStrings) {
  TestEncodedStringVector({"", "", ""}, 4);
}

TEST(EncodedStringVectorTest, OneString) {
  TestEncodedStringVector({"apples"}, 8);
}

TEST(EncodedStringVectorTest, TwoStrings) {
  TestEncodedStringVector({"fuji", "mutsu"}, 12);
}

TEST(EncodedStringVectorTest, TwoBigStrings) {
  TestEncodedStringVector({string(10000, 'x'), string(100000, 'y')},
                          110007);
}

}  // namespace s2coding
