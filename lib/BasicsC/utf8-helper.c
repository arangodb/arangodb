////////////////////////////////////////////////////////////////////////////////
/// @brief utf8 helper functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "utf8-helper.h"

#include "unicode/unorm2.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Helper functions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a utf-8 string to a uchar (utf-16)
////////////////////////////////////////////////////////////////////////////////

UChar* TRI_Utf8ToUChar (TRI_memory_zone_t* zone,
                           const char* utf8,
                           const size_t inLength,
                           size_t* outLength) {
  UErrorCode status;
  UChar* utf16;
  int32_t utf16Length;

  // 1. convert utf8 string to utf16
  // calculate utf16 string length
  status = U_ZERO_ERROR;
  u_strFromUTF8(NULL, 0, &utf16Length, utf8, inLength, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    return NULL;
  }

  utf16 = (UChar *) TRI_Allocate(zone, (utf16Length + 1) * sizeof(UChar), false);
  if (utf16 == NULL) {
    return NULL;
  }

  // now convert
  status = U_ZERO_ERROR;
  // the +1 will append a 0 byte at the end
  u_strFromUTF8(utf16, utf16Length + 1, NULL, utf8, inLength, &status);
  if (status != U_ZERO_ERROR) {
    TRI_Free(zone, utf16);
    return 0;
  }

  *outLength = (size_t) utf16Length;

  return utf16;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uchar (utf-16) to a utf-8 string
////////////////////////////////////////////////////////////////////////////////

char* TRI_UCharToUtf8 (TRI_memory_zone_t* zone,
                       const UChar* uchar,
                       const size_t inLength,
                       size_t* outLength) {
  UErrorCode status;
  char* utf8;
  int32_t utf8Length;

  // calculate utf8 string length
  status = U_ZERO_ERROR;
  u_strToUTF8(NULL, 0, &utf8Length, uchar, inLength, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    return NULL;
  }

  utf8 = TRI_Allocate(zone, (utf8Length + 1) * sizeof(char), false);
  if (utf8 == NULL) {
    return NULL;
  }

  // convert to utf8
  status = U_ZERO_ERROR;
  // the +1 will append a 0 byte at the end
  u_strToUTF8(utf8, utf8Length + 1, NULL, uchar, inLength, &status);
  if (status != U_ZERO_ERROR) {
    TRI_Free(zone, utf8);
    return NULL;
  }

  *outLength = ((size_t) utf8Length);

  return utf8;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Helper functions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf8 string (NFC)
////////////////////////////////////////////////////////////////////////////////

char* TRI_normalize_utf8_to_NFC (TRI_memory_zone_t* zone,
                                 const char* utf8,
                                 const size_t inLength,
                                 size_t* outLength) {
  UChar* utf16;
  size_t utf16Length;
  char* utf8Dest;

  *outLength = 0;

  if (inLength == 0) {
    utf8Dest = TRI_Allocate(zone, sizeof(char), false);
    if (utf8Dest != 0) {
      utf8Dest[0] = '\0';
    }
    return utf8Dest;
  }

  utf16 = TRI_Utf8ToUChar(zone, utf8, inLength, &utf16Length);
  if (utf16 == NULL) {
    return NULL;
  }

  // continue in TR_normalize_utf16_to_NFC
  utf8Dest = TRI_normalize_utf16_to_NFC(zone, utf16, (int32_t) utf16Length, outLength);
  TRI_Free(zone, utf16);

  return utf8Dest;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf8 string (NFC)
////////////////////////////////////////////////////////////////////////////////

char* TRI_normalize_utf16_to_NFC (TRI_memory_zone_t* zone,
                                  const uint16_t* utf16,
                                  const size_t inLength,
                                  size_t* outLength) {
  UErrorCode status;
  UChar * utf16Dest;
  int32_t utf16DestLength;
  char * utf8Dest;
  const UNormalizer2 *norm2;

  *outLength = 0;

  if (inLength == 0) {
    utf8Dest = TRI_Allocate(zone, sizeof(char), false);
    if (utf8Dest != 0) {
      utf8Dest[0] = '\0';
    }
    return utf8Dest;
  }

  status = U_ZERO_ERROR;
  norm2 = unorm2_getInstance(NULL, "nfc", UNORM2_COMPOSE, &status);

  if (status != U_ZERO_ERROR) {
    return 0;
  }

  // normalize UChar (UTF-16)
  utf16Dest = (UChar *) TRI_Allocate(zone, (inLength + 1) * sizeof(UChar), false);
  if (utf16Dest == NULL) {
    return 0;
  }

  status = U_ZERO_ERROR;
  utf16DestLength = unorm2_normalize(norm2, (UChar*) utf16, inLength, utf16Dest, inLength + 1, &status);
  if (status != U_ZERO_ERROR) {
    TRI_Free(zone, utf16Dest);
    return 0;
  }

  // Convert data back from UChar (UTF-16) to UTF-8
  utf8Dest = TRI_UCharToUtf8(zone, utf16Dest, (size_t) utf16DestLength, outLength);
  TRI_Free(zone, utf16Dest);

  return utf8Dest;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

