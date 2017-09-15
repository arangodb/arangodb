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

#include "Basics/Common.h"

#include "catch.hpp"

#include <string>

#include "Basics/json.h"
#include "Basics/files.h"
#include "Basics/directories.h"
#include "Basics/StringBuffer.h"
#include "Basics/Utf8Helper.h"

#include "icu-helper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------
  
#define INIT_BUFFER  TRI_string_buffer_t* sb = TRI_CreateStringBuffer();
#define FREE_BUFFER  TRI_FreeStringBuffer(sb);
#define STRINGIFY    TRI_StringifyJson(sb, json);
#define STRING_VALUE sb->_buffer
#define FREE_JSON    TRI_FreeJson(json);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

extern char const* ARGV0;

struct CJsonSetup {
  CJsonSetup () {
    IcuInitializer::setup(ARGV0);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CJsonTest", "[cjson]") {
  CJsonSetup s;

////////////////////////////////////////////////////////////////////////////////
/// @brief test null value
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_null") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNullJson();
  CHECK(false == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("null") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test true value
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_true") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateBooleanJson(true);
  CHECK(false == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("true") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test false value
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_false") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateBooleanJson(false);
  CHECK(false == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("false") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value 0
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_number0") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(0.0);
  CHECK(false == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("0") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (positive)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_number_positive1") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(1.0);
  CHECK(false == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("1") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (positive)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_number_positive2") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(46281);
  CHECK(false == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("46281") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (negative)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_number_negative1") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(-1.0);
  CHECK(false == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("-1") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test number value (negative)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_number_negative2") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateNumberJson(-2342);
  CHECK(false == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("-2342") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (empty)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_string_empty") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson("", strlen(""));
  CHECK(true == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("\"\"") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_string1") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson("the quick brown fox", strlen("the quick brown fox"));
  CHECK(true == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("\"the quick brown fox\"") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_string2") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateStringCopyJson("The Quick Brown Fox", strlen("The Quick Brown Fox"));
  CHECK(true == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("\"The Quick Brown Fox\"") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (escaped)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_string_escaped") {
  INIT_BUFFER
  
  char const* value = "\"the quick \"fox\" jumped over the \\brown\\ dog '\n\\\" \\' \\\\ lazy";

  TRI_json_t* json = TRI_CreateStringCopyJson(value, strlen(value));
  CHECK(true == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("\"\\\"the quick \\\"fox\\\" jumped over the \\\\brown\\\\ dog '\\n\\\\\\\" \\\\' \\\\\\\\ lazy\"") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (special chars)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_string_utf8_1") {
  INIT_BUFFER

  char const* value = "코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기";
  TRI_json_t* json = TRI_CreateStringCopyJson(value, strlen(value));
  CHECK(true == TRI_IsStringJson(json));

  STRINGIFY
  CHECK(std::string("\"코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기\"") == STRING_VALUE);
      
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (special chars)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_string_utf8_2") {
  INIT_BUFFER

  char const* value = "äöüßÄÖÜ€µ";
  TRI_json_t* json = TRI_CreateStringCopyJson(value, strlen(value));

  STRINGIFY
  CHECK(std::string("\"äöüßÄÖÜ€µ\"") == STRING_VALUE);
   
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test string value (unicode surrogate pair)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_string_utf8_3") {
  INIT_BUFFER

  char const* value = "a𝛢";
  TRI_json_t* json = TRI_CreateStringCopyJson(value, strlen(value));

  STRINGIFY
  CHECK(std::string("\"a𝛢\"") == STRING_VALUE);
   
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty json list
////////////////////////////////////////////////////////////////////////////////
   
SECTION("tst_json_list_empty") {
  INIT_BUFFER
          
  TRI_json_t* json = TRI_CreateArrayJson();
              
  STRINGIFY
  CHECK(std::string("[]") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test json list mixed
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_list_mixed") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateArrayJson();
  TRI_PushBack3ArrayJson(json, TRI_CreateNullJson());
  TRI_PushBack3ArrayJson(json, TRI_CreateBooleanJson(true));
  TRI_PushBack3ArrayJson(json, TRI_CreateBooleanJson(false));
  TRI_PushBack3ArrayJson(json, TRI_CreateNumberJson(-8093));
  TRI_PushBack3ArrayJson(json, TRI_CreateNumberJson(1.5));
  TRI_PushBack3ArrayJson(json, TRI_CreateStringCopyJson((char*) "the quick brown fox", strlen("the quick brown fox")));
  TRI_PushBack3ArrayJson(json, TRI_CreateArrayJson());
  TRI_PushBack3ArrayJson(json, TRI_CreateObjectJson());

  STRINGIFY
  CHECK(std::string("[null,true,false,-8093,1.5,\"the quick brown fox\",[],{}]") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test json lists nested
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_list_nested") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateArrayJson();
  TRI_json_t* list1 = TRI_CreateArrayJson();
  TRI_json_t* list2 = TRI_CreateArrayJson();
  TRI_json_t* list3 = TRI_CreateArrayJson();
  TRI_json_t* list4 = TRI_CreateArrayJson();

  TRI_PushBack3ArrayJson(list1, TRI_CreateBooleanJson(true));
  TRI_PushBack3ArrayJson(list1, TRI_CreateBooleanJson(false));
  TRI_PushBack3ArrayJson(list2, TRI_CreateNumberJson(-8093));
  TRI_PushBack3ArrayJson(list2, TRI_CreateNumberJson(1.5));
  TRI_PushBack3ArrayJson(list3, TRI_CreateStringCopyJson((char*) "the quick brown fox", strlen("the quick brown fox")));
  TRI_PushBack3ArrayJson(json, list1);
  TRI_PushBack3ArrayJson(json, list2);
  TRI_PushBack3ArrayJson(json, list3);
  TRI_PushBack3ArrayJson(json, list4);

  STRINGIFY
  CHECK(std::string("[[true,false],[-8093,1.5],[\"the quick brown fox\"],[]]") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty json array
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_array_empty") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson();

  STRINGIFY
  CHECK(std::string("{}") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test json array mixed
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_array_mixed") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson();
  TRI_Insert3ObjectJson(json, "one", TRI_CreateNullJson());
  TRI_Insert3ObjectJson(json, "two", TRI_CreateBooleanJson(true));
  TRI_Insert3ObjectJson(json, "three", TRI_CreateBooleanJson(false));
  TRI_Insert3ObjectJson(json, "four", TRI_CreateNumberJson(-8093));
  TRI_Insert3ObjectJson(json, "five", TRI_CreateNumberJson(1.5));
  TRI_Insert3ObjectJson(json, "six", TRI_CreateStringCopyJson((char*) "the quick brown fox", strlen("the quick brown fox")));
  TRI_Insert3ObjectJson(json, "seven", TRI_CreateArrayJson());
  TRI_Insert3ObjectJson(json, "eight", TRI_CreateObjectJson());

  STRINGIFY
  CHECK(std::string("{\"one\":null,\"two\":true,\"three\":false,\"four\":-8093,\"five\":1.5,\"six\":\"the quick brown fox\",\"seven\":[],\"eight\":{}}") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test nested json array 
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_array_nested") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson();
  TRI_json_t* array1 = TRI_CreateObjectJson();
  TRI_json_t* array2 = TRI_CreateObjectJson();
  TRI_json_t* array3 = TRI_CreateObjectJson();
  TRI_json_t* array4 = TRI_CreateObjectJson();
  TRI_Insert3ObjectJson(array1, "one", TRI_CreateNullJson());
  TRI_Insert3ObjectJson(array1, "two", TRI_CreateBooleanJson(true));
  TRI_Insert3ObjectJson(array1, "three", TRI_CreateBooleanJson(false));
  TRI_Insert3ObjectJson(array2, "four", TRI_CreateNumberJson(-8093));
  TRI_Insert3ObjectJson(array2, "five", TRI_CreateNumberJson(1.5));
  TRI_Insert3ObjectJson(array2, "six", TRI_CreateStringCopyJson((char*) "the quick brown fox", strlen("the quick brown fox")));
  TRI_Insert3ObjectJson(array3, "seven", TRI_CreateArrayJson());
  TRI_Insert3ObjectJson(array3, "eight", TRI_CreateObjectJson());
  TRI_Insert3ObjectJson(json, "one", array1);
  TRI_Insert3ObjectJson(json, "two", array2);
  TRI_Insert3ObjectJson(json, "three", array3);
  TRI_Insert3ObjectJson(json, "four", array4);

  STRINGIFY
  CHECK(std::string("{\"one\":{\"one\":null,\"two\":true,\"three\":false},\"two\":{\"four\":-8093,\"five\":1.5,\"six\":\"the quick brown fox\"},\"three\":{\"seven\":[],\"eight\":{}},\"four\":{}}") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test json array keys
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_array_keys") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson();
  TRI_Insert3ObjectJson(json, "\"quoted\"", TRI_CreateNumberJson(1));
  TRI_Insert3ObjectJson(json, "'quoted'", TRI_CreateNumberJson(2));
  TRI_Insert3ObjectJson(json, "\\slashed\\\"", TRI_CreateNumberJson(3));
  TRI_Insert3ObjectJson(json, "white spaced", TRI_CreateNumberJson(4));
  TRI_Insert3ObjectJson(json, "line\\nbreak", TRI_CreateNumberJson(5));
  TRI_Insert3ObjectJson(json, "", TRI_CreateNumberJson(6));
  TRI_Insert3ObjectJson(json, " ", TRI_CreateNumberJson(7));
  TRI_Insert3ObjectJson(json, "null", TRI_CreateNumberJson(8));
  TRI_Insert3ObjectJson(json, "true", TRI_CreateNumberJson(9));
  TRI_Insert3ObjectJson(json, "false", TRI_CreateNumberJson(10));

  STRINGIFY
  CHECK(std::string("{\"\\\"quoted\\\"\":1,\"'quoted'\":2,\"\\\\slashed\\\\\\\"\":3,\"white spaced\":4,\"line\\\\nbreak\":5,\"\":6,\" \":7,\"null\":8,\"true\":9,\"false\":10}") == STRING_VALUE);
  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test utf8 json array keys
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_json_array_keys_utf8") {
  INIT_BUFFER

  TRI_json_t* json = TRI_CreateObjectJson();
  TRI_Insert3ObjectJson(json, "äöüÄÖÜß", TRI_CreateNumberJson(1));
  TRI_Insert3ObjectJson(json, "코리아닷컴", TRI_CreateNumberJson(2));
  TRI_Insert3ObjectJson(json, "ジャパン", TRI_CreateNumberJson(3));
  TRI_Insert3ObjectJson(json, "мадридского", TRI_CreateNumberJson(4));

  STRINGIFY
  CHECK(std::string("{\"äöüÄÖÜß\":1,\"코리아닷컴\":2,\"ジャパン\":3,\"мадридского\":4}") == STRING_VALUE);

  FREE_JSON
  FREE_BUFFER
}
}
////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
