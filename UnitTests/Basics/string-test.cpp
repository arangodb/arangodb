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

#include <boost/test/unit_test.hpp>

#include "BasicsC/tri-strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CStringSetup {
  CStringSetup () {
    BOOST_TEST_MESSAGE("setup string test");
  }

  ~CStringSetup () {
    BOOST_TEST_MESSAGE("tear-down string test");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CStringTest, CStringSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing (no changes) 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_lower_ascii_no_change) {
  char* result;

  result = TRI_LowerAsciiString("this is supposed to stay the same");
  BOOST_CHECK_EQUAL("this is supposed to stay the same", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  result = TRI_LowerAsciiString("this is also supposed to stay the same");
  BOOST_CHECK_EQUAL("this is also supposed to stay the same", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // punctuation should not change
  result = TRI_LowerAsciiString("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>");
  BOOST_CHECK_EQUAL("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // whitespace should not change
  result = TRI_LowerAsciiString("  \t \n \r \n");
  BOOST_CHECK_EQUAL("  \t \n \r \n", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // test an empty string
  result = TRI_LowerAsciiString("");
  BOOST_CHECK_EQUAL("", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_lower_ascii) {
  char* result;

  result = TRI_LowerAsciiString("This MUST be converted into LOWER CASE!");
  BOOST_CHECK_EQUAL("this must be converted into lower case!", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  result = TRI_LowerAsciiString("SCREAMING OUT LOUD");
  BOOST_CHECK_EQUAL("screaming out loud", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing with non-ASCII
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_lower_ascii_non_ascii) {
  char* result;

  result = TRI_LowerAsciiString("äöüÄÖÜß");
  BOOST_CHECK_EQUAL("äöüÄÖÜß", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  result = TRI_LowerAsciiString("코리아닷컴");
  BOOST_CHECK_EQUAL("코리아닷컴", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  result = TRI_LowerAsciiString("своих партнеров");
  BOOST_CHECK_EQUAL("своих партнеров", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper casing (no changes) 
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_upper_ascii_no_change) {
  char* result;

  result = TRI_UpperAsciiString("THIS IS SUPPOSED TO STAY THE SAME");
  BOOST_CHECK_EQUAL("THIS IS SUPPOSED TO STAY THE SAME", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  result = TRI_UpperAsciiString("THIS IS ALSO SUPPOSED TO STAY THE SAME");
  BOOST_CHECK_EQUAL("THIS IS ALSO SUPPOSED TO STAY THE SAME", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // punctuation should not change
  result = TRI_UpperAsciiString("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>");
  BOOST_CHECK_EQUAL("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // whitespace should not change
  result = TRI_UpperAsciiString("  \t \n \r \n");
  BOOST_CHECK_EQUAL("  \t \n \r \n", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // test an empty string
  result = TRI_UpperAsciiString("");
  BOOST_CHECK_EQUAL("", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper casing
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_upper_ascii) {
  char* result;

  result = TRI_UpperAsciiString("This must be converted into upper CASE!");
  BOOST_CHECK_EQUAL("THIS MUST BE CONVERTED INTO UPPER CASE!", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  result = TRI_UpperAsciiString("silently whispering");
  BOOST_CHECK_EQUAL("SILENTLY WHISPERING", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper casing with non-ASCII
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_upper_ascii_non_ascii) {
  char* result;

  result = TRI_UpperAsciiString("äöüÄÖÜß");
  BOOST_CHECK_EQUAL("äöüÄÖÜß", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  result = TRI_UpperAsciiString("코리아닷컴");
  BOOST_CHECK_EQUAL("코리아닷컴", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  result = TRI_UpperAsciiString("своих партнеров");
  BOOST_CHECK_EQUAL("своих партнеров", result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test equal string
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_equal_string) {
  BOOST_CHECK_EQUAL(true, TRI_EqualString("", ""));
  BOOST_CHECK_EQUAL(true, TRI_EqualString(" ", " "));
  BOOST_CHECK_EQUAL(true, TRI_EqualString("a", "a"));
  BOOST_CHECK_EQUAL(true, TRI_EqualString("the quick brown fox", "the quick brown fox"));
  BOOST_CHECK_EQUAL(true, TRI_EqualString("The Quick Brown FOX", "The Quick Brown FOX"));
  BOOST_CHECK_EQUAL(true, TRI_EqualString("\"\t\r\n ", "\"\t\r\n "));
  
  BOOST_CHECK_EQUAL(false, TRI_EqualString("", " "));
  BOOST_CHECK_EQUAL(false, TRI_EqualString(" ", ""));
  BOOST_CHECK_EQUAL(false, TRI_EqualString("a", ""));
  BOOST_CHECK_EQUAL(false, TRI_EqualString("a", "a "));
  BOOST_CHECK_EQUAL(false, TRI_EqualString(" a", "a"));
  BOOST_CHECK_EQUAL(false, TRI_EqualString("A", "a"));
  BOOST_CHECK_EQUAL(false, TRI_EqualString("a", "A"));
  BOOST_CHECK_EQUAL(false, TRI_EqualString("", "0"));
  BOOST_CHECK_EQUAL(false, TRI_EqualString("0", ""));
  BOOST_CHECK_EQUAL(false, TRI_EqualString(" ", "0"));
  BOOST_CHECK_EQUAL(false, TRI_EqualString("0", " "));
  BOOST_CHECK_EQUAL(false, TRI_EqualString("case matters", "Case matters"));
  BOOST_CHECK_EQUAL(false, TRI_EqualString("CASE matters", "CASE matterS"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test case equal string
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_case_equal_string) {
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString("", ""));
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString(" ", " "));
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString("a", "a"));
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString("the quick brown fox", "the quick brown fox"));
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString("The Quick Brown FOX", "The Quick Brown FOX"));
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString("\"\t\r\n ", "\"\t\r\n "));
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString("A", "a"));
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString("a", "A"));
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString("case matters", "Case matters"));
  BOOST_CHECK_EQUAL(true, TRI_CaseEqualString("CASE matters", "CASE matterS"));
  
  BOOST_CHECK_EQUAL(false, TRI_CaseEqualString("", " "));
  BOOST_CHECK_EQUAL(false, TRI_CaseEqualString(" ", ""));
  BOOST_CHECK_EQUAL(false, TRI_CaseEqualString("a", ""));
  BOOST_CHECK_EQUAL(false, TRI_CaseEqualString("a", "a "));
  BOOST_CHECK_EQUAL(false, TRI_CaseEqualString(" a", "a"));
  BOOST_CHECK_EQUAL(false, TRI_CaseEqualString("", "0"));
  BOOST_CHECK_EQUAL(false, TRI_CaseEqualString("0", ""));
  BOOST_CHECK_EQUAL(false, TRI_CaseEqualString(" ", "0"));
  BOOST_CHECK_EQUAL(false, TRI_CaseEqualString("0", " "));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test prefix string
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_prefix_string) {
  BOOST_CHECK_EQUAL(true, TRI_IsPrefixString("the quick brown fox", "the"));
  BOOST_CHECK_EQUAL(true, TRI_IsPrefixString("the quick brown fox", "th"));
  BOOST_CHECK_EQUAL(true, TRI_IsPrefixString("the quick brown fox", "t"));
  BOOST_CHECK_EQUAL(true, TRI_IsPrefixString("the quick brown fox", "the q"));
  BOOST_CHECK_EQUAL(true, TRI_IsPrefixString(" the quick brown fox", " "));
  BOOST_CHECK_EQUAL(true, TRI_IsPrefixString("the fox", "the fox"));
  BOOST_CHECK_EQUAL(true, TRI_IsPrefixString("\t\r\n0", "\t"));
  BOOST_CHECK_EQUAL(true, TRI_IsPrefixString("\t\r\n0", "\t\r"));
  BOOST_CHECK_EQUAL(true, TRI_IsPrefixString("the fox", ""));
  
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", "The"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", " the"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", "the  quick"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", "the q "));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", "foo"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", "a"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", "quick"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", "he quick"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", "fox"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the quick brown fox", "T"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("The quick brown fox", "the"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("THE QUICK BROWN FOX", "The"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("THE QUICK BROWN FOX", "the"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("THE QUICK BROWN FOX", "THE quick"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString(" the quick brown fox", "the"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("the fox", " "));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("\r\n0", "\n"));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("\r\n0", " "));
  BOOST_CHECK_EQUAL(false, TRI_IsPrefixString("", "the"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
