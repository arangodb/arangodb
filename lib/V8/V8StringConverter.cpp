////////////////////////////////////////////////////////////////////////////////
/// @brief V8 string converter
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "V8StringConverter.h"

#include "unicode/normalizer2.h"

using namespace triagens::utils;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the converter
////////////////////////////////////////////////////////////////////////////////

V8StringConverter::V8StringConverter (TRI_memory_zone_t* zone) 
  : _str(nullptr),
    _temp(nullptr),
    _length(0),
    _strCapacity(0),
    _tempCapacity(0),
    _memoryZone(zone) {

  UErrorCode status = U_ZERO_ERROR;
  _normalizer = unorm2_getInstance(nullptr, "nfc", UNORM2_COMPOSE, &status);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the converter
////////////////////////////////////////////////////////////////////////////////
    
V8StringConverter::~V8StringConverter () {
  if (_temp != nullptr) {
    TRI_Free(_memoryZone, _temp);
  }
  if (_str != nullptr) {
    TRI_Free(_memoryZone, _str);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief assign a V8 string to the converter and convert it
////////////////////////////////////////////////////////////////////////////////

bool V8StringConverter::assign (v8::Handle<v8::Value> const obj) {
  v8::String::Value str(obj);
  size_t const length = (size_t) str.length();
   
  if (length == 0) {
    // empty string. no need for unicode conversion
    reserveBuffer(_str, _strCapacity, 1);
    if (_str != nullptr) {
      _str[0] = '\0';
    } 
    _length = 0; 
  }
  else {
    char buffer[64];
    UChar* dest;

    if (length + 1 < sizeof(buffer) / sizeof(UChar)) {
      // use a static buffer if string is small enough
      dest = (UChar*) &buffer[0];
    }
    else {
      // use dynamic memory
      if (! reserveBuffer(_temp, _tempCapacity, (length + 1) * sizeof(UChar))) {
        return false;
      }
      dest = (UChar*) _temp;
    }

    // do unicode normalization
    UErrorCode status = U_ZERO_ERROR;
    int32_t utf16Length = unorm2_normalize(_normalizer, (UChar*) *str, (int32_t) length, dest, (int32_t) length + 1, &status);
    if (status != U_ZERO_ERROR) {
      _length = 0; 
      return false;
    }

    // calculate UTF-8 length
    int32_t utf8Length;
    status = U_ZERO_ERROR;
    u_strToUTF8(nullptr, 0, &utf8Length, dest, (int32_t) utf16Length, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
      _length = 0; 
      return false;
    }

    // allocate big enough target buffer
    reserveBuffer(_str, _strCapacity, utf8Length + 1);
    if (_str == nullptr) {
      _length = 0;
      return false;
    } 

    // convert from UTF-16 to UTF-8
    status = U_ZERO_ERROR;
    u_strToUTF8(_str, utf8Length + 1, nullptr, dest, (int32_t) utf16Length, &status);
    _length = utf8Length;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve a buffer for operations
////////////////////////////////////////////////////////////////////////////////

bool V8StringConverter::reserveBuffer (char*& what,
                                       size_t& oldSize,
                                       size_t length) {
  if (length < 64) {
    // minimum buffer size
    length = 64;
  }

  if (oldSize < length) {
    char* ptr = static_cast<char*>(TRI_Reallocate(_memoryZone, what, length));

    if (ptr == nullptr) {
      return false;
    }

    what = ptr;
    oldSize = length;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
