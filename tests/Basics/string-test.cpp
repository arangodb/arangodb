////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for string utility functions
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

#include "catch.hpp"

#include "Basics/tri-strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

TEST_CASE("CStringTest", "[string]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing (no changes) 
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_lower_ascii_no_change") {
  char* result;

  result = TRI_LowerAsciiString("this is supposed to stay the same");
  CHECK("this is supposed to stay the same" == std::string(result));
  TRI_FreeString(result);
  
  result = TRI_LowerAsciiString("this is also supposed to stay the same");
  CHECK("this is also supposed to stay the same" == std::string(result));
  TRI_FreeString(result);
  
  // punctuation should not change
  result = TRI_LowerAsciiString("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>");
  CHECK(std::string("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>") == std::string(result));
  TRI_FreeString(result);
  
  // whitespace should not change
  result = TRI_LowerAsciiString(("  \t \n \r \n"));
  CHECK(std::string("  \t \n \r \n") == result);
  TRI_FreeString(result);
  
  // test an empty string
  result = TRI_LowerAsciiString("");
  CHECK(std::string("") == std::string(result));
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_lower_ascii") {
  char* result;

  result = TRI_LowerAsciiString("This MUST be converted into LOWER CASE!");
  CHECK(std::string("this must be converted into lower case!") == result);
  TRI_FreeString(result);
  
  result = TRI_LowerAsciiString("SCREAMING OUT LOUD");
  CHECK(std::string("screaming out loud") == result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing with non-ASCII
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_lower_ascii_non_ascii") {
  char* result;

  result = TRI_LowerAsciiString("äöüÄÖÜß");
  CHECK(std::string("äöüÄÖÜß") == result);
  TRI_FreeString(result);
  
  result = TRI_LowerAsciiString("코리아닷컴");
  CHECK(std::string("코리아닷컴") == result);
  TRI_FreeString(result);
  
  result = TRI_LowerAsciiString("своих партнеров");
  CHECK(std::string("своих партнеров") == result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper casing (no changes) 
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_upper_ascii_no_change") {
  char* result;

  result = TRI_UpperAsciiString("THIS IS SUPPOSED TO STAY THE SAME");
  CHECK(std::string("THIS IS SUPPOSED TO STAY THE SAME") == result);
  TRI_FreeString(result);
  
  result = TRI_UpperAsciiString("THIS IS ALSO SUPPOSED TO STAY THE SAME");
  CHECK(std::string("THIS IS ALSO SUPPOSED TO STAY THE SAME") == result);
  TRI_FreeString(result);
  
  // punctuation should not change
  result = TRI_UpperAsciiString("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>");
  CHECK(std::string("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>") == result);
  TRI_FreeString(result);
  
  // whitespace should not change
  result = TRI_UpperAsciiString("  \t \n \r \n");
  CHECK(std::string("  \t \n \r \n") == result);
  TRI_FreeString(result);
  
  // test an empty string
  result = TRI_UpperAsciiString("");
  CHECK(std::string("") == result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper casing
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_upper_ascii") {
  char* result;

  result = TRI_UpperAsciiString("This must be converted into upper CASE!");
  CHECK(std::string("THIS MUST BE CONVERTED INTO UPPER CASE!") == result);
  TRI_FreeString(result);
  
  result = TRI_UpperAsciiString("silently whispering");
  CHECK(std::string("SILENTLY WHISPERING") == result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper casing with non-ASCII
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_upper_ascii_non_ascii") {
  char* result;

  result = TRI_UpperAsciiString("äöüÄÖÜß");
  CHECK(std::string("äöüÄÖÜß") == result);
  TRI_FreeString(result);
  
  result = TRI_UpperAsciiString("코리아닷컴");
  CHECK(std::string("코리아닷컴") == result);
  TRI_FreeString(result);
  
  result = TRI_UpperAsciiString("своих партнеров");
  CHECK(std::string("своих партнеров") == result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test equal string
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_equal_string") {
  CHECK(true == TRI_EqualString("", ""));
  CHECK(true == TRI_EqualString(" ", " "));
  CHECK(true == TRI_EqualString("a", "a"));
  CHECK(true == TRI_EqualString("the quick brown fox", "the quick brown fox"));
  CHECK(true == TRI_EqualString("The Quick Brown FOX", "The Quick Brown FOX"));
  CHECK(true == TRI_EqualString("\"\t\r\n ", "\"\t\r\n "));
  
  CHECK(false == TRI_EqualString("", " "));
  CHECK(false == TRI_EqualString(" ", ""));
  CHECK(false == TRI_EqualString("a", ""));
  CHECK(false == TRI_EqualString("a", "a "));
  CHECK(false == TRI_EqualString(" a", "a"));
  CHECK(false == TRI_EqualString("A", "a"));
  CHECK(false == TRI_EqualString("a", "A"));
  CHECK(false == TRI_EqualString("", "0"));
  CHECK(false == TRI_EqualString("0", ""));
  CHECK(false == TRI_EqualString(" ", "0"));
  CHECK(false == TRI_EqualString("0", " "));
  CHECK(false == TRI_EqualString("case matters", "Case matters"));
  CHECK(false == TRI_EqualString("CASE matters", "CASE matterS"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test case equal string
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_case_equal_string") {
  CHECK(true == TRI_CaseEqualString("", ""));
  CHECK(true == TRI_CaseEqualString(" ", " "));
  CHECK(true == TRI_CaseEqualString("a", "a"));
  CHECK(true == TRI_CaseEqualString("the quick brown fox", "the quick brown fox"));
  CHECK(true == TRI_CaseEqualString("The Quick Brown FOX", "The Quick Brown FOX"));
  CHECK(true == TRI_CaseEqualString("\"\t\r\n ", "\"\t\r\n "));
  CHECK(true == TRI_CaseEqualString("A", "a"));
  CHECK(true == TRI_CaseEqualString("a", "A"));
  CHECK(true == TRI_CaseEqualString("case matters", "Case matters"));
  CHECK(true == TRI_CaseEqualString("CASE matters", "CASE matterS"));
  
  CHECK(false == TRI_CaseEqualString("", " "));
  CHECK(false == TRI_CaseEqualString(" ", ""));
  CHECK(false == TRI_CaseEqualString("a", ""));
  CHECK(false == TRI_CaseEqualString("a", "a "));
  CHECK(false == TRI_CaseEqualString(" a", "a"));
  CHECK(false == TRI_CaseEqualString("", "0"));
  CHECK(false == TRI_CaseEqualString("0", ""));
  CHECK(false == TRI_CaseEqualString(" ", "0"));
  CHECK(false == TRI_CaseEqualString("0", " "));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test prefix string
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_prefix_string") {
  CHECK(true == TRI_IsPrefixString("the quick brown fox", "the"));
  CHECK(true == TRI_IsPrefixString("the quick brown fox", "th"));
  CHECK(true == TRI_IsPrefixString("the quick brown fox", "t"));
  CHECK(true == TRI_IsPrefixString("the quick brown fox", "the q"));
  CHECK(true == TRI_IsPrefixString(" the quick brown fox", " "));
  CHECK(true == TRI_IsPrefixString("the fox", "the fox"));
  CHECK(true == TRI_IsPrefixString("\t\r\n0", "\t"));
  CHECK(true == TRI_IsPrefixString("\t\r\n0", "\t\r"));
  CHECK(true == TRI_IsPrefixString("the fox", ""));
  
  CHECK(false == TRI_IsPrefixString("the quick brown fox", "The"));
  CHECK(false == TRI_IsPrefixString("the quick brown fox", " the"));
  CHECK(false == TRI_IsPrefixString("the quick brown fox", "the  quick"));
  CHECK(false == TRI_IsPrefixString("the quick brown fox", "the q "));
  CHECK(false == TRI_IsPrefixString("the quick brown fox", "foo"));
  CHECK(false == TRI_IsPrefixString("the quick brown fox", "a"));
  CHECK(false == TRI_IsPrefixString("the quick brown fox", "quick"));
  CHECK(false == TRI_IsPrefixString("the quick brown fox", "he quick"));
  CHECK(false == TRI_IsPrefixString("the quick brown fox", "fox"));
  CHECK(false == TRI_IsPrefixString("the quick brown fox", "T"));
  CHECK(false == TRI_IsPrefixString("The quick brown fox", "the"));
  CHECK(false == TRI_IsPrefixString("THE QUICK BROWN FOX", "The"));
  CHECK(false == TRI_IsPrefixString("THE QUICK BROWN FOX", "the"));
  CHECK(false == TRI_IsPrefixString("THE QUICK BROWN FOX", "THE quick"));
  CHECK(false == TRI_IsPrefixString(" the quick brown fox", "the"));
  CHECK(false == TRI_IsPrefixString("the fox", " "));
  CHECK(false == TRI_IsPrefixString("\r\n0", "\n"));
  CHECK(false == TRI_IsPrefixString("\r\n0", " "));
  CHECK(false == TRI_IsPrefixString("", "the"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
