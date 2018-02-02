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

    return rv.takeResult(); //still move the result forward
}


TEST_CASE("ResultTest", "[string]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test_StringBuffer1
////////////////////////////////////////////////////////////////////////////////

SECTION("testResultTest1") {
  int  integer = 43;
  int& integerLvalueRef = integer;
  int const&  integerLvalueCref = integer;
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

// pointer and ref
#ifdef RESULT_DEBUG
  std::cout << "\nint* - int* - copy" << endl;
#endif
  ResultValue<int*> intPtrResult(integerPtr);
  CHECK((std::is_same<decltype(intPtrResult.value), int*>::value));

#ifdef RESULT_DEBUG
  std::cout << "\nint& - int& - copy" << endl;
#endif
  ResultValue<int&> lvalueRefIntResult(integerLvalueRef);
  CHECK((std::is_same<decltype(lvalueRefIntResult.value), int&>::value));

#ifdef RESULT_DEBUG
  std::cout << "\nint const& - int const& - copy" << endl;
#endif
  ResultValue<int const&> lvalueCrefIntResult(integerLvalueCref);
  CHECK((std::is_same<decltype(lvalueCrefIntResult.value), int const&>::value));

// lvalues
#ifdef RESULT_DEBUG
  std::cout << "\nint - int - copy" << endl;
#endif
  ResultValue<int> intResult(integer);
  CHECK((std::is_same<decltype(intResult.value), int>::value));

#ifdef RESULT_DEBUG
  std::cout << "\nint - int& - copy" << endl;
#endif
  ResultValue<int> lvalueIntResult(integerLvalueRef);
  CHECK((std::is_same<decltype(lvalueIntResult.value), int>::value));

#ifdef RESULT_DEBUG
  std::cout << "\nstring - string - copy" << endl;
#endif
  ResultValue<std::string> stringResult{str};
  CHECK((std::is_same<decltype(stringResult.value), std::string>::value));

// rvalues / xvalues
#ifdef RESULT_DEBUG
  std::cout << "\nstring - string&& - move" << endl;
#endif
  ResultValue<std::string> stringMoveResult{std::move(str)};
  CHECK((std::is_same<decltype(stringMoveResult.value), std::string>::value));

#ifdef RESULT_DEBUG
  std::cout << "\nnoMove - noMove&& - move" << endl;
#endif
  ResultValue<noMove>  noMoveResult(noMove{});
  CHECK((std::is_same<decltype(noMoveResult.value), noMove>::value));

#ifdef RESULT_DEBUG
  std::cout << "\nnoMoveAssign - noMoveAssign&& - move" << endl;
#endif
  ResultValue<noMoveAssign>  noMoveAssignResult{noMoveAssign{}};
  CHECK((std::is_same<decltype(noMoveAssignResult.value), noMoveAssign>::value));

#ifdef RESULT_DEBUG
  std::cout << "\nfunction_b" << endl;
#endif
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
