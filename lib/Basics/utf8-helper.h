////////////////////////////////////////////////////////////////////////////////
/// @brief utf8 helper functions
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_C_UTF8__HELPER_H
#define ARANGODB_BASICS_C_UTF8__HELPER_H 1

#include "Basics/Common.h"
#include "Basics/vector.h"

#include "unicode/ustring.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a utf-8 string to a uchar (utf-16)
////////////////////////////////////////////////////////////////////////////////

UChar* TRI_Utf8ToUChar (TRI_memory_zone_t* zone,
                           const char* utf8,
                           const size_t inLength,
                           size_t* outLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uchar (utf-16) to a utf-8 string
////////////////////////////////////////////////////////////////////////////////

char* TRI_UCharToUtf8 (TRI_memory_zone_t* zone,
                          const UChar* uchar,
                          const size_t inLength,
                          size_t* outLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf8 string (NFC)
////////////////////////////////////////////////////////////////////////////////

char* TRI_normalize_utf8_to_NFC (TRI_memory_zone_t* zone,
                                 const char* utf8,
                                 const size_t inLength,
                                 size_t* outLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf16 string (NFC) and export it to utf8
////////////////////////////////////////////////////////////////////////////////

char * TRI_normalize_utf16_to_NFC (TRI_memory_zone_t* zone,
                                   const uint16_t* utf16,
                                   const size_t inLength,
                                   size_t* outLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf16 strings (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

int TRI_compare_utf16 (const uint16_t* left,
                       size_t leftLength,
                       const uint16_t* right,
                       size_t rightLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf8 strings (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

int TRI_compare_utf8 (const char* left, const char* right);

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

char* TRI_tolower_utf8 (TRI_memory_zone_t* zone,
                        const char *src,
                        int32_t srcLength,
                        int32_t* dstLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

char* TRI_toupper_utf8 (TRI_memory_zone_t* zone,
                        const char *src,
                        int32_t srcLength,
                        int32_t* dstLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get words of an UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t* TRI_get_words (const char* const text,
                                    const size_t textLength,
                                    const size_t minimalWordLength,
                                    const size_t maximalWordLength,
                                    bool lowerCase);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
