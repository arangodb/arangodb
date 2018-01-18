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

#include "Basics/Result.h"

#include "catch.hpp"
#include <type_traits>


using namespace arangodb;
using namespace std;

TEST_CASE("ResultTest", "[string]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test_StringBuffer1
////////////////////////////////////////////////////////////////////////////////

SECTION("test_StringBuffer1") {
  int  integer = 43;
  int& integer_lvalue_ref = integer;
 
  TypedResult<int&> b(integer_lvalue_ref);
  TypedResult<int>  a(integer);
  TypedResult<int>  c(std::move(integer_lvalue_ref));

  CHECK((std::is_same<decltype(a.value), int>::value));
  CHECK((std::is_same<decltype(b.value), int&>::value));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
