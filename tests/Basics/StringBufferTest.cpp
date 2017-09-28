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

#include "catch.hpp"

#include "Basics/StringBuffer.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace std;

TEST_CASE("StringBufferTest", "[string]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test_StringBuffer1
////////////////////////////////////////////////////////////////////////////////

SECTION("test_StringBuffer1") {
  StringBuffer buffer(true);

  CHECK(buffer.length() == (size_t) 0);
  CHECK(std::string(buffer.c_str()) == "");

  buffer = "";

  CHECK(buffer.length() == (size_t) 0);
  CHECK(std::string(buffer.c_str()) == "");

  buffer = "Hallo World!";

  CHECK(buffer.length() == (size_t) 12);
  CHECK(std::string(buffer.c_str()) == "Hallo World!");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_StringBuffer2
////////////////////////////////////////////////////////////////////////////////

SECTION("test_StringBuffer2") {
  StringBuffer buffer(true);

  CHECK(buffer.length() == (size_t) 0);
  CHECK(std::string(buffer.c_str()) == "");
  
  buffer.appendText("Hallo World");
  CHECK(buffer.length() == (size_t) 11);
  CHECK(std::string(buffer.c_str()) == "Hallo World");
  
  buffer.appendInteger4(1234);
  CHECK(buffer.length() == (size_t) 15);
  CHECK(std::string(buffer.c_str()) == "Hallo World1234");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
