////////////////////////////////////////////////////////////////////////////////
/// @brief a string buffer for sequential string concatenation
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

#include "string-buffer.h"
#include <stdlib.h>
#include "Basics/conversions.h"
#include "Zip/zip.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief append a character without check
////////////////////////////////////////////////////////////////////////////////

static inline void AppendChar (TRI_string_buffer_t * self, char chr) {
  *self->_current++ = chr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief how much space is presently left in buffer?
////////////////////////////////////////////////////////////////////////////////

static inline size_t Remaining (TRI_string_buffer_t * self) {
  return (size_t) (self->_len - (size_t) (self->_current - self->_buffer));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve space
////////////////////////////////////////////////////////////////////////////////

static int Reserve (TRI_string_buffer_t * self, size_t size) {
  if (size < 1) {
    return TRI_ERROR_NO_ERROR;
  }

  if (size > Remaining(self)) {
    ptrdiff_t off;
    size_t len;

    off = self->_current - self->_buffer;
    len = (size_t) (1.2 * (self->_len + size));
    TRI_ASSERT(len > 0);

    char* ptr = static_cast<char*>(TRI_Reallocate(self->_memoryZone, self->_buffer, len + 1));

    if (ptr == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    self->_buffer = ptr;
    self->_len = len;
    self->_current = self->_buffer + off;

    memset(self->_current, 0, Remaining(self) + 1);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append a string to a string buffer
////////////////////////////////////////////////////////////////////////////////

static int AppendString (TRI_string_buffer_t* self, char const* str, const size_t len) {
  if (0 < len) {
    int res;

    res = Reserve(self, len);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    memcpy(self->_current, str, len);
    self->_current += len;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes UTF-8 range U+0000 to U+007F
////////////////////////////////////////////////////////////////////////////////

static void EscapeUtf8Range0000T007F (TRI_string_buffer_t* self, char const** src) {
  uint8_t c;
  uint16_t i1;
  uint16_t i2;

  c = (uint8_t) *(*src);

  i1 = (((uint16_t) c) & 0xF0) >> 4;
  i2 = (((uint16_t) c) & 0x0F);

  AppendString(self, "\\u00", 4);
  AppendChar(self, (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10));
  AppendChar(self, (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes UTF-8 range U+0080 to U+07FF
////////////////////////////////////////////////////////////////////////////////

static void EscapeUtf8Range0080T07FF (TRI_string_buffer_t* self, char const** src) {
  uint8_t c;
  uint8_t d;

  c = (uint8_t) *((*src) + 0);
  d = (uint8_t) *((*src) + 1);

  // correct UTF-8
  if ((d & 0xC0) == 0x80) {
    uint16_t n;

    uint16_t i1;
    uint16_t i2;
    uint16_t i3;
    uint16_t i4;

    n = ((c & 0x1F) << 6) | (d & 0x3F);
    TRI_ASSERT(n >= 128);

    i1 = (n & 0xF000) >> 12;
    i2 = (n & 0x0F00) >>  8;
    i3 = (n & 0x00F0) >>  4;
    i4 = (n & 0x000F);

    AppendChar(self, '\\');
    AppendChar(self, 'u');

    AppendChar(self, (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10));
    AppendChar(self, (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10));
    AppendChar(self, (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10));
    AppendChar(self, (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10));

    (*src) += 1;
  }

  // corrupted UTF-8
  else {
    AppendChar(self, *(*src));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes UTF-8 range U+0800 to U+D7FF and U+E000 to U+FFFF
////////////////////////////////////////////////////////////////////////////////

static void EscapeUtf8Range0800TFFFF (TRI_string_buffer_t* self, char const** src) {
  uint8_t c;
  uint8_t d;
  uint8_t e;

  c = (uint8_t) *((*src) + 0);
  d = (uint8_t) *((*src) + 1);
  e = (uint8_t) *((*src) + 2);

  // correct UTF-8 (3-byte sequence UTF-8 1110xxxx 10xxxxxx)
  if ((d & 0xC0) == 0x80 && (e & 0xC0) == 0x80) {
    uint16_t n;

    uint16_t i1;
    uint16_t i2;
    uint16_t i3;
    uint16_t i4;

    n = ((c & 0x0F) << 12) | ((d & 0x3F) << 6) | (e & 0x3F);

    TRI_ASSERT(n >= 2048 && (n < 55296 || n > 57343));

    i1 = (n & 0xF000) >> 12;
    i2 = (n & 0x0F00) >>  8;
    i3 = (n & 0x00F0) >>  4;
    i4 = (n & 0x000F);

    AppendChar(self, '\\');
    AppendChar(self, 'u');

    AppendChar(self, (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10));
    AppendChar(self, (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10));
    AppendChar(self, (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10));
    AppendChar(self, (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10));

    (*src) += 2;
  }

  // corrupted UTF-8
  else {
    AppendChar(self, *(*src));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes UTF-8 range U+10000 to U+10FFFF
////////////////////////////////////////////////////////////////////////////////

static void EscapeUtf8Range10000T10FFFF (TRI_string_buffer_t* self, char const** src) {
  uint8_t c;
  uint8_t d;
  uint8_t e;
  uint8_t f;

  c = (uint8_t) *((*src) + 0);
  d = (uint8_t) *((*src) + 1);
  e = (uint8_t) *((*src) + 2);
  f = (uint8_t) *((*src) + 3);

  // correct UTF-8 (4-byte sequence UTF-8 1110xxxx 10xxxxxx 10xxxxxx)
  if ((d & 0xC0) == 0x80 && (e & 0xC0) == 0x80 && (f & 0xC0) == 0x80) {
    uint32_t n;

    uint32_t s1;
    uint32_t s2;

    uint16_t i1;
    uint16_t i2;
    uint16_t i3;
    uint16_t i4;

    n = ((c & 0x0F) << 18) | ((d & 0x3F) << 12) | ((e & 0x3F) << 6) | (f & 0x3F);
    TRI_ASSERT(n >= 65536 && n <= 1114111);

    // construct the surrogate pairs
    n -= 0x10000;

    s1 = ((n & 0xFFC00) >> 10) + 0xD800;
    s2 = (n & 0x3FF) + 0xDC00;

    // encode high surrogate
    i1 = (s1 & 0xF000) >> 12;
    i2 = (s1 & 0x0F00) >>  8;
    i3 = (s1 & 0x00F0) >>  4;
    i4 = (s1 & 0x000F);

    AppendChar(self, '\\');
    AppendChar(self, 'u');

    AppendChar(self, (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10));
    AppendChar(self, (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10));
    AppendChar(self, (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10));
    AppendChar(self, (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10));

    // encode low surrogate
    i1 = (s2 & 0xF000) >> 12;
    i2 = (s2 & 0x0F00) >>  8;
    i3 = (s2 & 0x00F0) >>  4;
    i4 = (s2 & 0x000F);

    AppendChar(self, '\\');
    AppendChar(self, 'u');

    AppendChar(self, (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10));
    AppendChar(self, (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10));
    AppendChar(self, (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10));
    AppendChar(self, (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10));

    // advance src
    (*src) += 3;
  }

  // corrupted UTF-8
  else {
    AppendChar(self, *(*src));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new string buffer and initialise it
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* TRI_CreateStringBuffer (TRI_memory_zone_t* zone) {
  TRI_string_buffer_t* self = (TRI_string_buffer_t*) TRI_Allocate(zone, sizeof(TRI_string_buffer_t), false);

  if (self == nullptr) {
    return nullptr;
  }

  TRI_InitStringBuffer(self, zone);

  return self;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new string buffer and initialise it with a specific size
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* TRI_CreateSizedStringBuffer (TRI_memory_zone_t* zone,
                                                  const size_t size) {
  TRI_string_buffer_t* self = (TRI_string_buffer_t*) TRI_Allocate(zone, sizeof(TRI_string_buffer_t), false);

  if (self == nullptr) {
    return nullptr;
  }

  TRI_InitSizedStringBuffer(self, zone, size);

  return self;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the string buffer
///
/// @warning You must call initialise before using the string buffer.
////////////////////////////////////////////////////////////////////////////////

void TRI_InitStringBuffer (TRI_string_buffer_t * self, TRI_memory_zone_t* zone) {
  self->_memoryZone = zone;
  self->_buffer = nullptr;
  self->_current = 0;
  self->_len = 0;

  Reserve(self, 120);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the string buffer with a specific size
///
/// @warning You must call initialise before using the string buffer.
////////////////////////////////////////////////////////////////////////////////

void TRI_InitSizedStringBuffer (TRI_string_buffer_t * self,
                                TRI_memory_zone_t* zone,
                                const size_t length) {
  self->_memoryZone = zone;
  self->_buffer = 0;
  self->_current = 0;
  self->_len = 0;

  Reserve(self, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the string buffer
///
/// @warning You must call free or destroy after using the string buffer.
////////////////////////////////////////////////////////////////////////////////

void  TRI_DestroyStringBuffer (TRI_string_buffer_t * self) {
  if (self->_buffer != nullptr) {
    TRI_Free(self->_memoryZone, self->_buffer);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the string buffer and cleans the buffer
///
/// @warning You must call free or destroy after using the string buffer.
////////////////////////////////////////////////////////////////////////////////

void TRI_AnnihilateStringBuffer (TRI_string_buffer_t * self) {
  if (self->_buffer != nullptr) {
    // somewhat paranoid? don't ask me
    memset(self->_buffer, 0, self->_len);
    TRI_Free(self->_memoryZone, self->_buffer);
    self->_buffer = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the string buffer and the pointer
////////////////////////////////////////////////////////////////////////////////

void  TRI_FreeStringBuffer (TRI_memory_zone_t* zone, TRI_string_buffer_t * self) {
  TRI_DestroyStringBuffer(self);
  TRI_Free(zone, self);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief compress the string buffer using deflate
////////////////////////////////////////////////////////////////////////////////

int TRI_DeflateStringBuffer (TRI_string_buffer_t* self,
                             size_t bufferSize) {
  TRI_string_buffer_t deflated;
  const char* ptr;
  const char* end;
  char* buffer;
  int res;

  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree  = Z_NULL;
  strm.opaque = Z_NULL;

  // initialise deflate procedure
  res = deflateInit(&strm, Z_DEFAULT_COMPRESSION);

  if (res != Z_OK) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  buffer = (char*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, bufferSize, false);

  if (buffer == nullptr) {
    (void) deflateEnd(&strm);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // we'll use this buffer for the output
  TRI_InitStringBuffer(&deflated, TRI_UNKNOWN_MEM_ZONE);

  ptr = TRI_BeginStringBuffer(self);
  end = ptr + TRI_LengthStringBuffer(self);

  while (ptr < end) {
    int flush;

    strm.next_in = (unsigned char*) ptr;

    if (end - ptr > (int) bufferSize) {
      strm.avail_in = (int) bufferSize;
      flush = Z_NO_FLUSH;
    }
    else {
      strm.avail_in = (uInt) (end - ptr);
      flush = Z_FINISH;
    }
    ptr += strm.avail_in;

    do {
      strm.avail_out = (int) bufferSize;
      strm.next_out = (unsigned char*) buffer;
      res = deflate(&strm, flush);

      if (res == Z_STREAM_ERROR) {
        (void) deflateEnd(&strm);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);
        TRI_DestroyStringBuffer(&deflated);

        return TRI_ERROR_INTERNAL;
      }

      if (TRI_AppendString2StringBuffer(&deflated, (char*) buffer, bufferSize - strm.avail_out) != TRI_ERROR_NO_ERROR) {
        (void) deflateEnd(&strm);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);
        TRI_DestroyStringBuffer(&deflated);

        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
    while (strm.avail_out == 0);
  }

  // deflate successful
  (void) deflateEnd(&strm);

  TRI_SwapStringBuffer(self, &deflated);
  TRI_DestroyStringBuffer(&deflated);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure the string buffer has a specific capacity
////////////////////////////////////////////////////////////////////////////////

int TRI_ReserveStringBuffer (TRI_string_buffer_t* self, const size_t length) {
  return Reserve(self, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief swaps content with another string buffer
////////////////////////////////////////////////////////////////////////////////

void TRI_SwapStringBuffer (TRI_string_buffer_t * self, TRI_string_buffer_t * other) {
  char * otherBuffer           = other->_buffer;
  char * otherCurrent          = other->_current;
  size_t otherLen              = other->_len;
  TRI_memory_zone_t* otherZone = other->_memoryZone;

  other->_buffer      = self->_buffer;
  other->_current     = self->_current;
  other->_len         = self->_len;
  other->_memoryZone  = self->_memoryZone;

  self->_buffer       = otherBuffer;
  self->_current      = otherCurrent;
  self->_len          = otherLen;
  self->_memoryZone   = otherZone;
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
  return self->_current;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns length of the character buffer
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthStringBuffer (TRI_string_buffer_t const * self) {
  return (size_t) (self->_current - self->_buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increases length of the character buffer
////////////////////////////////////////////////////////////////////////////////

void TRI_IncreaseLengthStringBuffer (TRI_string_buffer_t * self, size_t n) {
  self->_current += n;
  *self->_current = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if buffer is empty
////////////////////////////////////////////////////////////////////////////////

bool TRI_EmptyStringBuffer (TRI_string_buffer_t const* self) {
  return self->_buffer == self->_current;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the buffer
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearStringBuffer (TRI_string_buffer_t* self) {
  if (self->_buffer != nullptr) {
    if (self->_len > 0 && self->_current == self->_buffer) {
      // we're at the beginning of the buffer
      // avoid double erasure and exit early
      *self->_current = '\0';
      return;
    }

    self->_current = self->_buffer;
    memset(self->_buffer, 0, self->_len + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resets the buffer (without clearing)
////////////////////////////////////////////////////////////////////////////////

void TRI_ResetStringBuffer (TRI_string_buffer_t* self) {
  if (self->_buffer != nullptr) {
    self->_current = self->_buffer;

    if (self->_len > 0) {
      *self->_current = '\0';
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief steals the buffer of a string buffer
////////////////////////////////////////////////////////////////////////////////

char* TRI_StealStringBuffer (TRI_string_buffer_t* self) {
  char* result = self->_buffer;

  // reset everthing
  self->_buffer  = nullptr;
  self->_current = nullptr;
  self->_len     = 0;

  // might be nullptr
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the character at the end of the string buffer
////////////////////////////////////////////////////////////////////////////////

char TRI_LastCharStringBuffer (TRI_string_buffer_t const* self) {
  if (self->_buffer != nullptr && self->_current - self->_buffer > 0) {
    return *(self->_current - 1);
  }

  return '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the string buffer
////////////////////////////////////////////////////////////////////////////////

int TRI_CopyStringBuffer (TRI_string_buffer_t * self, TRI_string_buffer_t const * source) {
  return TRI_ReplaceStringStringBuffer(self, source->_buffer, (size_t) (source->_current - source->_buffer));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the first characters
////////////////////////////////////////////////////////////////////////////////

void TRI_EraseFrontStringBuffer (TRI_string_buffer_t * self, size_t len) {
  size_t off = (size_t) (self->_current - self->_buffer);

  if (off <= len) {
    TRI_ClearStringBuffer(self);
  }
  else if (0 < len) {
    memmove(self->_buffer, self->_buffer + len, off - len);
    self->_current -= len;
    memset(self->_current, 0, self->_len - (self->_current - self->_buffer));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the first characters but does not clear the remaining
/// buffer space
////////////////////////////////////////////////////////////////////////////////

void TRI_MoveFrontStringBuffer (TRI_string_buffer_t * self, size_t len) {
  size_t off = (size_t) (self->_current - self->_buffer);

  if (off <= len) {
    TRI_ResetStringBuffer(self);
  }
  else if (0 < len) {
    memmove(self->_buffer, self->_buffer + len, off - len);
    self->_current -= len;
    *self->_current = '\0';
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces characters
////////////////////////////////////////////////////////////////////////////////

int TRI_ReplaceStringStringBuffer (TRI_string_buffer_t * self, char const * str, size_t len) {
  self->_current = self->_buffer;

  return TRI_AppendString2StringBuffer(self, str, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces characters
////////////////////////////////////////////////////////////////////////////////

int TRI_ReplaceStringBufferStringBuffer (TRI_string_buffer_t * self, TRI_string_buffer_t const * text) {
  self->_current = self->_buffer;

  return TRI_AppendString2StringBuffer(self, text->_buffer, (size_t) (text->_current - text->_buffer));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  STRING APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends character
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendCharStringBuffer (TRI_string_buffer_t * self, char chr) {
  int res = Reserve(self, 1);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, chr);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendStringStringBuffer (TRI_string_buffer_t * self, char const * str) {
  return AppendString(self, str, strlen(str));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendString2StringBuffer (TRI_string_buffer_t * self, char const * str, size_t len) {
  return AppendString(self, str, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string buffer
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendStringBufferStringBuffer (TRI_string_buffer_t * self, TRI_string_buffer_t const * text) {
  return TRI_AppendString2StringBuffer(self, text->_buffer, (size_t) (text->_current - text->_buffer));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a blob
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendBlobStringBuffer (TRI_string_buffer_t * self, TRI_blob_t const * text) {
  return TRI_AppendString2StringBuffer(self, text->data, text->length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends eol character
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendEolStringBuffer (TRI_string_buffer_t * self) {
  return TRI_AppendCharStringBuffer(self, '\n');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters but url-encode the string
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendUrlEncodedStringStringBuffer (TRI_string_buffer_t * self,
                                            char const * src) {
  static char hexChars[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

  size_t len = strlen(src);
  int res;

  char const* end;

  res = Reserve(self, len * 3);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  end = src + len;

  for (; src < end;  ++src) {
    if ('0' <= *src && *src <= '9') {
      AppendChar(self, *src);
    }

    else if ('a' <= *src && *src <= 'z') {
      AppendChar(self, *src);
    }

    else if ('A' <= *src && *src <= 'Z') {
      AppendChar(self, *src);
    }

    else if (*src == '-' || *src == '_' || *src == '.' || *src == '~') {
      AppendChar(self, *src);
    }

    else {
      uint8_t n = (uint8_t)(*src);
      uint8_t n1 = n >> 4;
      uint8_t n2 = n & 0x0F;

      AppendChar(self, '%');
      AppendChar(self, hexChars[n1]);
      AppendChar(self, hexChars[n2]);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters but json-encode the string
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendJsonEncodedStringStringBuffer (TRI_string_buffer_t * self,
                                             char const * src,
                                             bool escapeSlash) {
  char const* ptr = src;
  int res = TRI_ERROR_NO_ERROR;

  while (*ptr != '\0') {
    res = Reserve(self, 2);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    switch (*ptr) {
      case '/':
        if (escapeSlash) {
          AppendChar(self, '\\');
        }

        AppendChar(self, *ptr);
        break;

      case '\\':
      case '"':
        AppendChar(self, '\\');
        AppendChar(self, *ptr);
        break;

      case '\b':
        AppendChar(self, '\\');
        AppendChar(self, 'b');
        break;

      case '\f':
        AppendChar(self, '\\');
        AppendChar(self, 'f');
        break;

      case '\n':
        AppendChar(self, '\\');
        AppendChar(self, 'n');
        break;

      case '\r':
        AppendChar(self, '\\');
        AppendChar(self, 'r');
        break;

      case '\t':
        AppendChar(self, '\\');
        AppendChar(self, 't');
        break;

      case '\0':
        res = Reserve(self, 6);
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
        AppendString(self, "\\u0000", 6);
        break;

      default:
        {
          // next character as unsigned char
          uint8_t c = (uint8_t) *ptr;

          // character is in the normal latin1 range
          if ((c & 0x80) == 0) {
            // special character, escape
            if (c < 32) {
              res = Reserve(self, 6);
              if (res != TRI_ERROR_NO_ERROR) {
                return res;
              }

              EscapeUtf8Range0000T007F(self, &ptr);
            }

            // normal latin1
            else {
              AppendChar(self, *ptr);
            }
          }

          // unicode range 0080 - 07ff (2-byte sequence UTF-8)
          else if ((c & 0xE0) == 0xC0) {
            // hopefully correct UTF-8
            if (*(ptr + 1) != '\0') {
              res = Reserve(self, 6);
              if (res != TRI_ERROR_NO_ERROR) {
                return res;
              }
              EscapeUtf8Range0080T07FF(self, &ptr);
            }

            // corrupted UTF-8
            else {
              AppendChar(self, *ptr);
            }
          }

          // unicode range 0800 - ffff (3-byte sequence UTF-8)
          else if ((c & 0xF0) == 0xE0) {
            // hopefully correct UTF-8
            if (*(ptr + 1) != '\0' && *(ptr + 2) != '\0') {
              res = Reserve(self, 6);
              if (res != TRI_ERROR_NO_ERROR) {
                return res;
              }
              EscapeUtf8Range0800TFFFF(self, &ptr);
            }

            // corrupted UTF-8
            else {
              AppendChar(self, *ptr);
            }
          }

          // unicode range 10000 - 10ffff (4-byte sequence UTF-8)
          else if ((c & 0xF8) == 0xF0) {
            // hopefully correct UTF-8
            if (*(ptr + 1) != '\0' && *(ptr + 2) != '\0' && *(ptr + 3) != '\0') {
              res = Reserve(self, 12);
              if (res != TRI_ERROR_NO_ERROR) {
                return res;
              }
              EscapeUtf8Range10000T10FFFF(self, &ptr);
            }

            // corrupted UTF-8
            else {
              AppendChar(self, *ptr);
            }
          }

          // unicode range above 10ffff -- NOT IMPLEMENTED
          else {
            AppendChar(self, *ptr);
          }
        }

        break;
    }

    ++ptr;
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 INTEGER APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with two digits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendInteger2StringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  int res = Reserve(self, 2);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, (attr / 10U) % 10 + '0');
  AppendChar(self,  attr % 10        + '0');

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with three digits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendInteger3StringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  int res = Reserve(self, 3);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, (attr / 100U) % 10 + '0');
  AppendChar(self, (attr /  10U) % 10 + '0');
  AppendChar(self,  attr         % 10 + '0');

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with four digits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendInteger4StringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  int res = Reserve(self, 4);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, (attr / 1000U) % 10 + '0');
  AppendChar(self, (attr /  100U) % 10 + '0');
  AppendChar(self, (attr /   10U) % 10 + '0');
  AppendChar(self,  attr          % 10 + '0');

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 8 bits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendInt8StringBuffer (TRI_string_buffer_t * self, int8_t attr) {
  int res = Reserve(self, 4);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringInt8InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 8 bits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendUInt8StringBuffer (TRI_string_buffer_t * self, uint8_t attr) {
  int res = Reserve(self, 3);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt8InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 16 bits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendInt16StringBuffer (TRI_string_buffer_t * self, int16_t attr) {
  int res = Reserve(self, 6);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringInt16InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendUInt16StringBuffer (TRI_string_buffer_t * self, uint16_t attr) {
  int res = Reserve(self, 5);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt16InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 32 bits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendInt32StringBuffer (TRI_string_buffer_t * self, int32_t attr) {
  int res = Reserve(self, 11);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringInt32InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendUInt32StringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  int res = Reserve(self, 10);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt32InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 64 bits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendInt64StringBuffer (TRI_string_buffer_t * self, int64_t attr) {
  int res = Reserve(self, 20);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringInt64InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendUInt64StringBuffer (TRI_string_buffer_t * self, uint64_t attr) {
  int res = Reserve(self, 21);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt64InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendSizeStringBuffer (TRI_string_buffer_t * self, size_t attr) {
#if TRI_SIZEOF_SIZE_T == 8
  return TRI_AppendUInt64StringBuffer(self, (uint64_t) attr);
#else
  return TRI_AppendUInt32StringBuffer(self, (uint32_t) attr);
#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                           INTEGER OCTAL APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits in octal
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendUInt32OctalStringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  int res = Reserve(self, 11);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt32OctalInPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits in octal
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendUInt64OctalStringBuffer (TRI_string_buffer_t * self, uint64_t attr) {
  int res = Reserve(self, 22);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt64OctalInPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t in octal
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendSizeOctalStringBuffer (TRI_string_buffer_t * self, size_t attr) {
#if TRI_SIZEOF_SIZE_T == 8
  return TRI_AppendUInt64OctalStringBuffer(self, (uint64_t) attr);
#else
  return TRI_AppendUInt32OctalStringBuffer(self, (uint32_t) attr);
#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                             INTEGER HEX APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits in hex
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendUInt32HexStringBuffer (TRI_string_buffer_t * self, uint32_t attr) {
  int res = Reserve(self, 5);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt32HexInPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits in hex
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendUInt64HexStringBuffer (TRI_string_buffer_t * self, uint64_t attr) {
  int res = Reserve(self, 9);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt64HexInPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t in hex
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendSizeHexStringBuffer (TRI_string_buffer_t * self, size_t attr) {
#if TRI_SIZEOF_SIZE_T == 8
  return TRI_AppendUInt64HexStringBuffer(self, (uint64_t) attr);
#else
  return TRI_AppendUInt32HexStringBuffer(self, (uint32_t) attr);
#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   FLOAT APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends floating point number
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendDoubleStringBuffer (TRI_string_buffer_t * self, double attr) {
  // IEEE754 NaN values have an interesting property that we can exploit...
  // if the architecture does not use IEEE754 values then this shouldn't do
  // any harm either
  if (attr != attr) {
    return TRI_AppendStringStringBuffer(self, "NaN");
  }

  if (attr == HUGE_VAL) {
    return TRI_AppendStringStringBuffer(self, "inf");
  }
  if (attr == -HUGE_VAL) {
    return TRI_AppendStringStringBuffer(self, "-inf");
  }

  int res = Reserve(self, 1);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (attr < 0.0) {
    AppendChar(self, '-');
    attr = -attr;
  }
  else if (attr == 0.0) {
    AppendChar(self, '0');
    return TRI_ERROR_NO_ERROR;
  }

  if (((double)((uint32_t) attr)) == attr) {
    return TRI_AppendUInt32StringBuffer(self, (uint32_t) attr);
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

        res = Reserve(self, 6);

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

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

      }

      return TRI_ERROR_NO_ERROR;
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

    res = TRI_AppendUInt64StringBuffer(self, (uint64_t) attr);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    res = Reserve(self, n);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    for (;  0 < n;  --n) {
      AppendChar(self, '0');
    }

    return TRI_ERROR_NO_ERROR;
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
        return TRI_AppendUInt32StringBuffer(self, 0);
      }
    }

    res = Reserve(self, n + 2);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    AppendChar(self, '0');
    AppendChar(self, '.');

    for (--n;  0 < n;  --n) {
      AppendChar(self, '0');
    }

    attr = 10000000000000000.0 * attr;

    return TRI_AppendUInt64StringBuffer(self, (uint64_t) attr);
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
        return TRI_ERROR_NO_ERROR;
      }
    }

    res = Reserve(self, n + 1);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    AppendChar(self, '.');

    for (--n;  0 < n;  --n) {
      AppendChar(self, '0');
    }

    d = 10000000000000000.0 * d;

    return TRI_AppendUInt64StringBuffer(self, (uint64_t) d);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                           DATE AND TIME APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends time in standard format
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendTimeStringBuffer (TRI_string_buffer_t * self, int32_t attr) {
  int hour;
  int minute;
  int second;
  int res;

  hour = attr / 3600;
  minute = (attr / 60) % 60;
  second = attr % 60;

  res = Reserve(self, 9);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_AppendInteger2StringBuffer(self, (uint32_t) hour);
  AppendChar(self, ':');
  TRI_AppendInteger2StringBuffer(self, (uint32_t) minute);
  AppendChar(self, ':');
  TRI_AppendInteger2StringBuffer(self, (uint32_t) second);

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     CSV APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv 32-bit integer
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendCsvInt32StringBuffer (TRI_string_buffer_t * self, int32_t i) {
  int res;

  res = TRI_AppendInt32StringBuffer(self, i);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = Reserve(self, 1);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, ';');
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv unisgned 32-bit integer
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendCsvUInt32StringBuffer (TRI_string_buffer_t * self, uint32_t i) {
  int res;

  res = TRI_AppendUInt32StringBuffer(self, i);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = Reserve(self, 1);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, ';');
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv 64-bit integer
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendCsvInt64StringBuffer (TRI_string_buffer_t * self, int64_t i) {
  int res;

  res = TRI_AppendInt64StringBuffer(self, i);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = Reserve(self, 1);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, ';');
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv unsigned 64-bit integer
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendCsvUInt64StringBuffer (TRI_string_buffer_t * self, uint64_t i) {
  int res;

  res = TRI_AppendUInt64StringBuffer(self, i);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = Reserve(self, 1);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, ';');
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv double
////////////////////////////////////////////////////////////////////////////////

int TRI_AppendCsvDoubleStringBuffer (TRI_string_buffer_t * self, double d) {
  int res;

  res = TRI_AppendDoubleStringBuffer(self, d);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = Reserve(self, 1);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, ';');
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
