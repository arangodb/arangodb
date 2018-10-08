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

#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Basics/Utf8Helper.h"
#include "Basics/directories.h"

#include <velocypack/StringRef.h>

#include "icu-helper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CNormalizeStringTestSetup {
  CNormalizeStringTestSetup () {
    IcuInitializer::setup("./3rdParty/V8/v8/third_party/icu/common/icudtl.dat");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CNormalizeStringTest", "[string]") {
  CNormalizeStringTestSetup s;

////////////////////////////////////////////////////////////////////////////////
/// @brief test NFD to NFC
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_1") {
  
  /* "Grüß Gott. Здравствуйте! x=(-b±sqrt(b²-4ac))/(2a)  日本語,中文,한글" */
  static const unsigned char composed[] =
    { 'G', 'r', 0xC3, 0xBC, 0xC3, 0x9F, ' ', 'G', 'o', 't', 't', '.',
      ' ', 0xD0, 0x97, 0xD0, 0xB4, 0xD1, 0x80, 0xD0, 0xB0, 0xD0, 0xB2, 0xD1,
      0x81, 0xD1, 0x82, 0xD0, 0xB2, 0xD1, 0x83, 0xD0, 0xB9,
      0xD1, 0x82, 0xD0, 0xB5, '!', ' ', 'x', '=', '(', '-', 'b', 0xC2, 0xB1,
      's', 'q', 'r', 't', '(', 'b', 0xC2, 0xB2, '-', '4', 'a', 'c', ')', ')',
      '/', '(', '2', 'a', ')', ' ', ' ', 0xE6, 0x97, 0xA5, 0xE6, 0x9C, 0xAC,
      0xE8, 0xAA, 0x9E, ',', 0xE4, 0xB8, 0xAD, 0xE6, 0x96, 0x87, ',',
      0xED, 0x95, 0x9C,
      0xEA, 0xB8, 0x80, 'z', 0
    };
  
  static const unsigned char decomposed[] =
    { 'G', 'r', 0x75, 0xCC, 0x88, 0xC3, 0x9F, ' ', 'G', 'o', 't', 't', '.',
      ' ', 0xD0, 0x97, 0xD0, 0xB4, 0xD1, 0x80, 0xD0, 0xB0, 0xD0, 0xB2, 0xD1,
      0x81, 0xD1, 0x82, 0xD0, 0xB2, 0xD1, 0x83, 0xD0, 0xB8, 0xCC, 0x86,
      0xD1, 0x82, 0xD0, 0xB5, '!', ' ', 'x', '=', '(', '-', 'b', 0xC2, 0xB1,
      's', 'q', 'r', 't', '(', 'b', 0xC2, 0xB2, '-', '4', 'a', 'c', ')', ')',
      '/', '(', '2', 'a', ')', ' ', ' ', 0xE6, 0x97, 0xA5, 0xE6, 0x9C, 0xAC,
      0xE8, 0xAA, 0x9E, ',', 0xE4, 0xB8, 0xAD, 0xE6, 0x96, 0x87, ',',
      0xE1, 0x84, 0x92, 0xE1, 0x85, 0xA1, 0xE1, 0x86, 0xAB,
      0xE1, 0x84, 0x80, 0xE1, 0x85, 0xB3, 0xE1, 0x86, 0xAF, 'z', 0
    };    
  
  size_t len = 0;
  char* result = TRI_normalize_utf8_to_NFC((const char*) decomposed, strlen((const char*) decomposed),&len);
/*
  size_t outLength;
  char* uni = TRI_EscapeUtf8StringZ ((const char*) decomposed, strlen((const char*) decomposed), true, &outLength);
  printf("\nOriginal: %s\nEscaped: %s\n", decomposed, uni);

  char* uni2 = TRI_EscapeUtf8StringZ ((const char*) composed, strlen((const char*) composed), true, &outLength);
  printf("\nOriginal: %s\nEscaped: %s\n", composed, uni2);
*/  
  size_t l1 = sizeof(composed) - 1;
  size_t l2 = strlen(result);
  CHECK((l1) == l2);
  CHECK(std::string((char*) composed, l1) == std::string(result, l2));
  TRI_FreeString(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_2") {

  /* "Grüß Gott. Здравствуйте! x=(-b±sqrt(b²-4ac))/(2a)  日本語,中文,한글" */
  static const unsigned char gruessgott1[] =
    { 'G', 'r', 0xC3, 0xBC, 0xC3, 0x9F, ' ', 'G', 'o', 't', 't', '.', 0
    };
  
  static const unsigned char gruessgott2[] =
    { 'G', 'R', 0xC3, 0x9C, 0xC3, 0x9F, ' ', 'G', 'O', 'T', 't', '.', 0
    };    
  
  static const unsigned char lower[] =
    { 'g', 'r', 0xC3, 0xBC, 0xC3, 0x9F, ' ', 'g', 'o', 't', 't', '.', 0
    };    
  
  int32_t len = 0;
  char* result = TRI_tolower_utf8((const char*) gruessgott1, (int32_t) strlen((const char*) gruessgott1), &len);


  //printf("\nOriginal: %s\nLower: %s (%d)\n", gruessgott1, result, len);
  size_t l1 = sizeof(lower) - 1;
  size_t l2 = strlen(result);
  CHECK((l1) == l2);
  CHECK(std::string((char*) lower, l1) == std::string(result, l2));
  TRI_FreeString(result);  

  std::string testString((const char*) gruessgott1);
  std::string expectString((const char*) lower);
  std::string resultString = arangodb::basics::Utf8Helper::DefaultUtf8Helper.toLowerCase(testString);
  CHECK(std::string(expectString) == resultString);
  
  len = 0;
  result = TRI_tolower_utf8((const char*) gruessgott2, (int32_t) strlen((const char*) gruessgott2), &len);
  //printf("\nOriginal: %s\nLower: %s (%d)\n", gruessgott2, result, len);
  l2 = strlen(result);
  CHECK((l1) == l2);
  CHECK(std::string((char*) lower, l1) == std::string(result, l2));
  TRI_FreeString(result);    
}

SECTION("tst_3") {
  std::string testString   = "aäoöuüAÄOÖUÜ";
  std::string expectString = "aäoöuüaäoöuü";
  std::string resultString = arangodb::basics::Utf8Helper::DefaultUtf8Helper.toLowerCase(testString);
  CHECK(std::string(expectString) == resultString);

  testString   = "aäoöuüAÄOÖUÜ";
  expectString = "AÄOÖUÜAÄOÖUÜ";
  resultString = arangodb::basics::Utf8Helper::DefaultUtf8Helper.toUpperCase(testString);
  CHECK(std::string(expectString) == resultString);
}

SECTION("tst_4") {
  std::string testString   = "Der Müller geht in die Post.";
  
  std::set<std::string> words;
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(words, arangodb::velocypack::StringRef(testString), 3, UINT32_MAX, true);
  CHECK(!words.empty());
  
  CHECK((5UL) == words.size());
  CHECK(words.find(std::string("der")) != words.end());
  CHECK(words.find(std::string("müller")) != words.end());
  CHECK(words.find(std::string("geht")) != words.end());
  CHECK(words.find(std::string("die")) != words.end());
  CHECK(words.find(std::string("post")) != words.end());
  
  words.clear();
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(words, arangodb::velocypack::StringRef(testString), 4, UINT32_MAX, true);
  CHECK(!words.empty());
  
  CHECK((3UL) == words.size());
  CHECK(words.find(std::string("müller")) != words.end());
  CHECK(words.find(std::string("geht")) != words.end());
  CHECK(words.find(std::string("post")) != words.end());
  CHECK(words.find(std::string("der")) == words.end());
  CHECK(words.find(std::string("die")) == words.end());
  
  words.clear();
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(words, arangodb::velocypack::StringRef(""), 3, UINT32_MAX, true);
  CHECK(words.empty());
}

SECTION("tst_5") {
  std::string testString   = "Der Müller geht in die Post.";
  
  std::set<std::string> words;
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(words, arangodb::velocypack::StringRef(testString), 3, UINT32_MAX, false);
  CHECK(!words.empty());
  
  CHECK((5UL) == words.size());
  CHECK(words.find(std::string("Der")) != words.end());
  CHECK(words.find(std::string("Müller")) != words.end());
  CHECK(words.find(std::string("geht")) != words.end());
  CHECK(words.find(std::string("die")) != words.end());
  CHECK(words.find(std::string("Post")) != words.end());
    
  words.clear();
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(words, arangodb::velocypack::StringRef(testString), 4, UINT32_MAX, false);
  CHECK(!words.empty());
  
  CHECK((3UL) == words.size());
  CHECK(words.find(std::string("Müller")) != words.end());
  CHECK(words.find(std::string("geht")) != words.end());
  CHECK(words.find(std::string("Post")) != words.end());
  CHECK(words.find(std::string("der")) == words.end());
  CHECK(words.find(std::string("die")) == words.end());
  
  words.clear();
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(words, arangodb::velocypack::StringRef(""), 4, UINT32_MAX, false);
  CHECK(words.empty());
}
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

