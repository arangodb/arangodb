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

#include "BasicsC/utf8-helper.h"
#include "BasicsC/tri-strings.h"
#include "Basics/Utf8Helper.h"

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
  BOOST_CHECK_EQUAL((const char*) composed, (const char*) result);
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
  char* result = TRI_tolower_utf8(TRI_CORE_MEM_ZONE, (const char*) gruessgott1, strlen((const char*) gruessgott1), &len);


  //printf("\nOriginal: %s\nLower: %s (%d)\n", gruessgott1, result, len);
  BOOST_CHECK_EQUAL((const char*) lower, (const char*) result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);  

  std::string testString((const char*) gruessgott1);
  std::string expectString((const char*) lower);
  std::string resultString = triagens::basics::Utf8Helper::DefaultUtf8Helper.toLowerCase(testString);
  BOOST_CHECK_EQUAL(expectString, resultString);
  
  len = 0;
  result = TRI_tolower_utf8(TRI_CORE_MEM_ZONE, (const char*) gruessgott2, strlen((const char*) gruessgott2), &len);
  //printf("\nOriginal: %s\nLower: %s (%d)\n", gruessgott2, result, len);
  BOOST_CHECK_EQUAL((const char*) lower, (const char*) result);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);    
}

BOOST_AUTO_TEST_CASE (tst_3) {
  std::string testString   = "aäoöuüAÄOÖUÜ";
  std::string expectString = "aäoöuüaäoöuü";
  std::string resultString = triagens::basics::Utf8Helper::DefaultUtf8Helper.toLowerCase(testString);
  BOOST_CHECK_EQUAL(expectString, resultString);

  testString   = "aäoöuüAÄOÖUÜ";
  expectString = "AÄOÖUÜAÄOÖUÜ";
  resultString = triagens::basics::Utf8Helper::DefaultUtf8Helper.toUpperCase(testString);
  BOOST_CHECK_EQUAL(expectString, resultString);
}

BOOST_AUTO_TEST_CASE (tst_4) {
  std::string testString   = "Der Müller geht in die Post.";
  
  TRI_vector_string_t* words = triagens::basics::Utf8Helper::DefaultUtf8Helper.getWords(testString.c_str(), testString.length(), 3, UINT32_MAX, true);
  BOOST_CHECK(words != NULL);
  
  BOOST_CHECK_EQUAL(5, words->_length);
  BOOST_CHECK_EQUAL("der", words->_buffer[0]);
  BOOST_CHECK_EQUAL("müller", words->_buffer[1]);
  BOOST_CHECK_EQUAL("geht", words->_buffer[2]);
  BOOST_CHECK_EQUAL("die", words->_buffer[3]);
  BOOST_CHECK_EQUAL("post", words->_buffer[4]);
  
  TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);
  

  words = triagens::basics::Utf8Helper::DefaultUtf8Helper.getWords(testString.c_str(), testString.length(), 4, UINT32_MAX, true);
  BOOST_CHECK(words != NULL);
  
  BOOST_CHECK_EQUAL(3, words->_length);
  BOOST_CHECK_EQUAL("müller", words->_buffer[0]);
  BOOST_CHECK_EQUAL("geht", words->_buffer[1]);
  BOOST_CHECK_EQUAL("post", words->_buffer[2]);
    
  TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);

  words = triagens::basics::Utf8Helper::DefaultUtf8Helper.getWords(NULL, 0, 4, UINT32_MAX, true);
  BOOST_CHECK(words == NULL);
}

BOOST_AUTO_TEST_CASE (tst_5) {
  std::string testString   = "Der Müller geht in die Post.";
  
  TRI_vector_string_t* words = triagens::basics::Utf8Helper::DefaultUtf8Helper.getWords(testString.c_str(), testString.length(), 3, UINT32_MAX, false);
  BOOST_CHECK(words != NULL);
  
  BOOST_CHECK_EQUAL(5, words->_length);
  BOOST_CHECK_EQUAL("Der", words->_buffer[0]);
  BOOST_CHECK_EQUAL("Müller", words->_buffer[1]);
  BOOST_CHECK_EQUAL("geht", words->_buffer[2]);
  BOOST_CHECK_EQUAL("die", words->_buffer[3]);
  BOOST_CHECK_EQUAL("Post", words->_buffer[4]);
    
  TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);
  

  words = triagens::basics::Utf8Helper::DefaultUtf8Helper.getWords(testString.c_str(), testString.length(), 4, UINT32_MAX, false);
  BOOST_CHECK(words != NULL);
  
  BOOST_CHECK_EQUAL(3, words->_length);
  BOOST_CHECK_EQUAL("Müller", words->_buffer[0]);
  BOOST_CHECK_EQUAL("geht", words->_buffer[1]);
  BOOST_CHECK_EQUAL("Post", words->_buffer[2]);
    
  TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);

  words = triagens::basics::Utf8Helper::DefaultUtf8Helper.getWords(NULL, 0, 4, UINT32_MAX, false);
  BOOST_CHECK(words == NULL);
}

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

