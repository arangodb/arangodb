////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include "Zkd/ZkdHelper.h"

#include "gtest/gtest.h"

using namespace arangodb;
using namespace arangodb::zkd;

TEST(Zkd_byte_string_conversion, uint64) {
  auto tests = {std::pair{uint64_t{12}, byte_string{0_b, 0_b, 0_b, 0_b, 0_b, 0_b, 0_b, 12_b}},
                std::pair{uint64_t{0xAABBCCDD},
                          byte_string{0_b, 0_b, 0_b, 0_b, 0xAA_b, 0xBB_b, 0xCC_b, 0xDD_b}},
                std::pair{uint64_t{0x0123456789ABCDEF},
                          byte_string{0x01_b, 0x23_b, 0x45_b, 0x67_b, 0x89_b,
                                      0xAB_b, 0xCD_b, 0xEF_b}}};

  for (auto&& [v, bs] : tests) {
    auto result = to_byte_string_fixed_length(v);
    EXPECT_EQ(result, bs);
  }
}

TEST(Zkd_byte_string_conversion, uint64_compare) {
  auto tests = {
      std::pair{uint64_t{12}, uint64_t{7}},
      std::pair{uint64_t{4567}, uint64_t{768735456}},
      std::pair{uint64_t{4567}, uint64_t{4567}},
  };

  for (auto&& [a, b] : tests) {
    auto a_bs = to_byte_string_fixed_length(a);
    auto b_bs = to_byte_string_fixed_length(b);

    EXPECT_EQ(a < b, a_bs < b_bs)
        << "byte string of " << a << " and " << b
        << " does not compare equally: " << a_bs << " " << b_bs;
  }
}

TEST(Zkd_byte_string_conversion, int64) {
  auto tests = {std::pair{int64_t{12}, byte_string{0xff_b, 0_b, 0_b, 0_b, 0_b,
                                                   0_b, 0_b, 0_b, 12_b}},
                std::pair{int64_t{0xAABBCCDD}, byte_string{0xFF_b, 0_b, 0_b, 0_b, 0_b, 0xAA_b,
                                                           0xBB_b, 0xCC_b, 0xDD_b}},
                std::pair{int64_t{-0x0123456789ABCDEF},
                          byte_string{0x00_b, 0xFE_b, 0xDC_b, 0xBA_b, 0x98_b,
                                      0x76_b, 0x54_b, 0x32_b, 0x11_b}}};

  for (auto&& [v, bs] : tests) {
    auto result = to_byte_string_fixed_length(v);
    EXPECT_EQ(result, bs);
  }
}

TEST(Zkd_byte_string_conversion, int64_compare) {
  auto tests = {
      std::pair{int64_t{12}, int64_t{453}},
      std::pair{int64_t{-12}, int64_t{453}},
      std::pair{int64_t{-1458792}, int64_t{453}},
      std::pair{int64_t{17819835131}, int64_t{-894564}},
      std::pair{int64_t{-12}, int64_t{-8}},
      std::pair{int64_t{-5646872}, int64_t{-5985646871}},
      std::pair{int64_t{-5985646871}, int64_t{-5985646871}},
  };

  for (auto&& [a, b] : tests) {
    auto a_bs = to_byte_string_fixed_length(a);
    auto b_bs = to_byte_string_fixed_length(b);

    EXPECT_EQ(a < b, a_bs < b_bs)
        << "byte string of " << a << " and " << b
        << " does not compare equally: " << a_bs << " " << b_bs;
  }
}

auto const doubles_worth_testing = {0.0,
                                    0.1,
                                    0.2,
                                    0.3,
                                    0.4,
                                    1.0,
                                    10.0,
                                    -1.0,
                                    -0.001,
                                    1000.,
                                    -.00001,
                                    -100.0,
                                    4.e-12,
                                    100000.0
                                    -5e+15,
                                    std::numeric_limits<double>::epsilon(),
                                    std::numeric_limits<double>::max(),
                                    std::numeric_limits<double>::min(),
                                    std::numeric_limits<double>::denorm_min(),
                                    std::numeric_limits<double>::infinity(),
                                    -std::numeric_limits<double>::infinity(),
                                    std::numeric_limits<double>::lowest()};

TEST(Zkd_byte_string_conversion, double_float_cmp) {

  for (auto&& a : doubles_worth_testing) {
    for (auto&& b : doubles_worth_testing) {
      auto a_bs = to_byte_string_fixed_length(a);
      auto b_bs = to_byte_string_fixed_length(b);

      EXPECT_EQ(a < b, a_bs < b_bs)
          << "byte string of " << a << " and " << b
          << " does not compare equally: " << a_bs << " " << b_bs
          << " cnvrt = " << destruct_double(a) << " b = " << destruct_double(b);
      EXPECT_EQ(a == b, a_bs == b_bs)
          << "byte string of " << a << " and " << b
          << " does not compare equally: " << a_bs << " " << b_bs;
      EXPECT_EQ(a > b, a_bs > b_bs)
          << "byte string of " << a << " and " << b
          << " does not compare equally: " << a_bs << " " << b_bs;
      EXPECT_EQ(a >= b, a_bs >= b_bs)
          << "byte string of " << a << " and " << b
          << " does not compare equally: " << a_bs << " " << b_bs;
      EXPECT_EQ(a <= b, a_bs <= b_bs)
          << "byte string of " << a << " and " << b
          << " does not compare equally: " << a_bs << " " << b_bs;
    }
  }
}

TEST(Zkd_byte_string_conversion, bit_reader_test) {
  auto s = "1110 10101"_bs;

  BitReader r(s);
  auto v = r.read_big_endian_bits(4);
  EXPECT_EQ(0b1110, v);
  auto v2 = r.read_big_endian_bits(5);
  EXPECT_EQ(0b10101, v2);
}

TEST(Zkd_byte_string_conversion, bit_reader_test_different_sizes) {
  auto s = "1"_bs;

  {
    BitReader r(s);
    auto v = r.read_big_endian_bits(1);
    EXPECT_EQ(1, v);
  }
  {
    BitReader r(s);
    auto v = r.read_big_endian_bits(8);
    EXPECT_EQ(1 << 7, v);
  }
  {
    BitReader r(s);
    auto v = r.read_big_endian_bits(16);
    EXPECT_EQ(1ull << 15, v);
  }
  {
    BitReader r(s);
    auto v = r.read_big_endian_bits(32);
    EXPECT_EQ(1ull << 31, v);
  }
  {
    BitReader r(s);
    auto v = r.read_big_endian_bits(64);
    EXPECT_EQ(1ull << 63, v);
  }
}

TEST(Zkd_byte_string_conversion, construct_destruct_double) {

  for (auto const a : doubles_worth_testing) {
    auto destructed = destruct_double(a);
    auto reconstructed = construct_double(destructed);
    ASSERT_EQ(a, reconstructed) << "testee: " << a << ", "
                                << "destructed: " << destructed << ", "
                                << "reconstructed: " << reconstructed;
  }
}

TEST(Zkd_byte_string_conversion, double_from_byte_string) {

  for (auto a : doubles_worth_testing) {
    double a1;
    memcpy(&a1, &a, sizeof(double));

    auto a_bs = to_byte_string_fixed_length(a1);
    auto b = from_byte_string_fixed_length<double>(a_bs);

    EXPECT_EQ(a1, b) << "byte string of " << a1 << " is " << a_bs
                     << " and was read as " << b;
  }
}
