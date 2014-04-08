////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for bson utility functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <boost/test/unit_test.hpp>

#include "Basics/BsonHelper.h"

using namespace triagens;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct BsonSetup {
  BsonSetup () {
    BOOST_TEST_MESSAGE("setup bson test");
  }

  ~BsonSetup () {
    BOOST_TEST_MESSAGE("tear-down bson test");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(BsonTest, BsonSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing (no changes) 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_append) {
  Bson b;

  b.appendNull("Null");
  b.appendBool("Bool",true);
  b.appendDouble("Number",123456.789);
  b.appendUtf8("String","Hallo, \"dies\" ist{}\\ ein String!");
  string result;
  BOOST_CHECK_EQUAL(true, b.toJson(result));
  cout << "Json from Bson: " << result << endl;

}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
