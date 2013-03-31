////////////////////////////////////////////////////////////////////////////////
/// @brief basic string functions
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

#include "tri-strings.h"

#include "utf8-helper.h"
#include <openssl/sha.h>

#include "BasicsC/conversions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hex values for all characters
////////////////////////////////////////////////////////////////////////////////

static char const HexValues[513] = {
  "000102030405060708090a0b0c0d0e0f"
  "101112131415161718191a1b1c1d1e1f"
  "202122232425262728292a2b2c2d2e2f"
  "303132333435363738393a3b3c3d3e3f"
  "404142434445464748494a4b4c4d4e4f"
  "505152535455565758595a5b5c5d5e5f"
  "606162636465666768696a6b6c6d6e6f"
  "707172737475767778797a7b7c7d7e7f"
  "808182838485868788898a8b8c8d8e8f"
  "909192939495969798999a9b9c9d9e9f"
  "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
  "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
  "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
  "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
  "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
  "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"
};

////////////////////////////////////////////////////////////////////////////////
/// @brief integer values for all hex characters
////////////////////////////////////////////////////////////////////////////////

static uint8_t const HexDecodeLookup[256] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,1,2,3,4,5,6,7,8,9,             // 0123456789
  0,0,0,0,0,0,0,                   // :;<=>?@
  10,11,12,13,14,15,               // ABCDEF
  0,0,0,0,0,0,0,0,0,0,0,0,0,       // GHIJKLMNOPQRS
  0,0,0,0,0,0,0,0,0,0,0,0,0,       // TUVWXYZ[/]^_`
  10,11,12,13,14,15,               // abcdef
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes UTF-8 range U+0000 to U+007F
////////////////////////////////////////////////////////////////////////////////

static void EscapeUtf8Range0000T007F (char** dst, char const** src) {
  uint8_t c;
  uint16_t i1;
  uint16_t i2;

  c = (uint8_t) *(*src);

  i1 = (((uint16_t) c) & 0xF0) >> 4;
  i2 = (((uint16_t) c) & 0x0F);

  *(*dst)++ = '\\';
  *(*dst)++ = 'u';
  *(*dst)++ = '0';
  *(*dst)++ = '0';

  *(*dst)++ = (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10);
  *(*dst)   = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes UTF-8 range U+0080 to U+07FF
////////////////////////////////////////////////////////////////////////////////

static void EscapeUtf8Range0080T07FF (char** dst, char const** src) {
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
    assert(n >= 128);

    i1 = (n & 0xF000) >> 12;
    i2 = (n & 0x0F00) >>  8;
    i3 = (n & 0x00F0) >>  4;
    i4 = (n & 0x000F);

    *(*dst)++ = '\\';
    *(*dst)++ = 'u';

    *(*dst)++ = (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10);
    *(*dst)++ = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
    *(*dst)++ = (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10);
    *(*dst)   = (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10);

    (*src) += 1;
  }

  // corrupted UTF-8
  else {
    *(*dst) = *(*src);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes UTF-8 range U+0800 to U+D7FF and U+E000 to U+FFFF
////////////////////////////////////////////////////////////////////////////////

static void EscapeUtf8Range0800TFFFF (char** dst, char const** src) {
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

    assert(n >= 2048 && (n < 55296 || n > 57343));

    i1 = (n & 0xF000) >> 12;
    i2 = (n & 0x0F00) >>  8;
    i3 = (n & 0x00F0) >>  4;
    i4 = (n & 0x000F);

    *(*dst)++ = '\\';
    *(*dst)++ = 'u';

    *(*dst)++ = (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10);
    *(*dst)++ = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
    *(*dst)++ = (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10);
    *(*dst)   = (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10);

    (*src) += 2;
  }

  // corrupted UTF-8
  else {
    *(*dst) = *(*src);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes UTF-8 range U+10000 to U+10FFFF
////////////////////////////////////////////////////////////////////////////////

static void EscapeUtf8Range10000T10FFFF (char** dst, char const** src) {
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
    assert(n >= 65536 && n <= 1114111);

    // construct the surrogate pairs
    n -= 0x10000;

    s1 = ((n & 0xFFC00) >> 10) + 0xD800;
    s2 = (n & 0x3FF) + 0xDC00;

    // encode high surrogate
    i1 = (s1 & 0xF000) >> 12;
    i2 = (s1 & 0x0F00) >>  8;
    i3 = (s1 & 0x00F0) >>  4;
    i4 = (s1 & 0x000F);

    *(*dst)++ = '\\';
    *(*dst)++ = 'u';

    *(*dst)++ = (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10);
    *(*dst)++ = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
    *(*dst)++ = (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10);
    *(*dst)++ = (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10);

    // encode low surrogate
    i1 = (s2 & 0xF000) >> 12;
    i2 = (s2 & 0x0F00) >>  8;
    i3 = (s2 & 0x00F0) >>  4;
    i4 = (s2 & 0x000F);

    *(*dst)++ = '\\';
    *(*dst)++ = 'u';

    *(*dst)++ = (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10);
    *(*dst)++ = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
    *(*dst)++ = (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10);
    *(*dst)   = (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10);

    // advance src
    (*src) += 3;
  }

  // corrupted UTF-8
  else {
    *(*dst) = *(*src);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decodes a unicode escape sequence
////////////////////////////////////////////////////////////////////////////////

static void DecodeUnicodeEscape (char** dst, char const* src) {
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
  }
  else if (n <= 0x7FF) {
    *(*dst)++ = 0xC0 + (n >> 6);
    *(*dst)   = 0x80 + (n & 0x3F);

  }
  else {
    *(*dst)++ = 0xE0 + (n >> 12);
    *(*dst)++ = 0x80 + ((n >> 6) & 0x3F);
    *(*dst)   = 0x80 + (n & 0x3F);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decodes a unicode surrogate pair
////////////////////////////////////////////////////////////////////////////////

static void DecodeSurrogatePair (char** dst, char const* src1, char const* src2) {
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
  }
  else if (n <= 0x7FF) {
    *(*dst)++ = 0xC0 + (n >> 6);
    *(*dst)   = 0x80 + (n & 0x3F);
  }
  else if (n <= 0xFFFF) {
    *(*dst)++ = 0xE0 + (n >> 12);
    *(*dst)++ = 0x80 + ((n >> 6) & 0x3F);
    *(*dst)   = 0x80 + (n & 0x3F);
  }
  else {
    *(*dst)++ = 0xF0 + (n >> 18);
    *(*dst)++ = 0x80 + ((n >> 12) & 0x3F);
    *(*dst)++ = 0x80 + ((n >> 6) & 0x3F);
    *(*dst)   = 0x80 + (n & 0x3F);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to lower case
////////////////////////////////////////////////////////////////////////////////

char* TRI_LowerAsciiStringZ (TRI_memory_zone_t* zone, char const* value) {
  size_t length;
  char* buffer;
  char* p;
  char* out;
  char c;

  if (value == NULL) {
    return NULL;
  }

  length = strlen(value);

  buffer = TRI_Allocate(zone, (sizeof(char) * length) + 1, false);
  if (buffer == NULL) {
    return NULL;
  }

  p = (char*) value;
  out = buffer;

  while ((c = *p++)) {
    if (c >= 'A' && c <= 'Z') {
      *out++ = c + 32;
    }
    else {
      *out++ = c;
    }
  }

  *out = '\0';

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to lower case
////////////////////////////////////////////////////////////////////////////////

char* TRI_LowerAsciiString (char const* value) {
  return TRI_LowerAsciiStringZ(TRI_CORE_MEM_ZONE, value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to upper case
////////////////////////////////////////////////////////////////////////////////

char* TRI_UpperAsciiStringZ (TRI_memory_zone_t* zone, char const* value) {
  size_t length;
  char* buffer;
  char* p;
  char* out;
  char c;

  if (value == NULL) {
    return NULL;
  }

  length = strlen(value);

  buffer = TRI_Allocate(zone, (sizeof(char) * length) + 1, false);
  if (buffer == NULL) {
    return NULL;
  }

  p = (char*) value;
  out = buffer;

  while ((c = *p++)) {
    if (c >= 'a' && c <= 'z') {
      *out++ = c - 32;
    }
    else {
      *out++ = c;
    }
  }

  *out = '\0';

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to upper case
////////////////////////////////////////////////////////////////////////////////

char* TRI_UpperAsciiString (char const* value) {
  return TRI_UpperAsciiStringZ(TRI_CORE_MEM_ZONE, value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if strings are equal
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualString (char const* left, char const* right) {
  return strcmp(left, right) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if strings are equal
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualString2 (char const* left, char const* right, size_t n) {
  return strncmp(left, right, n) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if ASCII strings are equal ignoring case
////////////////////////////////////////////////////////////////////////////////

bool TRI_CaseEqualString (char const* left, char const* right) {
  return strcasecmp(left, right) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if ASCII strings are equal ignoring case
////////////////////////////////////////////////////////////////////////////////

bool TRI_CaseEqualString2 (char const* left, char const* right, size_t n) {
  return strncasecmp(left, right, n) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if second string is prefix of the first
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsPrefixString (char const* full, char const* prefix) {
  return strncmp(full, prefix, strlen(prefix)) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief duplicates a string, without using a memory zone
////////////////////////////////////////////////////////////////////////////////

char* TRI_SystemDuplicateString (char const* value) {
  size_t n;
  char* result;

  n = strlen(value) + 1;
  result = (char*) TRI_SystemAllocate(n, false);

  if (result) {
    memcpy(result, value, n);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief duplicates a string
////////////////////////////////////////////////////////////////////////////////

char* TRI_DuplicateString (char const* value) {
  size_t n;
  char* result;

  n = strlen(value) + 1;
  result = TRI_Allocate(TRI_CORE_MEM_ZONE, n, false);

  memcpy(result, value, n);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief duplicates a string
////////////////////////////////////////////////////////////////////////////////

char* TRI_DuplicateStringZ (TRI_memory_zone_t* zone, char const* value) {
  size_t n;
  char* result;

  n = strlen(value) + 1;
  result = TRI_Allocate(zone, n, false);

  if (result != NULL) {
    memcpy(result, value, n);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief duplicates a string of given length
////////////////////////////////////////////////////////////////////////////////

char* TRI_DuplicateString2 (char const* value, size_t length) {
  char* result;
  size_t n;

  n = length + 1;
  result = TRI_Allocate(TRI_CORE_MEM_ZONE, n, false);

  memcpy(result, value, length);
  result[length] = '\0';

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief duplicates a string of given length
////////////////////////////////////////////////////////////////////////////////

char* TRI_DuplicateString2Z (TRI_memory_zone_t* zone, char const* value, size_t length) {
  char* result;
  size_t n;

  n = length + 1;
  result = TRI_Allocate(zone, n, false);

  if (result != NULL) {
    memcpy(result, value, length);
    result[length] = '\0';
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends text to a string
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendString (char** dst, char const* src) {
  char* ptr;

  ptr = TRI_Concatenate2String(*dst, src);
  TRI_FreeString(TRI_CORE_MEM_ZONE, *dst);

  *dst = ptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a string
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyString (char* dst, char const* src, size_t length) {
  *dst = '\0';

  if (0 < length) {
    strncat(dst, src, length - 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate two strings
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate2String (char const* a, char const* b) {
  return TRI_Concatenate2StringZ(TRI_CORE_MEM_ZONE, a, b);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate two strings using a memory zone
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate2StringZ (TRI_memory_zone_t* zone, char const* a, char const* b) {
  char* result;
  size_t na;
  size_t nb;

  na = strlen(a);
  nb = strlen(b);

  result = TRI_Allocate(zone, na + nb + 1, false);
  if (result != NULL) {
    memcpy(result, a, na);
    memcpy(result + na, b, nb);

    result[na + nb] = '\0';
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate three strings
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate3String (char const* a, char const* b, char const* c) {
  return TRI_Concatenate3StringZ(TRI_CORE_MEM_ZONE, a, b, c);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate three strings using a memory zone
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate3StringZ (TRI_memory_zone_t* zone, char const* a, char const* b, char const* c) {
  char* result;
  size_t na;
  size_t nb;
  size_t nc;

  na = strlen(a);
  nb = strlen(b);
  nc = strlen(c);

  result = TRI_Allocate(zone, na + nb + nc + 1, false);
  if (result != NULL) {
    memcpy(result, a, na);
    memcpy(result + na, b, nb);
    memcpy(result + na + nb, c, nc);

    result[na + nb + nc] = '\0';
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate four strings
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate4String (char const* a, char const* b, char const* c, char const* d) {
  char* result;
  size_t na;
  size_t nb;
  size_t nc;
  size_t nd;

  na = strlen(a);
  nb = strlen(b);
  nc = strlen(c);
  nd = strlen(d);

  result = TRI_Allocate(TRI_CORE_MEM_ZONE, na + nb + nc + nd + 1, false);

  memcpy(result, a, na);
  memcpy(result + na, b, nb);
  memcpy(result + na + nb, c, nc);
  memcpy(result + na + nb + nc, d, nd);

  result[na + nb + nc + nd] = '\0';

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate five strings
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate5String (char const* a, char const* b, char const* c, char const* d, char const* e) {
  char* result;
  size_t na;
  size_t nb;
  size_t nc;
  size_t nd;
  size_t ne;

  na = strlen(a);
  nb = strlen(b);
  nc = strlen(c);
  nd = strlen(d);
  ne = strlen(e);

  result = TRI_Allocate(TRI_CORE_MEM_ZONE, na + nb + nc + nd + ne + 1, false);

  memcpy(result, a, na);
  memcpy(result + na, b, nb);
  memcpy(result + na + nb, c, nc);
  memcpy(result + na + nb + nc, d, nd);
  memcpy(result + na + nb + nc + nd, e, ne);

  result[na + nb + nc + nd + ne] = '\0';

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate six strings
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate6String (char const* a, char const* b, char const* c, char const* d, char const* e, char const* f) {
  char* result;
  size_t na;
  size_t nb;
  size_t nc;
  size_t nd;
  size_t ne;
  size_t nf;

  na = strlen(a);
  nb = strlen(b);
  nc = strlen(c);
  nd = strlen(d);
  ne = strlen(e);
  nf = strlen(f);

  result = TRI_Allocate(TRI_CORE_MEM_ZONE, na + nb + nc + nd + ne + nf + 1, false);

  memcpy(result, a, na);
  memcpy(result + na, b, nb);
  memcpy(result + na + nb, c, nc);
  memcpy(result + na + nb + nc, d, nd);
  memcpy(result + na + nb + nc + nd, e, ne);
  memcpy(result + na + nb + nc + nd + ne, f, nf);

  result[na + nb + nc + nd + ne + nf] = '\0';

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief splits a string
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t TRI_SplitString (char const* source, char delim) {
  TRI_vector_string_t result;
  char* buffer;
  char* p;
  char const* q;
  char const* e;
  size_t size;

  TRI_InitVectorString(&result, TRI_CORE_MEM_ZONE);

  if (source == NULL || *source == '\0') {
    return result;
  }

  size = strlen(source);
  buffer = TRI_Allocate(TRI_CORE_MEM_ZONE, size + 1, false);

  p = buffer;

  q = source;
  e = source + size;

  for (;  q < e;  ++q) {
    if (*q == delim) {
      *p = '\0';

      TRI_PushBackVectorString(&result, TRI_DuplicateString2(buffer, (size_t) (p - buffer)));

      p = buffer;
    }
    else {
      *p++ = *q;
    }
  }

  *p = '\0';

  TRI_PushBackVectorString(&result, TRI_DuplicateString2(buffer, (size_t) (p - buffer)));

  TRI_FreeString(TRI_CORE_MEM_ZONE, buffer);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief splits a string, using more than one delimiter
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t TRI_Split2String (char const* source, char const* delim) {
  TRI_vector_string_t result;
  char* buffer;
  char* p;
  char const* q;
  char const* e;
  size_t size;
  size_t delimiterSize;

  TRI_InitVectorString(&result, TRI_CORE_MEM_ZONE);

  if (delim == NULL || *delim == '\0') {
    return result;
  }

  if (source == NULL || *source == '\0') {
    return result;
  }

  delimiterSize = strlen(delim);
  size = strlen(source);
  buffer = TRI_Allocate(TRI_CORE_MEM_ZONE, size + 1, false);

  p = buffer;

  q = source;
  e = source + size;

  for (;  q < e;  ++q) {
    size_t i;
    bool found = false;

    for (i = 0; i < delimiterSize; ++i) {
      if (*q == delim[i]) {
        *p = '\0';

        TRI_PushBackVectorString(&result, TRI_DuplicateString2(buffer, (size_t) (p - buffer)));

        p = buffer;
        found = true;
        break;
      }
    }

    if (! found) {
      *p++ = *q;
    }
  }

  *p = '\0';

  TRI_PushBackVectorString(&result, TRI_DuplicateString2(buffer, p - buffer));

  TRI_FreeString(TRI_CORE_MEM_ZONE, buffer);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a string
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_ZONE_DEBUG

void TRI_FreeStringZ (TRI_memory_zone_t* zone, char* value, char const* file, int line) {
  TRI_FreeZ(zone, value, file, line);
}

#else

void TRI_FreeString (TRI_memory_zone_t* zone, char* value) {
  TRI_Free(zone, value);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           public escape functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Strings
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts into hex reqpresentation
////////////////////////////////////////////////////////////////////////////////

char* TRI_EncodeHexString (char const* source, size_t sourceLen, size_t* dstLen) {
  char* result;
  uint16_t* hex;
  uint16_t* dst;
  uint8_t* src;
  size_t j;

  *dstLen = (sourceLen * 2);
  dst = TRI_Allocate(TRI_CORE_MEM_ZONE, (*dstLen) + 1, false);
  result = (char*) dst;

  hex = (uint16_t*) HexValues;
  src = (uint8_t*)  source;

  for (j = 0;  j < sourceLen;  j++) {
    *dst = hex[*src];
    dst++;
    src++;
  }

  *((char*) dst) = 0; // terminate the string

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts from hex reqpresentation
////////////////////////////////////////////////////////////////////////////////

char* TRI_DecodeHexString (char const* source, size_t sourceLen, size_t* dstLen) {
  char* result;
  uint8_t* dst;
  uint8_t* src;
  uint8_t d;
  size_t j;

  *dstLen = (sourceLen / 2);
  dst = TRI_Allocate(TRI_CORE_MEM_ZONE, (*dstLen) + 1, false);
  result = (char*) dst;

  src = (uint8_t*) source;

  for (j = 0;  j < sourceLen;  j += 2) {
    d  = HexDecodeLookup[*src++] << 4;
    d |= HexDecodeLookup[*src++];

    *dst++ = d;
  }

  *dst = 0; // terminate the string

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha256 of a string
////////////////////////////////////////////////////////////////////////////////

char* TRI_SHA256String (char const* source, size_t sourceLen, size_t* dstLen) {
  unsigned char* dst;

  dst = TRI_Allocate(TRI_CORE_MEM_ZONE, SHA256_DIGEST_LENGTH, false);
  *dstLen = SHA256_DIGEST_LENGTH;

  SHA256((unsigned char const*) source, sourceLen, dst);

  return (char*) dst;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes constrol characters using C escapes
////////////////////////////////////////////////////////////////////////////////

char* TRI_EscapeCString (char const* in, size_t inLength, size_t* outLength) {
  char * buffer;
  char * qtr;
  char const * ptr;
  char const * end;

  buffer = TRI_Allocate(TRI_CORE_MEM_ZONE, 4 * inLength + 1, false);
  qtr = buffer;

  for (ptr = in, end = ptr + inLength;  ptr < end;  ptr++, qtr++) {
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

      case '\'':
      case '"':
        *qtr++ = '\\';
        *qtr = *ptr;
        break;

      default:
        n = (uint8_t)(*ptr);

        if (n < 32 || n > 127) {
          uint8_t n1 = n >> 4;
          uint8_t n2 = n & 0x0F;

          *qtr++ = '\\';
          *qtr++ = 'x';
          *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
          *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
        }
        else {
          *qtr = *ptr;
        }

        break;
    }
  }

  *qtr = '\0';
  *outLength = (size_t) (qtr - buffer);

  qtr = TRI_Allocate(TRI_CORE_MEM_ZONE, *outLength + 1, false);
  memcpy(qtr, buffer, *outLength + 1);

  TRI_Free(TRI_CORE_MEM_ZONE, buffer);
  return qtr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes special characters using C escapes
////////////////////////////////////////////////////////////////////////////////

char* TRI_EscapeControlsCString (char const* in, size_t inLength, size_t* outLength) {
  char * buffer;
  char * qtr;
  char const * ptr;
  char const * end;

  buffer = TRI_Allocate(TRI_CORE_MEM_ZONE, 4 * inLength + 1, false);
  qtr = buffer;

  for (ptr = in, end = ptr + inLength;  ptr < end;  ptr++, qtr++) {
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

      default:
        n = (uint8_t)(*ptr);

        if (n < 32 || n > 127) {
          uint8_t n1 = n >> 4;
          uint8_t n2 = n & 0x0F;

          *qtr++ = '\\';
          *qtr++ = 'x';
          *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
          *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
        }
        else {
          *qtr = *ptr;
        }

        break;
    }
  }

  *qtr = '\0';
  *outLength = (size_t) (qtr - buffer);

  qtr = TRI_Allocate(TRI_CORE_MEM_ZONE, *outLength + 1, false);
  memcpy(qtr, buffer, *outLength + 1);

  TRI_Free(TRI_CORE_MEM_ZONE, buffer);

  return qtr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes special characters using unicode escapes
////////////////////////////////////////////////////////////////////////////////

char* TRI_EscapeUtf8String (char const* in, size_t inLength, bool escapeSlash, size_t* outLength) {
  return TRI_EscapeUtf8StringZ(TRI_CORE_MEM_ZONE, in, inLength, escapeSlash, outLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes special characters using unicode escapes
////////////////////////////////////////////////////////////////////////////////

char* TRI_EscapeUtf8StringZ (TRI_memory_zone_t* zone,
                             char const* in,
                             size_t inLength,
                             bool escapeSlash,
                             size_t* outLength) {
  char * buffer;
  char * qtr;
  char const * ptr;
  char const * end;

  buffer = (char*) TRI_Allocate(zone, 6 * inLength + 1, false);

  if (buffer == NULL) {
    return NULL;
  }

  qtr = buffer;

  for (ptr = in, end = ptr + inLength;  ptr < end;  ++ptr, ++qtr) {
    switch (*ptr) {
      case '/':
        if (escapeSlash) {
          *qtr++ = '\\';
        }

        *qtr = *ptr;
        break;

      case '\\':
      case '"':
        *qtr++ = '\\';
        *qtr = *ptr;
        break;

      case '\b':
        *qtr++ = '\\';
        *qtr = 'b';
        break;

      case '\f':
        *qtr++ = '\\';
        *qtr = 'f';
        break;

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

      case '\0':
        *qtr++ = '\\';
        *qtr++ = 'u';
        *qtr++ = '0';
        *qtr++ = '0';
        *qtr++ = '0';
        *qtr = '0';
        break;

      default:
        {
          uint8_t c;

          // next character as unsigned char
          c = (uint8_t) *ptr;

          // character is in the normal latin1 range
          if ((c & 0x80) == 0) {

            // special character, escape
            if (c < 32) {
              EscapeUtf8Range0000T007F(&qtr, &ptr);
            }

            // normal latin1
            else {
              *qtr = *ptr;
            }
          }

          // unicode range 0080 - 07ff (2-byte sequence UTF-8)
          else if ((c & 0xE0) == 0xC0) {

            // hopefully correct UTF-8
            if (ptr + 1 < end) {
              EscapeUtf8Range0080T07FF(&qtr, &ptr);
            }

            // corrupted UTF-8
            else {
              *qtr = *ptr;
            }
          }

          // unicode range 0800 - ffff (3-byte sequence UTF-8)
          else if ((c & 0xF0) == 0xE0) {

            // hopefully correct UTF-8
            if (ptr + 2 < end) {
              EscapeUtf8Range0800TFFFF(&qtr, &ptr);
            }

            // corrupted UTF-8
            else {
              *qtr = *ptr;
            }
          }

          // unicode range 10000 - 10ffff (4-byte sequence UTF-8)
          else if ((c & 0xF8) == 0xF0) {

            // hopefully correct UTF-8
            if (ptr + 3 < end) {
              EscapeUtf8Range10000T10FFFF(&qtr, &ptr);
            }

            // corrupted UTF-8
            else {
              *qtr = *ptr;
            }
          }

          // unicode range above 10ffff -- NOT IMPLEMENTED
          else {
            *qtr = *ptr;
          }
        }

        break;
    }
  }

  *qtr = '\0';
  *outLength = (size_t) (qtr - buffer);

  qtr = TRI_Allocate(zone, *outLength + 1, false);

  if (qtr != NULL) {
    memcpy(qtr, buffer, *outLength + 1);
  }

  TRI_Free(zone, buffer);
  return qtr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unescapes unicode escape sequences
////////////////////////////////////////////////////////////////////////////////

char* TRI_UnescapeUtf8String (char const* in, size_t inLength, size_t* outLength) {
  return TRI_UnescapeUtf8StringZ(TRI_CORE_MEM_ZONE, in, inLength, outLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unescapes unicode escape sequences
////////////////////////////////////////////////////////////////////////////////

char* TRI_UnescapeUtf8StringZ (TRI_memory_zone_t* zone, char const* in, size_t inLength, size_t* outLength) {
  char * buffer;
  char * qtr;
  char const * ptr;
  char const * end;

#ifdef TRI_HAVE_ICU
  char * utf8_nfc;
  size_t tmpLength = 0;
#endif

  buffer = TRI_Allocate(zone, inLength + 1, false);

  if (buffer == NULL) {
    return NULL;
  }

  qtr = buffer;

  for (ptr = in, end = ptr + inLength;  ptr < end;  ++ptr, ++qtr) {
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
                sp &= (c2 == '8' || c2 == '9' || c2 == 'A' || c2 == 'a' || c2 == 'B' || c2 == 'b');
              }

              if (sp) {
                char c3 = ptr[7];

                sp &= (ptr[5] == '\\' && ptr[6] == 'u');
                sp &= (c3 == 'd' || c3 == 'D');
              }

              if (sp) {
                char c4 = ptr[8];
                sp &= (c4 == 'C' || c4 == 'c' || c4 == 'D' || c4 == 'd' || c4 == 'E' || c4 == 'e' || c4 == 'F' || c4 == 'f');
              }

              if (sp) {
                DecodeSurrogatePair(&qtr, ptr + 1, ptr + 7);
                ptr += 10;
              }
              else {
                DecodeUnicodeEscape(&qtr, ptr + 1);
                ptr += 4;
              }
            }
            else {
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

  *qtr = '\0';
  *outLength = (size_t) (qtr - buffer);

#ifdef TRI_HAVE_ICU
  if (*outLength > 0) {
    utf8_nfc = TRI_normalize_utf8_to_NFC(zone, buffer, *outLength, &tmpLength);
    if (utf8_nfc) {
      *outLength = tmpLength;
      TRI_Free(zone, buffer);
      return utf8_nfc;
    }
  }
#endif

  // we might have wasted some space if the unescaped string is shorter than the
  // escaped one. this is the case if the string contained escaped characters
  if (((ptr - in) > 0) && (*outLength < (size_t)(ptr - in))) {
    // result string is shorter than original string
    qtr = TRI_Allocate(zone, *outLength + 1, false);

    if (qtr != NULL) {
      memcpy(qtr, buffer, *outLength + 1);
      TRI_Free(zone, buffer);

      return qtr;
    }

    // intentional fall-through
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the number of characters in a UTF-8 string
/// the UTF-8 string must be well-formed and end with a NUL terminator
////////////////////////////////////////////////////////////////////////////////

size_t TRI_CharLengthUtf8String (const char* in) {
  size_t length;
  unsigned char* p;

  p = (unsigned char*) in;
  length = 0;

  while (*p) {
    unsigned char c = *p;

    if (c < 128) {
      // single byte
      p++;
    }
    else if (c < 224) {
      p += 2;
    }
    else if (c < 240) {
      p += 3;
    }
    else if (c < 248) {
      p += 4;
    }
    else {
      printf("invalid utf\n");
      // invalid UTF-8 sequence
      break;
    }

    ++length;
  }

  return length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string end position for a leftmost prefix of a UTF-8 string
/// eg. when specifying (müller, 2), the return value will be a pointer to the
/// first "l".
/// the UTF-8 string must be well-formed and end with a NUL terminator
////////////////////////////////////////////////////////////////////////////////

char* TRI_PrefixUtf8String (const char* in, const uint32_t maximumLength) {
  uint32_t length;
  unsigned char* p;

  p = (unsigned char*) in;
  length = 0;

  while (*p && length < maximumLength) {
    unsigned char c = *p;

    if (c < 128) {
      // single byte
      p++;
    }
    else if (c < 224) {
      p += 2;
    }
    else if (c < 240) {
      p += 3;
    }
    else if (c < 248) {
      p += 4;
    }
    else {
      // invalid UTF-8 sequence
      break;
    }

    ++length;
  }

  return (char*) p;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
