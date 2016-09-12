////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for fpconv.cpp
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
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/fpconv.h"
#include "Basics/json.h"
#include "Basics/StringBuffer.h"

using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CFpconvSetup {
  CFpconvSetup () {
    BOOST_TEST_MESSAGE("setup fpconv");
  }

  ~CFpconvSetup () {
    BOOST_TEST_MESSAGE("tear-down fpconv");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CFpconvTest, CFpconvSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test nan
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_nan) {
  char out[24];
  double value;
  int length;

  value = NAN;
  BOOST_CHECK_EQUAL(true, std::isnan(value));
  length = fpconv_dtoa(value, out);

#ifdef _WIN32
  BOOST_CHECK_EQUAL(std::string("-NaN"), std::string(out, length));
#else
  BOOST_CHECK_EQUAL(std::string("NaN"), std::string(out, length));
#endif
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("NaN"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test infinity
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_inf) {
  char out[24];
  double value;
  int length;

  value = INFINITY;
  BOOST_CHECK_EQUAL(false, std::isfinite(value));
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("inf"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("inf"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test huge val
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_huge_val) {
  char out[24];
  double value;
  int length;

  value = HUGE_VAL;
  BOOST_CHECK_EQUAL(false, std::isfinite(value));
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("inf"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("inf"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test huge val
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_huge_val_neg) {
  char out[24];
  double value;
  int length;

  value = -HUGE_VAL;
  BOOST_CHECK_EQUAL(false, std::isfinite(value));
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("-inf"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("-inf"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test zero
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_zero) {
  char out[24];
  double value;
  int length;

  value = 0;
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("0"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("0"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test zero
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_zero_neg) {
  char out[24];
  double value;
  int length;

  value = -0;
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("0"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("0"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test high
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_high) {
  char out[24];
  double value;
  int length;

  value = 4.32e261;
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("4.32e+261"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("4.32e+261"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test low
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_low) {
  char out[24];
  double value;
  int length;

  value = -4.32e261;
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("-4.32e+261"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("-4.32e+261"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test small
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_small) {
  char out[24];
  double value;
  int length;

  value = 4.32e-261;
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("4.32e-261"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("4.32e-261"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test mchacki's value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_mchacki1) {
  char out[24];
  double value;
  int length;

  value = 1.374;
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("1.374"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("1.374"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test mchacki's value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_mchacki2) {
  char out[24];
  double value;
  int length;

  value = 56.94837631946843;
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("56.94837631946843"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("56.94837631946843"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test one third roundtrip
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_mchacki2_roundtrip) {
  double value;

  value = 56.94837631946843;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

  auto json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, value);
  TRI_StringifyJson(&buffer, json);

  BOOST_CHECK_EQUAL(std::string("56.94837631946843"), std::string(buffer._buffer, buffer._current - buffer._buffer));

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test one third
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_one_third) {
  char out[24];
  double value;
  int length;

  value = 1.0 / 3.0;
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("0.3333333333333333"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("0.3333333333333333"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test one third roundtrip
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_one_third_roundtrip) {
  double value;

  value = 1.0 / 3.0;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

  auto json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, value);
  TRI_StringifyJson(&buffer, json);

  BOOST_CHECK_EQUAL(std::string("0.3333333333333333"), std::string(buffer._buffer, buffer._current - buffer._buffer));

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test 0.4
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_04) {
  char out[24];
  double value;
  int length;

  value = 0.1 + 0.3;
  length = fpconv_dtoa(value, out);

  BOOST_CHECK_EQUAL(std::string("0.4"), std::string(out, length));
  
  StringBuffer buf(TRI_UNKNOWN_MEM_ZONE);
  buf.appendDecimal(value);
  BOOST_CHECK_EQUAL(std::string("0.4"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test 0.4
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_04_roundtrip) {
  double value;

  value = 0.1 + 0.3;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

  auto json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, value);
  TRI_StringifyJson(&buffer, json);

  BOOST_CHECK_EQUAL(std::string("0.4"), std::string(buffer._buffer, buffer._current - buffer._buffer));
  
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test big roundtrip
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_high_roundtrip) {
  double value;

  value = 4.32e261;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

  auto json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, value);
  TRI_StringifyJson(&buffer, json);

  BOOST_CHECK_EQUAL(std::string("4.32e+261"), std::string(buffer._buffer, buffer._current - buffer._buffer));
  
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test small roundtrip
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_value_low_roundtrip) {
  double value;

  value = -4.32e261;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

  auto json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, value);
  TRI_StringifyJson(&buffer, json);

  BOOST_CHECK_EQUAL(std::string("-4.32e+261"), std::string(buffer._buffer, buffer._current - buffer._buffer));

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
