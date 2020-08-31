////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_NUMBER_UTILS_H
#define ARANGODB_BASICS_NUMBER_UTILS_H 1

#include "Basics/Common.h"
#include "Basics/system-compiler.h"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace arangodb {
namespace NumberUtils {

// low-level worker function to convert the string value between p
// (inclusive) and e (exclusive) into a negative number value of type T,
// without validation of the input string - use this only for trusted input!
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'.
// there is no validation of the input string, and overflow or underflow
// of the result value will not be detected.
// this function will not modify errno.
template <typename T>
inline T atoi_negative_unchecked(char const* p, char const* e) noexcept {
  T result = 0;
  while (p != e) {
    result = (result * 10) - (*(p++) - '0');
  }
  return result;
}

// low-level worker function to convert the string value between p
// (inclusive) and e (exclusive) into a positive number value of type T,
// without validation of the input string - use this only for trusted input!
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'.
// there is no validation of the input string, and overflow or underflow
// of the result value will not be detected.
// this function will not modify errno.
template <typename T>
inline T atoi_positive_unchecked(char const* p, char const* e) noexcept {
  T result = 0;
  while (p != e) {
    result = (result * 10) + *(p++) - '0';
  }

  return result;
}

// function to convert the string value between p
// (inclusive) and e (exclusive) into a number value of type T, without
// validation of the input string - use this only for trusted input!
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'. an
// optional '+' or '-' sign is allowed too.
// there is no validation of the input string, and overflow or underflow
// of the result value will not be detected.
// this function will not modify errno.
template <typename T>
inline T atoi_unchecked(char const* p, char const* e) noexcept {
  if (ADB_UNLIKELY(p == e)) {
    return T();
  }

  if (*p == '-') {
    if (!std::is_signed<T>::value) {
      return T();
    }
    return atoi_negative_unchecked<T>(++p, e);
  }
  if (ADB_UNLIKELY(*p == '+')) {
    ++p;
  }

  return atoi_positive_unchecked<T>(p, e);
}

// low-level worker function to convert the string value between p
// (inclusive) and e (exclusive) into a negative number value of type T
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'.
// if any other character is found, the output parameter "valid" will
// be set to false. if the parsed value is less than what type T can
// store without truncation, "valid" will also be set to false.
// this function will not modify errno.
template <typename T>
inline T atoi_negative(char const* p, char const* e, bool& valid) noexcept {
  if (ADB_UNLIKELY(p == e)) {
    valid = false;
    return T();
  }

  constexpr T cutoff = (std::numeric_limits<T>::min)() / 10;
  constexpr char cutlim = -((std::numeric_limits<T>::min)() % 10);
  T result = 0;

  do {
    char c = *p;
    // we expect only '0' to '9'. everything else is unexpected
    if (ADB_UNLIKELY(c < '0' || c > '9')) {
      valid = false;
      return result;
    }

    c -= '0';
    // we expect the bulk of values to not hit the bounds restrictions
    if (ADB_UNLIKELY(result < cutoff || (result == cutoff && c > cutlim))) {
      valid = false;
      return result;
    }
    result *= 10;
    result -= c;
  } while (++p < e);

  valid = true;
  return result;
}

// low-level worker function to convert the string value between p
// (inclusive) and e (exclusive) into a positive number value of type T
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'.
// if any other character is found, the output parameter "valid" will
// be set to false. if the parsed value is greater than what type T can
// store without truncation, "valid" will also be set to false.
// this function will not modify errno.
template <typename T>
inline T atoi_positive(char const* p, char const* e, bool& valid) noexcept {
  if (ADB_UNLIKELY(p == e)) {
    valid = false;
    return T();
  }

  constexpr T cutoff = (std::numeric_limits<T>::max)() / 10;
  constexpr char cutlim = (std::numeric_limits<T>::max)() % 10;
  T result = 0;

  do {
    char c = *p;

    // we expect only '0' to '9'. everything else is unexpected
    if (ADB_UNLIKELY(c < '0' || c > '9')) {
      valid = false;
      return result;
    }

    c -= '0';
    // we expect the bulk of values to not hit the bounds restrictions
    if (ADB_UNLIKELY(result > cutoff || (result == cutoff && c > cutlim))) {
      valid = false;
      return result;
    }
    result *= 10;
    result += static_cast<T>(c);
  } while (++p < e);

  valid = true;
  return result;
}

// function to convert the string value between p
// (inclusive) and e (exclusive) into a number value of type T
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'. an
// optional '+' or '-' sign is allowed too.
// if any other character is found, the output parameter "valid" will
// be set to false. if the parsed value is less or greater than what
// type T can store without truncation, "valid" will also be set to
// false.
// this function will not modify errno.
template <typename T>
inline typename std::enable_if<std::is_signed<T>::value, T>::type atoi(char const* p,
                                                                       char const* e,
                                                                       bool& valid) noexcept {
  if (ADB_UNLIKELY(p == e)) {
    valid = false;
    return T();
  }

  if (*p == '-') {
    return atoi_negative<T>(++p, e, valid);
  }
  if (ADB_UNLIKELY(*p == '+')) {
    ++p;
  }

  return atoi_positive<T>(p, e, valid);
}

template <typename T>
inline typename std::enable_if<std::is_unsigned<T>::value, T>::type atoi(
    char const* p, char const* e, bool& valid) noexcept {
  if (ADB_UNLIKELY(p == e)) {
    valid = false;
    return T();
  }

  if (*p == '-') {
    valid = false;
    return T();
  }
  if (ADB_UNLIKELY(*p == '+')) {
    ++p;
  }

  return atoi_positive<T>(p, e, valid);
}

// function to convert the string value between p
// (inclusive) and e (exclusive) into a number value of type T
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'. an
// optional '+' or '-' sign is allowed too.
// if any other character is found, the result will be set to 0.
// if the parsed value is less or greater than what type T can store
// without truncation, return result will also be set to 0.
// this function will not modify errno.
template <typename T>
inline T atoi_zero(char const* p, char const* e) noexcept {
  bool valid;
  T result = atoi<T>(p, e, valid);
  return valid ? result : 0;
}

template <typename T>
constexpr std::enable_if_t<std::is_integral<T>::value, bool> isPowerOf2(T n) {
  return n > 0 && (n & (n - 1)) == 0;
}

/// @brief calculate the integer log2 value for the given input
/// the result is undefined when calling this with a value of 0!
uint32_t log2(uint32_t value) noexcept;

}  // namespace NumberUtils
}  // namespace arangodb

#endif
