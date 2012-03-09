////////////////////////////////////////////////////////////////////////////////
/// @brief logging macros and functions
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

#include "string-buffer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief append a character without check
////////////////////////////////////////////////////////////////////////////////

static void AppendChar (TRI_string_buffer_t * self, char chr) {
  //*self->_bufferPtr++ = chr;
  *(self->_buffer + self->_off++) = chr;
}

static size_t Remaining (TRI_string_buffer_t * self) {
  return self->_len - self->_off;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve space
////////////////////////////////////////////////////////////////////////////////

static void Reserve (TRI_string_buffer_t * self, size_t size) {
  if (size > Remaining(self)) {
    //size_t newlen = (size_t)(1.2 * (self->_len + size));
    self->_len = (size_t)(1.2 * (self->_len + size));
    self->_buffer = TRI_Reallocate(self->_buffer, self->_len+1);
  }
//  if (self->_buffer == NULL) {
//    self->_buffer = (char*) TRI_Allocate(size + 1);
//    self->_off = 0;
//    self->_len = size; 
//    //self->_bufferEnd = self->_buffer + size;
//  }
//  //else if ((size_t)(self->_bufferEnd - self->_bufferPtr) < size) {
//  else if ((size_t)(self->_len - self->_off) < size) {
//    size_t newlen; //, offset;
//    //char * b;
//    
//    //printf(">b: %zx , bp: %zx , be:%zx \n", self->_buffer, self->_bufferPtr, self->_bufferEnd);
//
//    newlen = (size_t)(1.2 * ( self->_len + size));
////    b = TRI_Allocate(newlen + 1);
//    self->_buffer=TRI_Reallocate(self->_buffer,newlen+1+1);
//    self->_len = self->_len+newlen;

    //printf("<b: %zx , bp: %zx , be:%zx nl:%d \n", self->_buffer, self->_bufferPtr, self->_bufferEnd, newlen);
//
//    memcpy(b, self->_buffer, self->_bufferEnd - self->_buffer + 1);
//
//    TRI_Free(self->_buffer);
//
//    self->_bufferPtr = b + (self->_bufferPtr - self->_buffer);
//    self->_bufferEnd = b + newlen;
//    self->_buffer    = b;
//  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the string buffer
///
/// @warning You must call initialise before using the string buffer.
////////////////////////////////////////////////////////////////////////////////

void TRI_InitStringBuffer (TRI_string_buffer_t * self) {
//  self->_buffer = NULL;
//  self->_bufferPtr = NULL;
//  self->_bufferEnd = NULL;
//
//  Reserve(self, 1);
//  *self->_bufferPtr = 0;
  memset (self, 0, sizeof(TRI_string_buffer_t));
  Reserve(self, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the string buffer
///
/// @warning You must call free or destroy after using the string buffer.
////////////////////////////////////////////////////////////////////////////////

void  TRI_FreeStringBuffer (TRI_string_buffer_t * self) {
  if (self->_buffer != NULL) {
    TRI_Free(self->_buffer);

//    self->_buffer = NULL;
//    self->_bufferPtr = NULL;
//    self->_bufferEnd = NULL;
    
    memset (self, 0, sizeof(TRI_string_buffer_t));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the string buffer and cleans the buffer
///
/// @warning You must call free after or destroy using the string buffer.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyStringBuffer (TRI_string_buffer_t * self) {
  if (self->_buffer != NULL) {
    memset(self->_buffer, 0, self->_bufferEnd - self->_buffer);

    TRI_Free(self->_buffer);

    memset (self, 0, sizeof(TRI_string_buffer_t));
//    self->_buffer = NULL;
//    self->_bufferPtr = NULL;
//    self->_bufferEnd = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief swaps content with another string buffer
////////////////////////////////////////////////////////////////////////////////

void TRI_SwapStringBuffer (TRI_string_buffer_t * self, TRI_string_buffer_t * other) {
  char * otherBuffer    = other->_buffer;
  ptrdiff_t otherOff    = other->_off;
  size_t otherLen       = other->_len;

  other->_buffer    = self->_buffer;
  other->_off       = self->_off;
  other->_len       = self->_len;

  self->_buffer    = otherBuffer;
  self->_off       = otherOff;
  self->_len       = otherLen;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns pointer to the beginning of the character buffer
////////////////////////////////////////////////////////////////////////////////

char const * TRI_BeginStringBuffer (TRI_string_buffer_t const * self) {
  return self->_buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns pointer to the end of the character buffer
////////////////////////////////////////////////////////////////////////////////

char const * TRI_EndStringBuffer (TRI_string_buffer_t const * self) {
  return self->_buffer + self->_len;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns length of the character buffer
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthStringBuffer (TRI_string_buffer_t const * self) {
  return self->_off;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if buffer is empty
////////////////////////////////////////////////////////////////////////////////

bool TRI_EmptyStringBuffer (TRI_string_buffer_t const * self) {
  return self->_off == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the buffer
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearStringBuffer (TRI_string_buffer_t * self) {
  self->_off = 0; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the string buffer
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_CopyStringBuffer (TRI_string_buffer_t * self, TRI_string_buffer_t const * source) {
  TRI_ReplaceStringStringBuffer(self, source->_buffer, source->_bufferPtr - source->_buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the first characters
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_EraseFrontStringBuffer (TRI_string_buffer_t * self, size_t len) {
  if ((size_t)(self->_bufferPtr - self->_buffer) <= len) {
    self->_bufferPtr = self->_buffer;
  }
  else if (0 < len) {
    memmove(self->_buffer, self->_buffer + len, self->_bufferPtr - self->_buffer - len);
    self->_bufferPtr -= len;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces characters
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_ReplaceStringStringBuffer (TRI_string_buffer_t * self, char const * str, size_t len) {
  self->_bufferPtr = self->_buffer;

  TRI_AppendString2StringBuffer(self, str, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces characters
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_ReplaceStringBufferStringBuffer (TRI_string_buffer_t * self, TRI_string_buffer_t const * text) {
  self->_bufferPtr = self->_buffer;

  TRI_AppendString2StringBuffer(self, text->_buffer, text->_bufferPtr - text->_buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  STRING APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief appends character
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendCharStringBuffer (TRI_string_buffer_t * self, char chr) {
  Reserve(self, 2);

  *(self->_buffer + self->_off) = chr;
  ++(self->_off);
  // bzero at init?
  *(self->_buffer + self->_off)   = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendStringStringBuffer (TRI_string_buffer_t * self, char const * str) {
  size_t len = strlen(str);

  Reserve(self, len);

  memcpy((self->_buffer + self->_off), str, len);
  self->_off += len;
  *(self->_buffer + self->_off) = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters
////////////////////////////////////////////////////////////////////////////////
// TODO
void TRI_AppendString2StringBuffer (TRI_string_buffer_t * self, char const * str, size_t len) {
  if (len == 0) {
    Reserve(self, 1);
    *self->_bufferPtr = '\0';
  }
  else {
    Reserve(self, len + 1);
    memcpy(self->_bufferPtr, str, len);
    self->_bufferPtr += len;
    *self->_bufferPtr = '\0';
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string buffer
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_AppendStringBufferStringBuffer (TRI_string_buffer_t * self, TRI_string_buffer_t const * text) {
  TRI_AppendString2StringBuffer(self, text->_buffer, text->_bufferPtr - text->_buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a blob
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_AppendBlobStringBuffer (TRI_string_buffer_t * self, TRI_blob_t const * text) {
  TRI_AppendString2StringBuffer(self, text->data, text->length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends eol character
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_AppendEolStringBuffer (TRI_string_buffer_t * self) {
  TRI_AppendCharStringBuffer(self, '\n');
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 INTEGER APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with two digits
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_AppendInteger2StringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  Reserve(self, 3);

  AppendChar(self, (attr / 10U) % 10 + '0');
  AppendChar(self,  attr % 10        + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with three digits
////////////////////////////////////////////////////////////////////////////////
// TODO
void TRI_AppendInteger3StringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  Reserve(self, 4);

  AppendChar(self, (attr / 100U) % 10 + '0');
  AppendChar(self, (attr /  10U) % 10 + '0');
  AppendChar(self,  attr         % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with four digits
////////////////////////////////////////////////////////////////////////////////
// TODO
void TRI_AppendInteger4StringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  Reserve(self, 5);

  AppendChar(self, (attr / 1000U) % 10 + '0');
  AppendChar(self, (attr /  100U) % 10 + '0');
  AppendChar(self, (attr /   10U) % 10 + '0');
  AppendChar(self,  attr          % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 8 bits
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_AppendInt8StringBuffer (TRI_string_buffer_t * self, int8_t attr) {
  if (attr == INT8_MIN) {
    TRI_AppendString2StringBuffer(self, "-128", 4);
    return;
  }

  Reserve(self, 5);

  if (attr < 0) {
    AppendChar(self, '-');
    attr = -attr;
  }


  if (100 <= attr) { AppendChar(self, (attr / 100) % 10 + '0'); }
  if ( 10 <= attr) { AppendChar(self, (attr /  10) % 10 + '0'); }

  AppendChar(self, attr % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 8 bits
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_AppendUInt8StringBuffer (TRI_string_buffer_t * self, uint8_t attr) {
  Reserve(self, 4);

  if (100U <= attr) { AppendChar(self, (attr / 100U) % 10 + '0'); }
  if ( 10U <= attr) { AppendChar(self, (attr /  10U) % 10 + '0'); }

  AppendChar(self, attr % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 16 bits
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_AppendInt16StringBuffer (TRI_string_buffer_t * self, int16_t attr) {
  if (attr == INT16_MIN) {
    TRI_AppendString2StringBuffer(self, "-32768", 6);
    return;
  }

  Reserve(self, 7);

  if (attr < 0) {
    AppendChar(self, '-');
    attr = -attr;
  }

  if (10000 <= attr) { AppendChar(self, (attr / 10000) % 10 + '0'); }
  if ( 1000 <= attr) { AppendChar(self, (attr /  1000) % 10 + '0'); }
  if (  100 <= attr) { AppendChar(self, (attr /   100) % 10 + '0'); }
  if (   10 <= attr) { AppendChar(self, (attr /    10) % 10 + '0'); }

  AppendChar(self, attr % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits
////////////////////////////////////////////////////////////////////////////////
// TODO
void TRI_AppendUInt16StringBuffer (TRI_string_buffer_t * self, uint16_t attr) {
  Reserve(self, 6);

  if (10000U <= attr) { AppendChar(self, (attr / 10000U) % 10 + '0'); }
  if ( 1000U <= attr) { AppendChar(self, (attr /  1000U) % 10 + '0'); }
  if (  100U <= attr) { AppendChar(self, (attr /   100U) % 10 + '0'); }
  if (   10U <= attr) { AppendChar(self, (attr /    10U) % 10 + '0'); }

  AppendChar(self, attr % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 32 bits
////////////////////////////////////////////////////////////////////////////////
// TODO
void TRI_AppendInt32StringBuffer (TRI_string_buffer_t * self, int32_t attr) {
  if (attr == INT32_MIN) {
    TRI_AppendString2StringBuffer(self, "-2147483648", 11);
    return;
  }

  Reserve(self, 12);

  if (attr < 0) {
    AppendChar(self, '-');
    attr = -attr;
  }

  if (1000000000 <= attr) { AppendChar(self, (attr / 1000000000) % 10 + '0'); }
  if ( 100000000 <= attr) { AppendChar(self, (attr /  100000000) % 10 + '0'); }
  if (  10000000 <= attr) { AppendChar(self, (attr /   10000000) % 10 + '0'); }
  if (   1000000 <= attr) { AppendChar(self, (attr /    1000000) % 10 + '0'); }
  if (    100000 <= attr) { AppendChar(self, (attr /     100000) % 10 + '0'); }
  if (     10000 <= attr) { AppendChar(self, (attr /      10000) % 10 + '0'); }
  if (      1000 <= attr) { AppendChar(self, (attr /       1000) % 10 + '0'); }
  if (       100 <= attr) { AppendChar(self, (attr /        100) % 10 + '0'); }
  if (        10 <= attr) { AppendChar(self, (attr /         10) % 10 + '0'); }

  AppendChar(self, attr % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits
////////////////////////////////////////////////////////////////////////////////
// TODO
void TRI_AppendUInt32StringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  Reserve(self, 11);

  if (1000000000U <= attr) { AppendChar(self, (attr / 1000000000U) % 10 + '0'); }
  if ( 100000000U <= attr) { AppendChar(self, (attr /  100000000U) % 10 + '0'); }
  if (  10000000U <= attr) { AppendChar(self, (attr /   10000000U) % 10 + '0'); }
  if (   1000000U <= attr) { AppendChar(self, (attr /    1000000U) % 10 + '0'); }
  if (    100000U <= attr) { AppendChar(self, (attr /     100000U) % 10 + '0'); }
  if (     10000U <= attr) { AppendChar(self, (attr /      10000U) % 10 + '0'); }
  if (      1000U <= attr) { AppendChar(self, (attr /       1000U) % 10 + '0'); }
  if (       100U <= attr) { AppendChar(self, (attr /        100U) % 10 + '0'); }
  if (        10U <= attr) { AppendChar(self, (attr /         10U) % 10 + '0'); }

  AppendChar(self, attr % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 64 bits
////////////////////////////////////////////////////////////////////////////////
// TODO
void TRI_AppendInt64StringBuffer (TRI_string_buffer_t * self, int64_t attr) {
  if (attr == INT64_MIN) {
    TRI_AppendString2StringBuffer(self, "-9223372036854775808", 20);
    return;
  }
  else if (attr < 0) {
    Reserve(self, 1);
    AppendChar(self, '-');
    attr = -attr;
  }

  if ((attr >> 32) == 0) {
    TRI_AppendUInt32StringBuffer(self, (uint32_t) attr);
    return;
  }

  Reserve(self, 20);

  // uint64_t has one more decimal than int64_t
  if (1000000000000000000LL <= attr) { AppendChar(self, (attr / 1000000000000000000LL) % 10 + '0'); }
  if ( 100000000000000000LL <= attr) { AppendChar(self, (attr /  100000000000000000LL) % 10 + '0'); }
  if (  10000000000000000LL <= attr) { AppendChar(self, (attr /   10000000000000000LL) % 10 + '0'); }
  if (   1000000000000000LL <= attr) { AppendChar(self, (attr /    1000000000000000LL) % 10 + '0'); }
  if (    100000000000000LL <= attr) { AppendChar(self, (attr /     100000000000000LL) % 10 + '0'); }
  if (     10000000000000LL <= attr) { AppendChar(self, (attr /      10000000000000LL) % 10 + '0'); }
  if (      1000000000000LL <= attr) { AppendChar(self, (attr /       1000000000000LL) % 10 + '0'); }
  if (       100000000000LL <= attr) { AppendChar(self, (attr /        100000000000LL) % 10 + '0'); }
  if (        10000000000LL <= attr) { AppendChar(self, (attr /         10000000000LL) % 10 + '0'); }
  if (         1000000000LL <= attr) { AppendChar(self, (attr /          1000000000LL) % 10 + '0'); }
  if (          100000000LL <= attr) { AppendChar(self, (attr /           100000000LL) % 10 + '0'); }
  if (           10000000LL <= attr) { AppendChar(self, (attr /            10000000LL) % 10 + '0'); }
  if (            1000000LL <= attr) { AppendChar(self, (attr /             1000000LL) % 10 + '0'); }
  if (             100000LL <= attr) { AppendChar(self, (attr /              100000LL) % 10 + '0'); }
  if (              10000LL <= attr) { AppendChar(self, (attr /               10000LL) % 10 + '0'); }
  if (               1000LL <= attr) { AppendChar(self, (attr /                1000LL) % 10 + '0'); }
  if (                100LL <= attr) { AppendChar(self, (attr /                 100LL) % 10 + '0'); }
  if (                 10LL <= attr) { AppendChar(self, (attr /                  10LL) % 10 + '0'); }

  AppendChar(self, attr % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits
////////////////////////////////////////////////////////////////////////////////
//TODO
void TRI_AppendUInt64StringBuffer (TRI_string_buffer_t * self, uint64_t attr) {
  if ((attr >> 32) == 0) {
    TRI_AppendUInt32StringBuffer(self, (uint32_t) attr);
    return;
  }

  Reserve(self, 21);

  // uint64_t has one more decimal than int64_t
  if (10000000000000000000ULL <= attr) { AppendChar(self, (attr / 10000000000000000000ULL) % 10 + '0'); }
  if ( 1000000000000000000ULL <= attr) { AppendChar(self, (attr /  1000000000000000000ULL) % 10 + '0'); }
  if (  100000000000000000ULL <= attr) { AppendChar(self, (attr /   100000000000000000ULL) % 10 + '0'); }
  if (   10000000000000000ULL <= attr) { AppendChar(self, (attr /    10000000000000000ULL) % 10 + '0'); }
  if (    1000000000000000ULL <= attr) { AppendChar(self, (attr /     1000000000000000ULL) % 10 + '0'); }
  if (     100000000000000ULL <= attr) { AppendChar(self, (attr /      100000000000000ULL) % 10 + '0'); }
  if (      10000000000000ULL <= attr) { AppendChar(self, (attr /       10000000000000ULL) % 10 + '0'); }
  if (       1000000000000ULL <= attr) { AppendChar(self, (attr /        1000000000000ULL) % 10 + '0'); }
  if (        100000000000ULL <= attr) { AppendChar(self, (attr /         100000000000ULL) % 10 + '0'); }
  if (         10000000000ULL <= attr) { AppendChar(self, (attr /          10000000000ULL) % 10 + '0'); }
  if (          1000000000ULL <= attr) { AppendChar(self, (attr /           1000000000ULL) % 10 + '0'); }
  if (           100000000ULL <= attr) { AppendChar(self, (attr /            100000000ULL) % 10 + '0'); }
  if (            10000000ULL <= attr) { AppendChar(self, (attr /             10000000ULL) % 10 + '0'); }
  if (             1000000ULL <= attr) { AppendChar(self, (attr /              1000000ULL) % 10 + '0'); }
  if (              100000ULL <= attr) { AppendChar(self, (attr /               100000ULL) % 10 + '0'); }
  if (               10000ULL <= attr) { AppendChar(self, (attr /                10000ULL) % 10 + '0'); }
  if (                1000ULL <= attr) { AppendChar(self, (attr /                 1000ULL) % 10 + '0'); }
  if (                 100ULL <= attr) { AppendChar(self, (attr /                  100ULL) % 10 + '0'); }
  if (                  10ULL <= attr) { AppendChar(self, (attr /                   10ULL) % 10 + '0'); }

  AppendChar(self, attr % 10 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t
////////////////////////////////////////////////////////////////////////////////
// TODO
void TRI_AppendSizeStringBuffer (TRI_string_buffer_t * self, size_t attr) {
#if TRI_SIZEOF_SIZE_T == 8
  TRI_AppendUInt64StringBuffer(self, (uint64_t) attr);
#else
  TRI_AppendUInt32StringBuffer(self, (uint32_t) attr);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           INTEGER OCTAL APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits in octal
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendUInt32OctalStringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  Reserve(self, 9);

  if (010000000U <= attr) { AppendChar(self, (attr / 010000000U) % 010 + '0'); }
  if ( 01000000U <= attr) { AppendChar(self, (attr /  01000000U) % 010 + '0'); }
  if (  0100000U <= attr) { AppendChar(self, (attr /   0100000U) % 010 + '0'); }
  if (   010000U <= attr) { AppendChar(self, (attr /    010000U) % 010 + '0'); }
  if (    01000U <= attr) { AppendChar(self, (attr /     01000U) % 010 + '0'); }
  if (     0100U <= attr) { AppendChar(self, (attr /      0100U) % 010 + '0'); }
  if (      010U <= attr) { AppendChar(self, (attr /       010U) % 010 + '0'); }

  AppendChar(self, attr % 010 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits in octal
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendUInt64OctalStringBuffer (TRI_string_buffer_t * self, uint64_t attr) {
  Reserve(self, 17);

  if (01000000000000000ULL <= attr) { AppendChar(self, (attr / 01000000000000000ULL) % 010 + '0'); }
  if ( 0100000000000000ULL <= attr) { AppendChar(self, (attr /  0100000000000000ULL) % 010 + '0'); }
  if (  010000000000000ULL <= attr) { AppendChar(self, (attr /   010000000000000ULL) % 010 + '0'); }
  if (   01000000000000ULL <= attr) { AppendChar(self, (attr /    01000000000000ULL) % 010 + '0'); }
  if (    0100000000000ULL <= attr) { AppendChar(self, (attr /     0100000000000ULL) % 010 + '0'); }
  if (     010000000000ULL <= attr) { AppendChar(self, (attr /      010000000000ULL) % 010 + '0'); }
  if (      01000000000ULL <= attr) { AppendChar(self, (attr /       01000000000ULL) % 010 + '0'); }
  if (       0100000000ULL <= attr) { AppendChar(self, (attr /        0100000000ULL) % 010 + '0'); }
  if (        010000000ULL <= attr) { AppendChar(self, (attr /         010000000ULL) % 010 + '0'); }
  if (         01000000ULL <= attr) { AppendChar(self, (attr /          01000000ULL) % 010 + '0'); }
  if (          0100000ULL <= attr) { AppendChar(self, (attr /           0100000ULL) % 010 + '0'); }
  if (           010000ULL <= attr) { AppendChar(self, (attr /            010000ULL) % 010 + '0'); }
  if (            01000ULL <= attr) { AppendChar(self, (attr /             01000ULL) % 010 + '0'); }
  if (             0100ULL <= attr) { AppendChar(self, (attr /              0100ULL) % 010 + '0'); }
  if (              010ULL <= attr) { AppendChar(self, (attr /               010ULL) % 010 + '0'); }

  AppendChar(self, attr % 010 + '0');

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t in octal
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendSizeOctalStringBuffer (TRI_string_buffer_t * self, size_t attr) {
#if TRI_SIZEOF_SIZE_T == 8
  TRI_AppendUInt64OctalStringBuffer(self, (uint64_t) attr);
#else
  TRI_AppendUInt32OctalStringBuffer(self, (uint32_t) attr);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             INTEGER HEX APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits in hex
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendUInt32HexStringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  static char const * const HEX = "0123456789ABCDEF";

  Reserve(self, 5);

  if (0x1000U <= attr) { AppendChar(self, HEX[(attr / 0x1000U) % 0x10]); }
  if ( 0x100U <= attr) { AppendChar(self, HEX[(attr /  0x100U) % 0x10]); }
  if (  0x10U <= attr) { AppendChar(self, HEX[(attr /   0x10U) % 0x10]); }

  AppendChar(self, HEX[attr % 0x10]);

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits in hex
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendUInt64HexStringBuffer (TRI_string_buffer_t * self, uint64_t attr) {
  static char const * const HEX = "0123456789ABCDEF";

  Reserve(self, 9);

  if (0x10000000U <= attr) { AppendChar(self, HEX[(attr / 0x10000000U) % 0x10]); }
  if ( 0x1000000U <= attr) { AppendChar(self, HEX[(attr /  0x1000000U) % 0x10]); }
  if (  0x100000U <= attr) { AppendChar(self, HEX[(attr /   0x100000U) % 0x10]); }
  if (   0x10000U <= attr) { AppendChar(self, HEX[(attr /    0x10000U) % 0x10]); }
  if (    0x1000U <= attr) { AppendChar(self, HEX[(attr /     0x1000U) % 0x10]); }
  if (     0x100U <= attr) { AppendChar(self, HEX[(attr /      0x100U) % 0x10]); }
  if (      0x10U <= attr) { AppendChar(self, HEX[(attr /       0x10U) % 0x10]); }

  AppendChar(self, HEX[attr % 0x10]);

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t in hex
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendSizeHexStringBuffer (TRI_string_buffer_t * self, size_t attr) {
#if TRI_SIZEOF_SIZE_T == 8
  TRI_AppendUInt64HexStringBuffer(self, (uint64_t) attr);
#else
  TRI_AppendUInt32HexStringBuffer(self, (uint32_t) attr);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   FLOAT APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief appends floating point number with 8 bits
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendDoubleStringBuffer (TRI_string_buffer_t * self, double attr) {
  Reserve(self, 1);

  if (attr < 0.0) {
    AppendChar(self, '-');
    attr = -attr;
  }
  else if (attr == 0.0) {
    AppendChar(self, '0');
    *self->_bufferPtr = '\0';
    return;
  }

  if (((double)((uint32_t) attr)) == attr) {
    TRI_AppendUInt32StringBuffer(self, (uint32_t) attr);
    return;
  }
  else if (attr < (double) 429496U) {
    uint32_t smll;

    smll = (uint32_t)(attr * 10000.0);

    if (((double) smll) == attr * 10000.0) {
      uint32_t ep;

      TRI_AppendUInt32StringBuffer(self, smll / 10000);

      ep = smll % 10000;

      if (ep != 0) {
        size_t pos;
        char a1;
        char a2;
        char a3;
        char a4;

        pos = 0;

        Reserve(self, 6);

        AppendChar(self, '.');

        if ((ep / 1000L) % 10 != 0)  pos = 1;
        a1 = (char) ((ep / 1000L) % 10 + '0');

        if ((ep / 100L) % 10 != 0)  pos = 2;
        a2 = (char) ((ep / 100L) % 10 + '0');

        if ((ep / 10L) % 10 != 0)  pos = 3;
        a3 = (char) ((ep / 10L) % 10 + '0');

        if (ep % 10 != 0)  pos = 4;
        a4 = (char) (ep % 10 + '0');

        AppendChar(self, a1);
        if (pos > 1) { AppendChar(self, a2); }
        if (pos > 2) { AppendChar(self, a3); }
        if (pos > 3) { AppendChar(self, a4); }

        *self->_bufferPtr = '\0';
      }

      return;
    }
  }

  // we do not habe a small integral number nor small decimal number with only a few decimal digits

  // there at most 16 significant digits, first find out if we have an integer value
  if (10000000000000000.0 < attr) {
    size_t n;

    n = 0;

    while (10000000000000000.0 < attr) {
      attr /= 10.0;
      ++n;
    }

    TRI_AppendUInt64StringBuffer(self, (uint64_t) attr);

    Reserve(self, n);

    for (;  0 < n;  --n) {
      AppendChar(self, '0');
    }

    *self->_bufferPtr = '\0';
    return;
  }


  // very small, i. e. less than 1
  else if (attr < 1.0) {
    size_t n;

    n = 0;

    while (attr < 1.0) {
      attr *= 10.0;
      ++n;

      // should not happen, so it must be almost 0
      if (n > 400) {
        TRI_AppendUInt32StringBuffer(self, 0);
        return;
      }
    }

    Reserve(self, n + 2);

    AppendChar(self, '0');
    AppendChar(self, '.');

    for (--n;  0 < n;  --n) {
      AppendChar(self, '0');
    }

    attr = 10000000000000000.0 * attr;

    TRI_AppendUInt64StringBuffer(self, (uint64_t) attr);

    return;
  }


  // somewhere in between
  else {
    uint64_t m;
    double d;
    size_t n;

    m = (uint64_t) attr;
    d = attr - m;
    n = 0;

    TRI_AppendUInt64StringBuffer(self, m);

    while (d < 1.0) {
      d *= 10.0;
      ++n;

      // should not happen, so it must be almost 0
      if (n > 400) {
        return;
      }
    }

    Reserve(self, n + 1);

    AppendChar(self, '.');

    for (--n;  0 < n;  --n) {
      AppendChar(self, '0');
    }

    d = 10000000000000000.0 * d;

    TRI_AppendUInt64StringBuffer(self, (uint64_t) d);

    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           DATE AND TIME APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief appends time in standard format
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendTimeStringBuffer (TRI_string_buffer_t * self, int32_t attr) {
  int hour;
  int minute;
  int second;

  hour = attr / 3600;
  minute = (attr / 60) % 60;
  second = attr % 60;

  Reserve(self, 9);

  TRI_AppendInteger2StringBuffer(self, hour);
  AppendChar(self, ':');
  TRI_AppendInteger2StringBuffer(self, minute);
  AppendChar(self, ':');
  TRI_AppendInteger2StringBuffer(self, second);

  *self->_bufferPtr = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     CSV APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv integer
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendCsvInt32StringBuffer (TRI_string_buffer_t * self, int32_t i) {
  TRI_AppendInt32StringBuffer(self, i);
  Reserve(self, 1);
  AppendChar(self, ';');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv integer
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendCsvUInt32StringBuffer (TRI_string_buffer_t * self, uint32_t i) {
  TRI_AppendUInt32StringBuffer(self, i);
  Reserve(self, 1);
  AppendChar(self, ';');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv integer
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendCsvUInt64StringBuffer (TRI_string_buffer_t * self, uint64_t i) {
  TRI_AppendUInt64StringBuffer(self, i);
  Reserve(self, 1);
  AppendChar(self, ';');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv double
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendCsvDoubleStringBuffer (TRI_string_buffer_t * self, double d) {
  TRI_AppendDoubleStringBuffer(self, d);
  Reserve(self, 1);
  AppendChar(self, ';');
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
