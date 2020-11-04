////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/NumberUtils.h"

#include "gtest/gtest.h"

using namespace arangodb;

template<typename T>
static void test(bool shouldBeValid, std::string const& value) {
  bool valid;
  T result = arangodb::NumberUtils::atoi<T>(value.c_str(), value.c_str() + value.size(), valid);
  ASSERT_EQ(shouldBeValid, valid);
  if (shouldBeValid) {
    ASSERT_EQ(value, std::to_string(result));
    T result2 = arangodb::NumberUtils::atoi_unchecked<T>(value.c_str(), value.c_str() + value.size());
    ASSERT_EQ(value, std::to_string(result2));
  }
}

template<typename T>
static void test(T expected, std::string const& value) {
  bool valid;
  T result = arangodb::NumberUtils::atoi<T>(value.c_str(), value.c_str() + value.size(), valid);
  ASSERT_TRUE(valid);
  ASSERT_EQ(expected, result);
  
  T result2 = arangodb::NumberUtils::atoi_unchecked<T>(value.c_str(), value.c_str() + value.size());
  ASSERT_EQ(expected, result2);
}

TEST(NumberUtilsTest, testStrangeNumbers) {
  test<int64_t>(int64_t(0), "00"); 
  test<int64_t>(int64_t(0), "00000000000000000000000000000"); 
  test<int64_t>(int64_t(1), "01"); 
  test<int64_t>(int64_t(0), "-0"); 
  test<int64_t>(int64_t(-1), "-01"); 
  test<int64_t>(int64_t(-10), "-010"); 
  test<int64_t>(int64_t(0), "-00000"); 
  test<int64_t>(int64_t(-2), "-000002"); 
  test<int64_t>(int64_t(0), "+0"); 
  test<int64_t>(int64_t(0), "+00"); 
  test<int64_t>(int64_t(10), "+010"); 
  test<int64_t>(int64_t(0), "+00000000"); 
  test<int64_t>(int64_t(2), "+000000002"); 
  test<int64_t>(int64_t(0), "+0000000000000000000000000000000000000000"); 
  test<int64_t>(int64_t(22), "+000000000000000000000000000000000000000022"); 
}

TEST(NumberUtilsTest, testPredefinedConstants) {
  test<int16_t>(INT16_MIN, std::to_string(INT16_MIN)); 
  test<int16_t>(INT16_MAX, std::to_string(INT16_MAX));

  test<int32_t>(INT32_MIN, std::to_string(INT32_MIN)); 
  test<int32_t>(INT32_MAX, std::to_string(INT32_MAX));
   
  test<int64_t>(INT64_MIN, std::to_string(INT64_MIN)); 
  test<int64_t>(INT64_MAX, std::to_string(INT64_MAX)); 

  test<uint8_t>(UINT8_MAX, std::to_string(UINT8_MAX)); 
  test<uint16_t>(UINT16_MAX, std::to_string(UINT16_MAX)); 
  test<uint32_t>(UINT32_MAX, std::to_string(UINT32_MAX)); 
  test<uint64_t>(UINT64_MAX, std::to_string(UINT64_MAX)); 
  
  test<std::size_t>(SIZE_MAX, std::to_string(SIZE_MAX));
}

TEST(NumberUtilsTest, testInvalidChars) {
  test<int64_t>(false, ""); 
  test<int64_t>(false, " "); 
  test<int64_t>(false, "  "); 
  test<int64_t>(false, "1a"); 
  test<int64_t>(false, "11234b"); 
  test<int64_t>(false, "1 "); 
  test<int64_t>(false, "1234 "); 
  test<int64_t>(false, "-"); 
  test<int64_t>(false, "+"); 
  test<int64_t>(false, "- "); 
  test<int64_t>(false, "+ "); 
  test<int64_t>(false, "-11234a"); 
  test<int64_t>(false, "-11234 "); 
  test<int64_t>(false, "o"); 
  test<int64_t>(false, "ooooo"); 
  test<int64_t>(false, "1A2B3C"); 
  test<int64_t>(false, "aaaaa14453"); 
  test<int64_t>(false, "02a"); 
}

TEST(NumberUtilsTest, testInt64OutOfBoundsLow) {
  // out of bounds
  test<int64_t>(false, "-1111111111111111111111111111111111111111111111111111111"); 
  test<int64_t>(false, "-111111111111111111111111111111111111111"); 
  test<int64_t>(false, "-9223372036854775810943"); 
  test<int64_t>(false, "-9223372036854775810"); 
  test<int64_t>(false, "-9223372036854775809"); 
}

TEST(NumberUtilsTest, testInt64InBounds) {
  // in bounds
  test<int64_t>(true,  "-9223372036854775808"); 
  test<int64_t>(true,  "-9223372036854775807");
  test<int64_t>(true,  "-9223372036854775801");
  test<int64_t>(true,  "-9223372036854775800");
  test<int64_t>(true,  "-9223372036854775799");
  test<int64_t>(true,  "-123456789012");
  test<int64_t>(true,  "-999999999");
  test<int64_t>(true,  "-98765543");
  test<int64_t>(true,  "-10000");
  test<int64_t>(true,  "-100");
  test<int64_t>(true,  "-99");
  test<int64_t>(true,  "-9");
  test<int64_t>(true,  "-2");
  test<int64_t>(true,  "-1");
  test<int64_t>(true,  "0");
  test<int64_t>(true,  "1");
  test<int64_t>(true,  "10");
  test<int64_t>(true,  "10000");
  test<int64_t>(true,  "1234567890");
  test<int64_t>(true,  "1844674407370955161");
  test<int64_t>(true,  "9223372036854775799"); 
  test<int64_t>(true,  "9223372036854775800"); 
  test<int64_t>(true,  "9223372036854775806"); 
  test<int64_t>(true,  "9223372036854775807"); 
}
  
TEST(NumberUtilsTest, testInt64OutOfBoundsHigh) {
  // out of bounds
  test<int64_t>(false, "9223372036854775808"); 
  test<int64_t>(false, "9223372036854775809"); 
  test<int64_t>(false, "18446744073709551610");
  test<int64_t>(false, "18446744073709551614");
  test<int64_t>(false, "18446744073709551615");
  test<int64_t>(false, "18446744073709551616");
  test<int64_t>(false, "118446744073709551612");
  test<int64_t>(false, "111111111111111111111111111111");
  test<int64_t>(false, "11111111111111111111111111111111111111111111111111111111111111111");
} 

TEST(NumberUtilsTest, testUint64OutOfBoundsNegative) {
  // out of bounds
  test<uint64_t>(false, "-1111111111111111111111111111111111111111111111111111111111111");
  test<uint64_t>(false, "-1111111111111111111111111111111111111");
  test<uint64_t>(false, "-9223372036854775809"); 
  test<uint64_t>(false, "-9223372036854775808");
  test<uint64_t>(false, "-9223372036854775807");
  test<uint64_t>(false, "-10000");
  test<uint64_t>(false, "-10000");
  test<uint64_t>(false, "-1");
  test<uint64_t>(false, "-0");
}

TEST(NumberUtilsTest, testUint64InBounds) {
  // in bounds
  test<uint64_t>(true,  "0");
  test<uint64_t>(true,  "1");
  test<uint64_t>(true,  "10");
  test<uint64_t>(true,  "10000");
  test<uint64_t>(true,  "1234567890");
  test<uint64_t>(true,  "9223372036854775807"); 
  test<uint64_t>(true,  "9223372036854775808"); 
  test<uint64_t>(true,  "1844674407370955161");
  test<uint64_t>(true,  "18446744073709551610");
  test<uint64_t>(true,  "18446744073709551614");
  test<uint64_t>(true,  "18446744073709551615");
}

TEST(NumberUtilsTest, testUint64OutOfBoundsHigh) {
  // out of bounds
  test<uint64_t>(false, "18446744073709551616");
  test<uint64_t>(false, "118446744073709551612");
  test<uint64_t>(false, "1111111111111111111111111111111111111");
  test<uint64_t>(false, "1111111111111111111111111111111111111111111111111111111111111");
}
