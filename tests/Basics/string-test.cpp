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

#include "gtest/gtest.h"

#include "Basics/tri-strings.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing (no changes) 
////////////////////////////////////////////////////////////////////////////////

TEST(CStringTest, tst_lower_ascii_no_change) {
  char* result;

  result = TRI_LowerAsciiString("this is supposed to stay the same");
  EXPECT_EQ("this is supposed to stay the same", std::string(result));
  TRI_FreeString(result);
  
  result = TRI_LowerAsciiString("this is also supposed to stay the same");
  EXPECT_EQ("this is also supposed to stay the same", std::string(result));
  TRI_FreeString(result);
  
  // punctuation should not change
  result = TRI_LowerAsciiString("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>");
  EXPECT_EQ(std::string("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>"), std::string(result));
  TRI_FreeString(result);
  
  // whitespace should not change
  result = TRI_LowerAsciiString(("  \t \n \r \n"));
  EXPECT_EQ(std::string("  \t \n \r \n"), result);
  TRI_FreeString(result);
  
  // test an empty string
  result = TRI_LowerAsciiString("");
  EXPECT_EQ(std::string(""), std::string(result));
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing
////////////////////////////////////////////////////////////////////////////////

TEST(CStringTest, tst_lower_ascii) {
  char* result;

  result = TRI_LowerAsciiString("This MUST be converted into LOWER CASE!");
  EXPECT_EQ(std::string("this must be converted into lower case!"), result);
  TRI_FreeString(result);
  
  result = TRI_LowerAsciiString("SCREAMING OUT LOUD");
  EXPECT_EQ(std::string("screaming out loud"), result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test lower casing with non-ASCII
////////////////////////////////////////////////////////////////////////////////

TEST(CStringTest, tst_lower_ascii_non_ascii) {
  char* result;

  result = TRI_LowerAsciiString("äöüÄÖÜß");
  EXPECT_EQ(std::string("äöüÄÖÜß"), result);
  TRI_FreeString(result);
  
  result = TRI_LowerAsciiString("코리아닷컴");
  EXPECT_EQ(std::string("코리아닷컴"), result);
  TRI_FreeString(result);
  
  result = TRI_LowerAsciiString("своих партнеров");
  EXPECT_EQ(std::string("своих партнеров"), result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper casing (no changes) 
////////////////////////////////////////////////////////////////////////////////

TEST(CStringTest, tst_upper_ascii_no_change) {
  char* result;

  result = TRI_UpperAsciiString("THIS IS SUPPOSED TO STAY THE SAME");
  EXPECT_EQ(std::string("THIS IS SUPPOSED TO STAY THE SAME"), result);
  TRI_FreeString(result);
  
  result = TRI_UpperAsciiString("THIS IS ALSO SUPPOSED TO STAY THE SAME");
  EXPECT_EQ(std::string("THIS IS ALSO SUPPOSED TO STAY THE SAME"), result);
  TRI_FreeString(result);
  
  // punctuation should not change
  result = TRI_UpperAsciiString("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>");
  EXPECT_EQ(std::string("01234567890,.;:-_#'+*~!\"§$%&/()[]{}=?\\|<>"), result);
  TRI_FreeString(result);
  
  // whitespace should not change
  result = TRI_UpperAsciiString("  \t \n \r \n");
  EXPECT_EQ(std::string("  \t \n \r \n"), result);
  TRI_FreeString(result);
  
  // test an empty string
  result = TRI_UpperAsciiString("");
  EXPECT_EQ(std::string(""), result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper casing
////////////////////////////////////////////////////////////////////////////////

TEST(CStringTest, tst_upper_ascii) {
  char* result;

  result = TRI_UpperAsciiString("This must be converted into upper CASE!");
  EXPECT_EQ(std::string("THIS MUST BE CONVERTED INTO UPPER CASE!"), result);
  TRI_FreeString(result);
  
  result = TRI_UpperAsciiString("silently whispering");
  EXPECT_EQ(std::string("SILENTLY WHISPERING"), result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test upper casing with non-ASCII
////////////////////////////////////////////////////////////////////////////////

TEST(CStringTest, tst_upper_ascii_non_ascii) {
  char* result;

  result = TRI_UpperAsciiString("äöüÄÖÜß");
  EXPECT_EQ(std::string("äöüÄÖÜß"), result);
  TRI_FreeString(result);
  
  result = TRI_UpperAsciiString("코리아닷컴");
  EXPECT_EQ(std::string("코리아닷컴"), result);
  TRI_FreeString(result);
  
  result = TRI_UpperAsciiString("своих партнеров");
  EXPECT_EQ(std::string("своих партнеров"), result);
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test equal string
////////////////////////////////////////////////////////////////////////////////

TEST(CStringTest, tst_equal_string) {
  EXPECT_TRUE(TRI_EqualString("", ""));
  EXPECT_TRUE(TRI_EqualString(" ", " "));
  EXPECT_TRUE(TRI_EqualString("a", "a"));
  EXPECT_TRUE(TRI_EqualString("the quick brown fox", "the quick brown fox"));
  EXPECT_TRUE(TRI_EqualString("The Quick Brown FOX", "The Quick Brown FOX"));
  EXPECT_TRUE(TRI_EqualString("\"\t\r\n ", "\"\t\r\n "));
  
  EXPECT_FALSE(TRI_EqualString("", " "));
  EXPECT_FALSE(TRI_EqualString(" ", ""));
  EXPECT_FALSE(TRI_EqualString("a", ""));
  EXPECT_FALSE(TRI_EqualString("a", "a "));
  EXPECT_FALSE(TRI_EqualString(" a", "a"));
  EXPECT_FALSE(TRI_EqualString("A", "a"));
  EXPECT_FALSE(TRI_EqualString("a", "A"));
  EXPECT_FALSE(TRI_EqualString("", "0"));
  EXPECT_FALSE(TRI_EqualString("0", ""));
  EXPECT_FALSE(TRI_EqualString(" ", "0"));
  EXPECT_FALSE(TRI_EqualString("0", " "));
  EXPECT_FALSE(TRI_EqualString("case matters", "Case matters"));
  EXPECT_FALSE(TRI_EqualString("CASE matters", "CASE matterS"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test case equal string
////////////////////////////////////////////////////////////////////////////////

TEST(CStringTest, tst_case_equal_string) {
  EXPECT_TRUE(TRI_CaseEqualString("", ""));
  EXPECT_TRUE(TRI_CaseEqualString(" ", " "));
  EXPECT_TRUE(TRI_CaseEqualString("a", "a"));
  EXPECT_TRUE(TRI_CaseEqualString("the quick brown fox", "the quick brown fox"));
  EXPECT_TRUE(TRI_CaseEqualString("The Quick Brown FOX", "The Quick Brown FOX"));
  EXPECT_TRUE(TRI_CaseEqualString("\"\t\r\n ", "\"\t\r\n "));
  EXPECT_TRUE(TRI_CaseEqualString("A", "a"));
  EXPECT_TRUE(TRI_CaseEqualString("a", "A"));
  EXPECT_TRUE(TRI_CaseEqualString("case matters", "Case matters"));
  EXPECT_TRUE(TRI_CaseEqualString("CASE matters", "CASE matterS"));
  
  EXPECT_FALSE(TRI_CaseEqualString("", " "));
  EXPECT_FALSE(TRI_CaseEqualString(" ", ""));
  EXPECT_FALSE(TRI_CaseEqualString("a", ""));
  EXPECT_FALSE(TRI_CaseEqualString("a", "a "));
  EXPECT_FALSE(TRI_CaseEqualString(" a", "a"));
  EXPECT_FALSE(TRI_CaseEqualString("", "0"));
  EXPECT_FALSE(TRI_CaseEqualString("0", ""));
  EXPECT_FALSE(TRI_CaseEqualString(" ", "0"));
  EXPECT_FALSE(TRI_CaseEqualString("0", " "));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test prefix string
////////////////////////////////////////////////////////////////////////////////

TEST(CStringTest, tst_prefix_string) {
  EXPECT_TRUE(TRI_IsPrefixString("the quick brown fox", "the"));
  EXPECT_TRUE(TRI_IsPrefixString("the quick brown fox", "th"));
  EXPECT_TRUE(TRI_IsPrefixString("the quick brown fox", "t"));
  EXPECT_TRUE(TRI_IsPrefixString("the quick brown fox", "the q"));
  EXPECT_TRUE(TRI_IsPrefixString(" the quick brown fox", " "));
  EXPECT_TRUE(TRI_IsPrefixString("the fox", "the fox"));
  EXPECT_TRUE(TRI_IsPrefixString("\t\r\n0", "\t"));
  EXPECT_TRUE(TRI_IsPrefixString("\t\r\n0", "\t\r"));
  EXPECT_TRUE(TRI_IsPrefixString("the fox", ""));
  
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", "The"));
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", " the"));
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", "the  quick"));
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", "the q "));
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", "foo"));
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", "a"));
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", "quick"));
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", "he quick"));
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", "fox"));
  EXPECT_FALSE(TRI_IsPrefixString("the quick brown fox", "T"));
  EXPECT_FALSE(TRI_IsPrefixString("The quick brown fox", "the"));
  EXPECT_FALSE(TRI_IsPrefixString("THE QUICK BROWN FOX", "The"));
  EXPECT_FALSE(TRI_IsPrefixString("THE QUICK BROWN FOX", "the"));
  EXPECT_FALSE(TRI_IsPrefixString("THE QUICK BROWN FOX", "THE quick"));
  EXPECT_FALSE(TRI_IsPrefixString(" the quick brown fox", "the"));
  EXPECT_FALSE(TRI_IsPrefixString("the fox", " "));
  EXPECT_FALSE(TRI_IsPrefixString("\r\n0", "\n"));
  EXPECT_FALSE(TRI_IsPrefixString("\r\n0", " "));
  EXPECT_FALSE(TRI_IsPrefixString("", "the"));
}
