////////////////////////////////////////////////////////////////////////////////
/// @brief Utf8/Utf16 Helper class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Frank Celler
/// @author Achim Brandt
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Utf8Helper.h"

#include "unicode/normalizer2.h"
#include "unicode/ucasemap.h"
#include "unicode/brkiter.h"
#include "unicode/ustdio.h"

#include "Logger/Logger.h"
#include "BasicsC/strings.h"
#include "BasicsC/utf8-helper.h"

using namespace triagens::basics;
using namespace std;


Utf8Helper Utf8Helper::DefaultUtf8Helper;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup UTF8
/// @{
////////////////////////////////////////////////////////////////////////////////

Utf8Helper::Utf8Helper () : _coll(0) {
  setCollatorLanguage("");
}

Utf8Helper::Utf8Helper (const string& lang) : _coll(0) {
  setCollatorLanguage(lang);
}

Utf8Helper::~Utf8Helper () {
  if (_coll) {
    delete _coll;
  }
}

int Utf8Helper::compareUtf8 (const char* left, const char* right) {
  if (!_coll) {
    LOGGER_ERROR("no Collator in Utf8Helper::compareUtf8()!");
    return (strcmp(left, right));
  }
  
  UErrorCode status = U_ZERO_ERROR;
  int result = _coll->compareUTF8(StringPiece(left), StringPiece(right), status);
  if(U_FAILURE(status)) {
    LOGGER_ERROR("error in Collator::compareUTF8(...): " << u_errorName(status));
    return (strcmp(left, right));
  }
  
  return result;
}

int Utf8Helper::compareUtf16 (const uint16_t* left, size_t leftLength, const uint16_t* right, size_t rightLength) {  
  if (!_coll) {
    LOGGER_ERROR("no Collator in Utf8Helper::compareUtf16()!");
    
    if (leftLength == rightLength) {
      return memcmp((const void*)left, (const void*)right, leftLength * 2);
    }
  
    int result = memcmp((const void*)left, (const void*)right, leftLength < rightLength ? leftLength * 2 : rightLength * 2);
  
    if (result == 0) {
      if (leftLength < rightLength) {
        return -1;
      }
      return 1;
    }
    
    return result;    
  }
  // ..........................................................................
  // Take note here: we are assuming that the ICU type UChar is two bytes.
  // There is no guarantee that this will be the case on all platforms and
  // compilers. 
  // ..........................................................................
  return _coll->compare((const UChar *)left, leftLength, (const UChar *)right, rightLength);
}

void Utf8Helper::setCollatorLanguage (const string& lang) {  
  UErrorCode status = U_ZERO_ERROR;   
    
  if (_coll) {
    ULocDataLocaleType type = ULOC_ACTUAL_LOCALE;
    const Locale& locale = _coll->getLocale(type, status);
    
    if(U_FAILURE(status)) {
      LOGGER_ERROR("error in Collator::getLocale(...): " << u_errorName(status));
      return;
    }
    if (lang == locale.getName()) {
      return;
    }
  }
  
  Collator* coll;
  if (lang == "") {
    // get default collator for empty language
    coll = Collator::createInstance(status); 
  }
  else {
    Locale locale(lang.c_str());
    coll = Collator::createInstance(locale, status);     
  }
  
  if(U_FAILURE(status)) {
    LOGGER_ERROR("error in Collator::createInstance(): " << u_errorName(status));
    if (coll) {
      delete coll;
    }
    return;
  }
  
  // set the default attributes for sorting:
  coll->setAttribute(UCOL_CASE_FIRST, UCOL_UPPER_FIRST, status);  // A < a
  coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_OFF, status);  // no normalization
  coll->setAttribute(UCOL_STRENGTH, UCOL_IDENTICAL, status);      // UCOL_IDENTICAL, UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY

  if(U_FAILURE(status)) {
    LOGGER_ERROR("error in Collator::setAttribute(...): " << u_errorName(status));
    delete coll;
    return;
  }
  
  if (_coll) {
    delete _coll;
  }

  _coll = coll;  
}

string Utf8Helper::getCollatorLanguage () {
  if (_coll) {
    UErrorCode status = U_ZERO_ERROR;
    ULocDataLocaleType type = ULOC_VALID_LOCALE;
    const Locale& locale = _coll->getLocale(type, status);
    
    if(U_FAILURE(status)) {
      LOGGER_ERROR("error in Collator::getLocale(...): " << u_errorName(status));
      return "";
    }
    return locale.getLanguage();
  }
  return "";
}

string Utf8Helper::getCollatorCountry () {
  if (_coll) {
    UErrorCode status = U_ZERO_ERROR;
    ULocDataLocaleType type = ULOC_VALID_LOCALE;
    const Locale& locale = _coll->getLocale(type, status);
    
    if(U_FAILURE(status)) {
      LOGGER_ERROR("error in Collator::getLocale(...): " << u_errorName(status));
      return "";
    }
    return locale.getCountry();
  }
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string. 
////////////////////////////////////////////////////////////////////////////////

string Utf8Helper::toLowerCase (const string& src) {
  int32_t utf8len = 0;
  char* utf8 = tolower(TRI_UNKNOWN_MEM_ZONE, src.c_str(), src.length(), utf8len);
  if (utf8 == 0) {
    return string("");
  }

  string result(utf8, utf8len);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, utf8);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

char* Utf8Helper::tolower (TRI_memory_zone_t* zone, const char *src, int32_t srcLength, int32_t& dstLength) {
  char* utf8_dest = 0;
  
  if (src == 0 || srcLength == 0) {
    utf8_dest = (char*) TRI_Allocate(zone, sizeof(char), false);
    if (utf8_dest != 0) {
      utf8_dest[0] = '\0';
    }
    dstLength = 0;
    return utf8_dest;
  }

  uint32_t options = U_FOLD_CASE_DEFAULT;
  UErrorCode status = U_ZERO_ERROR;
  
  string locale = getCollatorLanguage();
  LocalUCaseMapPointer csm(ucasemap_open(locale.c_str(), options, &status));
  
  if (U_FAILURE(status)) {
    LOGGER_ERROR("error in ucasemap_open(...): " << u_errorName(status));
  }
  else {    
    utf8_dest = (char*) TRI_Allocate(zone, (srcLength + 1) * sizeof(char), false);
    if (utf8_dest == 0) {
      return 0;
    }
      
    dstLength = ucasemap_utf8ToLower(csm.getAlias(),
                    utf8_dest, 
                    srcLength + 1,
                    src, 
                    srcLength, 
                    &status);
    
    if (status == U_BUFFER_OVERFLOW_ERROR) {
      status = U_ZERO_ERROR;
      TRI_Free(zone, utf8_dest);
      utf8_dest = (char*) TRI_Allocate(zone, (dstLength + 1) * sizeof(char), false);
      if (utf8_dest == 0) {
        return 0;
      }
      
      dstLength = ucasemap_utf8ToLower(csm.getAlias(),
                    utf8_dest, 
                    dstLength + 1,
                    src, 
                    srcLength, 
                    &status);        
    }

    if (U_FAILURE(status)) {
      LOGGER_ERROR("error in ucasemap_utf8ToLower(...): " << u_errorName(status));
      TRI_Free(zone, utf8_dest);
    }
    else {
      return utf8_dest;
    }    
  }

  utf8_dest = TRI_LowerAsciiStringZ(zone, src);
  if (utf8_dest != 0) {
    dstLength = strlen(utf8_dest);
  }
  return utf8_dest;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string. 
////////////////////////////////////////////////////////////////////////////////

string Utf8Helper::toUpperCase (const string& src) {
  int32_t utf8len = 0;
  char* utf8 = toupper(TRI_UNKNOWN_MEM_ZONE, src.c_str(), src.length(), utf8len);
  if (utf8 == 0) {
    return string("");
  }
  
  string result(utf8, utf8len);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, utf8);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

char* Utf8Helper::toupper (TRI_memory_zone_t* zone, const char *src, int32_t srcLength, int32_t& dstLength) {
  char* utf8_dest = 0;
  
  if (src == 0 || srcLength == 0) {
    utf8_dest = (char*) TRI_Allocate(zone, sizeof(char), false);
    if (utf8_dest != 0) {
      utf8_dest[0] = '\0';
    }
    dstLength = 0;
    return utf8_dest;
  }

  uint32_t options = U_FOLD_CASE_DEFAULT;
  UErrorCode status = U_ZERO_ERROR;
  
  string locale = getCollatorLanguage();
  LocalUCaseMapPointer csm(ucasemap_open(locale.c_str(), options, &status));
  
  if (U_FAILURE(status)) {
    LOGGER_ERROR("error in ucasemap_open(...): " << u_errorName(status));
  }
  else {    
    utf8_dest = (char*) TRI_Allocate(zone, (srcLength+1) * sizeof(char), false);
    if (utf8_dest == 0) {
      return 0;
    }
      
    dstLength = ucasemap_utf8ToUpper(csm.getAlias(),
                    utf8_dest, 
                    srcLength,
                    src, 
                    srcLength, 
                    &status);
    
    if (status == U_BUFFER_OVERFLOW_ERROR) {
      status = U_ZERO_ERROR;
      TRI_Free(zone, utf8_dest);
      utf8_dest = (char*) TRI_Allocate(zone, (dstLength + 1) * sizeof(char), false);
      if (utf8_dest == 0) {
        return 0;
      }
      
      dstLength = ucasemap_utf8ToUpper(csm.getAlias(),
                    utf8_dest, 
                    dstLength + 1,
                    src, 
                    srcLength, 
                    &status);        
    }

    if (U_FAILURE(status)) {
      LOGGER_ERROR("error in ucasemap_utf8ToUpper(...): " << u_errorName(status));
      TRI_Free(zone, utf8_dest);
    }
    else {
      return utf8_dest;
    }    
  }

  utf8_dest = TRI_UpperAsciiStringZ(zone, src);
  if (utf8_dest != NULL) {
    dstLength = strlen(utf8_dest);
  }
  return utf8_dest;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extract the words from a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t* Utf8Helper::getWords (const char* const text, 
                                           const size_t textLength,
                                           const size_t minimalLength,
                                           const size_t maximalLength,
                                           bool lowerCase) {
  TRI_vector_string_t* words;
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString word;

  if (textLength == 0) {
    // input text is empty
    return NULL;
  }

  if (textLength < minimalLength) {
    // input text is shorter than required minimum length
    return NULL;
  }
  
  size_t textUtf16Length = 0;
  UChar* textUtf16 = NULL;

  if (lowerCase) {
    // lower case string
    int32_t lowerLength = 0;
    char* lower = tolower(TRI_UNKNOWN_MEM_ZONE, text, (int32_t) textLength, lowerLength);

    if (lower == NULL) {
      // out of memory
      return NULL;
    }

    if (lowerLength == 0) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, lower);
      return NULL;
    }

    textUtf16 = TRI_Utf8ToUChar(TRI_UNKNOWN_MEM_ZONE, lower, lowerLength, &textUtf16Length);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, lower);    
  }
  else {
    textUtf16 = TRI_Utf8ToUChar(TRI_UNKNOWN_MEM_ZONE, text, (int32_t) textLength, &textUtf16Length);    
  }

  if (textUtf16 == NULL) {
    return NULL;
  }
  
  ULocDataLocaleType type = ULOC_VALID_LOCALE;  
  const Locale& locale = _coll->getLocale(type, status);
  if (U_FAILURE(status)) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, textUtf16);
    LOGGER_ERROR("error in Collator::getLocale(...): " << u_errorName(status));
    return NULL;
  }

  size_t tempUtf16Length = 0;
  UChar* tempUtf16 = (UChar *) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (textUtf16Length + 1) * sizeof(UChar), false);  
  if (tempUtf16 == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, textUtf16);
    return NULL;
  }
  
  words = (TRI_vector_string_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vector_string_t), false);
  if (words == NULL) {
    return NULL;
  }

  // estimate an initial vector size. this is not accurate, but setting the initial size to some
  // value in the correct order of magnitude will save a lot of vector reallocations later
  size_t initialWordCount = textLength / (2 * (minimalLength + 1));
  if (initialWordCount < 32) {
    // alloc at least 32 pointers (= 256b)
    initialWordCount = 32;
  } 
  else if (initialWordCount > 8192) {
    // alloc at most 8192 pointers (= 64kb)
    initialWordCount = 8192;
  }
  TRI_InitVectorString2(words, TRI_UNKNOWN_MEM_ZONE, initialWordCount);
  
  BreakIterator* wordIterator = BreakIterator::createWordInstance(locale, status);
  UnicodeString utext(textUtf16);
  
  wordIterator->setText(utext);
  int32_t start = wordIterator->first();
  for(int32_t end = wordIterator->next(); end != BreakIterator::DONE; 
    start = end, end = wordIterator->next()) {
    
    tempUtf16Length = (size_t) (end - start);
    // end - start = word length
    if (tempUtf16Length >= minimalLength) {
      size_t chunkLength = tempUtf16Length;
      if (chunkLength > maximalLength) {
        chunkLength = maximalLength;
      }
      utext.extractBetween(start, start + chunkLength, tempUtf16, 0);      

      size_t utf8WordLength;
      char* utf8Word = TRI_UCharToUtf8(TRI_UNKNOWN_MEM_ZONE, tempUtf16, chunkLength, &utf8WordLength);
      if (utf8Word != 0) {
        TRI_PushBackVectorString(words, utf8Word);
      }
    }
  }

  delete wordIterator;
  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, textUtf16);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, tempUtf16);
  
  if (words->_length == 0) {
    // no words found
    TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);
    return NULL;
  }

  return words;
}

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf16 strings
////////////////////////////////////////////////////////////////////////////////

int TRI_compare_utf16 (const uint16_t* left, size_t leftLength, const uint16_t* right, size_t rightLength) {  
  return Utf8Helper::DefaultUtf8Helper.compareUtf16(left, leftLength, right, rightLength);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf8 strings
////////////////////////////////////////////////////////////////////////////////

int TRI_compare_utf8 (const char* left, const char* right) {  
  return Utf8Helper::DefaultUtf8Helper.compareUtf8(left, right);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

char* TRI_tolower_utf8 (TRI_memory_zone_t* zone, const char *src, int32_t srcLength, int32_t* dstLength) {
  return Utf8Helper::DefaultUtf8Helper.tolower(zone, src, srcLength, *dstLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

char* TRI_toupper_utf8 (TRI_memory_zone_t* zone, const char *src, int32_t srcLength, int32_t* dstLength) {
  return Utf8Helper::DefaultUtf8Helper.toupper(zone, src, srcLength, *dstLength);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get words of an UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t* TRI_get_words (const char* const text, 
                                    const size_t textLength,
                                    const size_t minimalWordLength,
                                    const size_t maximalWordLength,
                                    bool lowerCase) {
  return Utf8Helper::DefaultUtf8Helper.getWords(text, textLength, minimalWordLength, maximalWordLength, lowerCase);    
}

#ifdef __cplusplus
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
