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

#include <boost/test/unit_test.hpp>

#include "BasicsC/conversions.c"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief convert an int8_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_INT8(value, expectedValue, buffer)                    \
  actualLength = TRI_StringInt8InPlace((int8_t) value, (char*) &buffer);       \
  BOOST_CHECK_EQUAL(actualLength, strlen(expectedValue));                      \
  BOOST_CHECK_EQUAL(buffer, expectedValue); 

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint8_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT8(value, expectedValue, buffer)                   \
  actualLength = TRI_StringUInt8InPlace((uint8_t) value, (char*) &buffer);     \
  BOOST_CHECK_EQUAL(actualLength, strlen(expectedValue));                      \
  BOOST_CHECK_EQUAL(buffer, expectedValue); 


////////////////////////////////////////////////////////////////////////////////
/// @brief convert an int16_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_INT16(value, expectedValue, buffer)                   \
  actualLength = TRI_StringInt16InPlace((int16_t) value, (char*) &buffer);     \
  BOOST_CHECK_EQUAL(actualLength, strlen(expectedValue));                      \
  BOOST_CHECK_EQUAL(buffer, expectedValue); 

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint16_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT16(value, expectedValue, buffer)                  \
  actualLength = TRI_StringUInt16InPlace((uint16_t) value, (char*) &buffer);   \
  BOOST_CHECK_EQUAL(actualLength, strlen(expectedValue));                      \
  BOOST_CHECK_EQUAL(buffer, expectedValue); 


////////////////////////////////////////////////////////////////////////////////
/// @brief convert an int32_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_INT32(value, expectedValue, buffer)                   \
  actualLength = TRI_StringInt32InPlace((int32_t) value, (char*) &buffer);     \
  BOOST_CHECK_EQUAL(actualLength, strlen(expectedValue));                      \
  BOOST_CHECK_EQUAL(buffer, expectedValue); 

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint32_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT32(value, expectedValue, buffer)                  \
  actualLength = TRI_StringUInt32InPlace((uint32_t) value, (char*) &buffer);   \
  BOOST_CHECK_EQUAL(actualLength, strlen(expectedValue));                      \
  BOOST_CHECK_EQUAL(buffer, expectedValue); 

////////////////////////////////////////////////////////////////////////////////
/// @brief convert an int64_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_INT64(value, expectedValue, buffer)                   \
  actualLength = TRI_StringInt64InPlace((int64_t) value, (char*) &buffer);     \
  BOOST_CHECK_EQUAL(actualLength, strlen(expectedValue));                      \
  BOOST_CHECK_EQUAL(buffer, expectedValue); 

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uint64_t
////////////////////////////////////////////////////////////////////////////////

#define CHECK_CONVERSION_UINT64(value, expectedValue, buffer)                  \
  actualLength = TRI_StringUInt64InPlace((uint64_t) value, (char*) &buffer);   \
  BOOST_CHECK_EQUAL(actualLength, strlen(expectedValue));                      \
  BOOST_CHECK_EQUAL(buffer, expectedValue); 


// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CConversionsSetup {
  CConversionsSetup () {
    BOOST_TEST_MESSAGE("setup conversions");
  }

  ~CConversionsSetup () {
    BOOST_TEST_MESSAGE("tear-down conversions");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CConversionsTest, CConversionsSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test int8_t conversion
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_int8) {
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
  CHECK_CONVERSION_INT16(INT8_MIN, "-128", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint8_t conversion
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_uint8) {
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

BOOST_AUTO_TEST_CASE (tst_int16) {
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

BOOST_AUTO_TEST_CASE (tst_uint16) {
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

BOOST_AUTO_TEST_CASE (tst_int32) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_INT32(0, "0", buffer)
  CHECK_CONVERSION_INT32(1, "1", buffer)
  CHECK_CONVERSION_INT32(10, "10", buffer)
  CHECK_CONVERSION_INT32(100000, "100000", buffer)
  CHECK_CONVERSION_INT32(10000009, "10000009", buffer)
  CHECK_CONVERSION_INT32(INT32_MAX, "2147483647", buffer)
  
  CHECK_CONVERSION_INT32(-1, "-1", buffer)
  CHECK_CONVERSION_INT32(-10, "-10", buffer)
  CHECK_CONVERSION_INT32(-100000, "-100000", buffer)
  CHECK_CONVERSION_INT32(-10000009, "-10000009", buffer)
  CHECK_CONVERSION_INT32(-2147483646, "-2147483646", buffer)
  CHECK_CONVERSION_INT32(-2147483647, "-2147483647", buffer)
  CHECK_CONVERSION_INT32(INT32_MIN, "-2147483648", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint32_t conversion
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_uint32) {
  char buffer[128];
  size_t actualLength;

  CHECK_CONVERSION_UINT32(0, "0", buffer)
  CHECK_CONVERSION_UINT32(1, "1", buffer)
  CHECK_CONVERSION_UINT32(10, "10", buffer)
  CHECK_CONVERSION_UINT32(100000, "100000", buffer)
  CHECK_CONVERSION_UINT32(10000009, "10000009", buffer)
  CHECK_CONVERSION_UINT32(2147483647, "2147483647", buffer)
  CHECK_CONVERSION_UINT32(4294967295, "4294967295", buffer)
  CHECK_CONVERSION_UINT32(UINT32_MAX, "4294967295", buffer)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test int64_t conversion
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_int64) {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test uint64_t conversion
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_uint64) {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
