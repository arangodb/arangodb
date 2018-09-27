////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "http.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {
static inline int hex2int(char ch, int errorValue = 0) {
  if ('0' <= ch && ch <= '9') {
    return ch - '0';
  } else if ('A' <= ch && ch <= 'F') {
    return ch - 'A' + 10;
  } else if ('a' <= ch && ch <= 'f') {
    return ch - 'a' + 10;
  }

  return errorValue;
}

std::string urlEncode(char const* src, size_t const len) {
  static char hexChars[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                              '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  char const* end = src + len;

  if (len >= (SIZE_MAX - 1) / 3) {
    throw std::overflow_error("out of memory");
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

std::string urlEncode(char const* src) {
  if (src != nullptr) {
    size_t len = strlen(src);
    return urlEncode(src, len);
  }
  return "";
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
}}}}  // namespace arangodb::fuerte::v1::http
