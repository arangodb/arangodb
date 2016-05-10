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
#include "Basics/Utf8Helper.h"

#if _WIN32
#include "Basics/win-utils.h"
#define FIX_ICU_ENV     TRI_FixIcuDataEnv()
#else
#define FIX_ICU_ENV
#endif

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
    FIX_ICU_ENV;
    if (!arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollatorLanguage("")) {
      std::string msg =
        "cannot initialize ICU; please make sure ICU*dat is available; "
        "the variable ICU_DATA='";
      if (getenv("ICU_DATA") != nullptr) {
        msg += getenv("ICU_DATA");
      }
      msg += "' should point the directory containing the ICU*dat file.";
      BOOST_TEST_MESSAGE(msg);
      BOOST_CHECK_EQUAL(false, true);
    }
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
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
