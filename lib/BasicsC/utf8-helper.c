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
  
  // 1. convert utf8 string to utf16
  
  // calculate utf16 string length
  u_strFromUTF8(NULL, 0, &utf16_length, utf8, inLength, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    printf("error in u_strFromUTF8 1: %s\n", u_errorName(status));
    return 0;
  }
  
  status = U_ZERO_ERROR;
  utf16 = (UChar *) malloc((utf16_length+1) * sizeof(UChar));  
  if (utf16 == NULL) {
    printf("malloc error\r");
    return 0;
  }
  
  // now convert
  u_strFromUTF8(utf16, utf16_length+1, NULL, utf8, inLength, &status);  
  if (status != U_ZERO_ERROR) {
    printf("error in u_strFromUTF8 2: %s\n", u_errorName(status));
    
    free(utf16);
    return 0;
  }
  
  // continue in TR_normalize_utf16_to_NFC
  utf8_dest = TR_normalize_utf16_to_NFC(zone, utf16, utf16_length, outLength);
  free(utf16);
  
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
  const UNormalizer2 * norm2 = unorm2_getInstance(NULL, "nfc", UNORM2_COMPOSE ,&status);
  *outLength = 0;

  if (status != U_ZERO_ERROR) {
    printf("error in unorm2_getInstance: %s\n", u_errorName(status));
    return 0;
  }

  // 2. normalize UChar (UTF-16)

  utf16_dest = (UChar *) malloc((inLength+1) * sizeof(UChar));  
  if (utf16_dest == NULL) {
    printf("malloc error\n");
    
    return 0;
  }
  
  utf16_dest_length = unorm2_normalize(norm2, (UChar*) utf16, inLength, utf16_dest, inLength+1, &status);
  if (status != U_ZERO_ERROR) {
    printf("error in unorm2_normalize: %s\n", u_errorName(status));
    
    free(utf16_dest);    
    return 0;
  }
  
  // 3. Convert data back from UChar (UTF-16) to UTF-8 
    
  // calculate utf8 string length
  u_strToUTF8(NULL, 0, &out_length, utf16_dest, utf16_dest_length+1, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    printf("error in u_strToUTF8 1 %s\n", u_errorName(status));
    
    free(utf16_dest);    
    return 0;
  }
  
  status = U_ZERO_ERROR;  
  // utf8_dest = (char *) malloc((out_length+1) * sizeof(char));  
  utf8_dest = TRI_Allocate(zone, (out_length+1) * sizeof(char), false);
  if (utf8_dest == NULL) {
    printf("malloc error\n");
    
    free(utf16_dest);    
    return 0;
  }

  // convert to utf8  
  u_strToUTF8(utf8_dest, out_length+1, NULL, utf16_dest, utf16_dest_length+1, &status);
  if (status != U_ZERO_ERROR) {
    printf("error in u_strToUTF8 2 %s\n", u_errorName(status));
    
    free(utf16_dest);
    TRI_Free(zone, utf8_dest);    
    return 0;
  }
  
  *outLength = out_length - 1; // ?
  
  free(utf16_dest);
  
  return utf8_dest;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf8 strings
////////////////////////////////////////////////////////////////////////////////

int TR_compare_utf16 (const uint16_t* left, size_t leftLength, const uint16_t* right, size_t rightLength, UCollator* coll) {
  UErrorCode status = U_ZERO_ERROR; 
  UCollationResult result = UCOL_EQUAL;
  
  if (!coll) {
    coll = ucol_open("de_DE", &status); 
    if(U_FAILURE(status)) {
      printf("error in ucol_open %s\n", u_errorName(status));      
      return 0;
    }
    
    ucol_setAttribute(coll, UCOL_CASE_FIRST, UCOL_UPPER_FIRST, &status); // A < a
    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_OFF, &status);
    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_IDENTICAL, &status);     //UCOL_IDENTICAL, UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY
  
    if(U_FAILURE(status)) {
      printf("error in ucol_setAttribute %s\n", u_errorName(status));      
      return 0;
    }
    
    result = ucol_strcoll(coll, (const UChar *)left, leftLength, (const UChar *)right, rightLength);
    
    ucol_close(coll);
  }
  else {
    result = ucol_strcoll(coll, (const UChar *)left, leftLength, (const UChar *)right, rightLength);
  }
    
  switch (result) {
    case (UCOL_GREATER):
      return 1;
      
    case (UCOL_LESS):
      return -1;
      
    default:
      return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

