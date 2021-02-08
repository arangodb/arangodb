////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <memory>

#include "StringBuffer.h"

#include "Basics/ScopeGuard.h"
#include "Basics/conversions.h"
#include "Basics/debugging.h"
#include "Basics/fpconv.h"
#include "Zip/zip.h"

/// @brief append a character without check
static inline void AppendChar(TRI_string_buffer_t* self, char chr) {
  *self->_current++ = chr;
}

/// @brief how much space is presently left in buffer?
static inline size_t Remaining(TRI_string_buffer_t* self) {
  return self->_len - static_cast<size_t>(self->_current - self->_buffer);
}

/// @brief reserve space
static ErrorCode Reserve(TRI_string_buffer_t* self, size_t size) {
  if (size > Remaining(self)) {
    ptrdiff_t off = self->_current - self->_buffer;
    size_t len = static_cast<size_t>(1.3 * (self->_len + size));
    TRI_ASSERT(len > 0);

    char* ptr = static_cast<char*>(TRI_Reallocate(self->_buffer, len + 1));

    if (ptr == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    self->_buffer = ptr;
    self->_len = len;
    self->_current = self->_buffer + off;

    if (self->_initializeMemory) {
      memset(self->_current, 0, Remaining(self) + 1);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief append a string to a string buffer
static ErrorCode AppendString(TRI_string_buffer_t* self, char const* str, size_t const len) {
  if (0 < len) {
    auto res = Reserve(self, len);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    memcpy(self->_current, str, len);
    self->_current += len;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief create a new string buffer and initialize it
TRI_string_buffer_t* TRI_CreateStringBuffer() {
  auto self =
      static_cast<TRI_string_buffer_t*>(TRI_Allocate(sizeof(TRI_string_buffer_t)));

  if (self == nullptr) {
    return nullptr;
  }

  TRI_InitStringBuffer(self);

  return self;
}

/// @brief create a new string buffer and initialize it with a specific size
TRI_string_buffer_t* TRI_CreateSizedStringBuffer(size_t size) {
  auto self =
      static_cast<TRI_string_buffer_t*>(TRI_Allocate(sizeof(TRI_string_buffer_t)));

  if (self == nullptr) {
    return nullptr;
  }

  TRI_InitSizedStringBuffer(self, size);

  return self;
}

/// @brief initializes the string buffer
///
/// @warning You must call initialize before using the string buffer.
void TRI_InitStringBuffer(TRI_string_buffer_t* self, bool initializeMemory) {
  self->_buffer = nullptr;
  self->_current = nullptr;
  self->_len = 0;
  self->_initializeMemory = initializeMemory;

  Reserve(self, 120);
}

/// @brief initializes the string buffer with a specific size
///
/// @warning You must call initialize before using the string buffer.
void TRI_InitSizedStringBuffer(TRI_string_buffer_t* self, size_t const length,
                               bool initializeMemory) {
  self->_buffer = nullptr;
  self->_current = nullptr;
  self->_len = 0;
  self->_initializeMemory = initializeMemory;

  if (length == 0) {  // 0 does not work as allocation size
    Reserve(self, 4);
  } else {
    Reserve(self, length);
  }
}

/// @brief frees the string buffer
///
/// @warning You must call free or destroy after using the string buffer.
void TRI_DestroyStringBuffer(TRI_string_buffer_t* self) {
  if (self->_buffer != nullptr) {
    TRI_Free(self->_buffer);
  }
}

/// @brief frees the string buffer and cleans the buffer
///
/// @warning You must call free or destroy after using the string buffer.
void TRI_AnnihilateStringBuffer(TRI_string_buffer_t* self) {
  if (self->_buffer != nullptr) {
    // somewhat paranoid? don't ask me
    memset(self->_buffer, 0, self->_len);
    TRI_Free(self->_buffer);
    self->_buffer = nullptr;
  }
}

/// @brief frees the string buffer and the pointer
void TRI_FreeStringBuffer(TRI_string_buffer_t* self) {
  TRI_DestroyStringBuffer(self);
  TRI_Free(self);
}

/// @brief compress the string buffer using deflate
ErrorCode TRI_DeflateStringBuffer(TRI_string_buffer_t* self, size_t bufferSize) {
  TRI_string_buffer_t deflated;
  char const* ptr;
  char const* end;
  char* buffer;
  int res;

  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  // initialize deflate procedure
  res = deflateInit(&strm, Z_DEFAULT_COMPRESSION);

  if (res != Z_OK) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  buffer = static_cast<char*>(TRI_Allocate(bufferSize));

  if (buffer == nullptr) {
    (void)deflateEnd(&strm);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // we'll use this buffer for the output
  TRI_InitStringBuffer(&deflated);

  ptr = TRI_BeginStringBuffer(self);
  end = ptr + TRI_LengthStringBuffer(self);

  while (ptr < end) {
    int flush;

    strm.next_in = (unsigned char*)ptr;

    if (end - ptr > (int)bufferSize) {
      strm.avail_in = (int)bufferSize;
      flush = Z_NO_FLUSH;
    } else {
      strm.avail_in = (uInt)(end - ptr);
      flush = Z_FINISH;
    }
    ptr += strm.avail_in;

    do {
      strm.avail_out = (int)bufferSize;
      strm.next_out = (unsigned char*)buffer;
      res = deflate(&strm, flush);

      if (res == Z_STREAM_ERROR) {
        (void)deflateEnd(&strm);
        TRI_Free(buffer);
        TRI_DestroyStringBuffer(&deflated);

        return TRI_ERROR_INTERNAL;
      }

      if (TRI_AppendString2StringBuffer(&deflated, (char*)buffer,
                                        bufferSize - strm.avail_out) != TRI_ERROR_NO_ERROR) {
        (void)deflateEnd(&strm);
        TRI_Free(buffer);
        TRI_DestroyStringBuffer(&deflated);

        return TRI_ERROR_OUT_OF_MEMORY;
      }
    } while (strm.avail_out == 0);
  }

  // deflate successful
  (void)deflateEnd(&strm);

  TRI_SwapStringBuffer(self, &deflated);
  TRI_DestroyStringBuffer(&deflated);

  TRI_Free(buffer);

  return TRI_ERROR_NO_ERROR;
}

/// @brief ensure the string buffer has a specific capacity
ErrorCode TRI_ReserveStringBuffer(TRI_string_buffer_t* self, size_t const length) {
  if (length > 0) {
    return Reserve(self, length);
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief swaps content with another string buffer
void TRI_SwapStringBuffer(TRI_string_buffer_t* self, TRI_string_buffer_t* other) {
  char* otherBuffer = other->_buffer;
  char* otherCurrent = other->_current;
  size_t otherLen = other->_len;

  other->_buffer = self->_buffer;
  other->_current = self->_current;
  other->_len = self->_len;

  self->_buffer = otherBuffer;
  self->_current = otherCurrent;
  self->_len = otherLen;
}

/// @brief returns pointer to the beginning of the character buffer
char const* TRI_BeginStringBuffer(TRI_string_buffer_t const* self) {
  return self->_buffer;
}

/// @brief returns pointer to the end of the character buffer
char const* TRI_EndStringBuffer(TRI_string_buffer_t const* self) {
  return self->_current;
}

/// @brief returns length of the character buffer
size_t TRI_LengthStringBuffer(TRI_string_buffer_t const* self) {
  return (size_t)(self->_current - self->_buffer);
}

/// @brief returns capacity of the character buffer
size_t TRI_CapacityStringBuffer(TRI_string_buffer_t const* self) {
  return self->_len;
}

/// @brief increases length of the character buffer
void TRI_IncreaseLengthStringBuffer(TRI_string_buffer_t* self, size_t n) {
  self->_current += n;
  *self->_current = '\0';
}

/// @brief returns true if buffer is empty
bool TRI_EmptyStringBuffer(TRI_string_buffer_t const* self) {
  return self->_buffer == self->_current;
}

/// @brief clears the buffer
void TRI_ClearStringBuffer(TRI_string_buffer_t* self) {
  if (self->_buffer != nullptr) {
    if (self->_len > 0 && self->_current == self->_buffer) {
      // we're at the beginning of the buffer
      // avoid double erasure and exit early
      *self->_current = '\0';
      return;
    }

    self->_current = self->_buffer;
    if (self->_initializeMemory) {
      memset(self->_buffer, 0, self->_len + 1);
    } else {
      *self->_current = '\0';
    }
  }
}

/// @brief resets the buffer (without clearing)
void TRI_ResetStringBuffer(TRI_string_buffer_t* self) {
  if (self->_buffer != nullptr) {
    self->_current = self->_buffer;

    if (self->_len > 0) {
      *self->_current = '\0';
    }
  }
}

/// @brief steals the buffer of a string buffer
char* TRI_StealStringBuffer(TRI_string_buffer_t* self) {
  char* result = self->_buffer;

  // reset everthing
  self->_buffer = nullptr;
  self->_current = nullptr;
  self->_len = 0;

  // might be nullptr
  return result;
}

/// @brief copies the string buffer
ErrorCode TRI_CopyStringBuffer(TRI_string_buffer_t* self, TRI_string_buffer_t const* source) {
  return TRI_ReplaceStringStringBuffer(self, source->_buffer,
                                       (size_t)(source->_current - source->_buffer));
}

/// @brief removes the first characters
void TRI_EraseFrontStringBuffer(TRI_string_buffer_t* self, size_t len) {
  size_t off = (size_t)(self->_current - self->_buffer);

  if (off <= len) {
    TRI_ClearStringBuffer(self);
  } else if (0 < len) {
    memmove(self->_buffer, self->_buffer + len, off - len);
    self->_current -= len;
    memset(self->_current, 0, self->_len - (self->_current - self->_buffer));
  }
}

/// @brief replaces characters
ErrorCode TRI_ReplaceStringStringBuffer(TRI_string_buffer_t* self,
                                        char const* str, size_t len) {
  self->_current = self->_buffer;

  return TRI_AppendString2StringBuffer(self, str, len);
}

/// @brief appends character
ErrorCode TRI_AppendCharStringBuffer(TRI_string_buffer_t* self, char chr) {
  auto res = Reserve(self, 1);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, chr);
  return TRI_ERROR_NO_ERROR;
}

/// @brief appends characters
ErrorCode TRI_AppendStringStringBuffer(TRI_string_buffer_t* self, char const* str) {
  return AppendString(self, str, strlen(str));
}

/// @brief appends characters
ErrorCode TRI_AppendString2StringBuffer(TRI_string_buffer_t* self,
                                        char const* str, size_t len) {
  return AppendString(self, str, len);
}

ErrorCode AppendJsonEncoded(TRI_string_buffer_t* self, char const* src,
                            size_t length, bool escapeForwardSlashes) {
  static char const EscapeTable[256] = {
      // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E
      // F
      'u',  'u', 'u', 'u', 'u', 'u', 'u', 'u', 'b', 't', 'n', 'u', 'f', 'r',
      'u',
      'u',  // 00
      'u',  'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
      'u',
      'u',  // 10
      0,    0,   '"', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,
      '/',  // 20
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,
      0,  // 30~4F
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      '\\', 0,   0,   0,  // 50
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,
      0,  // 60~FF
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0};

  // reserve enough room for the whole string at once
  auto res = Reserve(self, 6 * length + 2);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  AppendChar(self, '"');

  uint8_t const* p = reinterpret_cast<uint8_t const*>(src);
  uint8_t const* e = p + length;
  while (p < e) {
    uint8_t c = *p;

    if ((c & 0x80) == 0) {
      // check for control characters
      char esc = EscapeTable[c];

      if (esc) {
        if (c != '/' || escapeForwardSlashes) {
          // escape forward slashes only when requested
          AppendChar(self, '\\');
        }
        AppendChar(self, static_cast<char>(esc));

        if (esc == 'u') {
          uint16_t i1 = (((uint16_t)c) & 0xf0) >> 4;
          uint16_t i2 = (((uint16_t)c) & 0x0f);

          AppendChar(self, '0');
          AppendChar(self, '0');
          AppendChar(self, static_cast<char>((i1 < 10) ? ('0' + i1) : ('A' + i1 - 10)));
          AppendChar(self, static_cast<char>((i2 < 10) ? ('0' + i2) : ('A' + i2 - 10)));
        }
      } else {
        AppendChar(self, static_cast<char>(c));
      }
    } else if ((c & 0xe0) == 0xc0) {
      // two-byte sequence
      if (p + 1 >= e) {
        return TRI_ERROR_INTERNAL;
      }

      memcpy(self->_current, reinterpret_cast<char const*>(p), 2);
      self->_current += 2;
      ++p;
    } else if ((c & 0xf0) == 0xe0) {
      // three-byte sequence
      if (p + 2 >= e) {
        return TRI_ERROR_INTERNAL;
      }

      memcpy(self->_current, reinterpret_cast<char const*>(p), 3);
      self->_current += 3;
      p += 2;
    } else if ((c & 0xf8) == 0xf0) {
      // four-byte sequence
      if (p + 3 >= e) {
        return TRI_ERROR_INTERNAL;
      }

      memcpy(self->_current, reinterpret_cast<char const*>(p), 4);
      self->_current += 4;
      p += 3;
    }

    ++p;
  }

  AppendChar(self, '"');
  return TRI_ERROR_NO_ERROR;
}

/// @brief appends characters but json-encode the string
ErrorCode TRI_AppendJsonEncodedStringStringBuffer(TRI_string_buffer_t* self,
                                                  char const* src, size_t length,
                                                  bool escapeSlash) {
  return AppendJsonEncoded(self, src, length, escapeSlash);
}

/// @brief appends integer with 8 bits
ErrorCode TRI_AppendInt8StringBuffer(TRI_string_buffer_t* self, int8_t attr) {
  auto res = Reserve(self, 4);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringInt8InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends unsigned integer with 8 bits
ErrorCode TRI_AppendUInt8StringBuffer(TRI_string_buffer_t* self, uint8_t attr) {
  auto res = Reserve(self, 3);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt8InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends integer with 16 bits
ErrorCode TRI_AppendInt16StringBuffer(TRI_string_buffer_t* self, int16_t attr) {
  auto res = Reserve(self, 6);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringInt16InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends unsigned integer with 32 bits
ErrorCode TRI_AppendUInt16StringBuffer(TRI_string_buffer_t* self, uint16_t attr) {
  auto res = Reserve(self, 5);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt16InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends integer with 32 bits
ErrorCode TRI_AppendInt32StringBuffer(TRI_string_buffer_t* self, int32_t attr) {
  auto res = Reserve(self, 11);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringInt32InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends unsigned integer with 32 bits
ErrorCode TRI_AppendUInt32StringBuffer(TRI_string_buffer_t* self, uint32_t attr) {
  auto res = Reserve(self, 10);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt32InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends integer with 64 bits
ErrorCode TRI_AppendInt64StringBuffer(TRI_string_buffer_t* self, int64_t attr) {
  auto res = Reserve(self, 20);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringInt64InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends unsigned integer with 64 bits
ErrorCode TRI_AppendUInt64StringBuffer(TRI_string_buffer_t* self, uint64_t attr) {
  auto res = Reserve(self, 21);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt64InPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends unsigned integer with 32 bits in hex
ErrorCode TRI_AppendUInt32HexStringBuffer(TRI_string_buffer_t* self, uint32_t attr) {
  auto res = Reserve(self, 5);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt32HexInPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends unsigned integer with 64 bits in hex
ErrorCode TRI_AppendUInt64HexStringBuffer(TRI_string_buffer_t* self, uint64_t attr) {
  auto res = Reserve(self, 9);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  size_t len = TRI_StringUInt64HexInPlace(attr, self->_current);
  self->_current += len;

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends floating point number
ErrorCode TRI_AppendDoubleStringBuffer(TRI_string_buffer_t* self, double attr) {
  if (std::isnan(attr)) {
    return TRI_AppendStringStringBuffer(self, "NaN");
  }

  if (attr == HUGE_VAL) {
    return TRI_AppendStringStringBuffer(self, "inf");
  }
  if (attr == -HUGE_VAL) {
    return TRI_AppendStringStringBuffer(self, "-inf");
  }

  auto res = Reserve(self, 24);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  int length = fpconv_dtoa(attr, self->_current);
  self->_current += static_cast<size_t>(length);

  return TRI_ERROR_NO_ERROR;
}

/// @brief appends csv 32-bit integer
ErrorCode TRI_AppendCsvInt32StringBuffer(TRI_string_buffer_t* self, int32_t i) {
  auto res = TRI_AppendInt32StringBuffer(self, i);

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

/// @brief appends csv unisgned 32-bit integer
ErrorCode TRI_AppendCsvUInt32StringBuffer(TRI_string_buffer_t* self, uint32_t i) {
  auto res = TRI_AppendUInt32StringBuffer(self, i);

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

/// @brief appends csv 64-bit integer
ErrorCode TRI_AppendCsvInt64StringBuffer(TRI_string_buffer_t* self, int64_t i) {
  auto res = TRI_AppendInt64StringBuffer(self, i);

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

/// @brief appends csv unsigned 64-bit integer
ErrorCode TRI_AppendCsvUInt64StringBuffer(TRI_string_buffer_t* self, uint64_t i) {
  auto res = TRI_AppendUInt64StringBuffer(self, i);

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

/// @brief appends csv double
ErrorCode TRI_AppendCsvDoubleStringBuffer(TRI_string_buffer_t* self, double d) {
  auto res = TRI_AppendDoubleStringBuffer(self, d);

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

std::ostream& operator<<(std::ostream& stream, arangodb::basics::StringBuffer const& buffer) {
  stream.write(buffer.begin(), buffer.length());
  return stream;
}

/// @brief uncompress the buffer into stringstream out, using zlib-inflate
ErrorCode arangodb::basics::StringBuffer::inflate(std::stringstream& out,
                                                  size_t bufferSize, size_t skip) {
  z_stream strm;

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;

  int res = inflateInit(&strm);

  if (res != Z_OK) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  size_t len = this->length();

  if (len < skip) {
    len = 0;
  } else {
    len -= skip;
  }

  strm.avail_in = (int)len;
  strm.next_in = ((unsigned char*)this->c_str()) + skip;

  auto guard = scopeGuard([&strm] { (void)inflateEnd(&strm); });

  auto buffer = std::make_unique<char[]>(bufferSize);

  do {
    if (strm.avail_in == 0) {
      break;
    }

    do {
      strm.avail_out = (uInt)bufferSize;
      strm.next_out = (unsigned char*)buffer.get();

      res = ::inflate(&strm, Z_NO_FLUSH);
      TRI_ASSERT(res != Z_STREAM_ERROR);

      switch (res) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR: {
          return TRI_ERROR_INTERNAL;
        }
        default:;
      }

      out.write(buffer.get(), bufferSize - strm.avail_out);
    } while (strm.avail_out == 0);
  } while (res != Z_STREAM_END);

  if (res != Z_STREAM_END) {
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_INTERNAL;
}

/// @brief uncompress the buffer into StringBuffer out, using zlib-inflate
ErrorCode arangodb::basics::StringBuffer::inflate(arangodb::basics::StringBuffer& out,
                                                  size_t bufferSize, size_t skip) {
  z_stream strm;

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;

  size_t len = this->length();
  bool raw = true;

  if (len < skip) {
    len = 0;
  } else {
    len -= skip;
  }

  unsigned char* start = ((unsigned char*)this->c_str()) + skip;

  // nginx seems to skip the header - which is wrong according to the
  // RFC. The following is a hack to find out, if a header is present.
  // There is a 1 in 31 chance that this will not work.
  if (2 <= len) {
    uint32_t first = (((uint32_t)start[0]) << 8) | ((uint32_t)start[1]);

    if (first % 31 == 0) {
      raw = false;
    }
  }

  int res = raw ? inflateInit2(&strm, -15) : inflateInit(&strm);

  if (res != Z_OK) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  strm.avail_in = (int)len;
  strm.next_in = start;

  auto guard = scopeGuard([&strm] { (void)inflateEnd(&strm); });

  auto buffer = std::make_unique<char[]>(bufferSize);

  do {
    if (strm.avail_in == 0) {
      break;
    }

    do {
      strm.avail_out = (uInt)bufferSize;
      strm.next_out = (unsigned char*)buffer.get();

      res = ::inflate(&strm, Z_NO_FLUSH);
      TRI_ASSERT(res != Z_STREAM_ERROR);

      switch (res) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR: {
          return TRI_ERROR_INTERNAL;
        }
        default:;
      }

      out.appendText(buffer.get(), bufferSize - strm.avail_out);
    } while (strm.avail_out == 0);
  } while (res != Z_STREAM_END);

  if (res != Z_STREAM_END) {
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_INTERNAL;
}
