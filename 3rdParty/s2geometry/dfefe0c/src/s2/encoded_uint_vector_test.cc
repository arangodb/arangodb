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

#include "s2/encoded_uint_vector.h"

#include <vector>
#include <gtest/gtest.h>

using std::vector;

namespace s2coding {

static_assert(sizeof(EncodedUintVector<uint64>) == 16, "too big");

template <class T>
void TestEncodedUintVector(const vector<T>& expected, size_t expected_bytes) {
  Encoder encoder;
  EncodeUintVector<T>(expected, &encoder);
  EXPECT_EQ(expected_bytes, encoder.length());
  Decoder decoder(encoder.base(), encoder.length());
  EncodedUintVector<T> actual;
  ASSERT_TRUE(actual.Init(&decoder));
  EXPECT_EQ(actual.Decode(), expected);
}

TEST(EncodedUintVectorTest, Empty) {
  TestEncodedUintVector(vector<uint32>{}, 1);
}

TEST(EncodedUintVectorTest, Zero) {
  TestEncodedUintVector(vector<uint64>{0}, 2);
}

TEST(EncodedUintVectorTest, RepeatedZeros) {
  TestEncodedUintVector(vector<uint16>{0, 0, 0}, 4);
}

TEST(EncodedUintVectorTest, MaxInt) {
  TestEncodedUintVector(vector<uint64>{~0ULL}, 9);
}

TEST(EncodedUintVectorTest, OneByte) {
  TestEncodedUintVector(vector<uint64>{0, 255, 1, 254}, 5);
}

TEST(EncodedUintVectorTest, TwoBytes) {
  TestEncodedUintVector(vector<uint64>{0, 255, 256, 254}, 9);
}

TEST(EncodedUintVectorTest, ThreeBytes) {
  TestEncodedUintVector(vector<uint64>{0xffffff, 0x0102, 0, 0x050403}, 13);
}

TEST(EncodedUintVectorTest, EightBytes) {
  TestEncodedUintVector(vector<uint64>{~0ULL, 0, 0x0102030405060708}, 25);
}

}  // namespace s2coding
