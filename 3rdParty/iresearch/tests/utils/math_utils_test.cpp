////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "utils/math_utils.hpp"

namespace math = iresearch::math;

TEST(math_utils_test, is_power2) {
  using namespace iresearch::math;

  //static_assert(!is_power2(0), "Invalid answer"); // 0 gives false negative
  static_assert(is_power2(1), "Invalid answer");
  static_assert(is_power2(2), "Invalid answer");
  static_assert(!is_power2(3), "Invalid answer");
  static_assert(is_power2(4), "Invalid answer");
  static_assert(!is_power2(999), "Invalid answer");
  static_assert(is_power2(1024), "Invalid answer");
  static_assert(is_power2(UINT64_C(1) << 63), "Invalid answer");
  static_assert(!is_power2(std::numeric_limits<size_t>::max()), "Invalid answer");
}

TEST(math_utils_test, roundup_power2) {
  ASSERT_EQ(0, math::roundup_power2(0));
  ASSERT_EQ(1, math::roundup_power2(1));
  ASSERT_EQ(2, math::roundup_power2(2));
  ASSERT_EQ(4, math::roundup_power2(3));
  ASSERT_EQ(4, math::roundup_power2(4));
  ASSERT_EQ(8, math::roundup_power2(5));
  ASSERT_EQ(16, math::roundup_power2(11));
  ASSERT_EQ(1024, math::roundup_power2(900));
  ASSERT_EQ(std::numeric_limits<size_t>::max(), math::roundup_power2(std::numeric_limits<size_t>::max())-1); // round to the max possible value
  ASSERT_EQ(0, math::roundup_power2(std::numeric_limits<size_t>::max()));
}

TEST(math_utils_test, log2_floor_32) {
  ASSERT_EQ(0, math::log2_floor_32(1));
  ASSERT_EQ(1, math::log2_floor_32(2));
  ASSERT_EQ(1, math::log2_floor_32(3));
  ASSERT_EQ(2, math::log2_floor_32(4));
  ASSERT_EQ(2, math::log2_floor_32(6));
  ASSERT_EQ(3, math::log2_floor_32(8));
  ASSERT_EQ(9, math::log2_floor_32(999));
  ASSERT_EQ(10, math::log2_floor_32(1024));
  ASSERT_EQ(10, math::log2_floor_32(1025));
  ASSERT_EQ(31, math::log2_floor_32(std::numeric_limits<uint32_t>::max()));
}

TEST(math_utils_test, log2_floor_64) {
  ASSERT_EQ(0, math::log2_floor_64(UINT64_C(1)));
  ASSERT_EQ(1, math::log2_floor_64(UINT64_C(2)));
  ASSERT_EQ(1, math::log2_floor_64(UINT64_C(3)));
  ASSERT_EQ(2, math::log2_floor_64(UINT64_C(4)));
  ASSERT_EQ(2, math::log2_floor_64(UINT64_C(6)));
  ASSERT_EQ(3, math::log2_floor_64(UINT64_C(8)));
  ASSERT_EQ(9, math::log2_floor_64(UINT64_C(999)));
  ASSERT_EQ(10, math::log2_floor_64(UINT64_C(1024)));
  ASSERT_EQ(10, math::log2_floor_64(UINT64_C(1025)));
  ASSERT_EQ(31, math::log2_floor_64(std::numeric_limits<uint32_t>::max()));
  ASSERT_EQ(32, math::log2_floor_64(UINT64_C(1) << 32));
  ASSERT_EQ(32, math::log2_floor_64(UINT64_C(1) + (UINT64_C(1) << 32)));
  ASSERT_EQ(63, math::log2_floor_64(std::numeric_limits<uint64_t>::max()));
}

TEST(math_utils_test, log) {
  ASSERT_EQ(0, math::log(1,2));
  ASSERT_EQ(1, math::log(235,235));
  ASSERT_EQ(2, math::log(49,7));
  ASSERT_EQ(3, math::log(8,2));
  ASSERT_EQ(4, math::log(12341, 7)); 
  ASSERT_EQ(6, math::log(34563456, 17)); 
  ASSERT_EQ(31, math::log(std::numeric_limits<uint32_t>::max(), 2));
}

TEST(math_utils_test, log2_32) {
  ASSERT_EQ(0, math::log2_32(1));
  ASSERT_EQ(1, math::log2_32(2));
  ASSERT_EQ(1, math::log2_32(3));
  ASSERT_EQ(2, math::log2_32(4));
  ASSERT_EQ(7, math::log2_32(128));
  ASSERT_EQ(10, math::log2_32(1025));
  ASSERT_EQ(31, math::log2_32(std::numeric_limits<uint32_t>::max()));
}

TEST(math_utils_test, log2_64) {
  ASSERT_EQ(0, math::log2_64(1));
  ASSERT_EQ(1, math::log2_64(2));
  ASSERT_EQ(1, math::log2_64(3));
  ASSERT_EQ(2, math::log2_64(4));
  ASSERT_EQ(7, math::log2_64(128));
  ASSERT_EQ(10, math::log2_64(1025));
  ASSERT_EQ(31, math::log2_64(std::numeric_limits<uint32_t>::max()));
  ASSERT_EQ(63, math::log2_64(std::numeric_limits<uint64_t>::max()));
}

TEST(math_utils_test, clz32) {
  ASSERT_EQ(31, math::math_traits<uint32_t>::clz(UINT32_C(1)));
  ASSERT_EQ(10, math::math_traits<uint32_t>::clz(UINT32_C(0x200000)));
  ASSERT_EQ(10, math::math_traits<uint32_t>::clz(UINT32_C(0x311480)));
  ASSERT_EQ(6, math::math_traits<uint32_t>::clz(UINT32_C(0x2122000)));
  ASSERT_EQ(0, math::math_traits<uint32_t>::clz(UINT32_C(0xFFFFFFFF)));
}

TEST(math_utils_test, ctz32) {
  ASSERT_EQ(0, math::math_traits<uint32_t>::ctz(UINT32_C(1)));
  ASSERT_EQ(21, math::math_traits<uint32_t>::ctz(UINT32_C(0x200000)));
  ASSERT_EQ(7, math::math_traits<uint32_t>::ctz(UINT32_C(0x311480)));
  ASSERT_EQ(0, math::math_traits<uint32_t>::ctz(UINT32_C(0xFFFFFFFF)));
  ASSERT_EQ(13, math::math_traits<uint32_t>::ctz(UINT32_C(0x2122000)));
  ASSERT_EQ(31, math::math_traits<uint32_t>::ctz(UINT32_C(0x80000000)));
}

TEST(math_utils_test, clz64) {
  ASSERT_EQ(63, math::math_traits<uint64_t>::clz(UINT64_C(1)));
  ASSERT_EQ(42, math::math_traits<uint64_t>::clz(UINT64_C(0x200000)));
  ASSERT_EQ(42, math::math_traits<uint64_t>::clz(UINT64_C(0x311480)));
  ASSERT_EQ(32, math::math_traits<uint64_t>::clz(UINT64_C(0xFFFFFFFF)));
  ASSERT_EQ(10, math::math_traits<uint64_t>::clz(UINT64_C(0x29008000000000)));
  ASSERT_EQ(0, math::math_traits<uint64_t>::clz(UINT64_C(0xFFFFFFFFFFFFFFFF)));
}

TEST(math_utils_test, ctz64) {
  ASSERT_EQ(0, math::math_traits<uint64_t>::ctz(UINT64_C(1)));
  ASSERT_EQ(21, math::math_traits<uint64_t>::ctz(UINT64_C(0x200000)));
  ASSERT_EQ(7, math::math_traits<uint64_t>::ctz(UINT64_C(0x311480)));
  ASSERT_EQ(0, math::math_traits<uint64_t>::ctz(UINT64_C(0xFFFFFFFF)));
  ASSERT_EQ(0, math::math_traits<uint64_t>::ctz(UINT64_C(0xFFFFFFFFFFFFFFFF)));
  ASSERT_EQ(31, math::math_traits<uint64_t>::ctz(UINT64_C(0x80000000)));
  ASSERT_EQ(39, math::math_traits<uint64_t>::ctz(UINT64_C(0x29008000000000)));
  ASSERT_EQ(63, math::math_traits<uint64_t>::ctz(UINT64_C(0x8000000000000000)));
}

TEST(math_utils, popcnt) {
  {
    const uint32_t value = 0;
    ASSERT_EQ(0, math::popcnt(&value, sizeof(uint32_t)));
  }
  
  {
    const uint32_t value = 1;
    ASSERT_EQ(1, math::popcnt(&value, sizeof(uint32_t)));
  }
  
  {
    const uint32_t value = 32768;
    ASSERT_EQ(0, math::popcnt(&value, 1)); // number of 1 in the 1st byte
    ASSERT_EQ(1, math::popcnt(&value, 2));
    ASSERT_EQ(1, math::popcnt(&value, sizeof(uint32_t)));
  }
 
  {
    const uint32_t value = 2863311530;
    ASSERT_EQ(16, math::popcnt(&value, sizeof(uint32_t)));
  }
 
  {
    const uint32_t value = std::numeric_limits<uint32_t>::max();
    ASSERT_EQ(8, math::popcnt(&value, 1)); // number of 1 in the 1st byte
    ASSERT_EQ(16, math::popcnt(&value, 2));
    ASSERT_EQ(24, math::popcnt(&value, 3));
    ASSERT_EQ(32, math::popcnt(&value, sizeof(uint32_t)));
  }
  
  {
    const uint64_t value = std::numeric_limits<uint64_t>::max();
    ASSERT_EQ(8, math::popcnt(&value, 1)); // number of 1 in the 1st byte
    ASSERT_EQ(16, math::popcnt(&value, 2));
    ASSERT_EQ(24, math::popcnt(&value, 3));
    ASSERT_EQ(32, math::popcnt(&value, 4));
    ASSERT_EQ(64, math::popcnt(&value, sizeof(uint64_t)));
  }
} 

TEST(math_utils, popcnt32) {
  ASSERT_EQ(0, math::popcnt32(0));
  ASSERT_EQ(1, math::popcnt32(1));
  ASSERT_EQ(16, math::popcnt32(2863311530));
  ASSERT_EQ(32, math::popcnt32(std::numeric_limits<uint32_t>::max()));
  
  ASSERT_EQ(0, math::pop32(0));
  ASSERT_EQ(1, math::pop32(1));
  ASSERT_EQ(16, math::pop32(2863311530));
  ASSERT_EQ(32, math::pop32(std::numeric_limits<uint32_t>::max()));
}

TEST(math_utils, popcnt64) {
  ASSERT_EQ(0, math::popcnt64(0));
  ASSERT_EQ(1, math::popcnt64(1));
  ASSERT_EQ(16, math::popcnt64(2863311530));
  ASSERT_EQ(32, math::popcnt64(std::numeric_limits<uint32_t>::max()));
  ASSERT_EQ(64, math::popcnt64(std::numeric_limits<uint64_t>::max()));

  ASSERT_EQ(0, math::pop64(0));
  ASSERT_EQ(1, math::pop64(1));
  ASSERT_EQ(16, math::pop64(2863311530));
  ASSERT_EQ(32, math::pop64(std::numeric_limits<uint32_t>::max()));
  ASSERT_EQ(64, math::pop64(std::numeric_limits<uint64_t>::max()));
}

TEST(math_utils, clz32) {
  ASSERT_EQ(31, math::clz32(1));
  ASSERT_EQ(9, math::clz32(4333568));
  ASSERT_EQ(0, math::clz32(std::numeric_limits<uint32_t>::max()));
}

TEST(math_utils, div_ceil32) {
  ASSERT_EQ(7, math::div_ceil32(49,7));
  ASSERT_EQ(7, math::div_ceil32(48,7));
  ASSERT_EQ(6, math::div_ceil32(41,7));
  ASSERT_EQ(6, math::div_ceil32(42,7));
  ASSERT_EQ(1, math::div_ceil32(2,2));
  ASSERT_EQ(2, math::div_ceil32(3,2));
  ASSERT_EQ(1, math::div_ceil32(5,7));
  ASSERT_EQ(0, math::div_ceil32(0,7));
  ASSERT_EQ(0, math::div_ceil32(0,1));
  ASSERT_EQ(7, math::div_ceil32(7,1));
  ASSERT_EQ(1, math::div_ceil32(std::numeric_limits<uint32_t>::max()/2,std::numeric_limits<uint32_t>::max()/2));
  ASSERT_EQ(1, math::div_ceil32(std::numeric_limits<uint32_t>::max()/2,std::numeric_limits<uint32_t>::max()/2));
  ASSERT_EQ(1, math::div_ceil32(-1+std::numeric_limits<uint32_t>::max()/2,1+std::numeric_limits<uint32_t>::max()/2));
}

TEST(math_utils, div_ceil64) {
  ASSERT_EQ(7, math::div_ceil64(49,7));
  ASSERT_EQ(7, math::div_ceil64(48,7));
  ASSERT_EQ(6, math::div_ceil64(41,7));
  ASSERT_EQ(6, math::div_ceil64(42,7));
  ASSERT_EQ(1, math::div_ceil64(2,2));
  ASSERT_EQ(2, math::div_ceil64(3,2));
  ASSERT_EQ(1, math::div_ceil64(5,7));
  ASSERT_EQ(0, math::div_ceil64(0,7));
  ASSERT_EQ(0, math::div_ceil64(0,1));
  ASSERT_EQ(7, math::div_ceil64(7,1));
  ASSERT_EQ(1, math::div_ceil64(std::numeric_limits<uint64_t>::max()/2,std::numeric_limits<uint64_t>::max()/2));
  ASSERT_EQ(1, math::div_ceil64(-1+std::numeric_limits<uint64_t>::max()/2,1+std::numeric_limits<uint64_t>::max()/2));
}

TEST(math_utils, ceil32) {
  ASSERT_EQ(41, math::ceil32(41, 1));
  ASSERT_EQ(0, math::ceil32(0, 7));
  ASSERT_EQ(42, math::ceil32(41, 7));
  ASSERT_EQ(42, math::ceil32(42, 7));
  ASSERT_EQ(49, math::ceil32(43, 7));
  ASSERT_EQ(uint32_t(std::numeric_limits<uint32_t>::max()-3), math::ceil32(std::numeric_limits<uint32_t>::max()-3, 2));
  ASSERT_EQ(uint32_t(std::numeric_limits<uint32_t>::max()-3), math::ceil32(std::numeric_limits<uint32_t>::max()-4, 2));
}

TEST(math_utils, ceil64) {
  ASSERT_EQ(41, math::ceil64(41, 1));
  ASSERT_EQ(0, math::ceil64(0, 7));
  ASSERT_EQ(42, math::ceil64(41, 7));
  ASSERT_EQ(42, math::ceil64(42, 7));
  ASSERT_EQ(49, math::ceil64(43, 7));
  ASSERT_EQ(uint64_t(std::numeric_limits<uint64_t>::max()-3), math::ceil64(std::numeric_limits<uint64_t>::max()-3, 2));
  ASSERT_EQ(uint64_t(std::numeric_limits<uint64_t>::max()-3), math::ceil64(std::numeric_limits<uint64_t>::max()-4, 2));
}
