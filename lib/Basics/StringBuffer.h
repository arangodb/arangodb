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

#ifndef ARANGODB_BASICS_STRING_BUFFER_H
#define ARANGODB_BASICS_STRING_BUFFER_H 1

#include <stddef.h>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <sstream>

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/memory.h"
#include "Basics/system-compiler.h"

/// @brief string buffer with formatting routines
struct TRI_string_buffer_t {
  char* _buffer;
  char* _current;
  size_t _len;
  bool _initializeMemory;
};

/// @brief create a new string buffer and initialize it
TRI_string_buffer_t* TRI_CreateStringBuffer();

/// @brief create a new string buffer and initialize it with a specific size
TRI_string_buffer_t* TRI_CreateSizedStringBuffer(size_t);

/// @brief initializes the string buffer
///
/// @warning You must call initialize before using the string buffer.
void TRI_InitStringBuffer(TRI_string_buffer_t*, bool initializeMemory = true);

/// @brief initializes the string buffer with a specific size
///
/// @warning You must call initialize before using the string buffer.
void TRI_InitSizedStringBuffer(TRI_string_buffer_t*, size_t const,
                               bool initializeMemory = true);

/// @brief frees the string buffer
///
/// @warning You must call free or destroy after using the string buffer.
void TRI_DestroyStringBuffer(TRI_string_buffer_t*);

/// @brief frees the string buffer and cleans the buffer
///
/// @warning You must call free after or destroy using the string buffer.
void TRI_AnnihilateStringBuffer(TRI_string_buffer_t*);

/// @brief frees the string buffer and the pointer
void TRI_FreeStringBuffer(TRI_string_buffer_t*);

/// @brief compress the string buffer using deflate
ErrorCode TRI_DeflateStringBuffer(TRI_string_buffer_t*, size_t);

/// @brief ensure the string buffer has a specific capacity
ErrorCode TRI_ReserveStringBuffer(TRI_string_buffer_t*, size_t const);

/// @brief swaps content with another string buffer
void TRI_SwapStringBuffer(TRI_string_buffer_t*, TRI_string_buffer_t*);

/// @brief returns pointer to the beginning of the character buffer
char const* TRI_BeginStringBuffer(TRI_string_buffer_t const*);

/// @brief returns pointer to the end of the character buffer
char const* TRI_EndStringBuffer(TRI_string_buffer_t const*);

/// @brief returns length of the character buffer
size_t TRI_LengthStringBuffer(TRI_string_buffer_t const*);

/// @brief increases length of the character buffer
void TRI_IncreaseLengthStringBuffer(TRI_string_buffer_t*, size_t);

/// @brief returns capacity of the character buffer
size_t TRI_CapacityStringBuffer(TRI_string_buffer_t const*);

/// @brief returns true if buffer is empty
bool TRI_EmptyStringBuffer(TRI_string_buffer_t const*);

/// @brief clears the buffer
void TRI_ClearStringBuffer(TRI_string_buffer_t*);

/// @brief resets the buffer (without clearing)
void TRI_ResetStringBuffer(TRI_string_buffer_t*);

/// @brief steals the buffer of a string buffer
char* TRI_StealStringBuffer(TRI_string_buffer_t*);

/// @brief copies the string buffer
ErrorCode TRI_CopyStringBuffer(TRI_string_buffer_t* self, TRI_string_buffer_t const* source);

/// @brief removes the first characters
void TRI_EraseFrontStringBuffer(TRI_string_buffer_t*, size_t);

/// @brief replaces characters
ErrorCode TRI_ReplaceStringStringBuffer(TRI_string_buffer_t* self, char const* str, size_t len);

/// @brief appends character
ErrorCode TRI_AppendCharStringBuffer(TRI_string_buffer_t* self, char chr);

/// @brief appends characters
ErrorCode TRI_AppendStringStringBuffer(TRI_string_buffer_t* self, char const* str);

/// @brief appends characters
ErrorCode TRI_AppendString2StringBuffer(TRI_string_buffer_t* self,
                                        char const* str, size_t len);

/// @brief appends characters but does not check buffer bounds
static inline void TRI_AppendCharUnsafeStringBuffer(TRI_string_buffer_t* self, char chr) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(self->_len - static_cast<size_t>(self->_current - self->_buffer) > 0);
#endif
  *self->_current++ = chr;
}

static inline void TRI_AppendStringUnsafeStringBuffer(TRI_string_buffer_t* self,
                                                      char const* str) {
  size_t len = strlen(str);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(self->_len - static_cast<size_t>(self->_current - self->_buffer) >= len);
#endif
  memcpy(self->_current, str, len);
  self->_current += len;
}

static inline void TRI_AppendStringUnsafeStringBuffer(TRI_string_buffer_t* self,
                                                      char const* str, size_t len) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(self->_len - static_cast<size_t>(self->_current - self->_buffer) >= len);
#endif
  memcpy(self->_current, str, len);
  self->_current += len;
}

static inline void TRI_AppendStringUnsafeStringBuffer(TRI_string_buffer_t* self,
                                                      std::string const& str) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(self->_len - static_cast<size_t>(self->_current - self->_buffer) >= str.size());
#endif
  memcpy(self->_current, str.c_str(), str.size());
  self->_current += str.size();
}

/// @brief appends characters but json-encode the string
ErrorCode TRI_AppendJsonEncodedStringStringBuffer(TRI_string_buffer_t* self,
                                                  char const* src, size_t length,
                                                  bool escapeSlash);

/// @brief appends integer with 8 bits
ErrorCode TRI_AppendInt8StringBuffer(TRI_string_buffer_t* self, int8_t attr);

/// @brief appends unsigned integer with 8 bits
ErrorCode TRI_AppendUInt8StringBuffer(TRI_string_buffer_t* self, uint8_t attr);

/// @brief appends integer with 16 bits
ErrorCode TRI_AppendInt16StringBuffer(TRI_string_buffer_t* self, int16_t attr);

/// @brief appends unsigned integer with 32 bits
ErrorCode TRI_AppendUInt16StringBuffer(TRI_string_buffer_t* self, uint16_t attr);

/// @brief appends integer with 32 bits
ErrorCode TRI_AppendInt32StringBuffer(TRI_string_buffer_t* self, int32_t attr);

/// @brief appends unsigned integer with 32 bits
ErrorCode TRI_AppendUInt32StringBuffer(TRI_string_buffer_t* self, uint32_t attr);

/// @brief appends integer with 64 bits
ErrorCode TRI_AppendInt64StringBuffer(TRI_string_buffer_t* self, int64_t attr);

/// @brief appends unsigned integer with 64 bits
ErrorCode TRI_AppendUInt64StringBuffer(TRI_string_buffer_t* self, uint64_t attr);

/// @brief appends unsigned integer with 32 bits in hex
ErrorCode TRI_AppendUInt32HexStringBuffer(TRI_string_buffer_t* self, uint32_t attr);

/// @brief appends unsigned integer with 64 bits in hex
ErrorCode TRI_AppendUInt64HexStringBuffer(TRI_string_buffer_t* self, uint64_t attr);

/// @brief appends floating point number with 8 bits
ErrorCode TRI_AppendDoubleStringBuffer(TRI_string_buffer_t* self, double attr);

/// @brief appends csv 32-bit integer
ErrorCode TRI_AppendCsvInt32StringBuffer(TRI_string_buffer_t* self, int32_t i);

/// @brief appends csv unisgned 32-bit integer
ErrorCode TRI_AppendCsvUInt32StringBuffer(TRI_string_buffer_t* self, uint32_t i);

/// @brief appends csv 64-bit integer
ErrorCode TRI_AppendCsvInt64StringBuffer(TRI_string_buffer_t* self, int64_t i);

/// @brief appends csv unsigned 64-bit integer
ErrorCode TRI_AppendCsvUInt64StringBuffer(TRI_string_buffer_t* self, uint64_t i);

/// @brief appends csv double
ErrorCode TRI_AppendCsvDoubleStringBuffer(TRI_string_buffer_t* self, double d);

// -----------------------------------------------------------------------------
// string buffer with formatting routines
// -----------------------------------------------------------------------------

namespace arangodb {
namespace basics {

/// @brief string buffer with formatting routines
class StringBuffer {
  StringBuffer(StringBuffer const&) = delete;
  StringBuffer& operator=(StringBuffer const&) = delete;

 public:
  /// @brief creates an uninitialized string buffer
  StringBuffer() {
    _buffer._buffer = nullptr;
    _buffer._current = nullptr;
    _buffer._len = 0;
    _buffer._initializeMemory = false;
  }

  /// @brief initializes the string buffer
  explicit StringBuffer(bool initializeMemory) {
    TRI_InitStringBuffer(&_buffer, initializeMemory);

    if (_buffer._buffer == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  /// @brief initializes the string buffer
  explicit StringBuffer(size_t initialSize, bool initializeMemory = true) {
    TRI_InitSizedStringBuffer(&_buffer, initialSize, initializeMemory);

    if (_buffer._buffer == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  /// @brief frees the string buffer
  ~StringBuffer() { TRI_DestroyStringBuffer(&_buffer); }

  std::string toString() const { return std::string(c_str(), length()); }

  /// @brief frees the string buffer and cleans the buffer
  void annihilate() { TRI_AnnihilateStringBuffer(&_buffer); }

  /// @brief ensure the string buffer has a specific capacity
  ErrorCode reserve(size_t length) {
    return TRI_ReserveStringBuffer(&_buffer, length);
  }

  /// @brief compress the buffer using deflate
  ErrorCode deflate(size_t bufferSize) {
    return TRI_DeflateStringBuffer(&_buffer, bufferSize);
  }

  /// @brief uncompress the buffer into stringstream out, using zlib-inflate
  ErrorCode inflate(std::stringstream& out, size_t bufferSize = 16384, size_t skip = 0);

  /// @brief uncompress the buffer into StringBuffer out, using zlib-inflate
  ErrorCode inflate(arangodb::basics::StringBuffer& out,
                    size_t bufferSize = 16384, size_t skip = 0);

  /// @brief returns the low level buffer
  TRI_string_buffer_t* stringBuffer() { return &_buffer; }

  /// @brief swaps content with another string buffer
  StringBuffer& swap(StringBuffer* other) {
    TRI_SwapStringBuffer(&_buffer, &other->_buffer);
    return *this;
  }

  char const* data() const { return TRI_BeginStringBuffer(&_buffer); }

  /// @brief returns pointer to the character buffer
  char const* c_str() const { return TRI_BeginStringBuffer(&_buffer); }

  /// @brief returns pointer to the beginning of the character buffer
  char const* begin() const { return TRI_BeginStringBuffer(&_buffer); }

  /// @brief returns pointer to the beginning of the character buffer
  char* begin() { return const_cast<char*>(TRI_BeginStringBuffer(&_buffer)); }

  /// @brief returns pointer to the end of the character buffer
  char const* end() const { return TRI_EndStringBuffer(&_buffer); }

  /// @brief returns pointer to the end of the character buffer
  char* end() { return const_cast<char*>(TRI_EndStringBuffer(&_buffer)); }

  /// @brief returns length of the character buffer
  size_t length() const { return TRI_LengthStringBuffer(&_buffer); }
  size_t size() const { return TRI_LengthStringBuffer(&_buffer); }

  /// @brief returns capacity of the character buffer
  size_t capacity() const { return TRI_CapacityStringBuffer(&_buffer); }

  /// @brief increases length of the character buffer
  void increaseLength(size_t n) { TRI_IncreaseLengthStringBuffer(&_buffer, n); }

  /// @brief returns true if buffer is empty
  bool empty() const { return TRI_EmptyStringBuffer(&_buffer); }

  /// @brief steals the pointer from the string buffer
  char* steal() { return TRI_StealStringBuffer(&_buffer); }

  /// @brief resets the buffer
  StringBuffer& reset() {
    TRI_ResetStringBuffer(&_buffer);
    return *this;
  }

  /// @brief clears the buffer
  StringBuffer& clear() {
    TRI_ClearStringBuffer(&_buffer);
    return *this;
  }

  /// @brief assigns text from a string
  StringBuffer& operator=(std::string const& str) {
    TRI_ReplaceStringStringBuffer(&_buffer, str.c_str(), str.length());
    return *this;
  }

  /// @brief copies the string buffer
  StringBuffer& copy(StringBuffer const& source) {
    TRI_CopyStringBuffer(&_buffer, &source._buffer);
    return *this;
  }

  /// @brief removes the first characters and clears the remaining buffer space
  StringBuffer& erase_front(size_t len) {
    TRI_EraseFrontStringBuffer(&_buffer, len);
    return *this;
  }

  /// @brief replaces characters
  StringBuffer& replaceText(char const* str, size_t len) {
    TRI_ReplaceStringStringBuffer(&_buffer, str, len);
    return *this;
  }

  /// @brief replaces characters
  StringBuffer& replaceText(StringBuffer const& text) {
    TRI_ReplaceStringStringBuffer(&_buffer, text.c_str(), text.length());
    return *this;
  }

  /// @brief set the buffer content
  void set(TRI_string_buffer_t const* other) {
    if (_buffer._buffer != nullptr) {
      TRI_Free(_buffer._buffer);
    }

    _buffer._buffer = other->_buffer;
    _buffer._current = other->_current;
    _buffer._len = other->_len;
  }

  /// @brief make sure the buffer is null-terminated
  void ensureNullTerminated() {
    TRI_AppendCharStringBuffer(&_buffer, '\0');
    --_buffer._current;
  }

  /// @brief appends character
  StringBuffer& appendChar(char chr) {
    TRI_AppendCharStringBuffer(&_buffer, chr);
    return *this;
  }

  void appendCharUnsafe(char chr) {
    TRI_AppendCharUnsafeStringBuffer(&_buffer, chr);
  }

  /// @brief appends as json-encoded
  StringBuffer& appendJsonEncoded(char const* str, size_t length) {
    TRI_AppendJsonEncodedStringStringBuffer(&_buffer, str, length, true);
    return *this;
  }

  /// @brief appends characters
  StringBuffer& appendText(char const* str, size_t len) {
    TRI_AppendString2StringBuffer(&_buffer, str, len);
    return *this;
  }

  void appendTextUnsafe(char const* str, size_t len) {
    TRI_AppendStringUnsafeStringBuffer(&_buffer, str, len);
  }

  /// @brief appends characters
  StringBuffer& appendText(char const* str) {
    TRI_AppendString2StringBuffer(&_buffer, str, strlen(str));
    return *this;
  }

  /// @brief appends string
  StringBuffer& appendText(std::string const& str) {
    TRI_AppendString2StringBuffer(&_buffer, str.c_str(), str.length());
    return *this;
  }

  void appendTextUnsafe(std::string const& str) {
    TRI_AppendStringUnsafeStringBuffer(&_buffer, str.c_str(), str.length());
  }

  /// @brief appends a string buffer
  StringBuffer& appendText(StringBuffer const& text) {
    TRI_AppendString2StringBuffer(&_buffer, text.c_str(), text.length());
    return *this;
  }

  /// @brief appends integer with 8 bits
  ///
  /// This method is implemented here in order to allow inlining.
  StringBuffer& appendInteger(int8_t attr) {
    TRI_AppendInt8StringBuffer(&_buffer, attr);
    return *this;
  }

  /// @brief appends unsigned integer with 8 bits
  ///
  /// This method is implemented here in order to allow inlining.
  StringBuffer& appendInteger(uint8_t attr) {
    TRI_AppendUInt8StringBuffer(&_buffer, attr);
    return *this;
  }

  /// @brief appends integer with 16 bits
  ///
  /// This method is implemented here in order to allow inlining.
  StringBuffer& appendInteger(int16_t attr) {
    TRI_AppendInt16StringBuffer(&_buffer, attr);
    return *this;
  }

  /// @brief appends unsigned integer with 32 bits
  ///
  /// This method is implemented here in order to allow inlining.
  StringBuffer& appendInteger(uint16_t attr) {
    TRI_AppendUInt16StringBuffer(&_buffer, attr);
    return *this;
  }

  /// @brief appends integer with 32 bits
  ///
  /// This method is implemented here in order to allow inlining.
  StringBuffer& appendInteger(int32_t attr) {
    TRI_AppendInt32StringBuffer(&_buffer, attr);
    return *this;
  }

  /// @brief appends unsigned integer with 32 bits
  ///
  /// This method is implemented here in order to allow inlining.
  StringBuffer& appendInteger(uint32_t attr) {
    TRI_AppendUInt32StringBuffer(&_buffer, attr);
    return *this;
  }

  /// @brief appends integer with 64 bits
  ///
  /// This method is implemented here in order to allow inlining.
  StringBuffer& appendInteger(int64_t attr) {
    TRI_AppendInt64StringBuffer(&_buffer, attr);
    return *this;
  }

  /// @brief appends unsigned integer with 64 bits
  ///
  /// This method is implemented here in order to allow inlining.
  StringBuffer& appendInteger(uint64_t attr) {
    TRI_AppendUInt64StringBuffer(&_buffer, attr);
    return *this;
  }

/// @brief appends size_t
///
/// This method is implemented here in order to allow inlining.
#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

  StringBuffer& appendInteger(size_t attr) {
    return appendInteger(sizetint_t(attr));
  }

#endif

  /// @brief appends unsigned integer with 32 bits in hex
  StringBuffer& appendHex(uint32_t attr) {
    TRI_AppendUInt32HexStringBuffer(&_buffer, attr);
    return *this;
  }

  /// @brief appends unsigned integer with 64 bits in hex
  StringBuffer& appendHex(uint64_t attr) {
    TRI_AppendUInt64HexStringBuffer(&_buffer, attr);
    return *this;
  }

/// @brief appends size_t in hex
#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

  StringBuffer& appendHex(size_t attr) { return appendHex(sizetint_t(attr)); }

#endif

  /// @brief appends floating point number with 8 bits
  StringBuffer& appendDecimal(double attr) {
    TRI_AppendDoubleStringBuffer(&_buffer, attr);
    return *this;
  }

  /// @brief appends csv string
  StringBuffer& appendCsvString(std::string const& text) {
    // do not escape here, because some string - i.e. lists of identifier - have
    // no special characters
    TRI_AppendString2StringBuffer(&_buffer, text.c_str(), text.size());
    TRI_AppendCharStringBuffer(&_buffer, ';');

    return *this;
  }

  /// @brief appends csv 32-bit integer
  StringBuffer& appendCsvInteger(int32_t i) {
    TRI_AppendCsvInt32StringBuffer(&_buffer, i);
    return *this;
  }

  /// @brief appends csv unisgned 32-bit integer
  StringBuffer& appendCsvInteger(uint32_t i) {
    TRI_AppendCsvUInt32StringBuffer(&_buffer, i);
    return *this;
  }

  /// @brief appends csv 64-bit integer
  StringBuffer& appendCsvInteger(int64_t i) {
    TRI_AppendCsvInt64StringBuffer(&_buffer, i);
    return *this;
  }

  /// @brief appends csv unsigned 64-bit integer
  StringBuffer& appendCsvInteger(uint64_t i) {
    TRI_AppendCsvUInt64StringBuffer(&_buffer, i);
    return *this;
  }

  /// @brief appends csv double
  StringBuffer& appendCsvDouble(double d) {
    TRI_AppendCsvDoubleStringBuffer(&_buffer, d);
    return *this;
  }

  /// @brief underlying C string buffer
  TRI_string_buffer_t _buffer;
};
}  // namespace basics
}  // namespace arangodb

std::ostream& operator<<(std::ostream&, arangodb::basics::StringBuffer const&);

#endif
