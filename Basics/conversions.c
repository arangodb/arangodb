////////////////////////////////////////////////////////////////////////////////
/// @brief conversion functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "conversions.h"

#include <limits.h>

#include <Basics/strings.h>
#include <Basics/string-buffer.h>

// -----------------------------------------------------------------------------
// --SECTION--                             public functions for string to number
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Conversions
/// @{
////////////////////////////////////////////////////////////////////////////////

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

  if (errno == ERANGE && (result == HUGE_VAL || result == -HUGE_VAL || result == 0)) {
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

#ifdef TRI_HAVE_STRTOL_R
  struct reent buffer;
#else
#ifdef TRI_HAVE__STRTOL_R
  struct reent buffer;
#endif
#endif

  TRI_set_errno(TRI_ERROR_NO_ERROR);

#ifdef TRI_HAVE_STRTOL_R
  result = strtol_r(&buffer, str, &endptr, 10);
#else
#ifdef TRI_HAVE__STRTOL_R
  result = _strtol_r(&buffer, str, &endptr, 10);
#else
  result = strtol(str, &endptr, 10);
#endif
#endif

  while (isspace(*endptr)) {
    ++endptr;
  }

  if (*endptr != '\0') {
    TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }

  if (errno == ERANGE && (result == INT32_MIN || result == INT32_MAX)) {
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

#ifdef TRI_HAVE_STRTOUL_R
  struct reent buffer;
#else
#ifdef TRI_HAVE__STRTOUL_R
  struct reent buffer;
#endif
#endif

  TRI_set_errno(TRI_ERROR_NO_ERROR);

#ifdef TRI_HAVE_STRTOUL_R
  result = strtoul_r(&buffer, str, &endptr, 10);
#else
#ifdef TRI_HAVE__STRTOUL_R
  result = _strtoul_r(&buffer, str, &endptr, 10);
#else
  result = strtoul(str, &endptr, 10);
#endif
#endif

  while (isspace(*endptr)) {
    ++endptr;
  }

  if (*endptr != '\0') {
    TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }

  if (errno == ERANGE && (result == 0 || result == UINT32_MAX)) {
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

#ifdef TRI_HAVE_STRTOLL_R
  struct reent buffer;
#else
#ifdef TRI_HAVE__STRTOLL_R
  struct reent buffer;
#endif
#endif

  TRI_set_errno(TRI_ERROR_NO_ERROR);

#ifdef TRI_HAVE_STRTOLL_R
  result = strtoll_r(&buffer, str, &endptr, 10);
#else
#ifdef TRI_HAVE__STRTOLL_R
  result = _strtoll_r(&buffer, str, &endptr, 10);
#else
  result = strtoll(str, &endptr, 10);
#endif
#endif

  while (isspace(*endptr)) {
    ++endptr;
  }

  if (*endptr != '\0') {
    TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }

  if (errno == ERANGE && (result == INT64_MIN || result == INT64_MAX)) {
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

#ifdef TRI_HAVE_STRTOULL_R
  struct reent buffer;
#else
#ifdef TRI_HAVE__STRTOULL_R
  struct reent buffer;
#endif
#endif

  TRI_set_errno(TRI_ERROR_NO_ERROR);

#ifdef TRI_HAVE_STRTOULL_R
  result = strtoull_r(&buffer, str, &endptr, 10);
#else
#ifdef TRI_HAVE__STRTOULL_R
  result = _strtoull_r(&buffer, str, &endptr, 10);
#else
  result = strtoull(str, &endptr, 10);
#endif
#endif

  while (isspace(*endptr)) {
    ++endptr;
  }

  if (*endptr != '\0') {
    TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }

  if (errno == ERANGE && (result == 0 || result == UINT64_MAX)) {
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
/// @brief convert to string from int32
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringInt32 (int32_t attr) {
  char buffer[12];
  char* p;

  if (attr == INT32_MIN) {
    return TRI_DuplicateString("-2147483648");
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

  return TRI_DuplicateString(buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint32
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt32 (uint32_t attr) {
  char buffer[11];
  char* p = buffer;

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

  return TRI_DuplicateString(buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from int64
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringInt64 (int64_t attr) {
  char buffer[21];
  char* p = buffer;

  if (attr == INT64_MIN) {
    return TRI_DuplicateString("-9223372036854775808");
  }

  if (attr < 0) {
    *p++ = '-';
    attr = -attr;
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

  return TRI_DuplicateString(buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from uint64
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringUInt64 (uint64_t attr) {
  char buffer[21];
  char* p = buffer;

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

  return TRI_DuplicateString(buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to string from double
////////////////////////////////////////////////////////////////////////////////

char* TRI_StringDouble (double value) {
  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer);
  TRI_AppendDoubleStringBuffer(&buffer, value);

  return buffer._buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
