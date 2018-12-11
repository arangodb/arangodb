////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_BASICS_STRING_UTILS_H
#define ARANGODB_BASICS_STRING_UTILS_H 1

#include "Basics/Common.h"

#include <vector>
#include <utility>

#if __cpp_lib_to_chars >= 201611
// use non-throwing, non-allocating std::from_chars etc. from standard library
#include <charconv>
#define TRI_STRING_UTILS_USE_FROM_CHARS 1
#else
// use own functionality
#undef TRI_STRING_UTILS_USE_FROM_CHARS 
#endif

namespace arangodb {
namespace basics {

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
std::string escapeUnicode(std::string const& name, bool escapeSlash = true);

/// @brief splits a string
std::vector<std::string> split(std::string const& source, char delim = ',',
                               char quote = '\\');

/// @brief splits a string
std::vector<std::string> split(std::string const& source,
                               std::string const& delim, char quote = '\\');

/// @brief joins a string
template <typename C>
std::string join(C const& source, std::string const& delim = ",") {
  std::string result;
  bool first = true;

  for (auto const& c : source) {
    if (first) {
      first = false;
    } else {
      result += delim;
    }

    result += c;
  }

  return result;
}

template <typename C>
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

/// @brief removes leading and trailing whitespace
std::string trim(std::string const& sourceStr,
                 std::string const& trimStr = " \t\n\r");

/// @brief removes leading and trailing whitespace in place
void trimInPlace(std::string& str, std::string const& trimStr = " \t\n\r");

/// @brief removes leading whitespace
std::string lTrim(std::string const& sourceStr,
                  std::string const& trimStr = " \t\n\r");

/// @brief removes trailing whitespace
std::string rTrim(std::string const& sourceStr,
                  std::string const& trimStr = " \t\n\r");

void rTrimInPlace(std::string& str, std::string const& trimStr = " \t\n\r");

/// @brief fills string from left
std::string lFill(std::string const& sourceStr, size_t size, char fill = ' ');

/// @brief fills string from right
std::string rFill(std::string const& sourceStr, size_t size, char fill = ' ');

/// @brief wrap longs lines
std::vector<std::string> wrap(std::string const& sourceStr, size_t size,
                              std::string const& breaks = " ");

/// @brief substring replace
std::string replace(std::string const& sourceStr, std::string const& fromString,
                    std::string const& toString);

/// @brief converts string to lower case in place
void tolowerInPlace(std::string* str);

/// @brief converts string to lower case
std::string tolower(std::string&& str);
std::string tolower(std::string const& str);

/// @brief converts string to upper case in place
void toupperInPlace(std::string* str);

/// @brief converts string to upper case
std::string toupper(std::string const& str);

/// @brief checks for a prefix
bool isPrefix(std::string const& str, std::string const& prefix);

/// @brief checks for a suffix
bool isSuffix(std::string const& str, std::string const& postfix);

/// @brief url decodes the string
std::string urlDecodePath(std::string const& str);
std::string urlDecode(std::string const& str);

/// @brief url encodes the string
std::string urlEncode(char const* src);

/// @brief url encodes the string
std::string urlEncode(char const* src, size_t const len);
    
/// @brief uri encodes the component string
std::string encodeURIComponent(std:: string const& str);
    
/// @brief uri encodes the component string
std::string encodeURIComponent(char const* src, size_t const len);
  
/// @brief converts input string to soundex code
std::string soundex(std::string const& str);
   
/// @brief converts input string to soundex code
std::string soundex(char const* src, size_t const len);
   
/// @brief converts input string to vector of character codes
std::vector<uint32_t> characterCodes(std::string const& str);
    
/// @brief calculates the levenshtein distance between the input strings
unsigned int levenshteinDistance(std::string const& str1, std::string const& str2);
  
/// @brief calculates the levenshtein distance between the input strings
size_t levenshteinDistance(std::vector<uint32_t> vect1, std::vector<uint32_t> vect2);

/// @brief url encodes the string
std::string urlEncode(std::string const& str);

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
bool boolean(std::string const& str);

/// @brief parses an integer
#ifdef TRI_STRING_UTILS_USE_FROM_CHARS
// use functionality provided by c++17
inline int64_t int64(char const* value, size_t size) noexcept {
  int64_t result = 0;
  std::from_chars(value, value + size, result, 10);
  return result;
}
inline int64_t int64(std::string const& value) noexcept {
  return StringUtils::int64(value.data(), value.size());
}
#else
int64_t int64(std::string const& value);
inline int64_t int64(char const* value, size_t size) {
  return StringUtils::int64(std::string(value, size));
}
#endif

/// @brief parses an unsigned integer
#ifdef TRI_STRING_UTILS_USE_FROM_CHARS
// use functionality provided by c++17
inline uint64_t uint64(char const* value, size_t size) noexcept {
  uint64_t result = 0;
  std::from_chars(value, value + size, result, 10);
  return result;
}
inline uint64_t uint64(std::string const& value) noexcept {
  return StringUtils::uint64(value.data(), value.size());
}
#else
uint64_t uint64(std::string const& value);
inline uint64_t uint64(char const* value, size_t size) {
  return StringUtils::uint64(std::string(value, size));
}
#endif

/// @brief parses an unsigned integer
/// the caller must make sure that the input buffer only contains valid
/// numeric characters - otherwise the uint64_t result will be wrong.
/// because the input is restricted to some valid characters, this function
/// is highly optimized
uint64_t uint64_trusted(char const* value, size_t length);
inline uint64_t uint64_trusted(std::string const& value) {
  return uint64_trusted(value.data(), value.size());
}

/// @brief parses an integer
#ifdef TRI_STRING_UTILS_USE_FROM_CHARS
// use functionality provided by c++17
inline int32_t int32(char const* value, size_t size) noexcept {
  int32_t result = 0;
  std::from_chars(value, value + size, result, 10);
  return result;
}
inline int32_t int32(std::string const& value) noexcept {
  return StringUtils::int32(value.data(), value.size());
}
#else
int32_t int32(std::string const& value);
int32_t int32(char const* value, size_t size);
#endif

/// @brief parses an unsigned integer
#ifdef TRI_STRING_UTILS_USE_FROM_CHARS
// use functionality provided by c++17
inline uint32_t uint32(char const* value, size_t size) noexcept {
  uint32_t result = 0;
  std::from_chars(value, value + size, result, 10);
  return result;
}
inline uint32_t uint32(std::string const& value) noexcept {
  return StringUtils::uint32(value.data(), value.size());
}
#else
uint32_t uint32(std::string const& value);
uint32_t uint32(char const* value, size_t size);
#endif

/// @brief parses a decimal
double doubleDecimal(std::string const& str);

/// @brief parses a decimal
double doubleDecimal(char const* value, size_t size);

/// @brief parses a decimal
float floatDecimal(std::string const& str);

/// @brief parses a decimal
float floatDecimal(char const* value, size_t size);

// -----------------------------------------------------------------------------
// BASE64
// -----------------------------------------------------------------------------

/// @brief converts to base64
std::string encodeBase64(std::string const&);

/// @brief converts from base64
std::string decodeBase64(std::string const&);

/// @brief converts to base64, URL friendly
///
/// '-' and '_' are used instead of '+' and '/'
std::string encodeBase64U(std::string const&);

/// @brief converts from base64, URL friendly
///
/// '-' and '_' are used instead of '+' and '/'
std::string decodeBase64U(std::string const&);

// -----------------------------------------------------------------------------
// ADDITIONAL STRING UTILITIES
// -----------------------------------------------------------------------------

/// @brief replaces incorrect path delimiter character for window and linux
std::string correctPath(std::string const& incorrectPath);

/// @brief finds n.th entry
std::string entry(size_t const pos, std::string const& sourceStr,
                  std::string const& delimiter = ",");

/// @brief counts number of entires
size_t numEntries(std::string const& sourceStr,
                  std::string const& delimiter = ",");

/// @brief converts to hex
std::string encodeHex(char const* value, size_t length);
std::string encodeHex(std::string const& value);

/// @brief converts from hex
/// any invalid character in the input sequence will make the function return
/// an empty string
std::string decodeHex(char const* value, size_t length);
std::string decodeHex(std::string const& value);

bool gzipUncompress(char const* compressed, size_t compressedLength,
                    std::string& uncompressed);

bool gzipUncompress(std::string const& compressed, std::string& uncompressed);

bool gzipDeflate(char const* compressed, size_t compressedLength,
                 std::string& uncompressed);

bool gzipDeflate(std::string const& compressed, std::string& uncompressed);
}
}
}

#endif
