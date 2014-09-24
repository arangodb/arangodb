////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for json-utilities.c
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

#include "Basics/string-buffer.h"
#include "Basics/json-utilities.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------
  
#define JSON_CHECK(expected, func, lValue, rValue)      \
  l = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, lValue); \
  r = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, rValue); \
  if (l && r) {                                     \
    BOOST_CHECK_EQUAL(expected, func(l, r)); \
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, l); \
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, r); \
  }

#define INIT_BUFFER  TRI_string_buffer_t* sb = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
#define FREE_BUFFER  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, sb);
#define STRINGIFY    TRI_StringifyJson(sb, json);
#define STRING_VALUE sb->_buffer
#define FREE_JSON    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CJsonUtilitiesSetup {
  CJsonUtilitiesSetup () {
    BOOST_TEST_MESSAGE("setup json utilities test");
  }

  ~CJsonUtilitiesSetup () {
    BOOST_TEST_MESSAGE("tear-down json utilities test");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CJsonUtilitiesTest, CJsonUtilitiesSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with equal values
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_compare_values_equal) {
  TRI_json_t* l;
  TRI_json_t* r;

  // With Utf8-mode:
  JSON_CHECK(0, TRI_CompareValuesJson, "null", "null");
  JSON_CHECK(0, TRI_CompareValuesJson, "false", "false");
  JSON_CHECK(0, TRI_CompareValuesJson, "true", "true");
  JSON_CHECK(0, TRI_CompareValuesJson, "0", "0");
  JSON_CHECK(0, TRI_CompareValuesJson, "1", "1");
  JSON_CHECK(0, TRI_CompareValuesJson, "1.5", "1.5");
  JSON_CHECK(0, TRI_CompareValuesJson, "-43.2", "-43.2");
  JSON_CHECK(0, TRI_CompareValuesJson, "\"\"", "\"\"");
  JSON_CHECK(0, TRI_CompareValuesJson, "\" \"", "\" \"");
  JSON_CHECK(0, TRI_CompareValuesJson, "\"the quick brown fox\"", "\"the quick brown fox\"");
  JSON_CHECK(0, TRI_CompareValuesJson, "[]", "[]");
  JSON_CHECK(0, TRI_CompareValuesJson, "[-1]", "[-1]");
  JSON_CHECK(0, TRI_CompareValuesJson, "[0]", "[0]");
  JSON_CHECK(0, TRI_CompareValuesJson, "[1]", "[1]");
  JSON_CHECK(0, TRI_CompareValuesJson, "[true]", "[true]");
  JSON_CHECK(0, TRI_CompareValuesJson, "{}", "{}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with unequal values
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_compare_values_unequal) {
  TRI_json_t* l;
  TRI_json_t* r;
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "false");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "true");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "-1");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "0");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "1");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "-10");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "\"\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "\"0\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "\" \"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "[]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "[null]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "[false]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "[true]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "[0]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "null", "{}");
  
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "true");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "-1");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "0");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "1");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "-10");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "\"\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "\"0\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "\" \"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "[]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "[null]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "[false]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "[true]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "[0]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "false", "{}");
  
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "-1");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "0");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "1");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "-10");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "\"\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "\"0\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "\" \"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "[]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "[null]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "[false]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "[true]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "[0]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "{}");
  
  JSON_CHECK(-1, TRI_CompareValuesJson, "-2", "-1");
  JSON_CHECK(-1, TRI_CompareValuesJson, "-10", "-9");
  JSON_CHECK(-1, TRI_CompareValuesJson, "-20", "-5");
  JSON_CHECK(-1, TRI_CompareValuesJson, "-5", "-2");
  JSON_CHECK(-1, TRI_CompareValuesJson, "true", "1");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1.5", "1.6");
  JSON_CHECK(-1, TRI_CompareValuesJson, "10.5", "10.51");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "\"\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "\"0\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "\"-1\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "\"-1\"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "\" \"");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "[]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "[-1]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "[0]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "[1]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "[null]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "[false]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "[true]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "0", "{}");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "[]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "[-1]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "[0]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "[1]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "[null]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "[false]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "[true]");
  JSON_CHECK(-1, TRI_CompareValuesJson, "1", "{}");

  // TODO: add more tests
}

// TODO: add tests for
  // TRI_CheckSameValueJson
  // TRI_BetweenListJson
  // TRI_UniquifyListJson
  // TRI_UnionizeListsJson
  // TRI_IntersectListsJson
  // TRI_SortListJson

////////////////////////////////////////////////////////////////////////////////
/// @brief test check in list
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_check_in_list) {
  TRI_json_t* l;
  TRI_json_t* r;

  JSON_CHECK(true, TRI_CheckInListJson, "null", "[1,2,3,null]");
  JSON_CHECK(true, TRI_CheckInListJson, "false", "[false]");
  JSON_CHECK(true, TRI_CheckInListJson, "true", "[false,true]");
  JSON_CHECK(true, TRI_CheckInListJson, "0", "[0]");
  JSON_CHECK(true, TRI_CheckInListJson, "0", "[0,1]");
  JSON_CHECK(true, TRI_CheckInListJson, "0", "[0,1,2]");
  JSON_CHECK(true, TRI_CheckInListJson, "0", "[2,1,0]");
  JSON_CHECK(true, TRI_CheckInListJson, "1", "[1,0]");
  JSON_CHECK(true, TRI_CheckInListJson, "1", "[2,1,0]");
  JSON_CHECK(true, TRI_CheckInListJson, "1", "[12,12,12,12,1]");
  JSON_CHECK(true, TRI_CheckInListJson, "12", "[0,9,100,7,12,8]");
  JSON_CHECK(true, TRI_CheckInListJson, "15", "[12,13,14,16,17,15]");
  JSON_CHECK(true, TRI_CheckInListJson, "\"\"", "[1,2,3,\"\"]");
  JSON_CHECK(true, TRI_CheckInListJson, "\"a\"", "[1,2,3,\"a\"]");
  JSON_CHECK(true, TRI_CheckInListJson, "\"A\"", "[1,2,\"A\"]");
  JSON_CHECK(true, TRI_CheckInListJson, "\"the fox\"", "[1,\"the fox\"]");
  JSON_CHECK(true, TRI_CheckInListJson, "[]", "[[]]");
  JSON_CHECK(true, TRI_CheckInListJson, "[]", "[2,3,[]]");
  JSON_CHECK(true, TRI_CheckInListJson, "[null]", "[[null]]");
  JSON_CHECK(true, TRI_CheckInListJson, "[false]", "[[false]]");
  JSON_CHECK(true, TRI_CheckInListJson, "[true]", "[[true]]");
  JSON_CHECK(true, TRI_CheckInListJson, "[true]", "[[false],[true]]");
  JSON_CHECK(true, TRI_CheckInListJson, "[0]", "[1,2,3,[0]]");
  JSON_CHECK(true, TRI_CheckInListJson, "[\"a\"]", "[\"b\",\"\",[\"a\"]]");

  JSON_CHECK(false, TRI_CheckInListJson, "null", "[0,1,2,3,\"\",false,\"null\"]");
  JSON_CHECK(false, TRI_CheckInListJson, "null", "[[null]]");
  JSON_CHECK(false, TRI_CheckInListJson, "false", "[0,1,2,3,\"\",\"false\",\"null\"]");
  JSON_CHECK(false, TRI_CheckInListJson, "false", "[[false]]");
  JSON_CHECK(false, TRI_CheckInListJson, "true", "[\"true\"]");
  JSON_CHECK(false, TRI_CheckInListJson, "true", "[[true]]");
  JSON_CHECK(false, TRI_CheckInListJson, "0", "[null,false,\"\",\" \"]");
  JSON_CHECK(false, TRI_CheckInListJson, "0", "[[0]]");
  JSON_CHECK(false, TRI_CheckInListJson, "15", "[12,13,14,16,17]");
  JSON_CHECK(false, TRI_CheckInListJson, "15", "[[15]]");
  JSON_CHECK(false, TRI_CheckInListJson, "120", "[12,121,1200]");
  JSON_CHECK(false, TRI_CheckInListJson, "\"a\"", "[\"A\"]");
  JSON_CHECK(false, TRI_CheckInListJson, "\"A\"", "[\"a\"]");
  JSON_CHECK(false, TRI_CheckInListJson, "\"a\"", "[\"abc\"]");
  JSON_CHECK(false, TRI_CheckInListJson, "\"a\"", "[\"a \"]");
  JSON_CHECK(false, TRI_CheckInListJson, "\"the fox\"", "[\"the\",\"fox\"]");
  JSON_CHECK(false, TRI_CheckInListJson, "\"a\"", "[[\"a\"]]");
  JSON_CHECK(false, TRI_CheckInListJson, "[]", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "[]", "[5,4,3,2,1]");
  JSON_CHECK(false, TRI_CheckInListJson, "[0]", "[0,1,2,3]");
  JSON_CHECK(false, TRI_CheckInListJson, "[]", "[0,1,2,3]");
  JSON_CHECK(false, TRI_CheckInListJson, "[false]", "[false,true]");
  JSON_CHECK(false, TRI_CheckInListJson, "[\"a\"]", "[\"a\"]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test check in list with an empty list
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_check_in_list_empty) {
  TRI_json_t* l;
  TRI_json_t* r;

  JSON_CHECK(false, TRI_CheckInListJson, "null", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "false", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "true", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "0", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "1", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "\"fox\"", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "\"\"", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "\" \"", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "[]", "[]");
  JSON_CHECK(false, TRI_CheckInListJson, "{}", "[]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lists union
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_unionize_lists_empty) {
  INIT_BUFFER
  
  TRI_json_t* json;
  TRI_json_t* list1; 
  TRI_json_t* list2;

  list1 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[]");
  list2 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[]");
  json = TRI_UnionizeListsJson(list1, list2, true);

  STRINGIFY
  BOOST_CHECK_EQUAL("[]", STRING_VALUE); 
 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list1); 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list2);

  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lists intersection
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_intersect_lists_empty1) {
  INIT_BUFFER
  
  TRI_json_t* json;
  TRI_json_t* list1; 
  TRI_json_t* list2;
  
  list1 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[]");
  list2 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[]");
  json = TRI_IntersectListsJson(list1, list2, true);

  STRINGIFY
  BOOST_CHECK_EQUAL("[]", STRING_VALUE); 
 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list1); 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list2);

  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lists intersection
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_intersect_lists_empty2) {
  INIT_BUFFER
  
  TRI_json_t* json;
  TRI_json_t* list1; 
  TRI_json_t* list2;
  
  list1 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[1]");
  list2 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[]");
  json = TRI_IntersectListsJson(list1, list2, true);

  STRINGIFY
  BOOST_CHECK_EQUAL("[]", STRING_VALUE); 
 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list1); 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list2);

  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lists intersection
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_intersect_lists_empty3) {
  INIT_BUFFER
  
  TRI_json_t* json;
  TRI_json_t* list1; 
  TRI_json_t* list2;
  
  list1 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[0]");
  list2 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[]");
  json = TRI_IntersectListsJson(list1, list2, true);

  STRINGIFY
  BOOST_CHECK_EQUAL("[]", STRING_VALUE); 
 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list1); 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list2);

  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lists intersection
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_intersect_lists_values1) {
  INIT_BUFFER
  
  TRI_json_t* json;
  TRI_json_t* list1; 
  TRI_json_t* list2;
  
  list1 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[0,1,2,3]");
  list2 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[2,3,4]");
  json = TRI_IntersectListsJson(list1, list2, true);

  STRINGIFY
  BOOST_CHECK_EQUAL("[2,3]", STRING_VALUE); 
 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list1); 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list2);

  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lists intersection
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_intersect_lists_values2) {
  INIT_BUFFER
  
  TRI_json_t* json;
  TRI_json_t* list1; 
  TRI_json_t* list2;
  
  list1 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[null,false,true,0,1,2,3,99,99.5,\"fox\",\"zoo\"]");
  list2 = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[false,2,99,99.2,\"Fox\",\"zoo\"]");
  json = TRI_IntersectListsJson(list1, list2, true);

  STRINGIFY
  BOOST_CHECK_EQUAL("[false,2,99,\"zoo\"]", STRING_VALUE); 
 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list1); 
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list2);

  FREE_JSON
  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate keys
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_duplicate_keys) {
  INIT_BUFFER
  
  TRI_json_t* json;

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[\"a\",\"a\"]");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1,\"b\":1}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1,\"b\":1,\"A\":1}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1,\"b\":1,\"a\":1}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1,\"b\":1,\"c\":1,\"d\":{},\"c\":1}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{}}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":1}}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":1,\"b\":1},\"b\":1}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":1,\"b\":1,\"a\":3},\"b\":1}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":1,\"b\":1,\"a\":3}}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":{\"a\":{}}}}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":{\"a\":{},\"a\":2}}}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON

  FREE_BUFFER
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hash_utf8) {
  TRI_json_t* json;

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"äöüßÄÖÜ€µ\"");
  BOOST_CHECK_EQUAL(17926322495289827824ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기\"");
  BOOST_CHECK_EQUAL(11647939066062684691ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"بان يأسف لمقتل لاجئين سوريين بتركيا المرزوقي يندد بعنف الأمن التونسي تنديد بقتل الجيش السوري مصورا تلفزيونيا 14 قتيلا وعشرات الجرحى بانفجار بالصومال\"");
  BOOST_CHECK_EQUAL(9773937585298648628ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"中华网以中国的市场为核心，致力为当地用户提供流动增值服务、网上娱乐及互联网服务。本公司亦推出网上游戏，及透过其门户网站提供包罗万有的网上产品及服务。\"");
  BOOST_CHECK_EQUAL(5348732066920102360ULL, TRI_HashJson(json));
  FREE_JSON
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hash) {
  TRI_json_t* json;

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "null");
  BOOST_CHECK_EQUAL(6601085983368743140ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "false");
  BOOST_CHECK_EQUAL(13113042584710199672ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "true");
  BOOST_CHECK_EQUAL(6583304908937478053ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "0");
  BOOST_CHECK_EQUAL(12161962213042174405ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "123");
  BOOST_CHECK_EQUAL(3423744850239007323ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"\"");
  BOOST_CHECK_EQUAL(12638153115695167455ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\" \"");
  BOOST_CHECK_EQUAL(560073664097094349ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"foobar\"");
  BOOST_CHECK_EQUAL(3770388817002598200ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"Foobar\"");
  BOOST_CHECK_EQUAL(6228943802847363544ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"FOOBAR\"");
  BOOST_CHECK_EQUAL(7710850877466186488ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[]");
  BOOST_CHECK_EQUAL(13796666053062066497ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ null ]");
  BOOST_CHECK_EQUAL(12579909069687325360ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ 0 ]");
  BOOST_CHECK_EQUAL(10101894954932532065ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ false ]");
  BOOST_CHECK_EQUAL(4554324570636443940ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ \"false\" ]");
  BOOST_CHECK_EQUAL(295270779373686828ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ [ ] ]");
  BOOST_CHECK_EQUAL(3935687115999630221ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ { } ]");
  BOOST_CHECK_EQUAL(13595004369025342186ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ [ false, 0 ] ]");
  BOOST_CHECK_EQUAL(8026218647638185280ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{}");
  BOOST_CHECK_EQUAL(5737045748118630438ULL, TRI_HashJson(json));
  FREE_JSON
 
  // the following hashes should be identical 
  const uint64_t a = 5721494255658103046ULL;
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"1\", \"b\": \"2\" }");
  BOOST_CHECK_EQUAL(a, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"2\", \"a\": \"1\" }");
  BOOST_CHECK_EQUAL(a, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"2\", \"b\": \"1\" }");
  BOOST_CHECK_EQUAL(a, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"b\": \"1\" }");
  BOOST_CHECK_EQUAL(2549570315580563109ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"1\" }");
  BOOST_CHECK_EQUAL(5635413490308263533ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": 123, \"b\": [ ] }");
  BOOST_CHECK_EQUAL(9398364376493393319ULL, TRI_HashJson(json));
  FREE_JSON
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hashattributes_single) {
  TRI_json_t* json;
  
  const char* v1[] = { "_key" };
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ }");
  const uint64_t h1 = TRI_HashJsonByAttributes(json, v1, 1, true, NULL);
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"_key\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"_key\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"keys\": { \"_key\": \"foobar\" } }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"KEY\": 1234, \"_KEY\": \"foobar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"_key\": \"i-am-a-foo\" }");
  const uint64_t h2 = TRI_HashJsonByAttributes(json, v1, 1, true, NULL);
  BOOST_CHECK(h1 != h2);
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"KEY\": 1234, \"_key\": \"i-am-a-foo\" }");
  BOOST_CHECK_EQUAL(h2, TRI_HashJsonByAttributes(json, v1, 1, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": [ \"foobar\" ], \"KEY\": { }, \"_key\": \"i-am-a-foo\" }");
  BOOST_CHECK_EQUAL(h2, TRI_HashJsonByAttributes(json, v1, 1, true, NULL));
  FREE_JSON
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hashattributes_mult1) {
  TRI_json_t* json;
  
  const char* v1[] = { "a", "b" };
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ }");
  const uint64_t h1 = TRI_HashJsonByAttributes(json, v1, 2, true, NULL);
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"b\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": null, \"a\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON

  // test if non-relevant attributes influence our hash
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"B\": 123 }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"B\": 1234, \"a\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"A\": 123, \"B\": \"hihi\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"c\": null, \"d\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"A\": 1, \"B\": 2, \" a\": \"bar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"ab\": 1, \"ba\": 2 }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hashattributes_mult2) {
  TRI_json_t* json;
  
  const char* v1[] = { "a", "b" };
  
  const uint64_t h1 = 6369173190757857502ULL;
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"bar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\", \"a\": \"foo\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"food\", \"b\": \"bar\" }");
  BOOST_CHECK_EQUAL(720060016857102700ULL, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"baz\" }");
  BOOST_CHECK_EQUAL(6361520589827022742ULL, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"FOO\", \"b\": \"BAR\" }");
  BOOST_CHECK_EQUAL(3595137217367956894ULL, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\" }");
  BOOST_CHECK_EQUAL(12739237936894360852ULL, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"meow\" }");
  BOOST_CHECK_EQUAL(13378327204915572311ULL, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\" }");
  BOOST_CHECK_EQUAL(10085884912118216755ULL, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\", \"a\": \"meow\" }");
  BOOST_CHECK_EQUAL(15753579192430387496ULL, TRI_HashJsonByAttributes(json, v1, 2, true, NULL));
  FREE_JSON
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names with incomplete docs
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hashattributes_mult3) {
  TRI_json_t* json;
  
  const char* v1[] = { "a", "b" };
  
  int error;
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"bar\" }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_NO_ERROR, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\" }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\" }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"c\": 12 }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": 1, \"b\": null }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_NO_ERROR, error);
  FREE_JSON
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
