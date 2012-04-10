////////////////////////////////////////////////////////////////////////////////
/// @brief basic string functions
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

#include "strings.h"

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

    n = ((c & 0x3f) << 12) | ((d & 0x3f) << 6) | (e & 0x3f);

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

    n = ((c & 0x08) << 18) | ((d & 0x3F) << 12) | ((e & 0x3F) << 6) | (f & 0x3F);

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
/// @brief tests if second string is prefix of the first
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsPrefixString (char const* full, char const* prefix) {
  return strncmp(full, prefix, strlen(prefix)) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief duplicates a string
////////////////////////////////////////////////////////////////////////////////

char* TRI_DuplicateString (char const* value) {
  size_t n;
  char* result;

  n = strlen(value) + 1;
  result = TRI_Allocate(n);
  if (!result) {
    return 0;
  }

  memcpy(result, value, n);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief duplicates a string of given length
////////////////////////////////////////////////////////////////////////////////

char* TRI_DuplicateString2 (char const* value, size_t length) {
  char* result;
  size_t n;

  n = length + 1;
  result = TRI_Allocate(n);

  if (!result) {
    return 0;
  }

  memcpy(result, value, length);
  result[length] = '\0';

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends text to a string
////////////////////////////////////////////////////////////////////////////////

void TRI_AppendString (char** dst, char const* src) {
  char* ptr;

  ptr = TRI_Concatenate2String(*dst, src);
  TRI_FreeString(*dst);

  *dst = ptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a string
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyString (char* dst, char const* src, size_t length) {
  *dst = '\0';
  strncat(dst, src, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief concatenate two strings
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate2String (char const* a, char const* b) {
  char* result;
  size_t na;
  size_t nb;

  na = strlen(a);
  nb = strlen(b);

  result = TRI_Allocate(na + nb + 1);
  if (result) {
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
  char* result;
  size_t na;
  size_t nb;
  size_t nc;

  na = strlen(a);
  nb = strlen(b);
  nc = strlen(c);

  result = TRI_Allocate(na + nb + nc + 1);
  if (result) {
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

  result = TRI_Allocate(na + nb + nc + nd + 1);
  if (result) {
    memcpy(result, a, na);
    memcpy(result + na, b, nb);
    memcpy(result + na + nb, c, nc);
    memcpy(result + na + nb + nc, d, nd);

    result[na + nb + nc + nd] = '\0';
  }

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

  result = TRI_Allocate(na + nb + nc + nd + ne + 1);
  if (result) {
    memcpy(result, a, na);
    memcpy(result + na, b, nb);
    memcpy(result + na + nb, c, nc);
    memcpy(result + na + nb + nc, d, nd);
    memcpy(result + na + nb + nc + nd, e, ne);

    result[na + nb + nc + nd + ne] = '\0';
  }

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

  result = TRI_Allocate(na + nb + nc + nd + ne + nf + 1);
  if (result) {
    memcpy(result, a, na);
    memcpy(result + na, b, nb);
    memcpy(result + na + nb, c, nc);
    memcpy(result + na + nb + nc, d, nd);
    memcpy(result + na + nb + nc + nd, e, ne);
    memcpy(result + na + nb + nc + nd + ne, f, nf);

    result[na + nb + nc + nd + ne + nf] = '\0';
  }
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

  TRI_InitVectorString(&result);

  if (source == NULL || *source == '\0') {
    return result;
  }

  size = strlen(source);
  buffer = TRI_Allocate(size + 1);
  if (buffer) {
    p = buffer;

    q = source;
    e = source + size;

    for (;  q < e;  ++q) {
      if (*q == delim) {
        *p = '\0';

        TRI_PushBackVectorString(&result, TRI_DuplicateString2(buffer, p - buffer));

        p = buffer;
      }
      else {
        *p++ = *q;
      }
    }

    *p = '\0';

    TRI_PushBackVectorString(&result, TRI_DuplicateString2(buffer, p - buffer));

    TRI_FreeString(buffer);
  }

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

  TRI_InitVectorString(&result);

  if (delim == NULL || *delim == '\0') {
    return result;
  }

  if (source == NULL || *source == '\0') {
    return result;
  }
  
  delimiterSize = strlen(delim);
  size = strlen(source);
  buffer = TRI_Allocate(size + 1);
  if (buffer) {
    p = buffer;

    q = source;
    e = source + size;

    for (;  q < e;  ++q) {
      size_t i;
      bool found = false;
      for (i = 0; i < delimiterSize; ++i) {
        if (*q == delim[i]) {
          *p = '\0';

          TRI_PushBackVectorString(&result, TRI_DuplicateString2(buffer, p - buffer));
          p = buffer;
          found = true;
          break;
        }
      }

      if (!found) {
        *p++ = *q;
      }
    }

    *p = '\0';

    TRI_PushBackVectorString(&result, TRI_DuplicateString2(buffer, p - buffer));

    TRI_FreeString(buffer);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a string
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeString (char* value) {
  TRI_Free(value);
}

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
/// @brief converts a single hex to an integer
////////////////////////////////////////////////////////////////////////////////

int TRI_IntHex (char ch, int errorValue) {
  if ('0' <= ch && ch <= '9') {
    return ch - '0';
  }
  else if ('A' <= ch && ch <= 'F') {
    return ch - 'A' + 10;
  }
  else if ('a' <= ch && ch <= 'f') {
    return ch - 'a' + 10;
  }

  return errorValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes special characters using C escapes
////////////////////////////////////////////////////////////////////////////////

char* TRI_EscapeCString (char const* in, size_t inLength, size_t* outLength) {
  char * buffer;
  char * qtr;
  char const * ptr;
  char const * end;

  buffer = TRI_Allocate(4 * inLength + 1);
  if (!buffer) {
    return NULL;
  }

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
  *outLength = qtr - buffer;

  qtr = TRI_Allocate(*outLength + 1);
  memcpy(qtr, buffer, *outLength + 1);

  TRI_Free(buffer);

  return qtr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes special characters using unicode escapes
////////////////////////////////////////////////////////////////////////////////

char* TRI_EscapeUtf8String (char const* in, size_t inLength, bool escapeSlash, size_t* outLength) {
  char * buffer;
  char * qtr;
  char const * ptr;
  char const * end;

  buffer = (char*) TRI_Allocate(6 * inLength + 1);

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
  *outLength = qtr - buffer;

  qtr = TRI_Allocate(*outLength + 1);

  if (qtr != NULL) {
    memcpy(qtr, buffer, *outLength + 1);
  }

  TRI_Free(buffer);

  return qtr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unescapes unicode escape sequences
////////////////////////////////////////////////////////////////////////////////

char* TRI_UnescapeUtf8String (char const* in, size_t inLength, size_t* outLength) {
  char * buffer;
  char * qtr;
  char const * ptr;
  char const * end;
  char c1;
  char c2;
  char c3;
  char c4;

  buffer = TRI_Allocate(inLength + 1);
  if (!buffer) {
    return NULL;
  }

  qtr = buffer;

  for (ptr = in, end = ptr + inLength;  ptr < end;  ++ptr, ++qtr) {
    if (*ptr == '\\' && ptr + 1 < end) {
      ++ptr;

      switch (*ptr) {
        case '/':
          *qtr = '/';
          break;

        case '\\':
          *qtr = '\\';
          break;

        case '"':
          *qtr = '"';
          break;

        case 'b':
          *qtr = '\b';
          break;

        case 'f':
          *qtr = 'f';
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
            c1 = ptr[1];
            c2 = ptr[2];

            // check, if we have a surrogate pair
            if (ptr + 10 < end) {
              bool sp;

              c3 = ptr[7];
              c4 = ptr[8];

              sp = (c1 == 'd' || c1 == 'D');
              sp &= (c2 == '8' || c2 == '9' || c2 == 'A' || c2 == 'a' || c2 == 'B' || c2 == 'b');
              sp &= (ptr[5] == '\\' || ptr[6] == 'u');
              sp &= (c3 == 'd' || c3 == 'D');
              sp &= (c4 == 'C' || c4 == 'c' || c4 == 'D' || c4 == 'd' || c4 == 'E' || c4 == 'e' || c4 == 'F' || c4 == 'f');

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
          *qtr = *ptr;
          break;

      }
    }
    else {
      *qtr = *ptr;
    }
  }

  *qtr = '\0';
  *outLength = qtr - buffer;

  qtr = TRI_Allocate(*outLength + 1);
  if (qtr) {
    memcpy(qtr, buffer, *outLength + 1);
  }
  TRI_Free(buffer);

  return qtr;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
