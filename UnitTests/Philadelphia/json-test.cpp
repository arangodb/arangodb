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
