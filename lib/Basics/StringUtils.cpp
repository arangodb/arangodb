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
////////////////////////////////////////////////////////////////////////////////

#include "StringUtils.h"

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/fpconv.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

// -----------------------------------------------------------------------------
// helper functions
// -----------------------------------------------------------------------------

namespace {

static char const* hexValuesLower = "0123456789abcdef";
static char const* hexValuesUpper = "0123456789ABCDEF";

char soundexCode(char c) {
  switch (c) {
    case 'b':
    case 'f':
    case 'p':
    case 'v':
      return '1';
    case 'c':
    case 'g':
    case 'j':
    case 'k':
    case 'q':
    case 's':
    case 'x':
    case 'z':
      return '2';
    case 'd':
    case 't':
      return '3';
    case 'l':
      return '4';
    case 'm':
    case 'n':
      return '5';
    case 'r':
      return '6';
    default:
      return '\0';
  }
}

char const* const BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

char const* const BASE64U_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";

unsigned char const BASE64_REVS[256] = {
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  //   0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  //  16
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', 62,   '\0', '\0', '\0', 63,  //  32 ' ', '!'
    52,   53,   54,   55,   56,   57,   58,   59,
    60,   61,   '\0', '\0', '\0', '\0', '\0', '\0',  //  48 '0', '1'
    '\0', 0,    1,    2,    3,    4,    5,    6,
    7,    8,    9,    10,   11,   12,   13,   14,  //  64 '@', 'A'
    15,   16,   17,   18,   19,   20,   21,   22,
    23,   24,   25,   '\0', '\0', '\0', '\0', '\0',  //  80
    '\0', 26,   27,   28,   29,   30,   31,   32,
    33,   34,   35,   36,   37,   38,   39,   40,  //  96 '`', 'a'
    41,   42,   43,   44,   45,   46,   47,   48,
    49,   50,   51,   '\0', '\0', '\0', '\0', '\0',  // 112
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 128
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 144
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 160
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 176
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 192
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 208
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 224
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 240
};

unsigned char const BASE64U_REVS[256] = {
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  //   0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  //  16
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', 62,   '\0', '\0',  //  32 ' ', '!'
    52,   53,   54,   55,   56,   57,   58,   59,
    60,   61,   '\0', '\0', '\0', '\0', '\0', '\0',  //  48 '0', '1'
    '\0', 0,    1,    2,    3,    4,    5,    6,
    7,    8,    9,    10,   11,   12,   13,   14,  //  64 '@', 'A'
    15,   16,   17,   18,   19,   20,   21,   22,
    23,   24,   25,   '\0', '\0', '\0', '\0', 63,  //  80
    '\0', 26,   27,   28,   29,   30,   31,   32,
    33,   34,   35,   36,   37,   38,   39,   40,  //  96 '`', 'a'
    41,   42,   43,   44,   45,   46,   47,   48,
    49,   50,   51,   '\0', '\0', '\0', '\0', '\0',  // 112
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 128
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 144
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 160
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 176
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 192
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 208
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 224
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // 240
};

inline bool isBase64(unsigned char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         c == '+' || c == '/';
}

inline bool isBase64U(unsigned char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         c == '-' || c == '_';
}

unsigned char consume(char const*& s) {
  return *reinterpret_cast<unsigned char const*>(s++);
}

template <typename InputType>
inline bool isEqual(InputType const& c1, InputType const& c2) {
  return c1 == c2;
}

template <typename InputType, typename LengthType>
LengthType levenshtein(InputType const* lhs, InputType const* rhs,
                       LengthType lhsSize, LengthType rhsSize) {
  TRI_ASSERT(lhsSize >= rhsSize);

  std::vector<LengthType> costs;
  costs.resize(rhsSize + 1);

  for (LengthType i = 0; i < rhsSize; ++i) {
    costs[i] = i;
  }

  LengthType next = 0;

  for (LengthType i = 0; i < lhsSize; ++i) {
    LengthType current = i + 1;

    for (LengthType j = 0; j < rhsSize; ++j) {
      LengthType cost = !(::isEqual<InputType>(lhs[i], rhs[j]) ||
                          (i && j && ::isEqual<InputType>(lhs[i - 1], rhs[j]) &&
                           ::isEqual<InputType>(lhs[i], rhs[j - 1])));
      next = std::min(std::min(costs[j + 1] + 1, current + 1), costs[j] + cost);
      costs[j] = current;
      current = next;
    }
    costs[rhsSize] = next;
  }
  return next;
}

size_t levenshteinDistance(std::vector<uint32_t>& vect1, std::vector<uint32_t>& vect2) {
  if (vect1.empty() || vect2.empty()) {
    return vect1.size() ? vect1.size() : vect2.size();
  }

  if (vect1.size() < vect2.size()) {
    vect1.swap(vect2);
  }

  size_t lhsSize = vect1.size();
  size_t rhsSize = vect2.size();

  uint32_t const* l = vect1.data();
  uint32_t const* r = vect2.data();

  if (lhsSize < std::numeric_limits<uint8_t>::max()) {
    return static_cast<size_t>(
        ::levenshtein<uint32_t, uint8_t>(l, r, static_cast<uint8_t>(lhsSize),
                                         static_cast<uint8_t>(rhsSize)));
  } else if (lhsSize < std::numeric_limits<uint16_t>::max()) {
    return static_cast<size_t>(
        ::levenshtein<uint32_t, uint16_t>(l, r, static_cast<uint16_t>(lhsSize),
                                          static_cast<uint16_t>(rhsSize)));
  } else if (lhsSize < std::numeric_limits<uint32_t>::max()) {
    return static_cast<size_t>(
        ::levenshtein<uint32_t, uint32_t>(l, r, static_cast<uint32_t>(lhsSize),
                                          static_cast<uint32_t>(rhsSize)));
  }
  return static_cast<size_t>(
      ::levenshtein<uint32_t, uint64_t>(l, r, static_cast<uint64_t>(lhsSize),
                                        static_cast<uint64_t>(rhsSize)));
}

}  // namespace

namespace arangodb {
namespace basics {
namespace StringUtils {

// .............................................................................
// STRING CONVERSION
// .............................................................................

std::string escapeUnicode(std::string const& name, bool escapeSlash) {
  size_t len = name.length();

  if (len == 0) {
    return name;
  }

  // cppcheck-suppress unsignedPositive
  if (len >= (SIZE_MAX - 1) / 6) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  bool corrupted = false;

  auto buffer = std::make_unique<char[]>(6 * len + 1);
  char* qtr = buffer.get();
  char const* ptr = name.c_str();
  char const* end = ptr + len;

  for (; ptr < end; ++ptr, ++qtr) {
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

      default: {
        uint8_t c = (uint8_t)*ptr;

        // character is in the normal latin1 range
        if ((c & 0x80) == 0) {
          // special character, escape
          if (c < 32) {
            *qtr++ = '\\';
            *qtr++ = 'u';
            *qtr++ = '0';
            *qtr++ = '0';

            uint16_t i1 = (static_cast<uint16_t>(c) & 0xF0) >> 4;
            uint16_t i2 = (static_cast<uint16_t>(c) & 0x0F);

            *qtr++ = (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10);
            *qtr = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
          }

          // normal latin1
          else {
            *qtr = *ptr;
          }
        }

        // unicode range 0080 - 07ff
        else if ((c & 0xE0) == 0xC0) {
          if (ptr + 1 < end) {
            uint8_t d = (uint8_t) * (ptr + 1);

            // correct unicode
            if ((d & 0xC0) == 0x80) {
              ++ptr;

              *qtr++ = '\\';
              *qtr++ = 'u';

              uint16_t n = ((c & 0x1F) << 6) | (d & 0x3F);

              uint16_t i1 = (n & 0xF000) >> 12;
              uint16_t i2 = (n & 0x0F00) >> 8;
              uint16_t i3 = (n & 0x00F0) >> 4;
              uint16_t i4 = (n & 0x000F);

              *qtr++ = (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10);
              *qtr++ = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
              *qtr++ = (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10);
              *qtr = (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10);
            }

            // corrupted unicode
            else {
              *qtr = *ptr;
              corrupted = true;
            }
          }

          // corrupted unicode
          else {
            *qtr = *ptr;
            corrupted = true;
          }
        }

        // unicode range 0800 - ffff
        else if ((c & 0xF0) == 0xE0) {
          if (ptr + 1 < end) {
            uint8_t d = (uint8_t) * (ptr + 1);

            // correct unicode
            if ((d & 0xC0) == 0x80) {
              if (ptr + 2 < end) {
                uint8_t e = (uint8_t) * (ptr + 2);

                // correct unicode
                *qtr = *ptr;
                if ((e & 0xC0) != 0x80) {
                  corrupted = true;
                }
              }
              // corrupted unicode
              else {
                *qtr = *ptr;
                corrupted = true;
              }
            }
            // corrupted unicode
            else {
              *qtr = *ptr;
              corrupted = true;
            }
          }
          // corrupted unicode
          else {
            *qtr = *ptr;
            corrupted = true;
          }

        }

        // unicode range 010000 - 10ffff -- NOT IMPLEMENTED
        else {
          *qtr = *ptr;
        }
      }

      break;
    }
  }

  *qtr = '\0';

  std::string result(buffer.get(), qtr - buffer.get());

  if (corrupted) {
    LOG_TOPIC("4c231", DEBUG, arangodb::Logger::FIXME)
        << "escaped corrupted unicode string";
  }

  return result;
}

std::vector<std::string> split(std::string const& source, char delim) {
  std::vector<std::string> result;

  char const* q = source.data();
  char const* e = q + source.size();

  if (q != e) {
    char const* last = q;

    for (; q < e; ++q) {
      if (*q == delim) {
        result.emplace_back(last, q - last);
        last = q + 1;
      }
    }
    result.emplace_back(last, q - last);
  }

  return result;
}

std::vector<std::string> split(std::string const& source, std::string const& delim) {
  std::vector<std::string> result;

  char const* q = source.data();
  char const* e = source.data() + source.size();

  if (q != e) {
    char const* last = q;

    for (; q < e; ++q) {
      if (delim.find(*q) != std::string::npos) {
        result.emplace_back(last, q - last);
        last = q + 1;
      }
    }
    result.emplace_back(last, q - last);
  }

  return result;
}

std::string trim(std::string const& sourceStr, std::string const& trimStr) {
  size_t s = sourceStr.find_first_not_of(trimStr);

  if (s == std::string::npos) {
    return std::string();
  }
  size_t e = sourceStr.find_last_not_of(trimStr);
  return std::string(sourceStr, s, e - s + 1);
}

void trimInPlace(std::string& str, std::string const& trimStr) {
  size_t s = str.find_first_not_of(trimStr);
  size_t e = str.find_last_not_of(trimStr);

  if (s == std::string::npos) {
    str.clear();
  } else if (s == 0 && e == str.length() - 1) {
    // nothing to do
  } else if (s == 0) {
    str.erase(e + 1);
  } else {
    str = str.substr(s, e - s + 1);
  }
}

std::string lTrim(std::string const& str, std::string const& trimStr) {
  size_t s = str.find_first_not_of(trimStr);

  if (s == std::string::npos) {
    return std::string();
  } 
  return std::string(str, s);
}

std::string rTrim(std::string const& sourceStr, std::string const& trimStr) {
  size_t e = sourceStr.find_last_not_of(trimStr);

  return std::string(sourceStr, 0, e + 1);
}

void rTrimInPlace(std::string& str, std::string const& trimStr) {
  size_t e = str.find_last_not_of(trimStr);

  if (e + 1 < str.length()) {
    str.erase(e + 1);
  }
}

std::string lFill(std::string const& sourceStr, size_t size, char fill) {
  size_t l = sourceStr.size();

  if (l >= size) {
    return sourceStr;
  }

  return std::string(size - l, fill) + sourceStr;
}

std::string rFill(std::string const& sourceStr, size_t size, char fill) {
  size_t l = sourceStr.size();

  if (l >= size) {
    return sourceStr;
  }

  return sourceStr + std::string(size - l, fill);
}

std::vector<std::string> wrap(std::string const& sourceStr, size_t size,
                              std::string const& breaks) {
  std::vector<std::string> result;
  std::string next = sourceStr;

  if (size > 0) {
    while (next.size() > size) {
      size_t m = next.find_last_of(breaks, size - 1);

      if (m == std::string::npos || m < size / 2) {
        m = size;
      } else {
        m += 1;
      }

      result.push_back(next.substr(0, m));
      next = next.substr(m);
    }
  }

  result.push_back(next);

  return result;
}

/// replaces the contents of the sourceStr = "aaebbbbcce" where ever the
/// occurence of
/// fromStr = "bb" exists with the toStr = "dd". No recursion performed on the
/// replaced string
/// e.g. replace("aaebbbbcce","bb","dd") = "aaeddddcce"
/// e.g. replace("aaebbbbcce","bb","bbb") = "aaebbbbbbcce"
/// e.g. replace("aaebbbbcce","bbb","bb") = "aaebbbcce"

std::string replace(std::string const& sourceStr, std::string const& fromStr,
                    std::string const& toStr) {
  size_t fromLength = fromStr.length();
  size_t toLength = toStr.length();
  size_t sourceLength = sourceStr.length();

  // cannot perform a replace if the sourceStr = "" or fromStr = ""
  if (fromLength == 0 || sourceLength == 0) {
    return sourceStr;
  }

  // the max amount of memory is:
  size_t mt = (std::max)(static_cast<size_t>(1), toLength);

  // cppcheck-suppress unsignedPositive
  if ((sourceLength / fromLength) + 1 >= (SIZE_MAX - toLength) / mt) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  size_t maxLength = (((sourceLength / fromLength) + 1) * mt) + toLength;

  // the min amount of memory we have to allocate for the "replace" (new) string
  // is length of sourceStr
  maxLength = (std::max)(maxLength, sourceLength) + 1;

  auto result = std::make_unique<char[]>(maxLength);
  char* ptr = result.get();
  size_t k = 0;

  for (size_t j = 0; j < sourceLength; ++j) {
    bool match = true;

    for (size_t i = 0; i < fromLength; ++i) {
      if (sourceStr[j + i] != fromStr[i]) {
        match = false;
        break;
      }
    }

    if (!match) {
      ptr[k] = sourceStr[j];
      ++k;
      continue;
    }

    for (size_t i = 0; i < toLength; ++i) {
      ptr[k] = toStr[i];
      ++k;
    }

    j += (fromLength - 1);
  }

  return std::string(ptr, k);
}

void tolowerInPlace(std::string& str) {
  // unrolled version of
  // for (auto& c : str) {
  //   c = StringUtils::tolower(c);
  // }
  auto pos = str.data();
  auto end = pos + str.size();

  while (pos != end) {
    size_t len = end - pos;
    if (len > 4) {
      len = 4;
    }

    switch (len) {
      case 4:
        pos[3] = StringUtils::tolower(pos[3]);
        [[fallthrough]];
      case 3:
        pos[2] = StringUtils::tolower(pos[2]);
        [[fallthrough]];
      case 2:
        pos[1] = StringUtils::tolower(pos[1]);
        [[fallthrough]];
      case 1:
        pos[0] = StringUtils::tolower(pos[0]);
    }
    pos += len;
  }
}

std::string tolower(std::string&& str) {
  tolowerInPlace(str);
  return std::move(str);
}

std::string tolower(std::string const& str) {
  std::string result = str;
  tolowerInPlace(result);
  return result;
}

void toupperInPlace(std::string& str) {
  // unrolled version of
  // for (auto& c : str) {
  //   c = StringUtils::toupper(c);
  // }
  auto pos = str.data();
  auto end = pos + str.size();

  while (pos != end) {
    size_t len = end - pos;
    if (len > 4) {
      len = 4;
    }

    switch (len) {
      case 4:
        pos[3] = StringUtils::toupper(pos[3]);
        [[fallthrough]];
      case 3:
        pos[2] = StringUtils::toupper(pos[2]);
        [[fallthrough]];
      case 2:
        pos[1] = StringUtils::toupper(pos[1]);
        [[fallthrough]];
      case 1:
        pos[0] = StringUtils::toupper(pos[0]);
    }
    pos += len;
  }
}

std::string toupper(std::string&& str) {
  toupperInPlace(str);
  return std::move(str);
}

std::string toupper(std::string const& str) {
  std::string result;
  result.resize(str.size());

  size_t i = 0;
  for (auto& c : result) {
    c = StringUtils::toupper(str[i++]);
  }

  return result;
}

bool isPrefix(std::string const& str, std::string const& prefix) {
  if (prefix.length() > str.length()) {
    return false;
  } else if (prefix.length() == str.length()) {
    return str == prefix;
  } else {
    return str.compare(0, prefix.length(), prefix) == 0;
  }
}

bool isSuffix(std::string const& str, std::string const& postfix) {
  if (postfix.length() > str.length()) {
    return false;
  } else if (postfix.length() == str.length()) {
    return str == postfix;
  } else {
    return str.compare(str.size() - postfix.length(), postfix.length(), postfix) == 0;
  }
}

std::string urlDecodePath(std::string const& str) {
  std::string result;
  // reserve enough room so we do not need to re-alloc
  result.reserve(str.size() + 16);

  char const* src = str.c_str();
  char const* end = src + str.size();

  while (src < end) {
    if (*src == '%') {
      if (src + 2 < end) {
        int h1 = hex2int(src[1], -1);
        int h2 = hex2int(src[2], -1);

        if (h1 == -1) {
          ++src;
        } else {
          if (h2 == -1) {
            result.push_back(h1);
            src += 2;
          } else {
            result.push_back(h1 << 4 | h2);
            src += 3;
          }
        }
      } else if (src + 1 < end) {
        int h1 = hex2int(src[1], -1);

        if (h1 == -1) {
          ++src;
        } else {
          result.push_back(h1);
          src += 2;
        }
      } else {
        ++src;
      }
    } else {
      result.push_back(*src);
      ++src;
    }
  }

  return result;
}

std::string urlDecode(std::string const& str) {
  std::string result;
  // reserve enough room so we do not need to re-alloc
  result.reserve(str.size() + 16);

  char const* src = str.c_str();
  char const* end = src + str.size();

  for (; src < end && *src != '%'; ++src) {
    if (*src == '+') {
      result.push_back(' ');
    } else {
      result.push_back(*src);
    }
  }

  while (src < end) {
    if (src + 2 < end) {
      int h1 = hex2int(src[1], -1);
      int h2 = hex2int(src[2], -1);

      if (h1 == -1) {
        src += 1;
      } else {
        if (h2 == -1) {
          result.push_back(h1);
          src += 2;
        } else {
          result.push_back(h1 << 4 | h2);
          src += 3;
        }
      }
    } else if (src + 1 < end) {
      int h1 = hex2int(src[1], -1);

      if (h1 == -1) {
        src += 1;
      } else {
        result.push_back(h1);
        src += 2;
      }
    } else {
      src += 1;
    }

    for (; src < end && *src != '%'; ++src) {
      if (*src == '+') {
        result.push_back(' ');
      } else {
        result.push_back(*src);
      }
    }
  }

  return result;
}

std::string urlEncode(std::string const& str) {
  return urlEncode(str.c_str(), str.size());
}

std::string urlEncode(char const* src, size_t const len) {
  static char hexChars[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                              '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  char const* end = src + len;

  // cppcheck-suppress unsignedPositive
  if (len >= (SIZE_MAX - 1) / 3) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::string result;
  result.reserve(3 * len);

  for (; src < end; ++src) {
    if ('0' <= *src && *src <= '9') {
      result.push_back(*src);
    }

    else if ('a' <= *src && *src <= 'z') {
      result.push_back(*src);
    }

    else if ('A' <= *src && *src <= 'Z') {
      result.push_back(*src);
    }

    else if (*src == '-' || *src == '_' || *src == '~') {
      result.push_back(*src);
    }

    else {
      uint8_t n = (uint8_t)(*src);
      uint8_t n1 = n >> 4;
      uint8_t n2 = n & 0x0F;

      result.push_back('%');
      result.push_back(hexChars[n1]);
      result.push_back(hexChars[n2]);
    }
  }

  return result;
}

void encodeURIComponent(std::string& result, char const* src, size_t len) {
  char const* end = src + len;

  // cppcheck-suppress unsignedPositive
  if (result.size() + len >= (SIZE_MAX - 1) / 3) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  result.reserve(result.size() + 3 * len);

  for (; src < end; ++src) {
    if (*src == '-' || *src == '_' || *src == '.' || *src == '!' ||
        *src == '~' || *src == '*' || *src == '(' || *src == ')' ||
        *src == '\'' || (*src >= 'a' && *src <= 'z') ||
        (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9')) {
      // no need to encode this character
      result.push_back(*src);
    } else {
      // hex-encode the following character
      result.push_back('%');
      auto c = static_cast<unsigned char>(*src);
      result.push_back(::hexValuesUpper[c >> 4]);
      result.push_back(::hexValuesUpper[c % 16]);
    }
  }
}

std::string encodeURIComponent(char const* src, size_t len) {
  std::string result;
  encodeURIComponent(result, src, len);
  return result;
}

std::string encodeURIComponent(std::string const& str) {
  return encodeURIComponent(str.data(), str.size());
}

std::string soundex(char const* src, size_t len) {
  char const* end = src + len;

  while (src < end) {
    // skip over characters (e.g. whitespace and other non-ASCII letters)
    // until we find something sensible
    if ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z')) {
      break;
    }
    ++src;
  }

  std::string result;

  if (src != end) {
    // emit an upper-case character
    result.push_back(::toupper(*src));
    src++;
    char previousCode = '\0';

    while (src < end) {
      char currentCode = ::soundexCode(*src);
      if (currentCode != '\0' && currentCode != previousCode) {
        result.push_back(currentCode);
        if (result.length() >= 4) {
          break;
        }
      }
      previousCode = currentCode;
      src++;
    }

    // pad result string with '0' chars up to a length of 4
    while (result.length() < 4) {
      result.push_back('0');
    }
  }

  return result;
}

std::string soundex(std::string const& str) {
  return soundex(str.data(), str.size());
}

unsigned int levenshteinDistance(char const* s1, size_t l1, char const* s2, size_t l2) {
  // convert input strings to vectors of (multi-byte) character numbers
  std::vector<uint32_t> vect1 = characterCodes(s1, l1);
  std::vector<uint32_t> vect2 = characterCodes(s2, l2);

  // calculate levenshtein distance on vectors of character numbers
  return static_cast<unsigned int>(::levenshteinDistance(vect1, vect2));
}

std::vector<uint32_t> characterCodes(char const* s, size_t length) {
  char const* e = s + length;

  std::vector<uint32_t> charNums;
  // be conservative, and reserve space for one number of input
  // string byte. this may be too much, but it avoids later
  // reallocation of the vector
  charNums.reserve(length);

  while (s < e) {
    // note: consume advances the *s* pointer by one byte
    unsigned char c = ::consume(s);
    uint32_t n = uint32_t(c);

    if ((c & 0x80U) == 0U) {
      // single-byte character
      charNums.push_back(n);
    } else if ((c & 0xE0U) == 0xC0U) {
      // two-byte character
      if (s >= e) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid UTF-8 sequence");
      }
      charNums.push_back((n << 8U) + uint32_t(::consume(s)));
    } else if ((c & 0xF0U) == 0xE0U) {
      // three-byte character
      if (s + 1 >= e) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid UTF-8 sequence");
      }
      charNums.push_back((n << 16U) + (uint32_t(::consume(s)) << 8U) +
                         (uint32_t(::consume(s))));
    } else if ((c & 0xF8U) == 0XF0U) {
      // four-byte character
      if (s + 2 >= e) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid UTF-8 sequence");
      }
      charNums.push_back((n << 24U) + (uint32_t(::consume(s)) << 16U) +
                         (uint32_t(::consume(s)) << 8U) + (uint32_t(::consume(s))));
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid UTF-8 sequence");
    }
  }

  return charNums;
}

std::vector<uint32_t> characterCodes(std::string const& str) {
  return characterCodes(str.data(), str.size());
}

unsigned int levenshteinDistance(std::string const& str1, std::string const& str2) {
  return levenshteinDistance(str1.data(), str1.size(), str2.data(), str2.size());
}

// .............................................................................
// CONVERT TO STRING
// .............................................................................

std::string itoa(int16_t attr) {
  if (attr == INT16_MIN) {
    return "-32768";
  }

  char buffer[7];
  char* p = buffer;

  if (attr < 0) {
    *p++ = '-';
    attr = -attr;
  }

  if (10000L <= attr) {
    *p++ = char((attr / 10000L) % 10 + '0');
  }
  if (1000L <= attr) {
    *p++ = char((attr / 1000L) % 10 + '0');
  }
  if (100L <= attr) {
    *p++ = char((attr / 100L) % 10 + '0');
  }
  if (10L <= attr) {
    *p++ = char((attr / 10L) % 10 + '0');
  }

  *p++ = char(attr % 10 + '0');
  *p = '\0';

  return buffer;
}

std::string itoa(uint16_t attr) {
  char buffer[6];
  char* p = buffer;

  if (10000L <= attr) {
    *p++ = char((attr / 10000L) % 10 + '0');
  }
  if (1000L <= attr) {
    *p++ = char((attr / 1000L) % 10 + '0');
  }
  if (100L <= attr) {
    *p++ = char((attr / 100L) % 10 + '0');
  }
  if (10L <= attr) {
    *p++ = char((attr / 10L) % 10 + '0');
  }

  *p++ = char(attr % 10 + '0');
  *p = '\0';

  return buffer;
}

std::string itoa(int32_t attr) {
  if (attr == INT32_MIN) {
    return "-2147483648";
  }

  char buffer[12];
  char* p = buffer;

  if (attr < 0) {
    *p++ = '-';
    attr = -attr;
  }

  if (1000000000L <= attr) {
    *p++ = char((attr / 1000000000L) % 10 + '0');
  }
  if (100000000L <= attr) {
    *p++ = char((attr / 100000000L) % 10 + '0');
  }
  if (10000000L <= attr) {
    *p++ = char((attr / 10000000L) % 10 + '0');
  }
  if (1000000L <= attr) {
    *p++ = char((attr / 1000000L) % 10 + '0');
  }
  if (100000L <= attr) {
    *p++ = char((attr / 100000L) % 10 + '0');
  }
  if (10000L <= attr) {
    *p++ = char((attr / 10000L) % 10 + '0');
  }
  if (1000L <= attr) {
    *p++ = char((attr / 1000L) % 10 + '0');
  }
  if (100L <= attr) {
    *p++ = char((attr / 100L) % 10 + '0');
  }
  if (10L <= attr) {
    *p++ = char((attr / 10L) % 10 + '0');
  }

  *p++ = char(attr % 10 + '0');
  *p = '\0';

  return buffer;
}

std::string itoa(uint32_t attr) {
  char buffer[11];
  char* p = buffer;

  if (1000000000L <= attr) {
    *p++ = char((attr / 1000000000L) % 10 + '0');
  }
  if (100000000L <= attr) {
    *p++ = char((attr / 100000000L) % 10 + '0');
  }
  if (10000000L <= attr) {
    *p++ = char((attr / 10000000L) % 10 + '0');
  }
  if (1000000L <= attr) {
    *p++ = char((attr / 1000000L) % 10 + '0');
  }
  if (100000L <= attr) {
    *p++ = char((attr / 100000L) % 10 + '0');
  }
  if (10000L <= attr) {
    *p++ = char((attr / 10000L) % 10 + '0');
  }
  if (1000L <= attr) {
    *p++ = char((attr / 1000L) % 10 + '0');
  }
  if (100L <= attr) {
    *p++ = char((attr / 100L) % 10 + '0');
  }
  if (10L <= attr) {
    *p++ = char((attr / 10L) % 10 + '0');
  }

  *p++ = char(attr % 10 + '0');
  *p = '\0';

  return buffer;
}

std::string itoa(int64_t attr) {
  if (attr == INT64_MIN) {
    return "-9223372036854775808";
  }

  char buffer[21];
  char* p = buffer;

  if (attr < 0) {
    *p++ = '-';
    attr = -attr;
  }

  if (1000000000000000000LL <= attr) {
    *p++ = char((attr / 1000000000000000000LL) % 10 + '0');
  }
  if (100000000000000000LL <= attr) {
    *p++ = char((attr / 100000000000000000LL) % 10 + '0');
  }
  if (10000000000000000LL <= attr) {
    *p++ = char((attr / 10000000000000000LL) % 10 + '0');
  }
  if (1000000000000000LL <= attr) {
    *p++ = char((attr / 1000000000000000LL) % 10 + '0');
  }
  if (100000000000000LL <= attr) {
    *p++ = char((attr / 100000000000000LL) % 10 + '0');
  }
  if (10000000000000LL <= attr) {
    *p++ = char((attr / 10000000000000LL) % 10 + '0');
  }
  if (1000000000000LL <= attr) {
    *p++ = char((attr / 1000000000000LL) % 10 + '0');
  }
  if (100000000000LL <= attr) {
    *p++ = char((attr / 100000000000LL) % 10 + '0');
  }
  if (10000000000LL <= attr) {
    *p++ = char((attr / 10000000000LL) % 10 + '0');
  }
  if (1000000000LL <= attr) {
    *p++ = char((attr / 1000000000LL) % 10 + '0');
  }
  if (100000000LL <= attr) {
    *p++ = char((attr / 100000000LL) % 10 + '0');
  }
  if (10000000LL <= attr) {
    *p++ = char((attr / 10000000LL) % 10 + '0');
  }
  if (1000000LL <= attr) {
    *p++ = char((attr / 1000000LL) % 10 + '0');
  }
  if (100000LL <= attr) {
    *p++ = char((attr / 100000LL) % 10 + '0');
  }
  if (10000LL <= attr) {
    *p++ = char((attr / 10000LL) % 10 + '0');
  }
  if (1000LL <= attr) {
    *p++ = char((attr / 1000LL) % 10 + '0');
  }
  if (100LL <= attr) {
    *p++ = char((attr / 100LL) % 10 + '0');
  }
  if (10LL <= attr) {
    *p++ = char((attr / 10LL) % 10 + '0');
  }

  *p++ = char(attr % 10 + '0');
  *p = '\0';

  return buffer;
}

std::string itoa(uint64_t attr) {
  char buffer[21];
  char* p = buffer;

  if (10000000000000000000ULL <= attr) {
    *p++ = char((attr / 10000000000000000000ULL) % 10 + '0');
  }
  if (1000000000000000000ULL <= attr) {
    *p++ = char((attr / 1000000000000000000ULL) % 10 + '0');
  }
  if (100000000000000000ULL <= attr) {
    *p++ = char((attr / 100000000000000000ULL) % 10 + '0');
  }
  if (10000000000000000ULL <= attr) {
    *p++ = char((attr / 10000000000000000ULL) % 10 + '0');
  }
  if (1000000000000000ULL <= attr) {
    *p++ = char((attr / 1000000000000000ULL) % 10 + '0');
  }
  if (100000000000000ULL <= attr) {
    *p++ = char((attr / 100000000000000ULL) % 10 + '0');
  }
  if (10000000000000ULL <= attr) {
    *p++ = char((attr / 10000000000000ULL) % 10 + '0');
  }
  if (1000000000000ULL <= attr) {
    *p++ = char((attr / 1000000000000ULL) % 10 + '0');
  }
  if (100000000000ULL <= attr) {
    *p++ = char((attr / 100000000000ULL) % 10 + '0');
  }
  if (10000000000ULL <= attr) {
    *p++ = char((attr / 10000000000ULL) % 10 + '0');
  }
  if (1000000000ULL <= attr) {
    *p++ = char((attr / 1000000000ULL) % 10 + '0');
  }
  if (100000000ULL <= attr) {
    *p++ = char((attr / 100000000ULL) % 10 + '0');
  }
  if (10000000ULL <= attr) {
    *p++ = char((attr / 10000000ULL) % 10 + '0');
  }
  if (1000000ULL <= attr) {
    *p++ = char((attr / 1000000ULL) % 10 + '0');
  }
  if (100000ULL <= attr) {
    *p++ = char((attr / 100000ULL) % 10 + '0');
  }
  if (10000ULL <= attr) {
    *p++ = char((attr / 10000ULL) % 10 + '0');
  }
  if (1000ULL <= attr) {
    *p++ = char((attr / 1000ULL) % 10 + '0');
  }
  if (100ULL <= attr) {
    *p++ = char((attr / 100ULL) % 10 + '0');
  }
  if (10ULL <= attr) {
    *p++ = char((attr / 10ULL) % 10 + '0');
  }

  *p++ = char(attr % 10 + '0');
  *p = '\0';

  return buffer;
}

size_t itoa(uint64_t attr, char* buffer) {
  char* p = buffer;

  if (10000000000000000000ULL <= attr) {
    *p++ = char((attr / 10000000000000000000ULL) % 10 + '0');
  }
  if (1000000000000000000ULL <= attr) {
    *p++ = char((attr / 1000000000000000000ULL) % 10 + '0');
  }
  if (100000000000000000ULL <= attr) {
    *p++ = char((attr / 100000000000000000ULL) % 10 + '0');
  }
  if (10000000000000000ULL <= attr) {
    *p++ = char((attr / 10000000000000000ULL) % 10 + '0');
  }
  if (1000000000000000ULL <= attr) {
    *p++ = char((attr / 1000000000000000ULL) % 10 + '0');
  }
  if (100000000000000ULL <= attr) {
    *p++ = char((attr / 100000000000000ULL) % 10 + '0');
  }
  if (10000000000000ULL <= attr) {
    *p++ = char((attr / 10000000000000ULL) % 10 + '0');
  }
  if (1000000000000ULL <= attr) {
    *p++ = char((attr / 1000000000000ULL) % 10 + '0');
  }
  if (100000000000ULL <= attr) {
    *p++ = char((attr / 100000000000ULL) % 10 + '0');
  }
  if (10000000000ULL <= attr) {
    *p++ = char((attr / 10000000000ULL) % 10 + '0');
  }
  if (1000000000ULL <= attr) {
    *p++ = char((attr / 1000000000ULL) % 10 + '0');
  }
  if (100000000ULL <= attr) {
    *p++ = char((attr / 100000000ULL) % 10 + '0');
  }
  if (10000000ULL <= attr) {
    *p++ = char((attr / 10000000ULL) % 10 + '0');
  }
  if (1000000ULL <= attr) {
    *p++ = char((attr / 1000000ULL) % 10 + '0');
  }
  if (100000ULL <= attr) {
    *p++ = char((attr / 100000ULL) % 10 + '0');
  }
  if (10000ULL <= attr) {
    *p++ = char((attr / 10000ULL) % 10 + '0');
  }
  if (1000ULL <= attr) {
    *p++ = char((attr / 1000ULL) % 10 + '0');
  }
  if (100ULL <= attr) {
    *p++ = char((attr / 100ULL) % 10 + '0');
  }
  if (10ULL <= attr) {
    *p++ = char((attr / 10ULL) % 10 + '0');
  }

  *p++ = char(attr % 10 + '0');

  return p - buffer;
}

void itoa(uint64_t attr, std::string& out) {
  if (10000000000000000000ULL <= attr) {
    out.push_back(char((attr / 10000000000000000000ULL) % 10 + '0'));
  }
  if (1000000000000000000ULL <= attr) {
    out.push_back(char((attr / 1000000000000000000ULL) % 10 + '0'));
  }
  if (100000000000000000ULL <= attr) {
    out.push_back(char((attr / 100000000000000000ULL) % 10 + '0'));
  }
  if (10000000000000000ULL <= attr) {
    out.push_back(char((attr / 10000000000000000ULL) % 10 + '0'));
  }
  if (1000000000000000ULL <= attr) {
    out.push_back(char((attr / 1000000000000000ULL) % 10 + '0'));
  }
  if (100000000000000ULL <= attr) {
    out.push_back(char((attr / 100000000000000ULL) % 10 + '0'));
  }
  if (10000000000000ULL <= attr) {
    out.push_back(char((attr / 10000000000000ULL) % 10 + '0'));
  }
  if (1000000000000ULL <= attr) {
    out.push_back(char((attr / 1000000000000ULL) % 10 + '0'));
  }
  if (100000000000ULL <= attr) {
    out.push_back(char((attr / 100000000000ULL) % 10 + '0'));
  }
  if (10000000000ULL <= attr) {
    out.push_back(char((attr / 10000000000ULL) % 10 + '0'));
  }
  if (1000000000ULL <= attr) {
    out.push_back(char((attr / 1000000000ULL) % 10 + '0'));
  }
  if (100000000ULL <= attr) {
    out.push_back(char((attr / 100000000ULL) % 10 + '0'));
  }
  if (10000000ULL <= attr) {
    out.push_back(char((attr / 10000000ULL) % 10 + '0'));
  }
  if (1000000ULL <= attr) {
    out.push_back(char((attr / 1000000ULL) % 10 + '0'));
  }
  if (100000ULL <= attr) {
    out.push_back(char((attr / 100000ULL) % 10 + '0'));
  }
  if (10000ULL <= attr) {
    out.push_back(char((attr / 10000ULL) % 10 + '0'));
  }
  if (1000ULL <= attr) {
    out.push_back(char((attr / 1000ULL) % 10 + '0'));
  }
  if (100ULL <= attr) {
    out.push_back(char((attr / 100ULL) % 10 + '0'));
  }
  if (10ULL <= attr) {
    out.push_back(char((attr / 10ULL) % 10 + '0'));
  }

  out.push_back(char(attr % 10 + '0'));
}

std::string ftoa(double i) {
  char buffer[24];
  int length = fpconv_dtoa(i, &buffer[0]);

  return std::string(&buffer[0], static_cast<size_t>(length));
}

// .............................................................................
// CONVERT FROM STRING
// .............................................................................

bool boolean(std::string const& str) {
  if (str.empty()) {
    return false;
  }
  std::string lower = trim(str);
  tolowerInPlace(lower);

  if (lower == "true" || lower == "yes" || lower == "on" || lower == "y" ||
      lower == "1" || lower == "✓") {
    return true;
  }
  return false;
}

uint64_t uint64_trusted(char const* value, size_t length) {
  uint64_t result = 0;

  switch (length) {
    case 20:
      result += (value[length - 20] - '0') * 10000000000000000000ULL;
    [[fallthrough]];
    case 19:
      result += (value[length - 19] - '0') * 1000000000000000000ULL;
    [[fallthrough]];
    case 18:
      result += (value[length - 18] - '0') * 100000000000000000ULL;
    [[fallthrough]];
    case 17:
      result += (value[length - 17] - '0') * 10000000000000000ULL;
    [[fallthrough]];
    case 16:
      result += (value[length - 16] - '0') * 1000000000000000ULL;
    [[fallthrough]];
    case 15:
      result += (value[length - 15] - '0') * 100000000000000ULL;
    [[fallthrough]];
    case 14:
      result += (value[length - 14] - '0') * 10000000000000ULL;
    [[fallthrough]];
    case 13:
      result += (value[length - 13] - '0') * 1000000000000ULL;
    [[fallthrough]];
    case 12:
      result += (value[length - 12] - '0') * 100000000000ULL;
    [[fallthrough]];
    case 11:
      result += (value[length - 11] - '0') * 10000000000ULL;
    [[fallthrough]];
    case 10:
      result += (value[length - 10] - '0') * 1000000000ULL;
    [[fallthrough]];
    case 9:
      result += (value[length - 9] - '0') * 100000000ULL;
    [[fallthrough]];
    case 8:
      result += (value[length - 8] - '0') * 10000000ULL;
    [[fallthrough]];
    case 7:
      result += (value[length - 7] - '0') * 1000000ULL;
    [[fallthrough]];
    case 6:
      result += (value[length - 6] - '0') * 100000ULL;
    [[fallthrough]];
    case 5:
      result += (value[length - 5] - '0') * 10000ULL;
    [[fallthrough]];
    case 4:
      result += (value[length - 4] - '0') * 1000ULL;
    [[fallthrough]];
    case 3:
      result += (value[length - 3] - '0') * 100ULL;
    [[fallthrough]];
    case 2:
      result += (value[length - 2] - '0') * 10ULL;
    [[fallthrough]];
    case 1:
      result += (value[length - 1] - '0');
  }

  return result;
}

double doubleDecimal(std::string const& str) {
  return doubleDecimal(str.c_str(), str.size());
}

double doubleDecimal(char const* value, size_t size) {
  double v = 0.0;
  double e = 1.0;

  bool seenDecimalPoint = false;

  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(value);
  uint8_t const* end = ptr + size;

  // check for the sign first
  if (*ptr == '-') {
    e = -e;
    ++ptr;
  } else if (*ptr == '+') {
    ++ptr;
  }

  for (; ptr < end; ++ptr) {
    uint8_t n = *ptr;

    if (n == '.' && !seenDecimalPoint) {
      seenDecimalPoint = true;
      continue;
    }

    if ('9' < n || n < '0') {
      break;
    }

    v = v * 10.0 + (n - 48);

    if (seenDecimalPoint) {
      e = e * 10.0;
    }
  }

  // we have reached the end without an exponent
  if (ptr == end) {
    return v / e;
  }

  // invalid decimal representation
  if (*ptr != 'e' && *ptr != 'E') {
    return 0.0;
  }

  ++ptr;  // move past the 'e' or 'E'

  int32_t expSign = 1;
  int32_t expValue = 0;

  // is there an exponent sign?
  if (*ptr == '-') {
    expSign = -1;
    ++ptr;
  } else if (*ptr == '+') {
    ++ptr;
  }

  for (; ptr < end; ++ptr) {
    uint8_t n = *ptr;

    if ('9' < n || n < '0') {
      return 0.0;
    }

    expValue = expValue * 10 + (n - 48);
  }

  expValue = expValue * expSign;

  return (v / e) * pow(10.0, double(expValue));
}

float floatDecimal(std::string const& str) {
  return floatDecimal(str.c_str(), str.size());
}

float floatDecimal(char const* value, size_t size) {
  return (float)doubleDecimal(value, size);
}

// .............................................................................
// BASE64
// .............................................................................

std::string encodeBase64(char const* in, size_t len) {
  std::string ret;
  ret.reserve((len * 4 / 3) + 2);

  unsigned char charArray3[3];
  unsigned char charArray4[4];
  unsigned char const* bytesToEncode = reinterpret_cast<unsigned char const*>(in);

  int i = 0;
  while (len--) {
    charArray3[i++] = *(bytesToEncode++);

    if (i == 3) {
      charArray4[0] = (charArray3[0] & 0xfc) >> 2;
      charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
      charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
      charArray4[3] = charArray3[2] & 0x3f;

      for (i = 0; i < 4; i++) {
        ret += BASE64_CHARS[charArray4[i]];
      }

      i = 0;
    }
  }

  if (i != 0) {
    for (int j = i; j < 3; j++) {
      charArray3[j] = '\0';
    }

    charArray4[0] = (charArray3[0] & 0xfc) >> 2;
    charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
    charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
    charArray4[3] = charArray3[2] & 0x3f;

    for (int j = 0; (j < i + 1); j++) {
      ret += BASE64_CHARS[charArray4[j]];
    }

    while ((i++ < 3)) {
      ret += '=';
    }
  }

  return ret;
}

std::string encodeBase64(std::string const& str) {
  return encodeBase64(str.data(), str.size());
}

std::string decodeBase64(std::string const& source) {
  unsigned char charArray4[4];
  unsigned char charArray3[3];

  int i = 0;
  int inp = 0;

  int in_len = (int)source.size();
  std::string ret;
  ret.reserve((source.size() / 4 * 3) + 1);

  while (in_len-- && (source[inp] != '=') && isBase64(source[inp])) {
    charArray4[i++] = source[inp];

    inp++;

    if (i == 4) {
      for (i = 0; i < 4; i++) {
        charArray4[i] = BASE64_REVS[charArray4[i]];
      }

      charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
      charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
      charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

      for (i = 0; (i < 3); i++) {
        ret += charArray3[i];
      }

      i = 0;
    }
  }

  if (i) {
    for (int j = i; j < 4; j++) {
      charArray4[j] = 0;
    }

    for (int j = 0; j < 4; j++) {
      charArray4[j] = BASE64_REVS[charArray4[j]];
    }

    charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
    charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
    charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

    for (int j = 0; j < i - 1; j++) {
      ret += charArray3[j];
    }
  }

  return ret;
}

std::string encodeBase64U(std::string const& in) {
  unsigned char charArray3[3];
  unsigned char charArray4[4];

  std::string ret;
  ret.reserve((in.size() * 4 / 3) + 2);

  int i = 0;

  unsigned char const* bytesToEncode =
      reinterpret_cast<unsigned char const*>(in.c_str());
  size_t in_len = in.size();

  while (in_len--) {
    charArray3[i++] = *(bytesToEncode++);

    if (i == 3) {
      charArray4[0] = (charArray3[0] & 0xfc) >> 2;
      charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
      charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
      charArray4[3] = charArray3[2] & 0x3f;

      for (i = 0; i < 4; i++) {
        ret += BASE64U_CHARS[charArray4[i]];
      }

      i = 0;
    }
  }

  if (i != 0) {
    for (size_t j = i; j < 3; j++) {
      charArray3[j] = '\0';
    }

    charArray4[0] = (charArray3[0] & 0xfc) >> 2;
    charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
    charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
    charArray4[3] = charArray3[2] & 0x3f;

    for (int j = 0; (j < i + 1); j++) {
      ret += BASE64U_CHARS[charArray4[j]];
    }

    while ((i++ < 3)) {
      ret += '=';
    }
  }

  return ret;
}

std::string decodeBase64U(std::string const& source) {
  unsigned char charArray4[4];
  unsigned char charArray3[3];

  std::string ret;
  ret.reserve((source.size() / 4 * 3) + 1);

  int i = 0;
  int inp = 0;

  int in_len = (int)source.size();

  while (in_len-- && (source[inp] != '=') && isBase64U(source[inp])) {
    charArray4[i++] = source[inp];

    inp++;

    if (i == 4) {
      for (i = 0; i < 4; i++) {
        charArray4[i] = BASE64U_REVS[charArray4[i]];
      }

      charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
      charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
      charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

      for (i = 0; (i < 3); i++) {
        ret += charArray3[i];
      }

      i = 0;
    }
  }

  if (i) {
    for (size_t j = i; j < 4; j++) {
      charArray4[j] = 0;
    }

    for (size_t j = 0; j < 4; j++) {
      charArray4[j] = BASE64U_REVS[charArray4[j]];
    }

    charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
    charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
    charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

    for (int j = 0; j < i - 1; j++) {
      ret += charArray3[j];
    }
  }

  return ret;
}

// .............................................................................
// ADDITIONAL STRING UTILITIES
// .............................................................................

std::string correctPath(std::string const& incorrectPath) {
#ifdef _WIN32
  return replace(incorrectPath, "/", "\\");
#else
  return replace(incorrectPath, "\\", "/");
#endif
}

// In a list str = "xx,yy,zz ...", entry(n,str,',') returns the nth entry of the
// list delimited
// by ','. E.g entry(2,str,',') = 'yy'

std::string entry(size_t const pos, std::string const& sourceStr, std::string const& delimiter) {
  size_t delLength = delimiter.length();
  size_t sourceLength = sourceStr.length();

  if (pos == 0) {
    return "";
  }

  if (delLength == 0 || sourceLength == 0) {
    return sourceStr;
  }

  size_t k = 0;
  size_t offSet = 0;

  while (true) {
    size_t delPos = sourceStr.find(delimiter, offSet);

    if ((delPos == sourceStr.npos) || (delPos >= sourceLength) || (offSet >= sourceLength)) {
      return sourceStr.substr(offSet);
    }

    ++k;

    if (k == pos) {
      return sourceStr.substr(offSet, delPos - offSet);
    }

    offSet = delPos + delLength;
  }

  return sourceStr;
}

/// Determines the number of entries in a list str = "xx,yyy,zz,www".
/// numEntries(str,',') = 4.

size_t numEntries(std::string const& sourceStr, std::string const& delimiter) {
  size_t delLength = delimiter.length();
  size_t sourceLength = sourceStr.length();

  if (sourceLength == 0) {
    return (0);
  }

  if (delLength == 0) {
    return (1);
  }

  size_t k = 1;

  for (size_t j = 0; j < sourceLength; ++j) {
    bool match = true;
    for (size_t i = 0; i < delLength; ++i) {
      if (sourceStr[j + i] != delimiter[i]) {
        match = false;
        break;
      }
    }
    if (match) {
      j += (delLength - 1);
      ++k;
      continue;
    }
  }
  return k;
}

std::string encodeHex(char const* value, size_t length) {
  std::string result;
  result.reserve(length * 2);

  char const* p = value;
  char const* e = p + length;
  while (p < e) {
    auto c = static_cast<unsigned char>(*p++);
    result.push_back(::hexValuesLower[c >> 4]);
    result.push_back(::hexValuesLower[c % 16]);
  }

  return result;
}

std::string encodeHex(std::string const& value) {
  return encodeHex(value.data(), value.size());
}

std::string decodeHex(char const* value, size_t length) {
  std::string result;
  // input string length should be divisable by 2
  // but we do not assert for this here, because it might
  // be an end user error
  if ((length & 1) != 0 || length == 0) {
    // invalid or empty
    return std::string();
  }

  result.reserve(length / 2);

  unsigned char const* p = reinterpret_cast<unsigned char const*>(value);
  unsigned char const* e = p + length;
  while (p + 2 <= e) {
    unsigned char c = *p++;
    unsigned char v = 0;
    if (c >= '0' && c <= '9') {
      v = (c - '0') << 4;
    } else if (c >= 'a' && c <= 'f') {
      v = (c - 'a' + 10) << 4;
    } else if (c >= 'A' && c <= 'F') {
      v = (c - 'A' + 10) << 4;
    } else {
      // invalid input character
      return std::string();
    }

    c = *p++;
    if (c >= '0' && c <= '9') {
      v += (c - '0');
    } else if (c >= 'a' && c <= 'f') {
      v += (c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      v += (c - 'A' + 10);
    } else {
      // invalid input character
      return std::string();
    }

    result.push_back(v);
  }

  return result;
}

std::string decodeHex(std::string const& value) {
  return decodeHex(value.data(), value.size());
}

void escapeRegexParams(std::string& out, const char* ptr, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    char const c = ptr[i];
    if (c == '?' || c == '+' || c == '[' || c == '(' || c == ')' || c == '{' || c == '}' ||
        c == '^' || c == '$' || c == '|' || c == '.' || c == '*' || c == '\\') {
      // character with special meaning in a regex
      out.push_back('\\');
    }
    out.push_back(c);
  }
}

std::string escapeRegexParams(std::string const& in) {
  std::string out;
  escapeRegexParams(out, in.data(), in.size());
  return out;
}

}  // namespace StringUtils
}  // namespace basics
}  // namespace arangodb
