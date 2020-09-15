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
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/fpconv.h"
#include "Basics/json.h"
#include "Basics/StringBuffer.h"

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief test nan
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_nan) {
  char out[24];
  double value;
  int length;

  value = NAN;
  EXPECT_TRUE(std::isnan(value));
  length = fpconv_dtoa(value, out);

#ifdef _WIN32
  EXPECT_EQ(std::string("-NaN"), std::string(out, length));
#else
  EXPECT_EQ(std::string("NaN"), std::string(out, length));
#endif
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("NaN"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test infinity
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_inf) {
  char out[24];
  double value;
  int length;

  value = INFINITY;
  EXPECT_FALSE(std::isfinite(value));
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("inf"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("inf"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test huge val
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_huge_val) {
  char out[24];
  double value;
  int length;

  value = HUGE_VAL;
  EXPECT_FALSE(std::isfinite(value));
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("inf"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("inf"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test huge val
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_huge_val_neg) {
  char out[24];
  double value;
  int length;

  value = -HUGE_VAL;
  EXPECT_FALSE(std::isfinite(value));
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("-inf"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("-inf"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test zero
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_zero) {
  char out[24];
  double value;
  int length;

  value = 0;
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("0"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("0"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test zero
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_zero_neg) {
  char out[24];
  double value;
  int length;

  value = -0;
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("0"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("0"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test high
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_value_high) {
  char out[24];
  double value;
  int length;

  value = 4.32e261;
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("4.32e+261"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("4.32e+261"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test low
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_value_low) {
  char out[24];
  double value;
  int length;

  value = -4.32e261;
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("-4.32e+261"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("-4.32e+261"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test small
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_value_small) {
  char out[24];
  double value;
  int length;

  value = 4.32e-261;
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("4.32e-261"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("4.32e-261"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test mchacki's value
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_value_mchacki1) {
  char out[24];
  double value;
  int length;

  value = 1.374;
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("1.374"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("1.374"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test mchacki's value
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_value_mchacki2) {
  char out[24];
  double value;
  int length;

  value = 56.94837631946843;
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("56.94837631946843"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("56.94837631946843"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test one third roundtrip
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_value_mchacki2_roundtrip) {
  double value;

  value = 56.94837631946843;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer);

  auto json = TRI_CreateNumberJson(value);
  TRI_StringifyJson(&buffer, json);

  EXPECT_EQ(std::string("56.94837631946843"), std::string(buffer._buffer, buffer._current - buffer._buffer));

  TRI_FreeJson(json);
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test one third
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_one_third) {
  char out[24];
  double value;
  int length;

  value = 1.0 / 3.0;
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("0.3333333333333333"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("0.3333333333333333"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test one third roundtrip
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_one_third_roundtrip) {
  double value;

  value = 1.0 / 3.0;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer);

  auto json = TRI_CreateNumberJson(value);
  TRI_StringifyJson(&buffer, json);

  EXPECT_EQ(std::string("0.3333333333333333"), std::string(buffer._buffer, buffer._current - buffer._buffer));

  TRI_FreeJson(json);
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test 0.4
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_04) {
  char out[24];
  double value;
  int length;

  value = 0.1 + 0.3;
  length = fpconv_dtoa(value, out);

  EXPECT_EQ(std::string("0.4"), std::string(out, length));
  
  StringBuffer buf(true);
  buf.appendDecimal(value);
  EXPECT_EQ(std::string("0.4"), std::string(buf.c_str(), buf.length()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test 0.4
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_04_roundtrip) {
  double value;

  value = 0.1 + 0.3;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer);

  auto json = TRI_CreateNumberJson(value);
  TRI_StringifyJson(&buffer, json);

  EXPECT_EQ(std::string("0.4"), std::string(buffer._buffer, buffer._current - buffer._buffer));
  
  TRI_FreeJson(json);
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test big roundtrip
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_value_high_roundtrip) {
  double value;

  value = 4.32e261;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer);

  auto json = TRI_CreateNumberJson(value);
  TRI_StringifyJson(&buffer, json);

  EXPECT_EQ(std::string("4.32e+261"), std::string(buffer._buffer, buffer._current - buffer._buffer));
  
  TRI_FreeJson(json);
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test small roundtrip
////////////////////////////////////////////////////////////////////////////////

TEST(CFpconvTest, tst_value_low_roundtrip) {
  double value;

  value = -4.32e261;

  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer);

  auto json = TRI_CreateNumberJson(value);
  TRI_StringifyJson(&buffer, json);

  EXPECT_EQ(std::string("-4.32e+261"), std::string(buffer._buffer, buffer._current - buffer._buffer));

  TRI_FreeJson(json);
  TRI_DestroyStringBuffer(&buffer);
}
