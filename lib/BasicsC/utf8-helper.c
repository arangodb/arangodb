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


#include "utf8-helper.h"

#ifdef TRI_HAVE_ICU

#include "unicode/ustring.h"
#include "unicode/unorm2.h"


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

char * TR_normalize_utf8_to_NFC (TRI_memory_zone_t* zone, const char* utf8, size_t inLength, size_t* outLength) {
  UErrorCode status = U_ZERO_ERROR;
  UChar*  utf16 = NULL;
  int32_t utf16_length = 0;
  char * utf8_dest = NULL;
  *outLength = 0;
  
  if (inLength == 0) {
    utf8_dest = TRI_Allocate(zone, sizeof(char), false);
    if (utf8_dest != 0) {
      utf8_dest[0] = '\0';
    }
    return utf8_dest;
  }
  
  // 1. convert utf8 string to utf16
  
  // calculate utf16 string length
  u_strFromUTF8(NULL, 0, &utf16_length, utf8, inLength, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    return 0;
  }
  
  status = U_ZERO_ERROR;
  utf16 = (UChar *) TRI_Allocate(zone, (utf16_length+1) * sizeof(UChar), false);  
  if (utf16 == NULL) {
    return 0;
  }
  
  // now convert
  u_strFromUTF8(utf16, utf16_length+1, NULL, utf8, inLength, &status);  
  if (status != U_ZERO_ERROR) {
    TRI_Free(zone, utf16);
    return 0;
  }
  
  // continue in TR_normalize_utf16_to_NFC
  utf8_dest = TR_normalize_utf16_to_NFC(zone, utf16, utf16_length, outLength);
  TRI_Free(zone, utf16);
  
  return utf8_dest;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf8 string (NFC)
////////////////////////////////////////////////////////////////////////////////

char * TR_normalize_utf16_to_NFC (TRI_memory_zone_t* zone, const uint16_t* utf16, size_t inLength, size_t* outLength) {
  UErrorCode status = U_ZERO_ERROR;
  UChar * utf16_dest = NULL;
  int32_t utf16_dest_length = 0;
  char * utf8_dest = NULL;
  int32_t out_length = 0;
  const UNormalizer2 *norm2;
  *outLength = 0;
  
  if (inLength == 0) {
    utf8_dest = TRI_Allocate(zone, sizeof(char), false);
    if (utf8_dest != 0) {
      utf8_dest[0] = '\0';
    }
    return utf8_dest;
  }
  
  norm2 = unorm2_getInstance(NULL, "nfc", UNORM2_COMPOSE ,&status);

  if (status != U_ZERO_ERROR) {
    return 0;
  }

  // 2. normalize UChar (UTF-16)

  utf16_dest = (UChar *) TRI_Allocate(zone, (inLength+1) * sizeof(UChar), false);  
  if (utf16_dest == NULL) {
    return 0;
  }
  
  utf16_dest_length = unorm2_normalize(norm2, (UChar*) utf16, inLength, utf16_dest, inLength+1, &status);
  if (status != U_ZERO_ERROR) {
    TRI_Free(zone, utf16_dest);    
    return 0;
  }
  
  // 3. Convert data back from UChar (UTF-16) to UTF-8 
    
  // calculate utf8 string length
  u_strToUTF8(NULL, 0, &out_length, utf16_dest, utf16_dest_length+1, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    TRI_Free(zone, utf16_dest);    
    return 0;
  }
  
  status = U_ZERO_ERROR;  
  utf8_dest = TRI_Allocate(zone, (out_length+1) * sizeof(char), false);
  if (utf8_dest == NULL) {
    TRI_Free(zone, utf16_dest);    
    return 0;
  }

  // convert to utf8  
  u_strToUTF8(utf8_dest, out_length+1, NULL, utf16_dest, utf16_dest_length+1, &status);
  if (status != U_ZERO_ERROR) {
    TRI_Free(zone, utf16_dest);
    TRI_Free(zone, utf8_dest);    
    return 0;
  }
  
  *outLength = out_length - 1; // ?
  
  TRI_Free(zone, utf16_dest);
  
  return utf8_dest;  
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

