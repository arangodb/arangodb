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

#include <cstring>

#include <openssl/sha.h>

#include "tri-strings.h"

#include "Basics/Utf8Helper.h"
#include "Basics/conversions.h"
#include "Basics/debugging.h"
#include "Basics/memory.h"
#include "Basics/operating-system.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief decodes a unicode escape sequence
////////////////////////////////////////////////////////////////////////////////

static void DecodeUnicodeEscape(char** dst, char const* src) {
  int i1;
  int i2;
  int i3;
  int i4;
  uint16_t n;

  i1 = TRI_IntHex(src[0], 0);
  i2 = TRI_IntHex(src[1], 0);
  i3 = TRI_IntHex(src[2], 0);
  i4 = TRI_IntHex(src[3], 0);

  n = ((i1 & 0xF) << 12) | ((i2 & 0xF) << 8) | ((i3 & 0xF) << 4) | (i4 & 0xF);

  if (n <= 0x7F) {
    *(*dst) = n & 0x7F;
  } else if (n <= 0x7FF) {
    *(*dst)++ = 0xC0 + (n >> 6);
    *(*dst) = 0x80 + (n & 0x3F);

  } else {
    *(*dst)++ = 0xE0 + (n >> 12);
    *(*dst)++ = 0x80 + ((n >> 6) & 0x3F);
    *(*dst) = 0x80 + (n & 0x3F);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decodes a unicode surrogate pair
////////////////////////////////////////////////////////////////////////////////

static void DecodeSurrogatePair(char** dst, char const* src1, char const* src2) {
  int i1;
  int i2;
  int i3;
  int i4;
  uint32_t n1;
  uint32_t n2;
  uint32_t n;

  i1 = TRI_IntHex(src1[0], 0);
  i2 = TRI_IntHex(src1[1], 0);
  i3 = TRI_IntHex(src1[2], 0);
  i4 = TRI_IntHex(src1[3], 0);

  n1 = ((i1 & 0xF) << 12) | ((i2 & 0xF) << 8) | ((i3 & 0xF) << 4) | (i4 & 0xF);
  n1 -= 0xD800;

  i1 = TRI_IntHex(src2[0], 0);
  i2 = TRI_IntHex(src2[1], 0);
  i3 = TRI_IntHex(src2[2], 0);
  i4 = TRI_IntHex(src2[3], 0);

  n2 = ((i1 & 0xF) << 12) | ((i2 & 0xF) << 8) | ((i3 & 0xF) << 4) | (i4 & 0xF);
  n2 -= 0xDC00;

  n = 0x10000 + ((n1 << 10) | n2);

  if (n <= 0x7F) {
    *(*dst) = n & 0x7F;
  } else if (n <= 0x7FF) {
    *(*dst)++ = 0xC0 + (n >> 6);
    *(*dst) = 0x80 + (n & 0x3F);
  } else if (n <= 0xFFFF) {
    *(*dst)++ = 0xE0 + (n >> 12);
    *(*dst)++ = 0x80 + ((n >> 6) & 0x3F);
    *(*dst) = 0x80 + (n & 0x3F);
  } else {
    *(*dst)++ = 0xF0 + (n >> 18);
    *(*dst)++ = 0x80 + ((n >> 12) & 0x3F);
    *(*dst)++ = 0x80 + ((n >> 6) & 0x3F);
    *(*dst) = 0x80 + (n & 0x3F);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to lower case
////////////////////////////////////////////////////////////////////////////////

char* TRI_LowerAsciiString(char const* value) {
  size_t length;
  char* buffer;
  char* p;
  char* out;
  char c;

  if (value == nullptr) {
    return nullptr;
  }

  length = strlen(value);

  buffer = static_cast<char*>(TRI_Allocate((sizeof(char) * length) + 1));

  if (buffer == nullptr) {
    return nullptr;
  }

  p = (char*)value;
  out = buffer;

  while ((c = *p++)) {
    if (c >= 'A' && c <= 'Z') {
      *out++ = c + 32;
    } else {
      *out++ = c;
    }
  }

  *out = '\0';

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to upper case
////////////////////////////////////////////////////////////////////////////////

char* TRI_UpperAsciiString(char const* value) {
  size_t length;
  char* buffer;
  char* p;
  char* out;
  char c;

  if (value == nullptr) {
    return nullptr;
  }

  length = strlen(value);

  buffer = static_cast<char*>(TRI_Allocate((sizeof(char) * length) + 1));

  if (buffer == nullptr) {
    return nullptr;
  }

  p = (char*)value;
  out = buffer;

  while ((c = *p++)) {
    if (c >= 'a' && c <= 'z') {
      *out++ = c - 32;
    } else {
      *out++ = c;
    }
  }

  *out = '\0';

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if strings are equal
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualString(char const* left, char const* right) {
  return strcmp(left, right) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if strings are equal
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualString(char const* left, char const* right, size_t n) {
  return strncmp(left, right, n) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if ASCII strings are equal ignoring case
////////////////////////////////////////////////////////////////////////////////

bool TRI_CaseEqualString(char const* left, char const* right) {
  return strcasecmp(left, right) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if ASCII strings are equal ignoring case
////////////////////////////////////////////////////////////////////////////////

bool TRI_CaseEqualString(char const* left, char const* right, size_t n) {
  return strncasecmp(left, right, n) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if second string is prefix of the first
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsPrefixString(char const* full, char const* prefix) {
  return strncmp(full, prefix, strlen(prefix)) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if second string is contained in the first, byte-safe
////////////////////////////////////////////////////////////////////////////////

char* TRI_IsContainedMemory(char const* full, size_t fullLength,
                            char const* part, size_t partLength) {
  if (fullLength == 0 || partLength == 0 || fullLength < partLength) {
    return nullptr;
  }

  if (partLength == 1) {
    return static_cast<char*>(const_cast<void*>(
        memchr(static_cast<void const*>(full), (int)*part, fullLength)));
  }

  char const* end = full + fullLength - partLength;

  for (char const* p = full; p <= end; ++p) {
    if (*p == *part && memcmp(static_cast<void const*>(p),
                              static_cast<void const*>(part), partLength) == 0) {
      return const_cast<char*>(p);
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief duplicates a string of given length
////////////////////////////////////////////////////////////////////////////////

char* TRI_DuplicateString(char const* value, size_t length) {
  char* result = static_cast<char*>(TRI_Allocate(length + 1));

  if (result != nullptr) {
    memcpy(result, value, length);
    result[length] = '\0';
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a string
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyString(char* dst, char const* src, size_t length) {
  *dst = '\0';
  strncat(dst, src, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a string
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeString(char* value) { TRI_Free(value); }

////////////////////////////////////////////////////////////////////////////////
/// @brief sha256 of a string
////////////////////////////////////////////////////////////////////////////////

char* TRI_SHA256String(char const* source, size_t sourceLen, size_t* dstLen) {
  unsigned char* dst = static_cast<unsigned char*>(TRI_Allocate(SHA256_DIGEST_LENGTH));
  if (dst == nullptr) {
    return nullptr;
  }
  *dstLen = SHA256_DIGEST_LENGTH;

  SHA256((unsigned char const*)source, sourceLen, dst);

  return (char*)dst;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes special characters using C escapes
/// the target buffer must have been allocated already and big enough to hold
/// the result of at most (4 * inLength) + 2 bytes!
////////////////////////////////////////////////////////////////////////////////

char* TRI_EscapeControlsCString(char const* in, size_t inLength, char* out,
                                size_t* outLength, bool appendNewline) {
  if (out == nullptr) {
    return nullptr;
  }

  char* qtr = out;
  char const* ptr;
  char const* end;

  for (ptr = in, end = ptr + inLength; ptr < end; ptr++, qtr++) {
    uint8_t n;

    switch (*ptr) {
      case '\n':
        *qtr++ = '\\';
        *qtr = 'n';
        break;

      case '\r':
        *qtr++ = '\\';
        *qtr = 'r';
        break;

      case '\t':
        *qtr++ = '\\';
        *qtr = 't';
        break;

      default:
        n = (uint8_t)(*ptr);

        if (n < 32) {
          uint8_t n1 = n >> 4;
          uint8_t n2 = n & 0x0F;

          *qtr++ = '\\';
          *qtr++ = 'x';
          *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
          *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
        } else {
          *qtr = *ptr;
        }

        break;
    }
  }

  if (appendNewline) {
    *qtr++ = '\n';
  }

  *qtr = '\0';
  *outLength = static_cast<size_t>(qtr - out);
  return out;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unescapes unicode escape sequences
////////////////////////////////////////////////////////////////////////////////

char* TRI_UnescapeUtf8String(char const* in, size_t inLength, size_t* outLength, bool normalize) {
  char* buffer = static_cast<char*>(TRI_Allocate(inLength + 1));

  if (buffer == nullptr) {
    return nullptr;
  }

  *outLength = TRI_UnescapeUtf8StringInPlace(buffer, in, inLength);
  buffer[*outLength] = '\0';

  if (normalize && *outLength > 0) {
    size_t tmpLength = 0;
    char* utf8_nfc = TRI_normalize_utf8_to_NFC(buffer, *outLength, &tmpLength);

    if (utf8_nfc != nullptr) {
      *outLength = tmpLength;
      TRI_Free(buffer);
      buffer = utf8_nfc;
    }
    // intentionally falls through
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unescapes unicode escape sequences into buffer "buffer".
/// the buffer must be big enough to hold at least inLength bytes of chars
/// returns the length of the unescaped string
////////////////////////////////////////////////////////////////////////////////

size_t TRI_UnescapeUtf8StringInPlace(char* buffer, char const* in, size_t inLength) {
  char* qtr = buffer;
  char const* ptr;
  char const* end;

  for (ptr = in, end = ptr + inLength; ptr < end; ++ptr, ++qtr) {
    if (*ptr == '\\' && ptr + 1 < end) {
      ++ptr;

      switch (*ptr) {
        case 'b':
          *qtr = '\b';
          break;

        case 'f':
          *qtr = '\f';
          break;

        case 'n':
          *qtr = '\n';
          break;

        case 'r':
          *qtr = '\r';
          break;

        case 't':
          *qtr = '\t';
          break;

        case 'u':
          // expecting at least 6 characters: \uXXXX
          if (ptr + 4 < end) {
            // check, if we have a surrogate pair
            if (ptr + 10 < end) {
              bool sp;
              char c1 = ptr[1];

              sp = (c1 == 'd' || c1 == 'D');

              if (sp) {
                char c2 = ptr[2];
                sp &= (c2 == '8' || c2 == '9' || c2 == 'A' || c2 == 'a' ||
                       c2 == 'B' || c2 == 'b');
              }

              if (sp) {
                char c3 = ptr[7];

                sp &= (ptr[5] == '\\' && ptr[6] == 'u');
                sp &= (c3 == 'd' || c3 == 'D');
              }

              if (sp) {
                char c4 = ptr[8];
                sp &= (c4 == 'C' || c4 == 'c' || c4 == 'D' || c4 == 'd' ||
                       c4 == 'E' || c4 == 'e' || c4 == 'F' || c4 == 'f');
              }

              if (sp) {
                DecodeSurrogatePair(&qtr, ptr + 1, ptr + 7);
                ptr += 10;
              } else {
                DecodeUnicodeEscape(&qtr, ptr + 1);
                ptr += 4;
              }
            } else {
              DecodeUnicodeEscape(&qtr, ptr + 1);
              ptr += 4;
            }
          }
          // ignore wrong format
          else {
            *qtr = *ptr;
          }
          break;

        default:
          // this includes cases \/, \\, and \"
          *qtr = *ptr;
          break;
      }

      continue;
    }

    *qtr = *ptr;
  }

  return static_cast<size_t>(qtr - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the number of characters in a UTF-8 string
////////////////////////////////////////////////////////////////////////////////

size_t TRI_CharLengthUtf8String(char const* in, size_t length) {
  unsigned char const* p = reinterpret_cast<unsigned char const*>(in);
  unsigned char const* e = p + length;
  size_t chars = 0;

  while (p < e) {
    unsigned char c = *p;

    if (c < 128) {
      // single byte
      p++;
    } else if (c < 224) {
      p += 2;
    } else if (c < 240) {
      p += 3;
    } else if (c < 248) {
      p += 4;
    } else {
      // invalid UTF-8 sequence
      break;
    }

    ++chars;
  }

  return chars;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string end position for a leftmost prefix of a UTF-8 string
/// eg. when specifying (müller, 2), the return value will be a pointer to the
/// first "l".
/// the UTF-8 string must be well-formed and end with a NUL terminator
////////////////////////////////////////////////////////////////////////////////

char* TRI_PrefixUtf8String(char const* in, const uint32_t maximumLength) {
  uint32_t length;
  unsigned char* p;

  p = (unsigned char*)in;
  length = 0;

  while (*p && length < maximumLength) {
    unsigned char c = *p;

    if (c < 128) {
      // single byte
      p++;
    } else if (c < 224) {
      p += 2;
    } else if (c < 240) {
      p += 3;
    } else if (c < 248) {
      p += 4;
    } else {
      // invalid UTF-8 sequence
      break;
    }

    ++length;
  }

  return (char*)p;
}
