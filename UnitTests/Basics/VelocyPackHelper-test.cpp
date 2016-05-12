////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for VelocyPackHelper.cpp
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

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/VelocyPackHelper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------
  
#define VPACK_CHECK(expected, func, lValue, rValue)  \
  l = VPackParser::fromJson(lValue);  \
  r = VPackParser::fromJson(rValue);  \
  BOOST_CHECK_EQUAL(expected, func(l->slice(), r->slice(), true)); \

#define INIT_BUFFER  TRI_string_buffer_t* sb = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
#define FREE_BUFFER  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, sb);
#define STRINGIFY    TRI_StringifyJson(sb, json);
#define STRING_VALUE sb->_buffer
#define FREE_JSON    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct VPackHelperSetup {
  VPackHelperSetup () {
    BOOST_TEST_MESSAGE("setup VelocyPackHelper test");
  }

  ~VPackHelperSetup () {
    BOOST_TEST_MESSAGE("tear-down VelocyPackHelper test");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(VPackHelperTest, VPackHelperSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with equal values
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_compare_values_equal) {
  std::shared_ptr<VPackBuilder> l;
  std::shared_ptr<VPackBuilder> r;

  // With Utf8-mode:
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "null", "null");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "false", "false");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "true", "true");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "0", "0");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "1", "1");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "1.5", "1.5");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "-43.2", "-43.2");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "\"\"", "\"\"");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "\" \"", "\" \"");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "\"the quick brown fox\"", "\"the quick brown fox\"");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[]", "[]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[-1]", "[-1]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[0]", "[0]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[1]", "[1]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[true]", "[true]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "{}", "{}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with unequal values
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_compare_values_unequal) {
  std::shared_ptr<VPackBuilder> l;
  std::shared_ptr<VPackBuilder> r;
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "false");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "true");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "-1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "0");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "-10");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "\"\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "\"0\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "\" \"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "{}");
  
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "true");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "-1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "0");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "-10");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "\"\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "\"0\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "\" \"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "{}");
  
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "-1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "0");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "-10");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "\"\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "\"0\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "\" \"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "{}");
  
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "-2", "-1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "-10", "-9");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "-20", "-5");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "-5", "-2");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1.5", "1.6");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "10.5", "10.51");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "\"\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "\"0\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "\"-1\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "\"-1\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "\" \"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[-1]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[1]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "{}");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[-1]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[1]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "{}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
