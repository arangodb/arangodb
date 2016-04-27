////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for StringBuffer class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2007-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/StringBuffer.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct StringBufferSetup {
  StringBufferSetup () {
    BOOST_TEST_MESSAGE("setup StringBuffer");
  }

  ~StringBufferSetup () {
    BOOST_TEST_MESSAGE("tear-down StringBuffer");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE (StringBufferTest, StringBufferSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test_StringBuffer1
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_StringBuffer1) {
  StringBuffer buffer(TRI_CORE_MEM_ZONE);

  BOOST_CHECK_EQUAL(buffer.length(), (size_t) 0);
  BOOST_CHECK_EQUAL(std::string(buffer.c_str()), "");

  buffer = "";

  BOOST_CHECK_EQUAL(buffer.length(), (size_t) 0);
  BOOST_CHECK_EQUAL(std::string(buffer.c_str()), "");

  buffer = "Hallo World!";

  BOOST_CHECK_EQUAL(buffer.length(), (size_t) 12);
  BOOST_CHECK_EQUAL(std::string(buffer.c_str()), "Hallo World!");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_StringBuffer2
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_StringBuffer2) {
  StringBuffer buffer(TRI_CORE_MEM_ZONE);

  BOOST_CHECK(buffer.length() == (size_t) 0);
  BOOST_CHECK(std::string(buffer.c_str()) == "");
  
  buffer.appendText("Hallo World");
  BOOST_CHECK(buffer.length() == (size_t) 11);
  BOOST_CHECK(std::string(buffer.c_str()) == "Hallo World");
  
  buffer.appendInteger4(1234);
  BOOST_CHECK(buffer.length() == (size_t) 15);
  BOOST_CHECK(std::string(buffer.c_str()) == "Hallo World1234");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
