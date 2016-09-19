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

#include <math.h>
#include <time.h>

#include "Logger/Logger.h"
#include "Basics/Exceptions.h"
#include "Basics/tri-strings.h"
#include "Basics/StringBuffer.h"

#include "zlib.h"
#include "zconf.h"  

// -----------------------------------------------------------------------------
// helper functions
// -----------------------------------------------------------------------------

namespace {
bool isSpace(char a) { return a == ' ' || a == '\t' || a == '_'; }

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
  return (isalnum(c) || (c == '+') || (c == '/'));
}

inline bool isBase64U(unsigned char c) {
  return (isalnum(c) || (c == '-') || (c == '_'));
}

bool parseHexanumber(char const* inputStr, size_t len, uint32_t* outputInt) {
  uint32_t const charVal[16] = {0, 1, 2,  3,  4,  5,  6,  7,
                                8, 9, 10, 11, 12, 13, 14, 15};
  bool ok = true;
  for (size_t j = 0; j < len; j++) {
    if (inputStr[j] > '/' && inputStr[j] < ':') {
      *outputInt = (*outputInt << 4) | charVal[inputStr[j] - 48];
    } else if (inputStr[j] > '@' && inputStr[j] < 'G') {
      *outputInt = (*outputInt << 4) | charVal[inputStr[j] - 55];
    } else if (inputStr[j] > '`' && inputStr[j] < 'g') {
      *outputInt = (*outputInt << 4) | charVal[inputStr[j] - 87];
    } else {
      // invalid sequence of characters received for now simply return whatever
      // we received
      // *outputStr = std::string(inputStr,len);
      ok = false;
      break;
    }
  }
  return ok;
}
///-------------------------------------------------------
/// @brief computes the unicode value of an ut16 symbol
///-------------------------------------------------------

bool toUnicode(uint16_t w1, uint16_t w2, uint32_t* v) {
  uint32_t vh = w1 - 0xD800;
  uint32_t vl = w2 - 0xDC00;
  uint32_t vp = (vh << 10) | vl;
  *v = vp + 0x10000;
  return true;
}

bool toUtf8(uint32_t outputInt, std::string& outputStr) {
  if ((outputInt >> 7) == 0) {
    outputStr.append(1, (char)(outputInt));
  }
  // unicode sequence is in the range \u0080 to \u07ff
  else if ((outputInt >> 11) == 0) {
    outputStr.append(1, (char)((3 << 6) | (outputInt >> 6)));
    outputStr.append(1, (char)((1 << 7) | (outputInt & 63)));
  }
  // unicode sequence is in the range \u0800 to \uffff
  else if ((outputInt >> 16) == 0) {
    outputStr.append(1, (char)((7 << 5) | (outputInt >> 12)));
    outputStr.append(
        1, (char)((1 << 7) | ((outputInt & 4095 /* 2^12 - 1*/) >> 6)));
    outputStr.append(1, (char)((1 << 7) | (outputInt & 63)));
  } else if ((outputInt >> 21) == 0) {
    outputStr.append(1, (char)((15 << 4) | (outputInt >> 18)));
    outputStr.append(
        1, (char)((1 << 7) | ((outputInt & 262143 /* 2^18 - 1*/) >> 12)));
    outputStr.append(
        1, (char)((1 << 7) | ((outputInt & 4095 /* 2^12 - 1*/) >> 6)));
    outputStr.append(1, (char)((1 << 7) | ((outputInt & 63))));
  } else {
    // can't handle it yet (need 5,6 etc utf8 characters), just send back the
    // string we got
    return false;
  }
  return true;
}
///-------------------------------------------------------
/// @brief true when number lays in the range
///        U+D800  U+DBFF
///-------------------------------------------------------
bool isHighSurrugate(uint32_t number) {
  return (number >= 0xD800) && (number <= 0xDBFF);
}
///-------------------------------------------------------
/// @brief true when number lays in the range
///        U+DC00  U+DFFF
bool isLowSurrugate(uint32_t number) {
  return (number >= 0xDC00) && (number <= 0xDFFF);
}
}

namespace arangodb {
namespace basics {
namespace StringUtils {

char* duplicate(std::string const& source) {
  size_t len = source.size();
  char* result = new char[len + 1];

  memcpy(result, source.c_str(), len);
  result[len] = '\0';

  return result;
}

char* duplicate(char const* source, size_t len) {
  if (source == 0) {
    return 0;
  }

  char* result = new char[len + 1];

  memcpy(result, source, len);
  result[len] = '\0';

  return result;
}

char* duplicate(char const* source) {
  if (source == 0) {
    return 0;
  }
  size_t len = strlen(source);
  char* result = new char[len + 1];

  memcpy(result, source, len);
  result[len] = '\0';

  return result;
}

void destroy(char*& source) {
  if (source != 0) {
    ::memset(source, 0, ::strlen(source));
    delete[] source;
    source = 0;
  }
}

void destroy(char*& source, size_t length) {
  if (source != 0) {
    ::memset(source, 0, length);
    delete[] source;
    source = 0;
  }
}

void erase(char*& source) {
  if (source != 0) {
    delete[] source;
    source = 0;
  }
}

// .............................................................................
// STRING CONVERSION
// .............................................................................

std::string capitalize(std::string const& name, bool first) {
  size_t len = name.length();

  if (len == 0) {
    std::string message("name must not be empty");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  char* buffer = new char[len + 1];
  char* qtr = buffer;
  char const* ptr = name.c_str();

  for (; 0 < len && isSpace(*ptr); len--, ptr++) {
  }

  if (len == 0) {
    std::string message("object or attribute name must not be empty");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  bool upper = first;

  for (; 0 < len; len--, ptr++) {
    if (isSpace(*ptr)) {
      upper = true;
    } else {
      if (upper) {
        *qtr++ = static_cast<char>(::toupper(*ptr));
      } else {
        *qtr++ = static_cast<char>(::tolower(*ptr));
      }
      upper = false;
    }
  }

  *qtr = '\0';

  std::string result(buffer);

  delete[] buffer;

  return result;
}

std::string separate(std::string const& name, char separator) {
  size_t len = name.length();

  if (len == 0) {
    std::string message("name must not be empty");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  char* buffer = new char[len + 1];
  char* qtr = buffer;
  char const* ptr = name.c_str();

  for (; 0 < len && isSpace(*ptr); len--, ptr++) {
  }

  if (len == 0) {
    std::string message("name must not be empty");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
  }

  bool useSeparator = false;

  for (; 0 < len; len--, ptr++) {
    for (; 0 < len && isSpace(*ptr); len--, ptr++) {
      useSeparator = true;
    }

    if (0 < len) {
      if (useSeparator) {
        *qtr++ = separator;
        useSeparator = false;
      }

      *qtr++ = static_cast<char>(::tolower(*ptr));
    } else {
      break;
    }
  }

  *qtr = '\0';

  std::string result(buffer);

  delete[] buffer;

  return result;
}

std::string escapeUnicode(std::string const& name, bool escapeSlash) {
  size_t len = name.length();

  if (len == 0) {
    return name;
  }

  if (len >= (SIZE_MAX - 1) / 6) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  bool corrupted = false;

  char* buffer = new char[6 * len + 1];
  char* qtr = buffer;
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

  std::string result(buffer, qtr - buffer);

  delete[] buffer;

  if (corrupted) {
    LOG(WARN) << "escaped corrupted unicode string";
  }

  return result;
}

std::string escapeHtml(std::string const& name) {
  size_t len = name.length();

  if (len == 0) {
    return name;
  }

  if (len >= (SIZE_MAX - 1) / 8) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  char* buffer = new char[8 * len + 1];
  char* qtr = buffer;
  char const* ptr = name.c_str();
  char const* end = ptr + len;

  for (; ptr < end; ++ptr, ++qtr) {
    if (*ptr == '<') {
      *qtr++ = '&';
      *qtr++ = 'l';
      *qtr++ = 't';
      *qtr = ';';
    } else if (*ptr == '>') {
      *qtr++ = '&';
      *qtr++ = 'g';
      *qtr++ = 't';
      *qtr = ';';
    } else if (*ptr == '&') {
      *qtr++ = '&';
      *qtr++ = 'a';
      *qtr++ = 'm';
      *qtr++ = 'p';
      *qtr = ';';
    } else if (*ptr == '"') {
      *qtr++ = '&';
      *qtr++ = 'q';
      *qtr++ = 'u';
      *qtr++ = 'o';
      *qtr++ = 't';
      *qtr = ';';
    } else {
      *qtr = *ptr;
    }
  }

  *qtr = '\0';

  std::string result(buffer, qtr - buffer);

  delete[] buffer;

  return result;
}

std::string escapeXml(std::string const& name) { return escapeHtml(name); }

std::string escapeHex(std::string const& name, char quote) {
  size_t len = name.length();

  if (len == 0) {
    return name;
  }

  if (len >= (SIZE_MAX - 1) / 3) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  char* buffer = new char[3 * len + 1];
  char* qtr = buffer;
  char const* ptr = name.c_str();
  char const* end = ptr + len;

  for (; ptr < end; ptr++, qtr++) {
    if (*ptr == quote || *ptr <= ' ' || static_cast<uint8_t>(*ptr) >= 128) {
      uint8_t n = (uint8_t)(*ptr);
      uint8_t n1 = n >> 4;
      uint8_t n2 = n & 0x0F;

      *qtr++ = quote;
      *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
      *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
    } else {
      *qtr = *ptr;
    }
  }

  *qtr = '\0';

  std::string result(buffer, qtr - buffer);

  delete[] buffer;

  return result;
}

std::string escapeHex(std::string const& name, std::string const& special,
                      char quote) {
  size_t len = name.length();

  if (len == 0) {
    return name;
  }

  if (len >= (SIZE_MAX - 1) / 3) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  char* buffer = new char[3 * len + 1];
  char* qtr = buffer;
  char const* ptr = name.c_str();
  char const* end = ptr + len;

  if (special.length() == 0) {
    for (; ptr < end; ptr++, qtr++) {
      if (*ptr == quote) {
        uint8_t n = (uint8_t)(*ptr);
        uint8_t n1 = n >> 4;
        uint8_t n2 = n & 0x0F;

        *qtr++ = quote;
        *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
        *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
      } else {
        *qtr = *ptr;
      }
    }
  } else if (special.length() == 1) {
    char s = special[0];

    for (; ptr < end; ptr++, qtr++) {
      if (*ptr == quote || *ptr == s) {
        uint8_t n = (uint8_t)(*ptr);
        uint8_t n1 = n >> 4;
        uint8_t n2 = n & 0x0F;

        *qtr++ = quote;
        *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
        *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
      } else {
        *qtr = *ptr;
      }
    }
  } else {
    for (; ptr < end; ptr++, qtr++) {
      if (*ptr == quote || special.find(*ptr) != std::string::npos) {
        uint8_t n = (uint8_t)(*ptr);
        uint8_t n1 = n >> 4;
        uint8_t n2 = n & 0x0F;

        *qtr++ = quote;
        *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
        *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
      } else {
        *qtr = *ptr;
      }
    }
  }

  *qtr = '\0';

  std::string result(buffer, qtr - buffer);

  delete[] buffer;

  return result;
}

std::string escapeC(std::string const& name) {
  size_t len = name.length();

  if (len == 0) {
    return name;
  }

  if (len >= (SIZE_MAX - 1) / 4) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  char* buffer = new char[4 * len + 1];
  char* qtr = buffer;
  char const* ptr = name.c_str();
  char const* end = ptr + len;

  for (; ptr < end; ptr++, qtr++) {
    uint8_t n = (uint8_t)(*ptr);

    switch (*ptr) {
      case '\n':
        *qtr++ = '\\';
        *qtr = 'n';
        break;

      case '\r':
        *qtr++ = '\\';
        *qtr = 'r';
        break;

      case '\'':
      case '"':
        *qtr++ = '\\';
        *qtr = *ptr;
        break;

      default:
        if (n < 32 || n > 127) {
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

  *qtr = '\0';

  std::string result(buffer, qtr - buffer);

  delete[] buffer;

  return result;
}

std::vector<std::string> split(std::string const& source, char delim,
                               char quote) {
  std::vector<std::string> result;

  if (source.empty()) {
    return result;
  }

  char* buffer = new char[source.size() + 1];
  char* p = buffer;

  char const* q = source.c_str();
  char const* e = source.c_str() + source.size();

  if (quote == '\0') {
    for (; q < e; ++q) {
      if (*q == delim) {
        *p = '\0';
        result.push_back(std::string(buffer, p - buffer));
        p = buffer;
      } else {
        *p++ = *q;
      }
    }
  } else {
    for (; q < e; ++q) {
      if (*q == quote) {
        if (q + 1 < e) {
          *p++ = *++q;
        }
      } else if (*q == delim) {
        *p = '\0';
        result.push_back(std::string(buffer, p - buffer));
        p = buffer;
      } else {
        *p++ = *q;
      }
    }
  }

  *p = '\0';
  result.push_back(std::string(buffer, p - buffer));

  delete[] buffer;

  return result;
}

std::vector<std::string> split(std::string const& source,
                               std::string const& delim, char quote) {
  std::vector<std::string> result;

  if (source.empty()) {
    return result;
  }

  char* buffer = new char[source.size() + 1];
  char* p = buffer;

  char const* q = source.c_str();
  char const* e = source.c_str() + source.size();

  if (quote == '\0') {
    for (; q < e; ++q) {
      if (delim.find(*q) != std::string::npos) {
        *p = '\0';
        result.push_back(std::string(buffer, p - buffer));
        p = buffer;
      } else {
        *p++ = *q;
      }
    }
  } else {
    for (; q < e; ++q) {
      if (*q == quote) {
        if (q + 1 < e) {
          *p++ = *++q;
        }
      } else if (delim.find(*q) != std::string::npos) {
        *p = '\0';
        result.push_back(std::string(buffer, p - buffer));
        p = buffer;
      } else {
        *p++ = *q;
      }
    }
  }

  *p = '\0';
  result.push_back(std::string(buffer, p - buffer));

  delete[] buffer;

  return result;
}

std::string trim(std::string const& sourceStr, std::string const& trimStr) {
  size_t s = sourceStr.find_first_not_of(trimStr);
  size_t e = sourceStr.find_last_not_of(trimStr);

  if (s == std::string::npos) {
    return std::string();
  } else {
    return std::string(sourceStr, s, e - s + 1);
  }
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
  } else {
    return std::string(str, s);
  }
}

std::string rTrim(std::string const& sourceStr, std::string const& trimStr) {
  size_t e = sourceStr.find_last_not_of(trimStr);

  return std::string(sourceStr, 0, e + 1);
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

  if ((sourceLength / fromLength) + 1 >= (SIZE_MAX - toLength) / mt) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  size_t maxLength = (((sourceLength / fromLength) + 1) * mt) + toLength;

  // the min amount of memory we have to allocate for the "replace" (new) string
  // is length of sourceStr
  maxLength = (std::max)(maxLength, sourceLength) + 1;

  char* result = new char[maxLength];
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
      result[k] = sourceStr[j];
      ++k;
      continue;
    }

    for (size_t i = 0; i < toLength; ++i) {
      result[k] = toStr[i];
      ++k;
    }

    j += (fromLength - 1);
  }

  result[k] = '\0';

  std::string retStr(result);

  delete[] result;

  return retStr;
}

void tolowerInPlace(std::string* str) {
  size_t len = str->length();

  if (len == 0) {
    return;
  }

  for (std::string::iterator i = str->begin(); i != str->end(); ++i) {
    *i = ::tolower(*i);
  }
}

std::string tolower(std::string&& str) {
  size_t const len = str.size();

  for (size_t i = 0; i < len; ++i) {
    str[i] = static_cast<char>(::tolower(str[i]));
  }

  return str;
}

std::string tolower(std::string const& str) {
  size_t len = str.length();

  if (len == 0) {
    return "";
  }

  char* buffer = new char[len];
  char* qtr = buffer;
  char const* ptr = str.c_str();

  for (; 0 < len; len--, ptr++, qtr++) {
    *qtr = static_cast<char>(::tolower(*ptr));
  }

  std::string result(buffer, str.size());

  delete[] buffer;

  return result;
}

void toupperInPlace(std::string* str) {
  size_t len = str->length();

  if (len == 0) {
    return;
  }

  for (std::string::iterator i = str->begin(); i != str->end(); ++i) {
    *i = ::toupper(*i);
  }
}

std::string toupper(std::string const& str) {
  size_t len = str.length();

  if (len == 0) {
    return "";
  }

  char* buffer = new char[len];
  char* qtr = buffer;
  char const* ptr = str.c_str();

  for (; 0 < len; len--, ptr++, qtr++) {
    *qtr = static_cast<char>(::toupper(*ptr));
  }

  std::string result(buffer, str.size());
  delete[] buffer;
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
    return str.compare(str.size() - postfix.length(), postfix.length(),
                       postfix) == 0;
  }
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

std::string urlEncode(char const* src) {
  if (src != nullptr) {
    size_t len = strlen(src);
    return urlEncode(src, len);
  }
  return "";
}

std::string urlEncode(char const* src, size_t const len) {
  static char hexChars[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                              '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  char const* end = src + len;

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

    else if (*src == '-' || *src == '_' || *src == '.' || *src == '~') {
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

// .............................................................................
// CONVERT TO STRING
// .............................................................................

std::string itoa(int16_t attr) {
  char buffer[7];
  char* p = buffer;

  if (attr == INT16_MIN) {
    return "-32768";
  }

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
  char buffer[12];
  char* p = buffer;

  if (attr == INT32_MIN) {
    return "-2147483648";
  }

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
  char buffer[21];
  char* p = buffer;

  if (attr == INT64_MIN) {
    return "-9223372036854775808";
  }

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

std::string ftoa(double i) {
  StringBuffer buffer(TRI_CORE_MEM_ZONE);

  buffer.appendDecimal(i);

  std::string result(buffer.c_str());

  return result;
}

// .............................................................................
// CONVERT FROM STRING
// .............................................................................

bool boolean(std::string const& str) {
  std::string lower = tolower(trim(str));

  if (lower == "true" || lower == "yes" || lower == "on" || lower == "y" ||
      lower == "1") {
    return true;
  } else {
    return false;
  }
}

int64_t int64(std::string const& str) {
  try {
    return std::stoll(str, 0, 10);
  } catch (...) {
    return 0;
  }
}

int64_t int64_check(std::string const& str) {
  size_t n;
  int64_t value = std::stoll(str, &n, 10);

  if (n < str.size()) {
    throw std::invalid_argument("cannot convert '" + str + "' to int64");
  }

  return value;
}

uint64_t uint64(std::string const& str) {
  try {
    return std::stoull(str, 0, 10);
  } catch (...) {
    return 0;
  }
}

uint64_t uint64_check(std::string const& str) {
  size_t n;
  int64_t value = std::stoull(str, &n, 10);

  if (n < str.size()) {
    throw std::invalid_argument("cannot convert '" + str + "' to uint64");
  }

  return value;
}

uint64_t uint64_trusted(char const* value, size_t length) {
  uint64_t result = 0;
    
  switch (length) { 
    case 20:    result += (value[length - 20] - '0') * 10000000000000000000ULL;
    case 19:    result += (value[length - 19] - '0') * 1000000000000000000ULL;
    case 18:    result += (value[length - 18] - '0') * 100000000000000000ULL;
    case 17:    result += (value[length - 17] - '0') * 10000000000000000ULL;
    case 16:    result += (value[length - 16] - '0') * 1000000000000000ULL;
    case 15:    result += (value[length - 15] - '0') * 100000000000000ULL;
    case 14:    result += (value[length - 14] - '0') * 10000000000000ULL;
    case 13:    result += (value[length - 13] - '0') * 1000000000000ULL;
    case 12:    result += (value[length - 12] - '0') * 100000000000ULL;
    case 11:    result += (value[length - 11] - '0') * 10000000000ULL;
    case 10:    result += (value[length - 10] - '0') * 1000000000ULL;
    case  9:    result += (value[length -  9] - '0') * 100000000ULL;
    case  8:    result += (value[length -  8] - '0') * 10000000ULL;
    case  7:    result += (value[length -  7] - '0') * 1000000ULL;
    case  6:    result += (value[length -  6] - '0') * 100000ULL;
    case  5:    result += (value[length -  5] - '0') * 10000ULL;
    case  4:    result += (value[length -  4] - '0') * 1000ULL;
    case  3:    result += (value[length -  3] - '0') * 100ULL;
    case  2:    result += (value[length -  2] - '0') * 10ULL;
    case  1:    result += (value[length -  1] - '0');
  }

  return result;
}

int32_t int32(std::string const& str) {
#ifdef TRI_HAVE_STRTOL_R
  struct reent buffer;
  return strtol_r(&buffer, str.c_str(), 0, 10);
#else
#ifdef TRI_HAVE__STRTOL_R
  struct reent buffer;
  return _strtol_r(&buffer, str.c_str(), 0, 10);
#else
  return strtol(str.c_str(), 0, 10);
#endif
#endif
}

int32_t int32(char const* value, size_t size) {
  char tmp[22];

  if (value[size] != '\0') {
    if (size >= sizeof(tmp)) {
      size = sizeof(tmp) - 1;
    }

    memcpy(tmp, value, size);
    tmp[size] = '\0';
    value = tmp;
  }

#ifdef TRI_HAVE_STRTOL_R
  struct reent buffer;
  return strtol_r(&buffer, value, 0, 10);
#else
#ifdef TRI_HAVE__STRTOL_R
  struct reent buffer;
  return _strtol_r(&buffer, value, 0, 10);
#else
  return strtol(value, 0, 10);
#endif
#endif
}

uint32_t uint32(std::string const& str) {
#ifdef TRI_HAVE_STRTOUL_R
  struct reent buffer;
  return strtoul_r(&buffer, str.c_str(), 0, 10);
#else
#ifdef TRI_HAVE__STRTOUL_R
  struct reent buffer;
  return _strtoul_r(&buffer, str.c_str(), 0, 10);
#else
  return strtoul(str.c_str(), 0, 10);
#endif
#endif
}

uint32_t unhexUint32(std::string const& str) {
#ifdef TRI_HAVE_STRTOUL_R
  struct reent buffer;
  return strtoul_r(&buffer, str.c_str(), 0, 16);
#else
#ifdef TRI_HAVE__STRTOUL_R
  struct reent buffer;
  return _strtoul_r(&buffer, str.c_str(), 0, 16);
#else
  return strtoul(str.c_str(), 0, 16);
#endif
#endif
}

uint32_t uint32(char const* value, size_t size) {
  char tmp[22];

  if (value[size] != '\0') {
    if (size >= sizeof(tmp)) {
      size = sizeof(tmp) - 1;
    }

    memcpy(tmp, value, size);
    tmp[size] = '\0';
    value = tmp;
  }

#ifdef TRI_HAVE_STRTOUL_R
  struct reent buffer;
  return strtoul_r(&buffer, value, 0, 10);
#else
#ifdef TRI_HAVE__STRTOUL_R
  struct reent buffer;
  return _strtoul_r(&buffer, value, 0, 10);
#else
  return strtoul(value, 0, 10);
#endif
#endif
}

uint32_t unhexUint32(char const* value, size_t size) {
  char tmp[22];

  if (value[size] != '\0') {
    if (size >= sizeof(tmp)) {
      size = sizeof(tmp) - 1;
    }

    memcpy(tmp, value, size);
    tmp[size] = '\0';
    value = tmp;
  }

#ifdef TRI_HAVE_STRTOUL_R
  struct reent buffer;
  return strtoul_r(&buffer, value, 0, 16);
#else
#ifdef TRI_HAVE__STRTOUL_R
  struct reent buffer;
  return _strtoul_r(&buffer, value, 0, 16);
#else
  return strtoul(value, 0, 16);
#endif
#endif
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
// UTF8
// .............................................................................

// this function takes a sequence of hexadecimal characters (inputStr) of a
// particular
// length (len) and output 1,2,3 or 4 characters which represents the utf8
// encoding
// of the unicode representation of that character.

bool unicodeToUTF8(char const* inputStr, size_t const& len,
                   std::string& outputStr) {
  uint32_t outputInt = 0;
  bool ok;

  ok = parseHexanumber(inputStr, len, &outputInt);
  if (ok == false) {
    outputStr = std::string(inputStr, len);
    return false;
  }
  ok = isHighSurrugate(outputInt) || isLowSurrugate(outputInt);
  if (ok == true) {
    outputStr.append("?");
    return false;
  }
  ok = toUtf8(outputInt, outputStr);
  if (ok == false) {
    outputStr = std::string(inputStr, len);
  }
  return ok;
}

// .............................................................................
// UTF16
// .............................................................................

// this function converts unicode symbols whose code points lies in U+10000 to
// U+10FFFF
//
// the parameters of the function correspond to the UTF16 description for
// the code points mentioned above
//
// high_surrogate is the high surrogate bytes sequence, valid values begin with
// D[89AB]
// low_surrogate  is the low  surrogate bytes sequence, valid values begin with
// D[CDEF]
// for details see:
// http://en.wikipedia.org/wiki/UTF-16#Code_points_U.2B10000_to_U.2B10FFFF
//

bool convertUTF16ToUTF8(char const* high_surrogate, char const* low_surrogate,
                        std::string& outputStr) {
  uint32_t w1 = 0;
  uint32_t w2 = 0;
  uint32_t v;
  bool ok = true;

  ok = ok && parseHexanumber(high_surrogate, 4, &w1);
  ok = ok && parseHexanumber(low_surrogate, 4, &w2);
  if (ok == false) {
    return false;
  }
  ok = isHighSurrugate(w1) && isLowSurrugate(w2);
  ok = ok && toUnicode(w1, w2, &v);
  if (ok == true) {
    toUtf8(v, outputStr);
  }
  return ok;
}

// .............................................................................
// BASE64
// .............................................................................

std::string encodeBase64(std::string const& in) {
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
      charArray4[1] =
          ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
      charArray4[2] =
          ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
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
    charArray4[1] =
        ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
    charArray4[2] =
        ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
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

std::string decodeBase64(std::string const& source) {
  unsigned char charArray4[4];
  unsigned char charArray3[3];
  
  std::string ret;

  int i = 0;
  int inp = 0;

  int in_len = (int)source.size();
  ret.reserve((source.size() / 4 * 3) + 1);

  while (in_len-- && (source[inp] != '=') && isBase64(source[inp])) {
    charArray4[i++] = source[inp];

    inp++;

    if (i == 4) {
      for (i = 0; i < 4; i++) {
        charArray4[i] = BASE64_REVS[charArray4[i]];
      }

      charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
      charArray3[1] =
          ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
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
    charArray3[1] =
        ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
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
      charArray4[1] =
          ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
      charArray4[2] =
          ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
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
    charArray4[1] =
        ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
    charArray4[2] =
        ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
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
      charArray3[1] =
          ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
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
    charArray3[1] =
        ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
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

std::string entry(size_t const pos, std::string const& sourceStr,
                  std::string const& delimiter) {
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

    if ((delPos == sourceStr.npos) || (delPos >= sourceLength) ||
        (offSet >= sourceLength)) {
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

std::string encodeHex(std::string const& str) {
  char* tmp;
  size_t len;

  tmp = TRI_EncodeHexString(str.c_str(), str.length(), &len);
  auto result = std::string(tmp, len);
  TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);

  return result;
}

std::string decodeHex(std::string const& str) {
  char* tmp;
  size_t len;

  tmp = TRI_DecodeHexString(str.c_str(), str.length(), &len);
  auto result = std::string(tmp, len);
  TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);

  return result;
}

bool gzipUncompress(char const* compressed, size_t compressedLength, std::string& uncompressed) {
  uncompressed.clear();

  if (compressedLength == 0) {
    /* empty input */
    return true;
  }  

  z_stream strm;  
  memset(&strm, 0, sizeof(strm));
  strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed));
  strm.avail_in = compressedLength; 

  if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {  
    return false;  
  }  
  
  int ret;
  char outbuffer[32768];

  do {
    strm.next_out = reinterpret_cast<Bytef*>(outbuffer);
    strm.avail_out = sizeof(outbuffer);

    ret = inflate(&strm, 0);

    if (uncompressed.size() < strm.total_out) {
      uncompressed.append(outbuffer, strm.total_out - uncompressed.size());
    }
  } while (ret == Z_OK);
  
  inflateEnd(&strm);

  return (ret == Z_STREAM_END);
}  

bool gzipUncompress(std::string const& compressed, std::string& uncompressed) {
  return gzipUncompress(compressed.c_str(), compressed.size(), uncompressed);
}

bool gzipDeflate(char const* compressed, size_t compressedLength, std::string& uncompressed) {
  uncompressed.clear();

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed));
  strm.avail_in = compressedLength;
  
  if (inflateInit(&strm) != Z_OK) {
    return false;
  }

  int ret;
  char outbuffer[32768];

  do {
    strm.next_out = reinterpret_cast<Bytef*>(outbuffer);
    strm.avail_out = sizeof(outbuffer);

    ret = inflate(&strm, 0);

    if (uncompressed.size() < strm.total_out) {
      uncompressed.append(outbuffer, strm.total_out - uncompressed.size());
    }
  } while (ret == Z_OK);

  inflateEnd(&strm);

  return (ret == Z_STREAM_END);
}

bool gzipDeflate(std::string const& compressed, std::string& uncompressed) {
  return gzipDeflate(compressed.c_str(), compressed.size(), uncompressed);
}

}
}
}
