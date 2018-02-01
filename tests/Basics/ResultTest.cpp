////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ResultValue class
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


ResultValue<int> function_a(int i){
  return ResultValue<int>(i);
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

SECTION("testResultTest1") {
  int  integer = 43;
  int& integerLvalueRef = integer;
  int const&  integerLvalue_cref = integer;
  int* integerPtr = &integer;
  std::string str = "arangodb rocks";

  struct noMove {
    int member = 4;
    noMove() = default;
    noMove(noMove&&) = delete;
    noMove& operator=(noMove&&) = default;
  };

  struct noMoveAssign {
    int member = 4;
    noMoveAssign() = default;
    noMoveAssign(noMoveAssign&&) = default;
    noMoveAssign& operator=(noMoveAssign&&) = delete;
  };

  ResultValue<int>        intResult(integer);
  CHECK((std::is_same<decltype(intResult.value), int>::value));

  ResultValue<int&>       lvaluerefIntResult(integerLvalueRef);
  CHECK((std::is_same<decltype(lvaluerefIntResult.value), int&>::value));

  ResultValue<int const&> lvalue_crefIntResult(integerLvalue_cref);
  CHECK((std::is_same<decltype(lvalue_crefIntResult.value), int const&>::value));

  ResultValue<int> rvalueIntResult(std::move(integerLvalueRef));
  CHECK((std::is_same<decltype(rvalueIntResult.value), int>::value));

  ResultValue<int*> intPtrResult(integerPtr);
  CHECK((std::is_same<decltype(intPtrResult.value), int*>::value));

  ResultValue<std::string> stringResult{str};
  std::cout << "stringMoveResult" << endl;
  ResultValue<std::string> stringMoveResult{std::move(str)};

  ResultValue<noMove>  noMoveResult(noMove{});
  std::cout << "noMoveAssign" << endl;
  ResultValue<noMoveAssign>  noMoveAssignResult(noMoveAssign{});

  //function_b();

}


////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
