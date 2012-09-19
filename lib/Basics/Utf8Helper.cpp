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
#include "BasicsC/utf8-helper.h"
#else
#include "string.h"
#endif

#include "Logger/Logger.h"

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
  
  switch (result) {
    case (UCOL_GREATER):
      return 1;
      
    case (UCOL_LESS):
      return -1;
      
    default:
      return 0;
  }
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
  else if (result < 0) {
    return -1;
  }
  else {
    return 1;
  }
#endif
}

int Utf8Helper::compareUtf16 (const uint16_t* left, size_t leftLength, const uint16_t* right, size_t rightLength) {  
#ifdef TRI_HAVE_ICU 
  if (!_coll) {
    LOGGER_ERROR << "no Collator in Utf8Helper::compareUtf16()!";
    return 0;
  }
  
  int result = _coll->compare((const UChar *)left, leftLength, (const UChar *)right, rightLength);
  
  switch (result) {
    case (UCOL_GREATER):
      return 1;
      
    case (UCOL_LESS):
      return -1;
      
    default:
      return 0;
  }
#else 
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
  else if (result < 0) {
    return -1;
  }
  else {
    return 1;
  } 
#endif  
}

v8::Handle<v8::Value> Utf8Helper::normalize (v8::Handle<v8::Value> obj) {
  v8::HandleScope scope;
  
  v8::String::Value str(obj);
  size_t str_len = str.length();
  if (str_len > 0) {
#ifdef TRI_HAVE_ICU  
    UErrorCode erroCode = U_ZERO_ERROR;
    const Normalizer2* normalizer = Normalizer2::getNFCInstance(erroCode);
    
    if (U_FAILURE(erroCode)) {
      LOGGER_ERROR << "error in Normalizer2::getNFCInstance(erroCode): " << u_errorName(erroCode);
      return scope.Close(v8::Null()); 
    }
    
    UnicodeString result = normalizer->normalize(UnicodeString(*str, str_len), erroCode);

    if (U_FAILURE(erroCode)) {
      LOGGER_ERROR << "error in normalizer->normalize(UnicodeString(*str, str_len), erroCode): " << u_errorName(erroCode);
      return scope.Close(v8::Null()); 
    }
    
    return scope.Close(v8::String::New(result.getBuffer(), result.length())); 
#else
    return scope.Close(v8::String::New(*str, str_len)); 
#endif
  }
  else {
    return scope.Close(v8::String::New("")); 
  }
}

void Utf8Helper::setCollatorLanguage (const string& lang) {
#ifdef TRI_HAVE_ICU
  
  UErrorCode status = U_ZERO_ERROR;   
  Collator* coll;
  if (lang == "") {
    coll = Collator::createInstance(status); 
  }
  else {
    Locale locale(lang.c_str());
    coll = Collator::createInstance(locale, status);     
  }
  
  if(U_FAILURE(status)) {
    LOGGER_ERROR << "error in Collator::createInstance(): " << u_errorName(status);
    return;
  }
  
  // set the default attributes for sorting:
  coll->setAttribute(UCOL_CASE_FIRST, UCOL_UPPER_FIRST, status);  // A < a
  coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_OFF, status);
  coll->setAttribute(UCOL_STRENGTH, UCOL_IDENTICAL, status);      // UCOL_IDENTICAL, UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY

  if(U_FAILURE(status)) {
    LOGGER_ERROR << "error in Collator::setAttribute(...): " << u_errorName(status);
    return;
  }
  
  if (_coll) {
    delete _coll;
  }

  _coll = coll;  
#endif
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
