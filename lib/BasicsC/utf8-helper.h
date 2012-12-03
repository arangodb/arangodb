////////////////////////////////////////////////////////////////////////////////
/// @brief utf8 helper functions
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_UTF8_HELPER_H
#define TRIAGENS_BASICS_C_UTF8_HELPER_H 1

#include "BasicsC/common.h"

#ifdef __cplusplus
extern "C" {
#endif
    
// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Helper functions
/// @{
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_ICU
  
////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf8 string (NFC)
////////////////////////////////////////////////////////////////////////////////

char * TR_normalize_utf8_to_NFC (TRI_memory_zone_t* zone, const char* utf8, size_t inLength, size_t* outLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf16 string (NFC) and export it to utf8
////////////////////////////////////////////////////////////////////////////////

char * TR_normalize_utf16_to_NFC (TRI_memory_zone_t* zone, const uint16_t* utf16, size_t inLength, size_t* outLength);

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf16 strings (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

int TR_compare_utf16 (const uint16_t* left, size_t leftLength, const uint16_t* right, size_t rightLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf8 strings (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

int TR_compare_utf8 (const char* left, const char* right);

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

char* TR_tolower_utf8 (TRI_memory_zone_t* zone, const char *src, int32_t srcLength, int32_t* dstLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

char* TR_toupper_utf8 (TRI_memory_zone_t* zone, const char *src, int32_t srcLength, int32_t* dstLength);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
