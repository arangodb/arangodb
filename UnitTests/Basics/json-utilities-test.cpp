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

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/json-utilities.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"

static bool Initialized = false;

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
    if (!Initialized) {
      Initialized = true;
      arangodb::basics::VelocyPackHelper::initialize();
    }
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hashattributes_single) {
  TRI_json_t* json;
  int error;
  
  const char* v1[] = { "_key" };
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ }");
  uint64_t const h1 = TRI_HashJsonByAttributes(json, v1, 1, true, error);
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"_key\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"_key\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"keys\": { \"_key\": \"foobar\" } }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"KEY\": 1234, \"_KEY\": \"foobar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"_key\": \"i-am-a-foo\" }");
  uint64_t const h2 = TRI_HashJsonByAttributes(json, v1, 1, true, error);
  BOOST_CHECK(h1 != h2);
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"KEY\": 1234, \"_key\": \"i-am-a-foo\" }");
  BOOST_CHECK_EQUAL(h2, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": [ \"foobar\" ], \"KEY\": { }, \"_key\": \"i-am-a-foo\" }");
  BOOST_CHECK_EQUAL(h2, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hashattributes_mult1) {
  TRI_json_t* json;
  int error;
  
  const char* v1[] = { "a", "b" };
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ }");
  const uint64_t h1 = TRI_HashJsonByAttributes(json, v1, 2, true, error);
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"b\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": null, \"a\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON

  // test if non-relevant attributes influence our hash
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"B\": 123 }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"B\": 1234, \"a\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"A\": 123, \"B\": \"hihi\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"c\": null, \"d\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"A\": 1, \"B\": 2, \" a\": \"bar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"ab\": 1, \"ba\": 2 }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hashattributes_mult2) {
  TRI_json_t* json;
  int error;
  
  const char* v1[] = { "a", "b" };
  
  uint64_t const h1 = 12539197276825819752ULL;
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"bar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\", \"a\": \"foo\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"food\", \"b\": \"bar\" }");
  BOOST_CHECK_EQUAL(16541519083655723759ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"baz\" }");
  BOOST_CHECK_EQUAL(7656993273597287052ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"FOO\", \"b\": \"BAR\" }");
  BOOST_CHECK_EQUAL(17704521675288781581ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\" }");
  BOOST_CHECK_EQUAL(13052740859585980364ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"meow\" }");
  BOOST_CHECK_EQUAL(5511414856106770809ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\" }");
  BOOST_CHECK_EQUAL(455614752263261981ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\", \"a\": \"meow\" }");
  BOOST_CHECK_EQUAL(1842251108617319700ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names with incomplete docs
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_json_hashattributes_mult3) {
  TRI_json_t* json;
  int error;
  
  const char* v1[] = { "a", "b" };
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"bar\" }");
  TRI_HashJsonByAttributes(json, v1, 2, false, error);
  BOOST_CHECK_EQUAL(TRI_ERROR_NO_ERROR, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\" }");
  TRI_HashJsonByAttributes(json, v1, 2, false, error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\" }");
  TRI_HashJsonByAttributes(json, v1, 2, false, error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ }");
  TRI_HashJsonByAttributes(json, v1, 2, false, error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"c\": 12 }");
  TRI_HashJsonByAttributes(json, v1, 2, false, error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": 1, \"b\": null }");
  TRI_HashJsonByAttributes(json, v1, 2, false, error);
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
