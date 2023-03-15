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

// Make sure that this class is compact since it is extensively used.
// 16 for 64-bit, 12 for 32-bit.
static_assert(sizeof(EncodedUintVector<uint64>) <= 16, "too big");

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

template <class T>
vector<T> MakeSortedTestVector(int bytes_per_value, int num_values) {
  S2_DCHECK_LE(bytes_per_value, sizeof(T));
  T limit_value = ~T{0} >> (8 * (sizeof(T) - bytes_per_value));
  vector<T> values;
  for (int i = 0; i + 1 < num_values; ++i) {
    values.push_back(limit_value * (static_cast<double>(i) / (num_values - 1)));
  }
  // The last value needs special handling since casting it to "double" loses
  // precision when T == uint64.
  values.push_back(limit_value);
  S2_CHECK(std::is_sorted(values.begin(), values.end()));
  return values;
}

template <class T>
EncodedUintVector<T> MakeEncodedVector(const vector<T>& values,
                                       Encoder* encoder) {
  EncodeUintVector<T>(values, encoder);
  Decoder decoder(encoder->base(), encoder->length());
  EncodedUintVector<T> actual;
  S2_CHECK(actual.Init(&decoder));
  return actual;
}

template <class T>
void TestLowerBound(int bytes_per_value, int num_values) {
  auto v = MakeSortedTestVector<T>(bytes_per_value, num_values);
  Encoder encoder;
  auto actual = MakeEncodedVector(v, &encoder);
  for (T x : v) {
    EXPECT_EQ(std::lower_bound(v.begin(), v.end(), x) - v.begin(),
              actual.lower_bound(x));
    if (x > 0) {
      EXPECT_EQ(std::lower_bound(v.begin(), v.end(), x - 1) - v.begin(),
                actual.lower_bound(x - 1));
    }
  }
}

TEST(EncodedUintVector, LowerBound) {
  for (int bytes_per_value = 8; bytes_per_value <= 8; ++bytes_per_value) {
    TestLowerBound<uint64>(bytes_per_value, 10);
    if (bytes_per_value <= 4) {
      TestLowerBound<uint32>(bytes_per_value, 500);
      if (bytes_per_value <= 2) {
        TestLowerBound<uint16>(bytes_per_value, 100);
      }
    }
  }
}

TEST(EncodedUintVectorTest, RoundtripEncoding) {
  std::vector<uint64> values{10, 20, 30, 40};

  Encoder a_encoder;
  auto a = MakeEncodedVector<uint64>(values, &a_encoder);
  ASSERT_EQ(a.Decode(), values);

  Encoder b_encoder;
  a.Encode(&b_encoder);
  Decoder decoder(b_encoder.base(), b_encoder.length());

  EncodedUintVector<uint64> v2;
  ASSERT_TRUE(v2.Init(&decoder));

  EXPECT_EQ(v2.Decode(), values);
}

}  // namespace s2coding
