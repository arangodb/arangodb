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

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/tri-strings.h"
#include "Basics/Utf8Helper.h"

#if _WIN32
#include "Basics/win-utils.h"
#define FIX_ICU_ENV     TRI_FixIcuDataEnv()
#else
#define FIX_ICU_ENV
#endif

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
    FIX_ICU_ENV;
    if (!arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollatorLanguage("")) {
      std::string msg =
        "cannot initialize ICU; please make sure ICU*dat is available; "
        "the variable ICU_DATA='";
      if (getenv("ICU_DATA") != nullptr) {
        msg += getenv("ICU_DATA");
      }
      msg += "' should point the directory containing the ICU*dat file.";
      BOOST_TEST_MESSAGE(msg);
      BOOST_CHECK_EQUAL(false, true);
    }
    BOOST_TEST_MESSAGE("setup utf8 string normalize test");
  }

  ~CNormalizeStringTestSetup () {
    BOOST_TEST_MESSAGE("tear-down utf8 string normalize test");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CNormalizeStringTest, CNormalizeStringTestSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test NFD to NFC
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_1) {
  
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
  char* result = TRI_normalize_utf8_to_NFC(TRI_CORE_MEM_ZONE, (const char*) decomposed, strlen((const char*) decomposed),&len);
/*
  size_t outLength;
  char* uni = TRI_EscapeUtf8StringZ (TRI_CORE_MEM_ZONE, (const char*) decomposed, strlen((const char*) decomposed), true, &outLength);
  printf("\nOriginal: %s\nEscaped: %s\n", decomposed, uni);

  char* uni2 = TRI_EscapeUtf8StringZ (TRI_CORE_MEM_ZONE, (const char*) composed, strlen((const char*) composed), true, &outLength);
  printf("\nOriginal: %s\nEscaped: %s\n", composed, uni2);
*/  
  size_t l1 = sizeof(composed) - 1;
  size_t l2 = strlen(result);
  BOOST_CHECK_EQUAL(l1, l2);
  BOOST_CHECK_EQUAL_COLLECTIONS((char*) composed, (char*) composed + l1, result, result + l2);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_2) {

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
  char* result = TRI_tolower_utf8(TRI_CORE_MEM_ZONE, (const char*) gruessgott1, (int32_t) strlen((const char*) gruessgott1), &len);


  //printf("\nOriginal: %s\nLower: %s (%d)\n", gruessgott1, result, len);
  size_t l1 = sizeof(lower) - 1;
  size_t l2 = strlen(result);
  BOOST_CHECK_EQUAL(l1, l2);
  BOOST_CHECK_EQUAL_COLLECTIONS((char*) lower, (char*) lower + l1, result, result + l2);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);  

  std::string testString((const char*) gruessgott1);
  std::string expectString((const char*) lower);
  std::string resultString = arangodb::basics::Utf8Helper::DefaultUtf8Helper.toLowerCase(testString);
  BOOST_CHECK_EQUAL(expectString, resultString);
  
  len = 0;
  result = TRI_tolower_utf8(TRI_CORE_MEM_ZONE, (const char*) gruessgott2, (int32_t) strlen((const char*) gruessgott2), &len);
  //printf("\nOriginal: %s\nLower: %s (%d)\n", gruessgott2, result, len);
  l2 = strlen(result);
  BOOST_CHECK_EQUAL(l1, l2);
  BOOST_CHECK_EQUAL_COLLECTIONS((char*) lower, (char*) lower + l1, result, result + l2);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);    
}

BOOST_AUTO_TEST_CASE (tst_3) {
  std::string testString   = "aäoöuüAÄOÖUÜ";
  std::string expectString = "aäoöuüaäoöuü";
  std::string resultString = arangodb::basics::Utf8Helper::DefaultUtf8Helper.toLowerCase(testString);
  BOOST_CHECK_EQUAL(expectString, resultString);

  testString   = "aäoöuüAÄOÖUÜ";
  expectString = "AÄOÖUÜAÄOÖUÜ";
  resultString = arangodb::basics::Utf8Helper::DefaultUtf8Helper.toUpperCase(testString);
  BOOST_CHECK_EQUAL(expectString, resultString);
}

BOOST_AUTO_TEST_CASE (tst_4) {
  std::string testString   = "Der Müller geht in die Post.";
  
  std::vector<std::string> words;
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.getWords(words, testString, 3, UINT32_MAX, true);
  BOOST_CHECK(!words.empty());
  
  BOOST_CHECK_EQUAL(5UL, words.size());
  BOOST_CHECK_EQUAL("der", words[0]);
  BOOST_CHECK_EQUAL("müller", words[1]);
  BOOST_CHECK_EQUAL("geht", words[2]);
  BOOST_CHECK_EQUAL("die", words[3]);
  BOOST_CHECK_EQUAL("post", words[4]);
  
  words.clear();
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.getWords(words, testString, 4, UINT32_MAX, true);
  BOOST_CHECK(!words.empty());
  
  BOOST_CHECK_EQUAL(3UL, words.size());
  BOOST_CHECK_EQUAL("müller", words[0]);
  BOOST_CHECK_EQUAL("geht", words[1]);
  BOOST_CHECK_EQUAL("post", words[2]);
    
  words.clear();
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.getWords(words, "", 3, UINT32_MAX, true);
  BOOST_CHECK(words.empty());
}

BOOST_AUTO_TEST_CASE (tst_5) {
  std::string testString   = "Der Müller geht in die Post.";
  
  std::vector<std::string> words;
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.getWords(words, testString, 3, UINT32_MAX, false);
  BOOST_CHECK(!words.empty());
  
  BOOST_CHECK_EQUAL(5UL, words.size());
  BOOST_CHECK_EQUAL("Der", words[0]);
  BOOST_CHECK_EQUAL("Müller", words[1]);
  BOOST_CHECK_EQUAL("geht", words[2]);
  BOOST_CHECK_EQUAL("die", words[3]);
  BOOST_CHECK_EQUAL("Post", words[4]);
    
  words.clear();
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.getWords(words, testString, 4, UINT32_MAX, false);
  BOOST_CHECK(!words.empty());
  
  BOOST_CHECK_EQUAL(3UL, words.size());
  BOOST_CHECK_EQUAL("Müller", words[0]);
  BOOST_CHECK_EQUAL("geht", words[1]);
  BOOST_CHECK_EQUAL("Post", words[2]);
    
  words.clear();
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.getWords(words, "", 4, UINT32_MAX, false);
  BOOST_CHECK(words.empty());
}

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

