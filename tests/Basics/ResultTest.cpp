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


struct Verbose {
  bool byDefault = false;
  bool byCopy = false;
  bool byMove = false;
  bool byAssign = false;
  bool byMoveAssign = false;

  Verbose() :  byDefault(true) {}
  Verbose(Verbose const&) :  byCopy(true) {}
  Verbose(Verbose &&) :  byMove(true) {}

  Verbose& operator=(Verbose &){
    byMoveAssign = true;
    return *this;
  }

  Verbose& operator=(Verbose &&){
    byMoveAssign = true;
    return *this;
  }

  void show(){
#ifdef RESULT_DEBUG
    std::cout << std::boolalpha   << std::endl
              << " default: "      << byDefault << std::endl
              << " copy: "        << byCopy << std::endl
              << " move: "        << byMove << std::endl
              << " assign: "      << byAssign << std::endl
              << " move assign: " << byMoveAssign << std::endl
              ;
#endif
  }

};


// Helper Classes
struct VerboseNoMoveCtor {
  bool byDefault = false;
  bool byCopy = false;
  bool byMove = false;
  bool byAssign = false;
  bool byMoveAssign = false;

  VerboseNoMoveCtor() :  byDefault(true) {}
  VerboseNoMoveCtor(VerboseNoMoveCtor const&) :  byCopy(true) {}
  VerboseNoMoveCtor(VerboseNoMoveCtor &&) = delete;

  VerboseNoMoveCtor& operator=(VerboseNoMoveCtor const&){
    byAssign = true;
    return *this;
  }

  VerboseNoMoveCtor& operator=(VerboseNoMoveCtor &&){
    byMoveAssign = true;
    return *this;
  }

  void show(){
#ifdef RESULT_DEBUG
    std::cout << std::boolalpha   << std::endl
              << " default: "      << byDefault << std::endl
              << " copy: "        << byCopy << std::endl
              << " move: "        << byMove << std::endl
              << " assign: "      << byAssign << std::endl
              << " move assign: " << byMoveAssign << std::endl
              ;
#endif
  }

};

struct VerboseNoMoveAssign {
  bool byDefault = false;
  bool byCopy = false;
  bool byMove = false;
  bool byAssign = false;
  bool byMoveAssign = false;

  VerboseNoMoveAssign() :  byDefault(true) {}
  VerboseNoMoveAssign(VerboseNoMoveAssign const&) :  byCopy(true) {}
  VerboseNoMoveAssign(VerboseNoMoveAssign &&) :  byMove(true) {}
  VerboseNoMoveAssign& operator=(VerboseNoMoveAssign &&) = delete;

  VerboseNoMoveAssign& operator=(VerboseNoMoveAssign const&){
    byAssign = true;
    return *this;
  }

  void show(){
#ifdef RESULT_DEBUG
    std::cout << std::boolalpha   << std::endl
              << " default: "      << byDefault << std::endl
              << " copy: "        << byCopy << std::endl
              << " move: "        << byMove << std::endl
              << " assign: "      << byAssign << std::endl
              << " move assign: " << byMoveAssign << std::endl
              ;
#endif
  }

};
// Helper Classes

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
  std::cout << "\nfunction_b" << endl;
#endif
  function_b();

  ResultValue<Verbose> res1;
  CHECK((res1.value.byDefault));

  ResultValue<Verbose> res2{{}};
  CHECK((res2.value.byMove));

  ResultValue<VerboseNoMoveCtor> res3{{}};
  res3.value.show();
  CHECK((res3.value.byMoveAssign));

  std::unique_ptr<Verbose> val;
  ResultValue<Verbose*>(val.get());

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
