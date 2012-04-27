////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for json.c
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

#include "BasicsC/json.h"
#include "BasicsC/string-buffer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------
  
#define INIT_BUFFER  TRI_string_buffer_t* sb = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
#define FREE_BUFFER  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, sb);
#define STRINGIFY    TRI_StringifyJson(sb, json);
#define STRING_VALUE sb->_buffer
#define FREE_JSON    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CJsonSetup {
  CJsonSetup () {
    BOOST_TEST_MESSAGE("setup json");
  }

  ~CJsonSetup () {
    BOOST_TEST_MESSAGE("tear-down json");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CJsonTest, CJsonSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test null value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_null) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE);

  STRINGIFY
  BOOST_CHECK_EQUAL("null", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test true value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_true) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true);

  STRINGIFY
  BOOST_CHECK_EQUAL("true", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test false value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_false) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, false);

  STRINGIFY
  BOOST_CHECK_EQUAL("false", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value 0
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number0) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 0.0);

  STRINGIFY
  BOOST_CHECK_EQUAL("0", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (positive)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number_positive1) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1.0);

  STRINGIFY
  BOOST_CHECK_EQUAL("1", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (positive)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number_positive2) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 46281);

  STRINGIFY
  BOOST_CHECK_EQUAL("46281", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (negative)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number_negative1) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, -1.0);

  STRINGIFY
  BOOST_CHECK_EQUAL("-1", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (negative)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_number_negative2) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, -2342);

  STRINGIFY
  BOOST_CHECK_EQUAL("-2342", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (empty)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string_empty) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "");

  STRINGIFY
  BOOST_CHECK_EQUAL("\"\"", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string1) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "the quick brown fox");

  STRINGIFY
  BOOST_CHECK_EQUAL("\"the quick brown fox\"", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string2) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "The Quick Brown Fox");

  STRINGIFY
  BOOST_CHECK_EQUAL("\"The Quick Brown Fox\"", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (escaped)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string_escaped) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "\"the quick \"fox\" jumped over the \\brown\\ dog '\n\\\" \\' \\\\ lazy");

  STRINGIFY
  BOOST_CHECK_EQUAL("\"\\\"the quick \\\"fox\\\" jumped over the \\\\brown\\\\ dog '\\n\\\\\\\" \\\\' \\\\\\\\ lazy\"", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (special chars)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string_utf8_1) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기");

  STRINGIFY
  BOOST_CHECK_EQUAL("\"\\uCF54\\uB9AC\\uC544\\uB2F7\\uCEF4 \\uBA54\\uC77C\\uC54C\\uB9AC\\uBBF8 \\uC11C\\uBE44\\uC2A4 \\uC911\\uB2E8\\uC548\\uB0B4 [\\uC548\\uB0B4] \\uAC1C\\uC778\\uC815\\uBCF4\\uCDE8\\uAE09\\uBC29\\uCE68 \\uBCC0\\uACBD \\uC548\\uB0B4 \\uD68C\\uC0AC\\uC18C\\uAC1C | \\uAD11\\uACE0\\uC548\\uB0B4 | \\uC81C\\uD734\\uC548\\uB0B4 | \\uAC1C\\uC778\\uC815\\uBCF4\\uCDE8\\uAE09\\uBC29\\uCE68 | \\uCCAD\\uC18C\\uB144\\uBCF4\\uD638\\uC815\\uCC45 | \\uC2A4\\uD338\\uBC29\\uC9C0\\uC815\\uCC45 | \\uC0AC\\uC774\\uBC84\\uACE0\\uAC1D\\uC13C\\uD130 | \\uC57D\\uAD00\\uC548\\uB0B4 | \\uC774\\uBA54\\uC77C \\uBB34\\uB2E8\\uC218\\uC9D1\\uAC70\\uBD80 | \\uC11C\\uBE44\\uC2A4 \\uC804\\uCCB4\\uBCF4\\uAE30\"", STRING_VALUE); 
      
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (special chars)
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_string_utf8_2) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "äöüßÄÖÜ€µ");

  STRINGIFY
  BOOST_CHECK_EQUAL("\"\\u00E4\\u00F6\\u00FC\\u00DF\\u00C4\\u00D6\\u00DC\\u20AC\\u00B5\"", STRING_VALUE);
   
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty json list
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_list_empty) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  STRINGIFY
  BOOST_CHECK_EQUAL("[]", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty json list mixed
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_list_mixed) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true));
  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, false));
  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, -8093));
  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1.5));
  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, (char*) "the quick brown fox"));

  STRINGIFY
  BOOST_CHECK_EQUAL("[null,true,false,-8093,1.5,\"the quick brown fox\"]", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty json array
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_array_empty) {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  STRINGIFY
  BOOST_CHECK_EQUAL("{}", STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
