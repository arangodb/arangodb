////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for conversions.c
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/conversions.h"

#include <cstring>


// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief convert an int8_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_INT8(value, expectedValue, buffer)            \
  actualLength = TRI_StringInt8InPlace((int8_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                  \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint8_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT8(value, expectedValue, buffer)             \
  actualLength = TRI_StringUInt8InPlace((uint8_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                    \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert an int16_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_INT16(value, expectedValue, buffer)             \
  actualLength = TRI_StringInt16InPlace((int16_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                    \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint16_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT16(value, expectedValue, buffer)              \
  actualLength = TRI_StringUInt16InPlace((uint16_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                      \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert an int32_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_INT32(value, expectedValue, buffer)             \
  actualLength = TRI_StringInt32InPlace((int32_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                    \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint32_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT32(value, expectedValue, buffer)              \
  actualLength = TRI_StringUInt32InPlace((uint32_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                      \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert an int64_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_INT64(value, expectedValue, buffer)             \
  actualLength = TRI_StringInt64InPlace((int64_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                    \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint64_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT64(value, expectedValue, buffer)              \
  actualLength = TRI_StringUInt64InPlace((uint64_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                      \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint32_t to hex
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT32_HEX(value, expectedValue, buffer)             \
  actualLength = TRI_StringUInt32HexInPlace((uint32_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                         \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint64_t to hex
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT64_HEX(value, expectedValue, buffer)             \
  actualLength = TRI_StringUInt64HexInPlace((uint64_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                         \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint32_t to octal
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT32_OCTAL(value, expectedValue, buffer)             \
  actualLength = TRI_StringUInt32OctalInPlace((uint32_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                           \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint64_t to octal
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT64_OCTAL(value, expectedValue, buffer)             \
  actualLength = TRI_StringUInt64OctalInPlace((uint64_t)value, (char*)&buffer); \
  EXPECT_EQ(actualLength, strlen(expectedValue));                           \
  EXPECT_EQ(std::string(buffer), std::string(expectedValue));

////////////////////////////////////////////////////////////////////////////////
/// @brief test int8_t conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_int8) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_INT8(0, "0", buffer)
  CHECK_CONVERSION_INT8(1, "1", buffer)
  CHECK_CONVERSION_INT8(10, "10", buffer)
  CHECK_CONVERSION_INT8(123, "123", buffer)
  CHECK_CONVERSION_INT8(126, "126", buffer)
  CHECK_CONVERSION_INT8(INT8_MAX, "127", buffer)

  CHECK_CONVERSION_INT8(-1, "-1", buffer)
  CHECK_CONVERSION_INT8(-10, "-10", buffer)
  CHECK_CONVERSION_INT8(-123, "-123", buffer)
  CHECK_CONVERSION_INT8(-126, "-126", buffer)
  CHECK_CONVERSION_INT8(-127, "-127", buffer)
  CHECK_CONVERSION_INT8(INT8_MIN, "-128", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint8_t conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_uint8) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_UINT8(0, "0", buffer)
  CHECK_CONVERSION_UINT8(1, "1", buffer)
  CHECK_CONVERSION_UINT8(10, "10", buffer)
  CHECK_CONVERSION_UINT8(127, "127", buffer)
  CHECK_CONVERSION_UINT8(254, "254", buffer)
  CHECK_CONVERSION_UINT8(UINT8_MAX, "255", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test int16_t conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_int16) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_INT16(0, "0", buffer)
  CHECK_CONVERSION_INT16(1, "1", buffer)
  CHECK_CONVERSION_INT16(10, "10", buffer)
  CHECK_CONVERSION_INT16(123, "123", buffer)
  CHECK_CONVERSION_INT16(1234, "1234", buffer)
  CHECK_CONVERSION_INT16(12345, "12345", buffer)
  CHECK_CONVERSION_INT16(32766, "32766", buffer)
  CHECK_CONVERSION_INT16(INT16_MAX, "32767", buffer)

  CHECK_CONVERSION_INT16(-1, "-1", buffer)
  CHECK_CONVERSION_INT16(-10, "-10", buffer)
  CHECK_CONVERSION_INT16(-123, "-123", buffer)
  CHECK_CONVERSION_INT16(-1234, "-1234", buffer)
  CHECK_CONVERSION_INT16(-12345, "-12345", buffer)
  CHECK_CONVERSION_INT16(-32766, "-32766", buffer)
  CHECK_CONVERSION_INT16(-32767, "-32767", buffer)
  CHECK_CONVERSION_INT16(INT16_MIN, "-32768", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint16_t conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_uint16) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_UINT16(0, "0", buffer)
  CHECK_CONVERSION_UINT16(1, "1", buffer)
  CHECK_CONVERSION_UINT16(10, "10", buffer)
  CHECK_CONVERSION_UINT16(32767, "32767", buffer)
  CHECK_CONVERSION_UINT16(32768, "32768", buffer)
  CHECK_CONVERSION_UINT16(65534, "65534", buffer)
  CHECK_CONVERSION_UINT16(UINT16_MAX, "65535", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test int32_t conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_int32) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_INT32(0L, "0", buffer)
  CHECK_CONVERSION_INT32(1L, "1", buffer)
  CHECK_CONVERSION_INT32(10L, "10", buffer)
  CHECK_CONVERSION_INT32(100000L, "100000", buffer)
  CHECK_CONVERSION_INT32(10000009L, "10000009", buffer)
  CHECK_CONVERSION_INT32(2147483646L, "2147483646", buffer)
  CHECK_CONVERSION_INT32(2147483647L, "2147483647", buffer)
  CHECK_CONVERSION_INT32(INT32_MAX, "2147483647", buffer)

  CHECK_CONVERSION_INT32(-1L, "-1", buffer)
  CHECK_CONVERSION_INT32(-10L, "-10", buffer)
  CHECK_CONVERSION_INT32(-100000L, "-100000", buffer)
  CHECK_CONVERSION_INT32(-10000009L, "-10000009", buffer)
  CHECK_CONVERSION_INT32(-2147483646L, "-2147483646", buffer)
  CHECK_CONVERSION_INT32(-2147483647L, "-2147483647", buffer)
  CHECK_CONVERSION_INT32(INT32_MIN, "-2147483648", buffer)

  CHECK_CONVERSION_INT32(65535L, "65535", buffer)
  CHECK_CONVERSION_INT32(65536L, "65536", buffer)
  CHECK_CONVERSION_INT32(-65535L, "-65535", buffer)
  CHECK_CONVERSION_INT32(-65536L, "-65536", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint32_t conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_uint32) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_UINT32(0UL, "0", buffer)
  CHECK_CONVERSION_UINT32(1UL, "1", buffer)
  CHECK_CONVERSION_UINT32(10UL, "10", buffer)
  CHECK_CONVERSION_UINT32(100000UL, "100000", buffer)
  CHECK_CONVERSION_UINT32(10000009UL, "10000009", buffer)
  CHECK_CONVERSION_UINT32(2147483647UL, "2147483647", buffer)
  CHECK_CONVERSION_UINT32(4294967295UL, "4294967295", buffer)
  CHECK_CONVERSION_UINT32(UINT32_MAX, "4294967295", buffer)

  CHECK_CONVERSION_UINT32(65535UL, "65535", buffer)
  CHECK_CONVERSION_UINT32(65536UL, "65536", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test int64_t conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_int64) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_INT64(0LL, "0", buffer)
  CHECK_CONVERSION_INT64(1LL, "1", buffer)
  CHECK_CONVERSION_INT64(10LL, "10", buffer)
  CHECK_CONVERSION_INT64(100000LL, "100000", buffer)
  CHECK_CONVERSION_INT64(10000009LL, "10000009", buffer)
  CHECK_CONVERSION_INT64(562949953421311LL, "562949953421311", buffer)
  CHECK_CONVERSION_INT64(9223372036854775807LL, "9223372036854775807", buffer)
  CHECK_CONVERSION_INT64(INT64_MAX, "9223372036854775807", buffer)

  CHECK_CONVERSION_INT64(-1LL, "-1", buffer)
  CHECK_CONVERSION_INT64(-10LL, "-10", buffer)
  CHECK_CONVERSION_INT64(-100000LL, "-100000", buffer)
  CHECK_CONVERSION_INT64(-10000009LL, "-10000009", buffer)
  CHECK_CONVERSION_INT64(-562949953421311LL, "-562949953421311", buffer)
  CHECK_CONVERSION_INT64(-9223372036854775807LL, "-9223372036854775807", buffer)
  CHECK_CONVERSION_INT64(INT64_MIN, "-9223372036854775808", buffer)

  CHECK_CONVERSION_INT64(2147483647LL, "2147483647", buffer)
  CHECK_CONVERSION_INT64(2147483648LL, "2147483648", buffer)
  CHECK_CONVERSION_INT64(-2147483647LL, "-2147483647", buffer)
  CHECK_CONVERSION_INT64(-2147483648LL, "-2147483648", buffer)
  CHECK_CONVERSION_INT64(-2147483649LL, "-2147483649", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint64_t conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_uint64) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_UINT64(0ULL, "0", buffer)
  CHECK_CONVERSION_UINT64(1ULL, "1", buffer)
  CHECK_CONVERSION_UINT64(10ULL, "10", buffer)
  CHECK_CONVERSION_UINT64(100000ULL, "100000", buffer)
  CHECK_CONVERSION_UINT64(10000009ULL, "10000009", buffer)
  CHECK_CONVERSION_UINT64(2147483647ULL, "2147483647", buffer)
  CHECK_CONVERSION_UINT64(562949953421311ULL, "562949953421311", buffer)
  CHECK_CONVERSION_UINT64(9223372036854775807ULL, "9223372036854775807", buffer)
  CHECK_CONVERSION_UINT64(9223372036854775808ULL, "9223372036854775808", buffer)
  CHECK_CONVERSION_UINT64(18446744073709551614ULL, "18446744073709551614", buffer)
  CHECK_CONVERSION_UINT64(UINT64_MAX, "18446744073709551615", buffer)

  CHECK_CONVERSION_UINT64(2147483647ULL, "2147483647", buffer)
  CHECK_CONVERSION_UINT64(2147483648ULL, "2147483648", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint32_t hex conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_uint32_hex) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_UINT32_HEX(0UL, "0", buffer)
  CHECK_CONVERSION_UINT32_HEX(1UL, "1", buffer)
  CHECK_CONVERSION_UINT32_HEX(9UL, "9", buffer)
  CHECK_CONVERSION_UINT32_HEX(10UL, "A", buffer)
  CHECK_CONVERSION_UINT32_HEX(128UL, "80", buffer)
  CHECK_CONVERSION_UINT32_HEX(3254UL, "CB6", buffer)
  CHECK_CONVERSION_UINT32_HEX(65535UL, "FFFF", buffer)
  CHECK_CONVERSION_UINT32_HEX(65536UL, "10000", buffer)
  CHECK_CONVERSION_UINT32_HEX(68863UL, "10CFF", buffer)
  CHECK_CONVERSION_UINT32_HEX(465765536ULL, "1BC304A0", buffer)
  CHECK_CONVERSION_UINT32_HEX(UINT32_MAX, "FFFFFFFF", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint64_t hex conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_uint64_hex) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_UINT64_HEX(0ULL, "0", buffer)
  CHECK_CONVERSION_UINT64_HEX(1ULL, "1", buffer)
  CHECK_CONVERSION_UINT64_HEX(9ULL, "9", buffer)
  CHECK_CONVERSION_UINT64_HEX(10ULL, "A", buffer)
  CHECK_CONVERSION_UINT64_HEX(128ULL, "80", buffer)
  CHECK_CONVERSION_UINT64_HEX(3254ULL, "CB6", buffer)
  CHECK_CONVERSION_UINT64_HEX(65535ULL, "FFFF", buffer)
  CHECK_CONVERSION_UINT64_HEX(65536ULL, "10000", buffer)
  CHECK_CONVERSION_UINT64_HEX(68863ULL, "10CFF", buffer)
  CHECK_CONVERSION_UINT64_HEX(465765536ULL, "1BC304A0", buffer)
  CHECK_CONVERSION_UINT64_HEX(47634665765536ULL, "2B52CF54FAA0", buffer)
  CHECK_CONVERSION_UINT64_HEX(8668398959769325ULL, "1ECBDCE8C4B6ED", buffer)
  CHECK_CONVERSION_UINT64_HEX(UINT64_MAX, "FFFFFFFFFFFFFFFF", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint32_t octal conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_uint32_octal) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_UINT32_OCTAL(0UL, "0", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(1UL, "1", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(8UL, "10", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(9UL, "11", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(10UL, "12", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(128UL, "200", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(257UL, "401", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(4096UL, "10000", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(32683UL, "77653", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(65535UL, "177777", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(65536UL, "200000", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(2147483648UL, "20000000000", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(4294967294UL, "37777777776", buffer)
  CHECK_CONVERSION_UINT32_OCTAL(UINT32_MAX, "37777777777", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint64_t octal conversion
////////////////////////////////////////////////////////////////////////////////

TEST(CConversionsTest, tst_uint64_octal) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_UINT64_OCTAL(0ULL, "0", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(1ULL, "1", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(8ULL, "10", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(9ULL, "11", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(10ULL, "12", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(128ULL, "200", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(257ULL, "401", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(4096ULL, "10000", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(32683ULL, "77653", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(65535ULL, "177777", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(65536ULL, "200000", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(2147483648UL, "20000000000", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(4294967294UL, "37777777776", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(96949632432ULL, "1322251376660", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(4611686018427387903ULL, "377777777777777777777", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(9694963243245737662ULL, "1032133333204010313276", buffer)
  CHECK_CONVERSION_UINT64_OCTAL(UINT64_MAX, "1777777777777777777777", buffer);
}
