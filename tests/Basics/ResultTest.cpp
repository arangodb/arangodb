////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for TypedResult class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018, ArangoDB GmbH, Cologne, Germany
//
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Result.h"

#include "catch.hpp"
#include <type_traits>


using namespace arangodb;
using namespace std;


TypedResult<int> function_a(int i){
  return TypedResult<int>(i);
}

Result function_b(){
    auto rv = function_a(42); // create one result and try modify / reuse
                              // it to make copy elision happen

    if(rv.ok()) {
      CHECK(rv.value == 42); // do something with the value
    } else {
      rv.reset(rv.errorNumber(), "error in function_b: " + rv.errorMessage());
    }

    if(false){
      rv.reset(23, std::string("the result is not valid because some other condition did not hold"));
    }

    return rv.takeResult(); //still move the result forward
}


TEST_CASE("ResultTest", "[string]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test_StringBuffer1
////////////////////////////////////////////////////////////////////////////////

SECTION("test_ResultTest1") {
  int  integer = 43;
  int& integer_lvalue_ref = integer;
  int const&  integer_lvalue_cref = integer;
  int* integer_ptr = &integer;
  std::string str = "arangodb rocks";

  struct no_move {
    int member = 4;
    no_move() = default;
    no_move(no_move&&) = delete;
    no_move& operator=(no_move&&) = default;
  };

  TypedResult<int>        int_result(integer);
  CHECK((std::is_same<decltype(int_result.value), int>::value));

  TypedResult<int&>       lvalue_ref_int_result(integer_lvalue_ref);
  CHECK((std::is_same<decltype(lvalue_ref_int_result.value), int&>::value));

  TypedResult<int const&> lvalue_cref_int_result(integer_lvalue_cref);
  CHECK((std::is_same<decltype(lvalue_cref_int_result.value), int const&>::value));

  TypedResult<int> rvalue_int_result(std::move(integer_lvalue_ref));
  CHECK((std::is_same<decltype(rvalue_int_result.value), int>::value));

  TypedResult<int*> int_ptr_result(integer_ptr);
  CHECK((std::is_same<decltype(int_ptr_result.value), int*>::value));

  TypedResult<std::string> string_result{str};
  TypedResult<std::string> string_move_result{std::move(str)};

  TypedResult<no_move>  no_move_result(no_move{});

  function_b();

}


////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
