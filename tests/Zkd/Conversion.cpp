#include "gtest/gtest.h"

#include "Zkd/ZkdHelper.h"

using namespace zkd;

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

TEST(Zkd_byte_string_conversion, double_float) {
  auto tests = {std::pair{0.0, byte_string{0b10111111_b, 0b11110000_b, 0_b, 0_b,
                                           0_b, 0_b, 0_b, 0_b}}};

  for (auto&& [v, bs] : tests) {
    auto result = to_byte_string_fixed_length(v);
    EXPECT_EQ(result, bs);
  }
}

TEST(Zkd_byte_string_conversion, double_float_cmp) {
  auto tests = {
      std::pair{1.0, 0.0},         std::pair{-1.0, 1.0},
      std::pair{1.0, 1.2},         std::pair{10.0, 1.2},
      std::pair{-10.0, 1.2},       std::pair{1.0, 1.0},
      std::pair{1000.0, 100000.0}, std::pair{-1000.0, 100000.0},
      std::pair{.0001, .001},      std::pair{-5., -10.},
      std::pair{0., -10.},         std::pair{0., -0.},
      std::pair{7., 7.},           std::pair{7.E15, 7.E15},
  };

  for (auto&& [a, b] : tests) {
    auto a_bs = to_byte_string_fixed_length(a);
    auto b_bs = to_byte_string_fixed_length(b);

    EXPECT_EQ(a < b, a_bs < b_bs)
        << "byte string of " << a << " and " << b
        << " does not compare equally: " << a_bs << " " << b_bs;
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

TEST(Zkd_byte_string_conversion, double_from_byte_string) {
  auto tests = {0.0,   1.0,     10.0,   -1.0,   -0.001,
                1000., -.00001, -100.0, 4.e-12, -5e+15};

  for (auto a : tests) {
    auto a_bs = to_byte_string_fixed_length(a);
    auto b = from_byte_string_fixed_length<double>(a_bs);

    EXPECT_EQ(a, b) << "byte string of " << a << " is " << a_bs
                    << " and was read as " << b;
  }
}
