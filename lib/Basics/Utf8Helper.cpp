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

#ifdef TRI_HAVE_ICU
#include "unicode/normalizer2.h"
#include "unicode/ucasemap.h"
#else
#include "string.h"
#endif

#include "Logger/Logger.h"
#include "BasicsC/strings.h"

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

/*
int Utf8Helper::compareUtf8 (const char* left, size_t leftLength, const char* right, size_t rightLength) {
#ifdef TRI_HAVE_ICU  
  if (!_coll) {
    LOGGER_ERROR << "no Collator in Utf8Helper::compareUtf8()!";
    return 0;
  }
  
  UErrorCode status = U_ZERO_ERROR;
  int result = _coll->compareUTF8(StringPiece(left, leftLength), StringPiece(right, rightLength), status);
  if(U_FAILURE(status)) {
    LOGGER_ERROR << "error in Collator::compareUTF8(...): " << u_errorName(status);
    return 0;
  }
  return result;  
#else
  if (leftLength == rightLength) {
    return memcmp((const void*)left, (const void*)right, leftLength);
  }
  
  int result = memcmp((const void*)left, (const void*)right, leftLength < rightLength ? leftLength : rightLength);
  
  if (result == 0) {
    if (leftLength < rightLength) {
      return -1;
    }
    return 1;
  }
  return result;
#endif
}
*/

int Utf8Helper::compareUtf8 (const char* left, const char* right) {
#ifdef TRI_HAVE_ICU  
  if (!_coll) {
    LOGGER_ERROR << "no Collator in Utf8Helper::compareUtf8()!";
    return (strcmp(left, right));
  }
  
  UErrorCode status = U_ZERO_ERROR;
  int result = _coll->compareUTF8(StringPiece(left), StringPiece(right), status);
  if(U_FAILURE(status)) {
    LOGGER_ERROR << "error in Collator::compareUTF8(...): " << u_errorName(status);
    return (strcmp(left, right));
  }
  
  return result;
#else
  return (strcmp(left, right));
#endif
}

int Utf8Helper::compareUtf16 (const uint16_t* left, size_t leftLength, const uint16_t* right, size_t rightLength) {  
#ifdef TRI_HAVE_ICU 
  if (!_coll) {
    LOGGER_ERROR << "no Collator in Utf8Helper::compareUtf16()!";
#endif  
    
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
#ifdef TRI_HAVE_ICU 
  }
  
  return _coll->compare((const UChar *)left, leftLength, (const UChar *)right, rightLength);
#endif  
}

void Utf8Helper::setCollatorLanguage (const string& lang) {
#ifdef TRI_HAVE_ICU
  
  UErrorCode status = U_ZERO_ERROR;   
    
  if (_coll) {
    ULocDataLocaleType type = ULOC_ACTUAL_LOCALE;
    const Locale& locale = _coll->getLocale(type, status);
    
    if(U_FAILURE(status)) {
      LOGGER_ERROR << "error in Collator::getLocale(...): " << u_errorName(status);
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
    LOGGER_ERROR << "error in Collator::createInstance(): " << u_errorName(status);
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
    LOGGER_ERROR << "error in Collator::setAttribute(...): " << u_errorName(status);
    delete coll;
    return;
  }
  
  if (_coll) {
    delete _coll;
  }

  _coll = coll;  
#endif
}

string Utf8Helper::getCollatorLanguage () {
#ifdef TRI_HAVE_ICU
  if (_coll) {
    UErrorCode status = U_ZERO_ERROR;
    ULocDataLocaleType type = ULOC_VALID_LOCALE;
    const Locale& locale = _coll->getLocale(type, status);
    
    if(U_FAILURE(status)) {
      LOGGER_ERROR << "error in Collator::getLocale(...): " << u_errorName(status);
      return "";
    }
    return locale.getLanguage();
  }
#endif
  return "";
}

string Utf8Helper::getCollatorCountry () {
#ifdef TRI_HAVE_ICU
  if (_coll) {
    UErrorCode status = U_ZERO_ERROR;
    ULocDataLocaleType type = ULOC_VALID_LOCALE;
    const Locale& locale = _coll->getLocale(type, status);
    
    if(U_FAILURE(status)) {
      LOGGER_ERROR << "error in Collator::getLocale(...): " << u_errorName(status);
      return "";
    }
    return locale.getCountry();
  }
#endif
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

#ifdef TRI_HAVE_ICU  
  uint32_t options = U_FOLD_CASE_DEFAULT;
  UErrorCode status = U_ZERO_ERROR;
  
  string locale = getCollatorLanguage();
  LocalUCaseMapPointer csm(ucasemap_open(locale.c_str(), options, &status));
  
  if (U_FAILURE(status)) {
    LOGGER_ERROR << "error in ucasemap_open(...): " << u_errorName(status);
  }
  else {    
    utf8_dest = (char*) TRI_Allocate(zone, (srcLength+1) * sizeof(char), false);
      
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
      LOGGER_ERROR << "error in ucasemap_utf8ToLower(...): " << u_errorName(status);
      TRI_Free(zone, utf8_dest);
    }
    else {
      return utf8_dest;
    }    
  }
#endif

  utf8_dest = TRI_LowerAsciiStringZ(zone, src);
  dstLength = strlen(utf8_dest);
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

#ifdef TRI_HAVE_ICU  
  uint32_t options = U_FOLD_CASE_DEFAULT;
  UErrorCode status = U_ZERO_ERROR;
  
  string locale = getCollatorLanguage();
  LocalUCaseMapPointer csm(ucasemap_open(locale.c_str(), options, &status));
  
  if (U_FAILURE(status)) {
    LOGGER_ERROR << "error in ucasemap_open(...): " << u_errorName(status);
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
      LOGGER_ERROR << "error in ucasemap_utf8ToUpper(...): " << u_errorName(status);
      TRI_Free(zone, utf8_dest);
    }
    else {
      return utf8_dest;
    }    
  }
#endif

  utf8_dest = TRI_UpperAsciiStringZ(zone, src);
  dstLength = strlen(utf8_dest);
  return utf8_dest;
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
