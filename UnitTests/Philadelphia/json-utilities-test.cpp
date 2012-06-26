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

#include "BasicsC/json-utilities.h"
#include "BasicsC/string-buffer.h"

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
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
