////////////////////////////////////////////////////////////////////////////////
/// @brief conversion functions
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

#include "conversions.h"

#include "BasicsC/strings.h"
#include "BasicsC/string-buffer.h"

// -----------------------------------------------------------------------------
// --SECTION--                          public functions for string to something
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Conversions
/// @{
////////////////////////////////////////////////////////////////////////////////

static char const * const HEX = "0123456789ABCDEF";

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
/// @brief convert to boolean from string
////////////////////////////////////////////////////////////////////////////////

bool TRI_BooleanString (char const* str) {
  if (TRI_CaseEqualString(str, "true")) {
    return true;
  }
  
  if (TRI_CaseEqualString(str, "yes")) {
    return true;
  }
  
  if (TRI_CaseEqualString(str, "on")) {
    return true;
  }
  
  if (TRI_CaseEqualString(str, "y")) {
    return true;
  }
  
  if (TRI_EqualString(str, "1")) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to double from string
////////////////////////////////////////////////////////////////////////////////

double TRI_DoubleString (char const* str) {
  double result;
  char* endptr;

  TRI_set_errno(TRI_ERROR_NO_ERROR);

  result = strtod(str, &endptr);

  while (isspace(*endptr)) {
    ++endptr;
  }

  if (*endptr != '\0') {
    TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }
  else if (errno == ERANGE && (result == HUGE_VAL || result == -HUGE_VAL || result == 0)) {
    TRI_set_errno(TRI_ERROR_NUMERIC_OVERFLOW);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to int32 from string
////////////////////////////////////////////////////////////////////////////////

int32_t TRI_Int32String (char const* str) {
  int32_t result;
  char* endptr;

#if defined(TRI_HAVE_STRTOL_R)
  struct reent buffer;
#elif defined(TRI_HAVE__STRTOL_R)
  struct reent buffer;
#endif

  TRI_set_errno(TRI_ERROR_NO_ERROR);

#if defined(TRI_HAVE_STRTOL_R)
  result = strtol_r(&buffer, str, &endptr, 10);
#elif defined(TRI_HAVE__STRTOL_R)
  result = _strtol_r(&buffer, str, &endptr, 10);
#else
  result = strtol(str, &endptr, 10);
#endif

  while (isspace(*endptr)) {
    ++endptr;
  }

  if (*endptr != '\0') {
    TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }
  else if (errno == ERANGE && (result == INT32_MIN || result == INT32_MAX)) {
    TRI_set_errno(TRI_ERROR_NUMERIC_OVERFLOW);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to int32 from string with given length
////////////////////////////////////////////////////////////////////////////////

int32_t TRI_Int32String2 (char const* str, size_t length) {
  char tmp[1024];

  if (str[length] != '\0') {
    if (length >= sizeof(tmp)) {
      length = sizeof(tmp) - 1;
    }

    memcpy(tmp, str, length);
    tmp[length] = '\0';
    str = tmp;
  }

  return TRI_Int32String(str);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to uint32 from string
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_UInt32String (char const* str) {
  uint32_t result;
  char* endptr;

#if defined(TRI_HAVE_STRTOUL_R)
  struct reent buffer;
#elif defined(TRI_HAVE__STRTOUL_R)
  struct reent buffer;
#endif

  TRI_set_errno(TRI_ERROR_NO_ERROR);

#if defined(TRI_HAVE_STRTOUL_R)
  result = strtoul_r(&buffer, str, &endptr, 10);
#elif defined(TRI_HAVE__STRTOUL_R)
  result = _strtoul_r(&buffer, str, &endptr, 10);
#else
  result = strtoul(str, &endptr, 10);
#endif

  while (isspace(*endptr)) {
    ++endptr;
  }

  if (*endptr != '\0') {
    TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }
  else if (errno == ERANGE && (result == 0 || result == UINT32_MAX)) {
    TRI_set_errno(TRI_ERROR_NUMERIC_OVERFLOW);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to uint32 from string with given length
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_UInt32String2 (char const* str, size_t length) {
  char tmp[1024];

  if (str[length] != '\0') {
    if (length >= sizeof(tmp)) {
      length = sizeof(tmp) - 1;
    }

    memcpy(tmp, str, length);
    tmp[length] = '\0';
    str = tmp;
  }

  return TRI_UInt32String(str);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to int64 from string
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_Int64String (char const* str) {
  int64_t result;
  char* endptr;

#if defined(TRI_HAVE_STRTOLL_R)
  struct reent buffer;
#elif defined(TRI_HAVE__STRTOLL_R)
  struct reent buffer;
#endif

  TRI_set_errno(TRI_ERROR_NO_ERROR);

#if defined(TRI_HAVE_STRTOLL_R)
  result = strtoll_r(&buffer, str, &endptr, 10);
#elif defined(TRI_HAVE__STRTOLL_R)
  result = _strtoll_r(&buffer, str, &endptr, 10);
#elif defined(TRI_HAVE_STRTOI64)
  result = _strtoi64(str, &endptr, 10);
#elif defined(TRI_HAVE_STRTOLL)
  result = strtoll(str, &endptr, 10);
#else
#warning cannot convert string to int64
#endif

  while (isspace(*endptr)) {
    ++endptr;
  }

  if (*endptr != '\0') {
    TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }
  else if (errno == ERANGE && (result == INT64_MIN || result == INT64_MAX)) {
    TRI_set_errno(TRI_ERROR_NUMERIC_OVERFLOW);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to int64 from string with given length
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_Int64String2 (char const* str, size_t length) {
  char tmp[1024];

  if (str[length] != '\0') {
    if (length >= sizeof(tmp)) {
      length = sizeof(tmp) - 1;
    }

    memcpy(tmp, str, length);
    tmp[length] = '\0';
    str = tmp;
  }

  return TRI_Int64String(str);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to uint64 from string
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_UInt64String (char const* str) {
  uint64_t result;
  char* endptr;

#if defined(TRI_HAVE_STRTOULL_R)
  struct reent buffer;
#elif defined(TRI_HAVE__STRTOULL_R)
  struct reent buffer;
#endif

  TRI_set_errno(TRI_ERROR_NO_ERROR);

#if defined(TRI_HAVE_STRTOULL_R)
  result = strtoull_r(&buffer, str, &endptr, 10);
#elif defined(TRI_HAVE__STRTOULL_R)
  result = _strtoull_r(&buffer, str, &endptr, 10);
#elif defined(TRI_HAVE_STRTOUI64)
  result = _strtoui64(str, &endptr, 10);
#elif defined(TRI_HAVE_STRTOULL)
  result = strtoull(str, &endptr, 10);
#else
#warning cannot convert string to int64
#endif

  while (isspace(*endptr)) {
    ++endptr;
  }

  if (*endptr != '\0') {
    TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }
  else if (errno == ERANGE && (result == 0 || result == UINT64_MAX)) {
    TRI_set_errno(TRI_ERROR_NUMERIC_OVERFLOW);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to uint64 from string with given length
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_UInt64String2 (char const* str, size_t length) {
  char tmp[1024];

  if (str[length] != '\0') {
    if (length >= sizeof(tmp)) {
      length = sizeof(tmp) - 1;
    }

    memcpy(tmp, str, length);
    tmp[length] = '\0';
    str = tmp;
  }

  return TRI_UInt64String(str);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                             public functions for number to string
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Conversions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int8, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringInt8InPlace (int8_t attr, char* buffer) {
  char* p;
  
  if (attr == INT8_MIN) {
    memcpy(buffer, "-128\0", 4);
    return 3;
  }

  p = buffer;

  if (attr < 0) {
    *p++ = '-';
    attr = -attr;
  }

  if (       100 <= attr) { *p++ = (char)((attr /        100) % 10 + '0'); }
  if (        10 <= attr) { *p++ = (char)((attr /         10) % 10 + '0'); }

  *p++ = (char)(attr % 10 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint8, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringUInt8InPlace (uint8_t attr, char* buffer) {
  char* p;

  p = buffer;

  if (       100U <= attr) { *p++ = (char)((attr /        100U) % 10 + '0'); }
  if (        10U <= attr) { *p++ = (char)((attr /         10U) % 10 + '0'); }

  *p++ = (char)(attr % 10 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int16, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringInt16InPlace (int16_t attr, char* buffer) {
  char* p;
  
  if (attr == INT16_MIN) {
    memcpy(buffer, "-32768\0", 7);
    return 6;
  }

  p = buffer;

  if (attr < 0) {
    *p++ = '-';
    attr = -attr;
  }

  if (     10000 <= attr) { *p++ = (char)((attr /      10000) % 10 + '0'); }
  if (      1000 <= attr) { *p++ = (char)((attr /       1000) % 10 + '0'); }
  if (       100 <= attr) { *p++ = (char)((attr /        100) % 10 + '0'); }
  if (        10 <= attr) { *p++ = (char)((attr /         10) % 10 + '0'); }

  *p++ = (char)(attr % 10 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint16, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringUInt16InPlace (uint16_t attr, char* buffer) {
  char* p;

  p = buffer;

  if (     10000U <= attr) { *p++ = (char)((attr /      10000U) % 10 + '0'); }
  if (      1000U <= attr) { *p++ = (char)((attr /       1000U) % 10 + '0'); }
  if (       100U <= attr) { *p++ = (char)((attr /        100U) % 10 + '0'); }
  if (        10U <= attr) { *p++ = (char)((attr /         10U) % 10 + '0'); }

  *p++ = (char)(attr % 10 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int32, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringInt32InPlace (int32_t attr, char* buffer) {
  char* p;
  
  if (attr == INT32_MIN) {
    memcpy(buffer, "-2147483648\0", 12);
    return 11;
  }

  p = buffer;

  if (attr < 0) {
    *p++ = '-';
    attr = -attr;
  }

  if (1000000000L <= attr) { *p++ = (char)((attr / 1000000000L) % 10 + '0'); }
  if ( 100000000L <= attr) { *p++ = (char)((attr /  100000000L) % 10 + '0'); }
  if (  10000000L <= attr) { *p++ = (char)((attr /   10000000L) % 10 + '0'); }
  if (   1000000L <= attr) { *p++ = (char)((attr /    1000000L) % 10 + '0'); }
  if (    100000L <= attr) { *p++ = (char)((attr /     100000L) % 10 + '0'); }
  if (     10000L <= attr) { *p++ = (char)((attr /      10000L) % 10 + '0'); }
  if (      1000L <= attr) { *p++ = (char)((attr /       1000L) % 10 + '0'); }
  if (       100L <= attr) { *p++ = (char)((attr /        100L) % 10 + '0'); }
  if (        10L <= attr) { *p++ = (char)((attr /         10L) % 10 + '0'); }

  *p++ = (char)(attr % 10 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint32, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringUInt32InPlace (uint32_t attr, char* buffer) {
  char* p;

  p = buffer;

  if (1000000000UL <= attr) { *p++ = (char)((attr / 1000000000UL) % 10 + '0'); }
  if ( 100000000UL <= attr) { *p++ = (char)((attr /  100000000UL) % 10 + '0'); }
  if (  10000000UL <= attr) { *p++ = (char)((attr /   10000000UL) % 10 + '0'); }
  if (   1000000UL <= attr) { *p++ = (char)((attr /    1000000UL) % 10 + '0'); }
  if (    100000UL <= attr) { *p++ = (char)((attr /     100000UL) % 10 + '0'); }
  if (     10000UL <= attr) { *p++ = (char)((attr /      10000UL) % 10 + '0'); }
  if (      1000UL <= attr) { *p++ = (char)((attr /       1000UL) % 10 + '0'); }
  if (       100UL <= attr) { *p++ = (char)((attr /        100UL) % 10 + '0'); }
  if (        10UL <= attr) { *p++ = (char)((attr /         10UL) % 10 + '0'); }

  *p++ = (char)(attr % 10 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int64, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringInt64InPlace (int64_t attr, char* buffer) {
  char* p;
  
  if (attr == INT64_MIN) {
    memcpy(buffer, "-9223372036854775808\0", 21);
    return 20;
  }

  if (attr >= 0 && (attr >> 32) == 0) {
    // shortcut
    return TRI_StringUInt32InPlace((uint32_t) attr, buffer);
  }

  p = buffer;
  
  if (attr < 0) {
    *p++ = '-';
    attr = -attr;

    if ((attr >> 32) == 0) {
      // shortcut
      return TRI_StringUInt32InPlace((uint32_t) attr, p) + 1;
    }
  }

  if (1000000000000000000LL <= attr) { *p++ = (char)((attr / 1000000000000000000LL) % 10 + '0'); }
  if ( 100000000000000000LL <= attr) { *p++ = (char)((attr /  100000000000000000LL) % 10 + '0'); }
  if (  10000000000000000LL <= attr) { *p++ = (char)((attr /   10000000000000000LL) % 10 + '0'); }
  if (   1000000000000000LL <= attr) { *p++ = (char)((attr /    1000000000000000LL) % 10 + '0'); }
  if (    100000000000000LL <= attr) { *p++ = (char)((attr /     100000000000000LL) % 10 + '0'); }
  if (     10000000000000LL <= attr) { *p++ = (char)((attr /      10000000000000LL) % 10 + '0'); }
  if (      1000000000000LL <= attr) { *p++ = (char)((attr /       1000000000000LL) % 10 + '0'); }
  if (       100000000000LL <= attr) { *p++ = (char)((attr /        100000000000LL) % 10 + '0'); }
  if (        10000000000LL <= attr) { *p++ = (char)((attr /         10000000000LL) % 10 + '0'); }
  if (         1000000000LL <= attr) { *p++ = (char)((attr /          1000000000LL) % 10 + '0'); }
  if (          100000000LL <= attr) { *p++ = (char)((attr /           100000000LL) % 10 + '0'); }
  if (           10000000LL <= attr) { *p++ = (char)((attr /            10000000LL) % 10 + '0'); }
  if (            1000000LL <= attr) { *p++ = (char)((attr /             1000000LL) % 10 + '0'); }
  if (             100000LL <= attr) { *p++ = (char)((attr /              100000LL) % 10 + '0'); }
  if (              10000LL <= attr) { *p++ = (char)((attr /               10000LL) % 10 + '0'); }
  if (               1000LL <= attr) { *p++ = (char)((attr /                1000LL) % 10 + '0'); }
  if (                100LL <= attr) { *p++ = (char)((attr /                 100LL) % 10 + '0'); }
  if (                 10LL <= attr) { *p++ = (char)((attr /                  10LL) % 10 + '0'); }

  *p++ = (char)(attr % 10 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint64, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringUInt64InPlace (uint64_t attr, char* buffer) {
  char* p;
  
  if ((attr >> 32) == 0) {
    // shortcut
    return TRI_StringUInt32InPlace((uint32_t) attr, buffer);
  }

  p = buffer;

  if (10000000000000000000ULL <= attr) { *p++ = (char)((attr / 10000000000000000000ULL) % 10 + '0'); }
  if ( 1000000000000000000ULL <= attr) { *p++ = (char)((attr /  1000000000000000000ULL) % 10 + '0'); }
  if (  100000000000000000ULL <= attr) { *p++ = (char)((attr /   100000000000000000ULL) % 10 + '0'); }
  if (   10000000000000000ULL <= attr) { *p++ = (char)((attr /    10000000000000000ULL) % 10 + '0'); }
  if (    1000000000000000ULL <= attr) { *p++ = (char)((attr /     1000000000000000ULL) % 10 + '0'); }
  if (     100000000000000ULL <= attr) { *p++ = (char)((attr /      100000000000000ULL) % 10 + '0'); }
  if (      10000000000000ULL <= attr) { *p++ = (char)((attr /       10000000000000ULL) % 10 + '0'); }
  if (       1000000000000ULL <= attr) { *p++ = (char)((attr /        1000000000000ULL) % 10 + '0'); }
  if (        100000000000ULL <= attr) { *p++ = (char)((attr /         100000000000ULL) % 10 + '0'); }
  if (         10000000000ULL <= attr) { *p++ = (char)((attr /          10000000000ULL) % 10 + '0'); }
  if (          1000000000ULL <= attr) { *p++ = (char)((attr /           1000000000ULL) % 10 + '0'); }
  if (           100000000ULL <= attr) { *p++ = (char)((attr /            100000000ULL) % 10 + '0'); }
  if (            10000000ULL <= attr) { *p++ = (char)((attr /             10000000ULL) % 10 + '0'); }
  if (             1000000ULL <= attr) { *p++ = (char)((attr /              1000000ULL) % 10 + '0'); }
  if (              100000ULL <= attr) { *p++ = (char)((attr /               100000ULL) % 10 + '0'); }
  if (               10000ULL <= attr) { *p++ = (char)((attr /                10000ULL) % 10 + '0'); }
  if (                1000ULL <= attr) { *p++ = (char)((attr /                 1000ULL) % 10 + '0'); }
  if (                 100ULL <= attr) { *p++ = (char)((attr /                  100ULL) % 10 + '0'); }
  if (                  10ULL <= attr) { *p++ = (char)((attr /                   10ULL) % 10 + '0'); }

  *p++ = (char)(attr % 10 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int8
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringInt8 (int8_t attr) {
  char buffer[4];
  size_t len;

  len = TRI_StringInt8InPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint8
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt8 (uint8_t attr) {
  char buffer[3];
  size_t len;
  
  len = TRI_StringUInt8InPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int16
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringInt16 (int16_t attr) {
  char buffer[7];
  size_t len;

  len = TRI_StringInt16InPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint16
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt16 (uint16_t attr) {
  char buffer[6];
  size_t len;
  
  len = TRI_StringUInt16InPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int32
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringInt32 (int32_t attr) {
  char buffer[12];
  size_t len;

  len = TRI_StringInt32InPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint32
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt32 (uint32_t attr) {
  char buffer[11];
  size_t len;
  
  len = TRI_StringUInt32InPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int64
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringInt64 (int64_t attr) {
  char buffer[21];
  size_t len;

  len = TRI_StringInt64InPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint64
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt64 (uint64_t attr) {
  char buffer[21];
  size_t len;
  
  len = TRI_StringUInt64InPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from double
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringDouble (double value) {
  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer, TRI_CORE_MEM_ZONE);
  TRI_AppendDoubleStringBuffer(&buffer, value);

  return buffer._buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to hex string from uint32, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringUInt32HexInPlace (uint32_t attr, char* buffer) {
  char* p;
  
  p = buffer;
  
  if (0x10000000U <= attr) { *p++ = HEX[(attr / 0x10000000U) % 0x10]; }
  if ( 0x1000000U <= attr) { *p++ = HEX[(attr /  0x1000000U) % 0x10]; }
  if (  0x100000U <= attr) { *p++ = HEX[(attr /   0x100000U) % 0x10]; }
  if (   0x10000U <= attr) { *p++ = HEX[(attr /    0x10000U) % 0x10]; }
  if (    0x1000U <= attr) { *p++ = HEX[(attr /     0x1000U) % 0x10]; }
  if (     0x100U <= attr) { *p++ = HEX[(attr /      0x100U) % 0x10]; }
  if (      0x10U <= attr) { *p++ = HEX[(attr /       0x10U) % 0x10]; }

  *p++ = HEX[attr % 0x10];
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to hex string from uint64, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringUInt64HexInPlace (uint64_t attr, char* buffer) {
  char* p;
  
  p = buffer;

  if (0x1000000000000000ULL <= attr) { *p++ = HEX[(attr / 0x1000000000000000ULL) % 0x10]; }
  if ( 0x100000000000000ULL <= attr) { *p++ = HEX[(attr /  0x100000000000000ULL) % 0x10]; }
  if (  0x10000000000000ULL <= attr) { *p++ = HEX[(attr /   0x10000000000000ULL) % 0x10]; }
  if (   0x1000000000000ULL <= attr) { *p++ = HEX[(attr /    0x1000000000000ULL) % 0x10]; }
  if (    0x100000000000ULL <= attr) { *p++ = HEX[(attr /     0x100000000000ULL) % 0x10]; }
  if (     0x10000000000ULL <= attr) { *p++ = HEX[(attr /      0x10000000000ULL) % 0x10]; }
  if (      0x1000000000ULL <= attr) { *p++ = HEX[(attr /       0x1000000000ULL) % 0x10]; }
  if (       0x100000000ULL <= attr) { *p++ = HEX[(attr /        0x100000000ULL) % 0x10]; }
  if (        0x10000000ULL <= attr) { *p++ = HEX[(attr /         0x10000000ULL) % 0x10]; }
  if (         0x1000000ULL <= attr) { *p++ = HEX[(attr /          0x1000000ULL) % 0x10]; }
  if (          0x100000ULL <= attr) { *p++ = HEX[(attr /           0x100000ULL) % 0x10]; }
  if (           0x10000ULL <= attr) { *p++ = HEX[(attr /            0x10000ULL) % 0x10]; }
  if (            0x1000ULL <= attr) { *p++ = HEX[(attr /             0x1000ULL) % 0x10]; }
  if (             0x100ULL <= attr) { *p++ = HEX[(attr /              0x100ULL) % 0x10]; }
  if (              0x10ULL <= attr) { *p++ = HEX[(attr /               0x10ULL) % 0x10]; }

  *p++ = HEX[attr % 0x10];
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to hex string from uint32
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt32Hex (uint32_t attr) {
  char buffer[9];
  size_t len;
  
  len = TRI_StringUInt32HexInPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to hex string from uint64
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt64Hex (uint64_t attr) {
  char buffer[17];
  size_t len;
  
  len = TRI_StringUInt64HexInPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief convert to octal string from uint32, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringUInt32OctalInPlace (uint32_t attr, char* buffer) {
  char* p;
  
  p = buffer;
  
  if (010000000000UL <= attr) { *p++ = (char) ((attr / 010000000000UL) % 010 + '0'); }
  if ( 01000000000UL <= attr) { *p++ = (char) ((attr /  01000000000UL) % 010 + '0'); }
  if (  0100000000UL <= attr) { *p++ = (char) ((attr /   0100000000UL) % 010 + '0'); }
  if (   010000000UL <= attr) { *p++ = (char) ((attr /    010000000UL) % 010 + '0'); }
  if (    01000000UL <= attr) { *p++ = (char) ((attr /     01000000UL) % 010 + '0'); }
  if (     0100000UL <= attr) { *p++ = (char) ((attr /      0100000UL) % 010 + '0'); }
  if (      010000UL <= attr) { *p++ = (char) ((attr /       010000UL) % 010 + '0'); }
  if (       01000UL <= attr) { *p++ = (char) ((attr /        01000UL) % 010 + '0'); }
  if (        0100UL <= attr) { *p++ = (char) ((attr /         0100UL) % 010 + '0'); }
  if (         010UL <= attr) { *p++ = (char) ((attr /          010UL) % 010 + '0'); }

  *p++ = (char) (attr % 010 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to octal string from uint64, using the specified buffer.
/// A NUL-byte will be appended at the end.
/// It is the caller's responsibility to ensure the buffer is big enough to
/// contain the result string and the NUL byte. 
/// The length of the string number in characters without the NUL byte is 
/// returned.
////////////////////////////////////////////////////////////////////////////////

size_t TRI_StringUInt64OctalInPlace (uint64_t attr, char* buffer) {
  char* p;
  
  p = buffer;
  if (01000000000000000000000ULL <= attr) { *p++ = (char) ((attr / 01000000000000000000000ULL) % 010 + '0'); }
  if ( 0100000000000000000000ULL <= attr) { *p++ = (char) ((attr /  0100000000000000000000ULL) % 010 + '0'); }
  if (  010000000000000000000ULL <= attr) { *p++ = (char) ((attr /   010000000000000000000ULL) % 010 + '0'); }
  if (   01000000000000000000ULL <= attr) { *p++ = (char) ((attr /    01000000000000000000ULL) % 010 + '0'); }
  if (    0100000000000000000ULL <= attr) { *p++ = (char) ((attr /     0100000000000000000ULL) % 010 + '0'); }
  if (     010000000000000000ULL <= attr) { *p++ = (char) ((attr /      010000000000000000ULL) % 010 + '0'); }
  if (      01000000000000000ULL <= attr) { *p++ = (char) ((attr /       01000000000000000ULL) % 010 + '0'); }
  if (       0100000000000000ULL <= attr) { *p++ = (char) ((attr /        0100000000000000ULL) % 010 + '0'); }
  if (        010000000000000ULL <= attr) { *p++ = (char) ((attr /         010000000000000ULL) % 010 + '0'); }
  if (         01000000000000ULL <= attr) { *p++ = (char) ((attr /          01000000000000ULL) % 010 + '0'); }
  if (          0100000000000ULL <= attr) { *p++ = (char) ((attr /           0100000000000ULL) % 010 + '0'); }
  if (           010000000000ULL <= attr) { *p++ = (char) ((attr /            010000000000ULL) % 010 + '0'); }
  if (            01000000000ULL <= attr) { *p++ = (char) ((attr /             01000000000ULL) % 010 + '0'); }
  if (             0100000000ULL <= attr) { *p++ = (char) ((attr /              0100000000ULL) % 010 + '0'); }
  if (              010000000ULL <= attr) { *p++ = (char) ((attr /               010000000ULL) % 010 + '0'); }
  if (               01000000ULL <= attr) { *p++ = (char) ((attr /                01000000ULL) % 010 + '0'); }
  if (                0100000ULL <= attr) { *p++ = (char) ((attr /                 0100000ULL) % 010 + '0'); }
  if (                 010000ULL <= attr) { *p++ = (char) ((attr /                  010000ULL) % 010 + '0'); }
  if (                  01000ULL <= attr) { *p++ = (char) ((attr /                   01000ULL) % 010 + '0'); }
  if (                   0100ULL <= attr) { *p++ = (char) ((attr /                    0100ULL) % 010 + '0'); }
  if (                    010ULL <= attr) { *p++ = (char) ((attr /                     010ULL) % 010 + '0'); }

  *p++ = (char) (attr % 010 + '0');
  *p = '\0';

  return (p - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to octal string from uint32
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt32Octal (uint32_t attr) {
  char buffer[9];
  size_t len;
  
  len = TRI_StringUInt32OctalInPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to octal string from uint64
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt64Octal (uint64_t attr) {
  char buffer[17];
  size_t len;
  
  len = TRI_StringUInt64OctalInPlace(attr, (char*) &buffer);

  return TRI_DuplicateString2(buffer, len);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
