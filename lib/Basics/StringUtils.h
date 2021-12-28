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
/// @author Dr. Oreste Costa-Panaia
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "Basics/Common.h"
#include "Basics/debugging.h"

namespace arangodb {
namespace basics {

static constexpr size_t maxUInt64StringSize = 21;

/// @brief collection of string utility functions
///
/// This namespace holds function used for string manipulation.
namespace StringUtils {

// -----------------------------------------------------------------------------
// STRING CONVERSION
// -----------------------------------------------------------------------------

/// @brief escape unicode
///
/// This method escapes a unicode character string by replacing the unicode
/// characters by a \\uXXXX sequence.
std::string escapeUnicode(std::string_view value, bool escapeSlash = true);

/// @brief splits a string
std::vector<std::string> split(std::string_view source, char delim = ',');

/// @brief splits a string
std::vector<std::string> split(std::string_view source, std::string_view delim);

/// @brief joins a string
template<typename C>
std::string join(C const& source, std::string_view delim) {
  std::string result;
  bool first = true;

  for (auto const& c : source) {
    if (first) {
      first = false;
    } else {
      result.append(delim);
    }

    result += c;
  }

  return result;
}

template<typename C>
std::string join(C const& source, char delim = ',') {
  std::string result;
  bool first = true;

  for (auto const& c : source) {
    if (first) {
      first = false;
    } else {
      result.push_back(delim);
    }

    result += c;
  }

  return result;
}

/// @brief joins a string
template<typename C, typename T>
std::string join(C const& source, std::string_view delim,
                 std::function<std::string(T)> const& cb) {
  std::string result;
  bool first = true;

  for (auto const& c : source) {
    if (first) {
      first = false;
    } else {
      result.append(delim);
    }

    result += cb(c);
  }

  return result;
}

/// @brief removes leading and trailing whitespace
std::string trim(std::string_view sourceStr,
                 std::string_view trimStr = " \t\n\r");

/// @brief removes leading and trailing whitespace in place
void trimInPlace(std::string& str, std::string_view trimStr = " \t\n\r");

/// @brief removes leading whitespace
std::string lTrim(std::string_view sourceStr,
                  std::string_view trimStr = " \t\n\r");

/// @brief removes trailing whitespace
std::string rTrim(std::string_view sourceStr,
                  std::string_view trimStr = " \t\n\r");

void rTrimInPlace(std::string& str, std::string_view trimStr = " \t\n\r");

/// @brief fills string from left
std::string lFill(std::string_view sourceStr, size_t size, char fill = ' ');

/// @brief fills string from right
std::string rFill(std::string_view sourceStr, size_t size, char fill = ' ');

/// @brief wrap longs lines
std::vector<std::string> wrap(std::string_view sourceStr, size_t size,
                              std::string_view breaks = " ");

/// @brief substring replace
std::string replace(std::string_view sourceStr, std::string_view fromString,
                    std::string_view toString);

static inline char tolower(char c) noexcept {
  return c + ((static_cast<unsigned char>(c - 65) < 26U) << 5);
}

static inline unsigned char tolower(unsigned char c) noexcept {
  return static_cast<unsigned char>(c + ((c - 65U < 26U) << 5));
}

static inline char toupper(char c) noexcept {
  return c - ((static_cast<unsigned char>(c - 97) < 26U) << 5);
}

static inline unsigned char toupper(unsigned char c) noexcept {
  return c - ((c - 97U < 26U) << 5);
}

/// @brief converts string to lower case - locale-independent, ASCII inputs
/// only!
void tolower(std::string_view str, std::string& result);

/// @brief converts string to lower case - locale-independent, ASCII inputs
/// only!
std::string tolower(std::string_view str);

/// @brief converts string to lower case in place - locale-independent, ASCII
/// inputs only!
void tolowerInPlace(std::string& str);

/// @brief converts string to upper case - locale-independent, ASCII inputs
/// only!
void toupper(std::string_view str, std::string& result);

/// @brief converts string to upper case - locale-independent, ASCII inputs
/// only!
std::string toupper(std::string_view str);

/// @brief converts string to upper case in place - locale-independent, ASCII
/// inputs only!
void toupperInPlace(std::string& str);

/// @brief case insensitive string comparison. locale-independent, ASCII inputs
/// only!
template<typename T1, typename T2>
[[nodiscard]] bool equalStringsCaseInsensitive(T1 const& lhs,
                                               T2 const& rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  size_t remain = lhs.size();
  typename T1::value_type const* l = lhs.data();
  typename T2::value_type const* r = rhs.data();
  int result = 0;

  while (remain > 0) {
    // hand-unrolled version, comparing 4 bytes in each loop iteration
    size_t len = std::min(remain, size_t(4));
    switch (len) {
      case 4:
        result += (tolower(l + 3) != tolower(r + 3));
        [[fallthrough]];
      case 3:
        result += (tolower(l + 2) != tolower(r + 2));
        [[fallthrough]];
      case 2:
        result += (tolower(l + 1) != tolower(r + 1));
        [[fallthrough]];
      case 1:
        result += (tolower(l + 0) != tolower(r + 0));
    }

    l += len;
    r += len;
    remain -= len;

    if (result != 0) {
      break;
    }
  }
  return (result == 0);
}

/// @brief checks for a prefix
bool isPrefix(std::string_view str, std::string_view prefix);

/// @brief checks for a suffix
bool isSuffix(std::string_view str, std::string_view postfix);

/// @brief url decodes the string
std::string urlDecodePath(std::string_view str);
std::string urlDecode(std::string_view str);

/// @brief url encodes the string
std::string urlEncode(char const* src, size_t len);
std::string urlEncode(std::string_view value);

/// @brief url encodes the string into the result buffer
void encodeURIComponent(std::string& result, char const* src, size_t len);

/// @brief uri encodes the component string
std::string encodeURIComponent(char const* src, size_t len);
std::string encodeURIComponent(std::string_view value);

/// @brief converts input string to soundex code
std::string soundex(char const* src, size_t len);
std::string soundex(std::string_view value);

/// @brief converts input string to vector of character codes
std::vector<uint32_t> characterCodes(char const* s, size_t length);

/// @brief calculates the levenshtein distance between the input strings
unsigned int levenshteinDistance(char const* s1, size_t l1, char const* s2,
                                 size_t l2);

/// @brief calculates the levenshtein distance between the input strings
size_t levenshteinDistance(std::vector<uint32_t> vect1,
                           std::vector<uint32_t> vect2);

// -----------------------------------------------------------------------------
// CONVERT TO STRING
// -----------------------------------------------------------------------------

/// @brief converts integer to string
std::string itoa(int16_t i);

/// @brief converts unsigned integer to string
std::string itoa(uint16_t i);

/// @brief converts integer to string
std::string itoa(int64_t i);

/// @brief converts unsigned integer to string
std::string itoa(uint64_t i);

/// @brief converts unsigned integer to string
size_t itoa(uint64_t i, char* result);

void itoa(uint64_t i, std::string& result);

/// @brief converts integer to string
std::string itoa(int32_t i);

/// @brief converts unsigned integer to string
std::string itoa(uint32_t i);

/// @brief converts size_t to string
#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

#if TRI_SIZEOF_SIZE_T == 4

inline std::string itoa(size_t i) { return itoa(uint32_t(i)); }

#elif TRI_SIZEOF_SIZE_T == 8

inline std::string itoa(size_t i) { return itoa(uint64_t(i)); }

#endif

#endif

/// @brief converts decimal to string
std::string ftoa(double i);

// -----------------------------------------------------------------------------
// CONVERT FROM STRING
// -----------------------------------------------------------------------------

/// @brief converts a single hex to integer
inline int hex2int(char ch, int errorValue = 0) {
  if ('0' <= ch && ch <= '9') {
    return ch - '0';
  } else if ('A' <= ch && ch <= 'F') {
    return ch - 'A' + 10;
  } else if ('a' <= ch && ch <= 'f') {
    return ch - 'a' + 10;
  }

  return errorValue;
}

/// @brief parses a boolean
bool boolean(std::string_view str);

/// @brief parses an integer
int64_t int64(char const* value, size_t size) noexcept;
int64_t int64(std::string_view value) noexcept;

/// @brief parses an unsigned integer
uint64_t uint64(char const* value, size_t size) noexcept;
uint64_t uint64(std::string_view value) noexcept;

/// @brief parses an unsigned integer
/// the caller must make sure that the input buffer only contains valid
/// numeric characters - otherwise the uint64_t result will be wrong.
/// because the input is restricted to some valid characters, this function
/// is highly optimized
uint64_t uint64_trusted(char const* value, size_t length) noexcept;
uint64_t uint64_trusted(std::string_view value) noexcept;

/// @brief parses an integer
int32_t int32(char const* value, size_t size) noexcept;
int32_t int32(std::string_view value) noexcept;

/// @brief parses an unsigned integer
uint32_t uint32(char const* value, size_t size) noexcept;
uint32_t uint32(std::string_view value) noexcept;

/// @brief parses a decimal
double doubleDecimal(char const* value, size_t size);
double doubleDecimal(std::string_view value);

/// @brief parses a decimal
float floatDecimal(char const* value, size_t size);
float floatDecimal(std::string_view value);

/// @brief convert char const* or std::string to number with error handling
template<typename T>
static bool toNumber(std::string const& key, T& val) noexcept {
  size_t n = key.size();
  if (n == 0) {
    return false;
  }
  try {
    if constexpr (std::is_integral<T>::value) {
      char const* s = key.data();
      // TODO: no error checking missing here
      std::from_chars(s, s + n, val);
    } else if constexpr (std::is_same<long double, typename std::remove_cv<
                                                       T>::type>::value) {
      // TODO: move this to std::from_chars(s, s + n, val). g++ only supports
      // from_chars for the long double type from g++11 onwards, although it is
      // a c++17 feature
      val = stold(key);
    } else if constexpr (std::is_same<
                             double, typename std::remove_cv<T>::type>::value) {
      // TODO: move this to std::from_chars(s, s + n, val). g++ only supports
      // from_chars for the double type from g++11 onwards, although it is
      // a c++17 feature
      val = stod(key);
    } else if constexpr (std::is_same<
                             float, typename std::remove_cv<T>::type>::value) {
      // TODO: move this to std::from_chars(s, s + n, val). g++ only supports
      // from_chars for the float type from g++11 onwards, although it is
      // a c++17 feature
      val = stof(key);
    }
  } catch (...) {
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
// BASE64
// -----------------------------------------------------------------------------

/// @brief converts to base64
std::string encodeBase64(char const* value, size_t length);
std::string encodeBase64(std::string_view value);

/// @brief converts from base64
std::string decodeBase64(std::string_view);

/// @brief converts to base64, URL friendly
///
/// '-' and '_' are used instead of '+' and '/'
std::string encodeBase64U(std::string_view);

/// @brief converts from base64, URL friendly
///
/// '-' and '_' are used instead of '+' and '/'
std::string decodeBase64U(std::string_view);

// -----------------------------------------------------------------------------
// ADDITIONAL STRING UTILITIES
// -----------------------------------------------------------------------------

/// @brief replaces incorrect path delimiter character for window and linux
std::string correctPath(std::string_view incorrectPath);

/// @brief converts to hex
std::string encodeHex(char const* value, size_t length);
std::string encodeHex(std::string_view value);

/// @brief converts from hex
/// any invalid character in the input sequence will make the function return
/// an empty string
std::string decodeHex(char const* value, size_t length);
std::string decodeHex(std::string_view value);

void escapeRegexParams(std::string& out, const char* ptr, size_t length);
std::string escapeRegexParams(std::string_view in);

/// @brief returns a human-readable size string, e.g.
/// - 0 => "0 bytes"
/// - 1 => "1 byte"
/// - 255 => "255 bytes"
/// - 2048 => "2.0 KB"
/// - 1048576 => "1.0 MB"
/// ...
std::string formatSize(uint64_t value);

namespace detail {
template<typename T>
auto constexpr isStringOrView =
    std::is_same_v<std::string, std::decay_t<T>> ||
    std::is_same_v<std::string_view, std::decay_t<T>>;

template<typename T>
auto toStringOrView(T&& arg) {
  using Arg = std::decay_t<T>;
  if constexpr (std::is_same_v<std::string, Arg> ||
                std::is_same_v<std::string_view, Arg>) {
    return arg;
  } else if constexpr (std::is_convertible_v<Arg, std::string_view>) {
    return std::string_view(arg);
  } else if constexpr (std::is_convertible_v<Arg, std::string>) {
    return std::string(arg);
  } else {
    // Use using, so ADL could also find to_string in other namespaces than std.
    using std::to_string;
    return to_string(arg);
  }
}

template<typename... Iters>
auto concatImplIter(std::pair<Iters, Iters>&&... iters) -> std::string {
  auto result = std::string{};

  auto const newcap = static_cast<std::size_t>(
      (std::distance(iters.first, iters.second) + ... + 0));
  result.reserve(newcap);

  ([&] { result.append(iters.first, iters.second); }(), ...);

  TRI_ASSERT(newcap == result.length());

  return result;
}

/// @brief Converts all arguments to a pair of iterators (begin, end), passing
/// them to concatImplIter.
/// All arguments must either be `std::string` or `std::string_view`.
template<typename... Args>
auto concatImplStr(Args&&... args) -> std::string {
  static_assert(((isStringOrView<Args>)&&...));
  return concatImplIter(std::make_pair(args.begin(), args.end())...);
}

template<typename Iter, typename... Iters>
auto joinImplIter(std::string_view delim, std::pair<Iter, Iter>&& head,
                  std::pair<Iters, Iters>&&... tail) -> std::string {
  auto result = std::string{};

  auto const valueSizes = std::distance(head.first, head.second) +
                          (std::distance(tail.first, tail.second) + ... + 0);
  auto const delimSizes = sizeof...(Iters) * delim.size();
  auto const newcap = valueSizes + delimSizes;
  result.reserve(newcap);

  result.append(head.first, head.second);

  (
      [&] {
        result.append(delim);
        result.append(tail.first, tail.second);
      }(),
      ...);

  TRI_ASSERT(newcap == result.length());

  return result;
}

/// @brief Converts all arguments to a pair of iterators (begin, end), passing
/// them to joinImplIter.
/// All arguments must either be `std::string` or `std::string_view`.
template<typename... Args>
auto joinImplStr(std::string_view delim, Args&&... args) -> std::string {
  static_assert(((isStringOrView<Args>)&&...));
  if constexpr (sizeof...(Args) == 0) {
    return std::string{};
  } else {
    return joinImplIter(delim, std::make_pair(args.begin(), args.end())...);
  }
}
}  // namespace detail

/// @brief Creates a string concatenation of all its arguments.
/// Arguments that aren't either a std::string, std::string_view,
/// are converted to a string first, either directly if they're convertible,
/// or via `to_string`.
template<typename... Args>
auto concatT(Args&&... args) -> std::string {
  return detail::concatImplStr(detail::toStringOrView(args)...);
}

/// @brief Creates a string, joining all of its arguments delimited by delim.
/// Arguments that aren't either a std::string, std::string_view,
/// are converted to a string first, either directly if they're convertible,
/// or via `to_string`.
template<typename... Args>
auto joinT(std::string_view delim, Args&&... args) -> std::string {
  return detail::joinImplStr(delim, detail::toStringOrView(args)...);
}

}  // namespace StringUtils
}  // namespace basics
}  // namespace arangodb
