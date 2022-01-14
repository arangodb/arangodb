// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

/*
    This is a derivative work
*/

#include <boost/json/detail/ryu/ryu.hpp>
#include <array>
#include <cmath>
#include "gtest.hpp"

BOOST_JSON_NS_BEGIN
namespace detail {

namespace ryu {

static double int64Bits2Double(uint64_t bits) {
  double f;
  memcpy(&f, &bits, sizeof(double));
  return f;
}

static double ieeeParts2Double(const bool sign, const uint32_t ieeeExponent, const uint64_t ieeeMantissa) {
  BOOST_ASSERT(ieeeExponent <= 2047);
  BOOST_ASSERT(ieeeMantissa <= ((std::uint64_t)1 << 53) - 1);
  return int64Bits2Double(((std::uint64_t)sign << 63) | ((std::uint64_t)ieeeExponent << 52) | ieeeMantissa);
}

char const* d2s_test_helper(char* result, double f) {
    int index = d2s_buffered_n(f, result);
    result[index] = '\0';
    return result;
}

TEST(D2sTest, Basic) {
  std::array<char, 25> buffer;
  ASSERT_STREQ("0E0", d2s_test_helper(buffer.data(), 0.0));
  ASSERT_STREQ("-0E0", d2s_test_helper(buffer.data(), -0.0));
  ASSERT_STREQ("1E0", d2s_test_helper(buffer.data(), 1.0));
  ASSERT_STREQ("-1E0", d2s_test_helper(buffer.data(), -1.0));
  ASSERT_STREQ("NaN", d2s_test_helper(buffer.data(), NAN));
  ASSERT_STREQ("Infinity", d2s_test_helper(buffer.data(), INFINITY));
  ASSERT_STREQ("-Infinity", d2s_test_helper(buffer.data(), -INFINITY));
}

TEST(D2sTest, SwitchToSubnormal) {
  std::array<char, 25> buffer;
  ASSERT_STREQ("2.2250738585072014E-308", d2s_test_helper(buffer.data(), 2.2250738585072014E-308));
}

TEST(D2sTest, MinAndMax) {
  std::array<char, 25> buffer;
  ASSERT_STREQ("1.7976931348623157E308", d2s_test_helper(buffer.data(), int64Bits2Double(0x7fefffffffffffff)));
  ASSERT_STREQ("5E-324", d2s_test_helper(buffer.data(), int64Bits2Double(1)));
}

TEST(D2sTest, LotsOfTrailingZeros) {
  std::array<char, 25> buffer;
  ASSERT_STREQ("2.9802322387695312E-8", d2s_test_helper(buffer.data(), 2.98023223876953125E-8));
}

TEST(D2sTest, Regression) {
  std::array<char, 25> buffer;
  ASSERT_STREQ("-2.109808898695963E16", d2s_test_helper(buffer.data(), -2.109808898695963E16));
  ASSERT_STREQ("4.940656E-318", d2s_test_helper(buffer.data(), 4.940656E-318));
  ASSERT_STREQ("1.18575755E-316", d2s_test_helper(buffer.data(), 1.18575755E-316));
  ASSERT_STREQ("2.989102097996E-312", d2s_test_helper(buffer.data(), 2.989102097996E-312));
  ASSERT_STREQ("9.0608011534336E15", d2s_test_helper(buffer.data(), 9.0608011534336E15));
  ASSERT_STREQ("4.708356024711512E18", d2s_test_helper(buffer.data(), 4.708356024711512E18));
  ASSERT_STREQ("9.409340012568248E18", d2s_test_helper(buffer.data(), 9.409340012568248E18));
  ASSERT_STREQ("1.2345678E0", d2s_test_helper(buffer.data(), 1.2345678));
}

TEST(D2sTest, LooksLikePow5) {
  std::array<char, 25> buffer;
  // These numbers have a mantissa that is a multiple of the largest power of 5 that fits,
  // and an exponent that causes the computation for q to result in 22, which is a corner
  // case for Ryu.
  ASSERT_STREQ("5.764607523034235E39", d2s_test_helper(buffer.data(), int64Bits2Double(0x4830F0CF064DD592)));
  ASSERT_STREQ("1.152921504606847E40", d2s_test_helper(buffer.data(), int64Bits2Double(0x4840F0CF064DD592)));
  ASSERT_STREQ("2.305843009213694E40", d2s_test_helper(buffer.data(), int64Bits2Double(0x4850F0CF064DD592)));
}

TEST(D2sTest, OutputLength) {
  std::array<char, 25> buffer;
  ASSERT_STREQ("1E0", d2s_test_helper(buffer.data(), 1)); // already tested in Basic
  ASSERT_STREQ("1.2E0", d2s_test_helper(buffer.data(), 1.2));
  ASSERT_STREQ("1.23E0", d2s_test_helper(buffer.data(), 1.23));
  ASSERT_STREQ("1.234E0", d2s_test_helper(buffer.data(), 1.234));
  ASSERT_STREQ("1.2345E0", d2s_test_helper(buffer.data(), 1.2345));
  ASSERT_STREQ("1.23456E0", d2s_test_helper(buffer.data(), 1.23456));
  ASSERT_STREQ("1.234567E0", d2s_test_helper(buffer.data(), 1.234567));
  ASSERT_STREQ("1.2345678E0", d2s_test_helper(buffer.data(), 1.2345678)); // already tested in Regression
  ASSERT_STREQ("1.23456789E0", d2s_test_helper(buffer.data(), 1.23456789));
  ASSERT_STREQ("1.234567895E0", d2s_test_helper(buffer.data(), 1.234567895)); // 1.234567890 would be trimmed
  ASSERT_STREQ("1.2345678901E0", d2s_test_helper(buffer.data(), 1.2345678901));
  ASSERT_STREQ("1.23456789012E0", d2s_test_helper(buffer.data(), 1.23456789012));
  ASSERT_STREQ("1.234567890123E0", d2s_test_helper(buffer.data(), 1.234567890123));
  ASSERT_STREQ("1.2345678901234E0", d2s_test_helper(buffer.data(), 1.2345678901234));
  ASSERT_STREQ("1.23456789012345E0", d2s_test_helper(buffer.data(), 1.23456789012345));
  ASSERT_STREQ("1.234567890123456E0", d2s_test_helper(buffer.data(), 1.234567890123456));
  ASSERT_STREQ("1.2345678901234567E0", d2s_test_helper(buffer.data(), 1.2345678901234567));

  // Test 32-bit chunking
  ASSERT_STREQ("4.294967294E0", d2s_test_helper(buffer.data(), 4.294967294)); // 2^32 - 2
  ASSERT_STREQ("4.294967295E0", d2s_test_helper(buffer.data(), 4.294967295)); // 2^32 - 1
  ASSERT_STREQ("4.294967296E0", d2s_test_helper(buffer.data(), 4.294967296)); // 2^32
  ASSERT_STREQ("4.294967297E0", d2s_test_helper(buffer.data(), 4.294967297)); // 2^32 + 1
  ASSERT_STREQ("4.294967298E0", d2s_test_helper(buffer.data(), 4.294967298)); // 2^32 + 2
}

// Test min, max shift values in shiftright128
TEST(D2sTest, MinMaxShift) {
  std::array<char, 25> buffer;
  const uint64_t maxMantissa = ((std::uint64_t)1 << 53) - 1;

  // 32-bit opt-size=0:  49 <= dist <= 50
  // 32-bit opt-size=1:  30 <= dist <= 50
  // 64-bit opt-size=0:  50 <= dist <= 50
  // 64-bit opt-size=1:  30 <= dist <= 50
  ASSERT_STREQ("1.7800590868057611E-307", d2s_test_helper(buffer.data(), ieeeParts2Double(false, 4, 0)));
  // 32-bit opt-size=0:  49 <= dist <= 49
  // 32-bit opt-size=1:  28 <= dist <= 49
  // 64-bit opt-size=0:  50 <= dist <= 50
  // 64-bit opt-size=1:  28 <= dist <= 50
  ASSERT_STREQ("2.8480945388892175E-306", d2s_test_helper(buffer.data(), ieeeParts2Double(false, 6, maxMantissa)));
  // 32-bit opt-size=0:  52 <= dist <= 53
  // 32-bit opt-size=1:   2 <= dist <= 53
  // 64-bit opt-size=0:  53 <= dist <= 53
  // 64-bit opt-size=1:   2 <= dist <= 53
  ASSERT_STREQ("2.446494580089078E-296", d2s_test_helper(buffer.data(), ieeeParts2Double(false, 41, 0)));
  // 32-bit opt-size=0:  52 <= dist <= 52
  // 32-bit opt-size=1:   2 <= dist <= 52
  // 64-bit opt-size=0:  53 <= dist <= 53
  // 64-bit opt-size=1:   2 <= dist <= 53
  ASSERT_STREQ("4.8929891601781557E-296", d2s_test_helper(buffer.data(), ieeeParts2Double(false, 40, maxMantissa)));

  // 32-bit opt-size=0:  57 <= dist <= 58
  // 32-bit opt-size=1:  57 <= dist <= 58
  // 64-bit opt-size=0:  58 <= dist <= 58
  // 64-bit opt-size=1:  58 <= dist <= 58
  ASSERT_STREQ("1.8014398509481984E16", d2s_test_helper(buffer.data(), ieeeParts2Double(false, 1077, 0)));
  // 32-bit opt-size=0:  57 <= dist <= 57
  // 32-bit opt-size=1:  57 <= dist <= 57
  // 64-bit opt-size=0:  58 <= dist <= 58
  // 64-bit opt-size=1:  58 <= dist <= 58
  ASSERT_STREQ("3.6028797018963964E16", d2s_test_helper(buffer.data(), ieeeParts2Double(false, 1076, maxMantissa)));
  // 32-bit opt-size=0:  51 <= dist <= 52
  // 32-bit opt-size=1:  51 <= dist <= 59
  // 64-bit opt-size=0:  52 <= dist <= 52
  // 64-bit opt-size=1:  52 <= dist <= 59
  ASSERT_STREQ("2.900835519859558E-216", d2s_test_helper(buffer.data(), ieeeParts2Double(false, 307, 0)));
  // 32-bit opt-size=0:  51 <= dist <= 51
  // 32-bit opt-size=1:  51 <= dist <= 59
  // 64-bit opt-size=0:  52 <= dist <= 52
  // 64-bit opt-size=1:  52 <= dist <= 59
  ASSERT_STREQ("5.801671039719115E-216", d2s_test_helper(buffer.data(), ieeeParts2Double(false, 306, maxMantissa)));

  // https://github.com/ulfjack/ryu/commit/19e44d16d80236f5de25800f56d82606d1be00b9#commitcomment-30146483
  // 32-bit opt-size=0:  49 <= dist <= 49
  // 32-bit opt-size=1:  44 <= dist <= 49
  // 64-bit opt-size=0:  50 <= dist <= 50
  // 64-bit opt-size=1:  44 <= dist <= 50
  ASSERT_STREQ("3.196104012172126E-27", d2s_test_helper(buffer.data(), ieeeParts2Double(false, 934, 0x000FA7161A4D6E0Cu)));
}

TEST(D2sTest, SmallIntegers) {
  std::array<char, 25> buffer;
  ASSERT_STREQ("9.007199254740991E15", d2s_test_helper(buffer.data(), 9007199254740991.0)); // 2^53-1
  ASSERT_STREQ("9.007199254740992E15", d2s_test_helper(buffer.data(), 9007199254740992.0)); // 2^53

  ASSERT_STREQ("1E0", d2s_test_helper(buffer.data(), 1.0e+0));
  ASSERT_STREQ("1.2E1", d2s_test_helper(buffer.data(), 1.2e+1));
  ASSERT_STREQ("1.23E2", d2s_test_helper(buffer.data(), 1.23e+2));
  ASSERT_STREQ("1.234E3", d2s_test_helper(buffer.data(), 1.234e+3));
  ASSERT_STREQ("1.2345E4", d2s_test_helper(buffer.data(), 1.2345e+4));
  ASSERT_STREQ("1.23456E5", d2s_test_helper(buffer.data(), 1.23456e+5));
  ASSERT_STREQ("1.234567E6", d2s_test_helper(buffer.data(), 1.234567e+6));
  ASSERT_STREQ("1.2345678E7", d2s_test_helper(buffer.data(), 1.2345678e+7));
  ASSERT_STREQ("1.23456789E8", d2s_test_helper(buffer.data(), 1.23456789e+8));
  ASSERT_STREQ("1.23456789E9", d2s_test_helper(buffer.data(), 1.23456789e+9));
  ASSERT_STREQ("1.234567895E9", d2s_test_helper(buffer.data(), 1.234567895e+9));
  ASSERT_STREQ("1.2345678901E10", d2s_test_helper(buffer.data(), 1.2345678901e+10));
  ASSERT_STREQ("1.23456789012E11", d2s_test_helper(buffer.data(), 1.23456789012e+11));
  ASSERT_STREQ("1.234567890123E12", d2s_test_helper(buffer.data(), 1.234567890123e+12));
  ASSERT_STREQ("1.2345678901234E13", d2s_test_helper(buffer.data(), 1.2345678901234e+13));
  ASSERT_STREQ("1.23456789012345E14", d2s_test_helper(buffer.data(), 1.23456789012345e+14));
  ASSERT_STREQ("1.234567890123456E15", d2s_test_helper(buffer.data(), 1.234567890123456e+15));

  // 10^i
  ASSERT_STREQ("1E0", d2s_test_helper(buffer.data(), 1.0e+0));
  ASSERT_STREQ("1E1", d2s_test_helper(buffer.data(), 1.0e+1));
  ASSERT_STREQ("1E2", d2s_test_helper(buffer.data(), 1.0e+2));
  ASSERT_STREQ("1E3", d2s_test_helper(buffer.data(), 1.0e+3));
  ASSERT_STREQ("1E4", d2s_test_helper(buffer.data(), 1.0e+4));
  ASSERT_STREQ("1E5", d2s_test_helper(buffer.data(), 1.0e+5));
  ASSERT_STREQ("1E6", d2s_test_helper(buffer.data(), 1.0e+6));
  ASSERT_STREQ("1E7", d2s_test_helper(buffer.data(), 1.0e+7));
  ASSERT_STREQ("1E8", d2s_test_helper(buffer.data(), 1.0e+8));
  ASSERT_STREQ("1E9", d2s_test_helper(buffer.data(), 1.0e+9));
  ASSERT_STREQ("1E10", d2s_test_helper(buffer.data(), 1.0e+10));
  ASSERT_STREQ("1E11", d2s_test_helper(buffer.data(), 1.0e+11));
  ASSERT_STREQ("1E12", d2s_test_helper(buffer.data(), 1.0e+12));
  ASSERT_STREQ("1E13", d2s_test_helper(buffer.data(), 1.0e+13));
  ASSERT_STREQ("1E14", d2s_test_helper(buffer.data(), 1.0e+14));
  ASSERT_STREQ("1E15", d2s_test_helper(buffer.data(), 1.0e+15));

  // 10^15 + 10^i
  ASSERT_STREQ("1.000000000000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+0));
  ASSERT_STREQ("1.00000000000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+1));
  ASSERT_STREQ("1.0000000000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+2));
  ASSERT_STREQ("1.000000000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+3));
  ASSERT_STREQ("1.00000000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+4));
  ASSERT_STREQ("1.0000000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+5));
  ASSERT_STREQ("1.000000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+6));
  ASSERT_STREQ("1.00000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+7));
  ASSERT_STREQ("1.0000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+8));
  ASSERT_STREQ("1.000001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+9));
  ASSERT_STREQ("1.00001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+10));
  ASSERT_STREQ("1.0001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+11));
  ASSERT_STREQ("1.001E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+12));
  ASSERT_STREQ("1.01E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+13));
  ASSERT_STREQ("1.1E15", d2s_test_helper(buffer.data(), 1.0e+15 + 1.0e+14));

  // Largest power of 2 <= 10^(i+1)
  ASSERT_STREQ("8E0", d2s_test_helper(buffer.data(), 8.0));
  ASSERT_STREQ("6.4E1", d2s_test_helper(buffer.data(), 64.0));
  ASSERT_STREQ("5.12E2", d2s_test_helper(buffer.data(), 512.0));
  ASSERT_STREQ("8.192E3", d2s_test_helper(buffer.data(), 8192.0));
  ASSERT_STREQ("6.5536E4", d2s_test_helper(buffer.data(), 65536.0));
  ASSERT_STREQ("5.24288E5", d2s_test_helper(buffer.data(), 524288.0));
  ASSERT_STREQ("8.388608E6", d2s_test_helper(buffer.data(), 8388608.0));
  ASSERT_STREQ("6.7108864E7", d2s_test_helper(buffer.data(), 67108864.0));
  ASSERT_STREQ("5.36870912E8", d2s_test_helper(buffer.data(), 536870912.0));
  ASSERT_STREQ("8.589934592E9", d2s_test_helper(buffer.data(), 8589934592.0));
  ASSERT_STREQ("6.8719476736E10", d2s_test_helper(buffer.data(), 68719476736.0));
  ASSERT_STREQ("5.49755813888E11", d2s_test_helper(buffer.data(), 549755813888.0));
  ASSERT_STREQ("8.796093022208E12", d2s_test_helper(buffer.data(), 8796093022208.0));
  ASSERT_STREQ("7.0368744177664E13", d2s_test_helper(buffer.data(), 70368744177664.0));
  ASSERT_STREQ("5.62949953421312E14", d2s_test_helper(buffer.data(), 562949953421312.0));
  ASSERT_STREQ("9.007199254740992E15", d2s_test_helper(buffer.data(), 9007199254740992.0));

  // 1000 * (Largest power of 2 <= 10^(i+1))
  ASSERT_STREQ("8E3", d2s_test_helper(buffer.data(), 8.0e+3));
  ASSERT_STREQ("6.4E4", d2s_test_helper(buffer.data(), 64.0e+3));
  ASSERT_STREQ("5.12E5", d2s_test_helper(buffer.data(), 512.0e+3));
  ASSERT_STREQ("8.192E6", d2s_test_helper(buffer.data(), 8192.0e+3));
  ASSERT_STREQ("6.5536E7", d2s_test_helper(buffer.data(), 65536.0e+3));
  ASSERT_STREQ("5.24288E8", d2s_test_helper(buffer.data(), 524288.0e+3));
  ASSERT_STREQ("8.388608E9", d2s_test_helper(buffer.data(), 8388608.0e+3));
  ASSERT_STREQ("6.7108864E10", d2s_test_helper(buffer.data(), 67108864.0e+3));
  ASSERT_STREQ("5.36870912E11", d2s_test_helper(buffer.data(), 536870912.0e+3));
  ASSERT_STREQ("8.589934592E12", d2s_test_helper(buffer.data(), 8589934592.0e+3));
  ASSERT_STREQ("6.8719476736E13", d2s_test_helper(buffer.data(), 68719476736.0e+3));
  ASSERT_STREQ("5.49755813888E14", d2s_test_helper(buffer.data(), 549755813888.0e+3));
  ASSERT_STREQ("8.796093022208E15", d2s_test_helper(buffer.data(), 8796093022208.0e+3));
}

} // ryu

} // detail
BOOST_JSON_NS_END
