////////////////////////////////////////////////////////////////////////////////
/// @brief collection of string utility functions
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
/// @author Copyright 2005-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "StringUtils.h"

#include <math.h>
#include <time.h>

#include <iostream>

#include "BasicsC/tri-strings.h"
#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"

// TODO need to this in another way
#ifdef GLOBAL_TIMEZONE
extern long GLOBAL_TIMEZONE;
#else
static long GLOBAL_TIMEZONE = 0;
#endif

using namespace std;

// -----------------------------------------------------------------------------
// helper functions
// -----------------------------------------------------------------------------

namespace {
  bool isSpace (char a) {
    return a == ' ' || a == '\t' || a == '_';
  }



  int32_t matchInteger (char const * & str, size_t size) {
    int32_t result = 0;

    for (size_t i = 0;  i < size;  i++, str++) {
      if ('0' <= *str && *str <= '9') {
        result = result * 10 + (*str - '0');
      }
      else {
        THROW_PARSE_ERROR("cannot parse date, expecting integer, got '" + string(1, *str) + "'");
      }
    }

    return result;
  }



  int32_t matchInteger (char const * & str) {
    int32_t result = 0;

    for (;  *str;  str++) {
      if ('0' <= *str && *str <= '9') {
        result = result * 10 + (*str - '0');
      }
      else {
        return result;
      }
    }

    return result;
  }



  char const * const BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";



  char const * const BASE64U_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";



  unsigned char const BASE64_REVS[256] = {
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   //   0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   //  16
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   62, '\0', '\0', '\0',   63,   //  32 ' ', '!'
      52,   53,   54,   55,   56,   57,   58,   59,   60,   61, '\0', '\0', '\0', '\0', '\0', '\0',   //  48 '0', '1'
    '\0',    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   //  64 '@', 'A'
      15,   16,   17,   18,   19,   20,   21,   22,   23,   24,   25, '\0', '\0', '\0', '\0', '\0',   //  80
    '\0',   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,   //  96 '`', 'a'
      41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51, '\0', '\0', '\0', '\0', '\0',   // 112
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 128
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 144
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 160
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 176
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 192
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 208
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 224
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 240
  };



  unsigned char const BASE64U_REVS[256] = {
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   //   0
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   //  16
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   62, '\0', '\0',   //  32 ' ', '!'
      52,   53,   54,   55,   56,   57,   58,   59,   60,   61, '\0', '\0', '\0', '\0', '\0', '\0',   //  48 '0', '1'
    '\0',    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   //  64 '@', 'A'
      15,   16,   17,   18,   19,   20,   21,   22,   23,   24,   25, '\0', '\0', '\0', '\0',   63,   //  80
    '\0',   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,   //  96 '`', 'a'
      41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51, '\0', '\0', '\0', '\0', '\0',   // 112
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 128
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 144
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 160
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 176
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 192
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 208
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 224
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // 240
  };



  inline bool isBase64 (unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
  }



  inline bool isBase64U (unsigned char c) {
    return (isalnum(c) || (c == '-') || (c == '_'));
  }



  bool parseHexanumber (char const *inputStr, size_t len, uint32_t * outputInt) {
    const uint32_t charVal[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    bool ok = true;
    for (size_t j = 0; j < len; j++) {
      if (inputStr[j] > '/' && inputStr[j] < ':') {
        *outputInt = (*outputInt << 4) | charVal[inputStr[j] - 48];
      }
      else if (inputStr[j] > '@' && inputStr[j] < 'G') {
        *outputInt = (*outputInt << 4) | charVal[inputStr[j] - 55];
      }
      else if (inputStr[j] > '`' && inputStr[j] < 'g') {
        *outputInt = (*outputInt << 4) | charVal[inputStr[j] - 87];
      }
      else {
        // invalid sequence of characters received for now simply return whatever we received
        // *outputStr = string(inputStr,len);
        ok = false;
        break;
      }
    }
    return ok;
  }
///-------------------------------------------------------
/// @brief computes the unicode value of an ut16 symbol
///-------------------------------------------------------

  bool toUnicode (uint16_t w1, uint16_t w2, uint32_t *v) {
    uint32_t vh = w1 - 0xD800;
    uint32_t vl = w2 - 0xDC00;
    uint32_t vp = (vh << 10) | vl;
    *v = vp + 0x10000;
    return true;
  }

  bool toUtf8 (uint32_t outputInt, string & outputStr) {
    if ((outputInt >> 7) == 0) {
      outputStr.append(1, (char) (outputInt));
    }
    // unicode sequence is in the range \u0080 to \u07ff
    else if ((outputInt >> 11) == 0) {
      outputStr.append(1, (char) ((3 << 6) | (outputInt >> 6)));
      outputStr.append(1, (char) ((1 << 7) | (outputInt & 63)));
    }
    // unicode sequence is in the range \u0800 to \uffff
    else if ((outputInt >> 16) == 0) {
      outputStr.append(1, (char) ((7 << 5) | (outputInt >> 12)));
      outputStr.append(1, (char) ((1 << 7) | ((outputInt & 4095 /* 2^12 - 1*/) >> 6)));
      outputStr.append(1, (char) ((1 << 7) | (outputInt & 63)));
    }
    else if ((outputInt >> 21) == 0) {
      outputStr.append(1, (char) ((15 << 4) | (outputInt >> 18)));
      outputStr.append(1, (char) ((1 << 7) | ((outputInt & 262143 /* 2^18 - 1*/) >> 12)));
      outputStr.append(1, (char) ((1 << 7) | ((outputInt & 4095 /* 2^12 - 1*/) >> 6)));
      outputStr.append(1, (char) ((1 << 7) | ((outputInt & 63))));
    }
    else {
      // can't handle it yet (need 5,6 etc utf8 characters), just send back the string we got
      return false;
    }
    return true;
  }
///-------------------------------------------------------
/// @brief true when number lays in the range
///        U+D800  U+DBFF
///-------------------------------------------------------
  bool isHighSurrugate (uint32_t number) {
    return (number >= 0xD800) && (number <= 0xDBFF);
  }
///-------------------------------------------------------
/// @brief true when number lays in the range
///        U+DC00  U+DFFF
  bool isLowSurrugate (uint32_t number) {
    return (number >= 0xDC00) && (number <= 0xDFFF);
  }
}

namespace triagens {
  namespace basics {
    namespace StringUtils {

// -----------------------------------------------------------------------------
      // STRING AND STRING POINTER
// -----------------------------------------------------------------------------

      blob_t duplicateBlob (const blob_t&  source) {
        blob_t result = {0,0};

        if (source.length == 0 || source.data == 0) {
          return result;
        }

        result.data = new char[source.length];

        memcpy(const_cast<char*>(result.data), source.data, source.length);
        result.length = source.length;

        return result;
      }



      blob_t duplicateBlob (char const* source, size_t len) {
        blob_t result = {0,0};

        if (source == 0 || len == 0) {
          return result;
        }

        result.data = new char[len];

        memcpy(const_cast<char*>(result.data), source, len);
        result.length = len;

        return result;
      }



      blob_t duplicateBlob (const string& source) {
        blob_t result = {0,0};

        if (source.size() == 0) {
          return result;
        }

        result.data = new char[source.size()];

        memcpy(const_cast<char*>(result.data), source.c_str(), source.size());
        result.length = source.size();

        return result;
      }



      char* duplicate (string const& source) {
        size_t len = source.size();
        char* result = new char[len + 1];

        memcpy(result, source.c_str(), len);
        result[len] = '\0';

        return result;
      }



      char* duplicate (char const* source, size_t len) {
        if (source == 0) {
          return 0;
        }

        char* result = new char[len + 1];

        memcpy(result, source, len);
        result[len] = '\0';

        return result;
      }



      char* duplicate (char const* source) {
        if (source == 0) {
          return 0;
        }
        size_t len = strlen(source);
        char* result = new char[len + 1];

        memcpy(result, source, len);
        result[len] = '\0';

        return result;
      }



      void destroy (char*& source) {
        if (source != 0) {
          ::memset(source, 0, ::strlen(source));
          delete[] source;
          source = 0;
        }
      }



      void destroy (char*& source, size_t length) {
        if (source != 0) {
          ::memset(source, 0, length);
          delete[] source;
          source = 0;
        }
      }



      void destroy (blob_t& source) {
        if (source.data != 0) {
          ::memset(const_cast<char*>(source.data), 0, source.length);
          delete[] source.data;
          source.data = 0;
          source.length = 0;
        }
      }


      void erase (char*& source) {
        if (source != 0) {
          delete[] source;
          source = 0;
        }
      }



      void erase (char*& source, size_t length) {
        if (source != 0) {
          delete[] source;
          source = 0;
        }
      }



      void erase (blob_t& source) {
        if (source.data != 0) {
          delete[] source.data;
          source.data = 0;
          source.length = 0;
        }
      }

// -----------------------------------------------------------------------------
      // STRING CONVERSION
// -----------------------------------------------------------------------------

      string capitalize (string const& name, bool first) {
        size_t len = name.length();

        if (len == 0) {
          THROW_PARSE_ERROR("name must not be empty");
        }

        char * buffer = new char[len + 1];
        char * qtr = buffer;
        char const * ptr = name.c_str();

        for (; 0 < len && isSpace(*ptr);  len--, ptr++) {
        }

        if (len == 0) {
          THROW_PARSE_ERROR("object or attribute name must not be empty");
        }

        bool upper = first;

        for (; 0 < len;  len--, ptr++) {
          if (isSpace(*ptr)) {
            upper = true;
          }
          else {
            *qtr++ = upper ? static_cast<char>(::toupper(*ptr)) : static_cast<char>(::tolower(*ptr));
            upper = false;
          }
        }

        *qtr = '\0';

        string result(buffer);

        delete[] buffer;

        return result;
      }



      string separate (string const& name, char separator) {
        size_t len = name.length();

        if (len == 0) {
          THROW_PARSE_ERROR("name must not be empty");
        }

        char * buffer = new char[len + 1];
        char * qtr = buffer;
        char const * ptr = name.c_str();

        for (; 0 < len && isSpace(*ptr);  len--, ptr++) {
        }

        if (len == 0) {
          THROW_PARSE_ERROR("name must not be empty");
        }

        bool useSeparator = false;

        for (; 0 < len;  len--, ptr++) {
          for (;  0 < len && isSpace(*ptr);  len--, ptr++) {
            useSeparator = true;
          }

          if (0 < len) {
            if (useSeparator) {
              *qtr++ = separator;
              useSeparator = false;
            }

            *qtr++ = static_cast<char>(::tolower(*ptr));
          }
          else {
            break;
          }
        }

        *qtr = '\0';

        string result(buffer);

        delete[] buffer;

        return result;
      }



      string escape (string const& name, string const& special, char quote) {
        size_t len = name.length();

        if (len == 0) {
          return name;
        }

        if (len >= (SIZE_MAX - 1) / 2) {
          THROW_OUT_OF_MEMORY_ERROR();
        }

        char * buffer = new char [2 * len + 1];
        char * qtr = buffer;
        char const * ptr = name.c_str();
        char const * end = ptr + len;

        if (special.length() == 0) {
          for (;  ptr < end;  ptr++, qtr++) {
            if (*ptr == quote) {
              *qtr++ = quote;
            }

            *qtr = *ptr;
          }
        }
        else if (special.length() == 1) {
          char s = special[0];

          for (;  ptr < end;  ptr++, qtr++) {
            if (*ptr == quote || *ptr == s) {
              *qtr++ = quote;
            }

            *qtr = *ptr;
          }
        }
        else {
          for (;  ptr < end;  ptr++, qtr++) {
            if (*ptr == quote || special.find(*ptr) != string::npos) {
              *qtr++ = quote;
            }

            *qtr = *ptr;
          }
        }

        *qtr = '\0';

        string result(buffer, qtr - buffer);

        delete[] buffer;

        return result;
      }



      string escape (string const& name, size_t len, string const& special, char quote) {
        if (len == 0) {
          return name;
        }

        if (len >= (SIZE_MAX - 1) / 2) {
          THROW_OUT_OF_MEMORY_ERROR();
        }

        char * buffer = new char [2 * len + 1];
        char * qtr = buffer;
        char const * ptr = name.c_str();
        char const * end = ptr + len;

        if (special.length() == 0) {
          for (;  ptr < end;  ptr++, qtr++) {
            if (*ptr == quote) {
              *qtr++ = quote;
            }

            *qtr = *ptr;
          }
        }
        else if (special.length() == 1) {
          char s = special[0];

          for (;  ptr < end;  ptr++, qtr++) {
            if (*ptr == quote || *ptr == s) {
              *qtr++ = quote;
            }

            *qtr = *ptr;
          }
        }
        else {
          for (;  ptr < end;  ptr++, qtr++) {
            if (*ptr == quote || special.find(*ptr) != string::npos) {
              *qtr++ = quote;
            }

            *qtr = *ptr;
          }
        }

        *qtr = '\0';

        string result(buffer, qtr - buffer);

        delete[] buffer;

        return result;
      }



      string escapeUnicode (string const& name, bool escapeSlash) {
        size_t len = name.length();

        if (len == 0) {
          return name;
        }

        if (len >= (SIZE_MAX - 1) / 6) {
          THROW_OUT_OF_MEMORY_ERROR();
        }

        char * buffer = new char [6 * len + 1];
        char * qtr = buffer;
        char const * ptr = name.c_str();
        char const * end = ptr + len;

        for (;  ptr < end;  ++ptr, ++qtr) {
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
                uint8_t c = (uint8_t) *ptr;

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
                    *qtr   = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
                  }

                  // normal latin1
                  else {
                    *qtr = *ptr;
                  }
                }

                // unicode range 0080 - 07ff
                else if ((c & 0xE0) == 0xC0) {
                  if (ptr + 1 < end) {
                    uint8_t d = (uint8_t) *(ptr + 1);

                    // correct unicode
                    if ((d & 0xC0) == 0x80) {
                      ++ptr;

                      *qtr++ = '\\';
                      *qtr++ = 'u';

                      uint16_t n = ((c & 0x1F) << 6) | (d & 0x3F);

                      uint16_t i1 = (n & 0xF000) >> 12;
                      uint16_t i2 = (n & 0x0F00) >>  8;
                      uint16_t i3 = (n & 0x00F0) >>  4;
                      uint16_t i4 = (n & 0x000F);

                      *qtr++ = (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10);
                      *qtr++ = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
                      *qtr++ = (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10);
                      *qtr   = (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10);
                    }

                    // corrupted unicode
                    else {
                      *qtr = *ptr;
                    }
                  }

                  // corrupted unicode
                  else {
                    *qtr = *ptr;
                  }
                }

                // unicode range 0800 - ffff
                else if ((c & 0xF0) == 0xE0) {
                  if (ptr + 1 < end) {
                    uint8_t d = (uint8_t) *(ptr + 1);

                    // correct unicode
                    if ((d & 0xC0) == 0x80) {
                      if (ptr + 2 < end) {
                        uint8_t e = (uint8_t) *(ptr + 2);

                        // correct unicode
                        if ((e & 0xC0) == 0x80) {
                          // TO DO:
                          *qtr = *ptr;
                        }
                        // corrupted unicode
                        else {
                          *qtr = *ptr;
                        }
                      }
                      // corrupted unicode
                      else {
                        *qtr = *ptr;
                      }
                    }
                    // corrupted unicode
                    else {
                      *qtr = *ptr;
                    }
                  }
                  // corrupted unicode
                  else {
                    *qtr = *ptr;
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

        string result(buffer, qtr - buffer);

        delete[] buffer;

        return result;
      }



      string escapeHtml (string const& name) {
        size_t len = name.length();

        if (len == 0) {
          return name;
        }

        if (len >= (SIZE_MAX - 1) / 8) {
          THROW_OUT_OF_MEMORY_ERROR();
        }

        char * buffer = new char [8 * len + 1];
        char * qtr = buffer;
        char const * ptr = name.c_str();
        char const * end = ptr + len;

        for (;  ptr < end;  ++ptr, ++qtr) {
          if (*ptr == '<') {
            *qtr++ = '&';
            *qtr++ = 'l';
            *qtr++ = 't';
            *qtr   = ';';
          }
          else if (*ptr == '>') {
            *qtr++ = '&';
            *qtr++ = 'g';
            *qtr++ = 't';
            *qtr   = ';';
          }
          else if (*ptr == '&') {
            *qtr++ = '&';
            *qtr++ = 'a';
            *qtr++ = 'm';
            *qtr++ = 'p';
            *qtr   = ';';
          }
          else if (*ptr == '"') {
            *qtr++ = '&';
            *qtr++ = 'q';
            *qtr++ = 'u';
            *qtr++ = 'o';
            *qtr++ = 't';
            *qtr   = ';';
          }
          else {
            *qtr = *ptr;
          }
        }

        *qtr = '\0';

        string result(buffer, qtr - buffer);

        delete[] buffer;

        return result;
      }




      string escapeXml (string const& name) {
        return escapeHtml(name);
      }




      string escapeHex (string const& name, char quote) {
        size_t len = name.length();

        if (len == 0) {
          return name;
        }

        if (len >= (SIZE_MAX - 1) / 3) {
          THROW_OUT_OF_MEMORY_ERROR();
        }

        char * buffer = new char [3 * len + 1];
        char * qtr = buffer;
        char const * ptr = name.c_str();
        char const * end = ptr + len;

        for (;  ptr < end;  ptr++, qtr++) {
          if (*ptr == quote || *ptr <= ' ' || static_cast<uint8_t>(*ptr) >= 128) {
            uint8_t n = (uint8_t)(*ptr);
            uint8_t n1 = n >> 4;
            uint8_t n2 = n & 0x0F;

            *qtr++ = quote;
            *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
            *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
          }
          else {
            *qtr = *ptr;
          }
        }

        *qtr = '\0';

        string result(buffer, qtr - buffer);

        delete[] buffer;

        return result;
      }



      string escapeHex (string const& name, string const& special, char quote) {
        size_t len = name.length();

        if (len == 0) {
          return name;
        }

        if (len >= (SIZE_MAX - 1) / 3) {
          THROW_OUT_OF_MEMORY_ERROR();
        }

        char * buffer = new char [3 * len + 1];
        char * qtr = buffer;
        char const * ptr = name.c_str();
        char const * end = ptr + len;

        if (special.length() == 0) {
          for (;  ptr < end;  ptr++, qtr++) {
            if (*ptr == quote) {
              uint8_t n = (uint8_t)(*ptr);
              uint8_t n1 = n >> 4;
              uint8_t n2 = n & 0x0F;

              *qtr++ = quote;
              *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
              *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
            }
            else {
              *qtr = *ptr;
            }
          }
        }
        else if (special.length() == 1) {
          char s = special[0];

          for (;  ptr < end;  ptr++, qtr++) {
            if (*ptr == quote || *ptr == s) {
              uint8_t n = (uint8_t)(*ptr);
              uint8_t n1 = n >> 4;
              uint8_t n2 = n & 0x0F;

              *qtr++ = quote;
              *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
              *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
            }
            else {
              *qtr = *ptr;
            }
          }
        }
        else {
          for (;  ptr < end;  ptr++, qtr++) {
            if (*ptr == quote || special.find(*ptr) != string::npos) {
              uint8_t n = (uint8_t)(*ptr);
              uint8_t n1 = n >> 4;
              uint8_t n2 = n & 0x0F;

              *qtr++ = quote;
              *qtr++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
              *qtr = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
            }
            else {
              *qtr = *ptr;
            }
          }
        }

        *qtr = '\0';

        string result(buffer, qtr - buffer);

        delete[] buffer;

        return result;
      }



      string escapeC (string const& name) {
        size_t len = name.length();

        if (len == 0) {
          return name;
        }

        if (len >= (SIZE_MAX - 1) / 4) {
          THROW_OUT_OF_MEMORY_ERROR();
        }

        char * buffer = new char [4 * len + 1];
        char * qtr = buffer;
        char const * ptr = name.c_str();
        char const * end = ptr + len;

        for (;  ptr < end;  ptr++, qtr++) {
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
              }
              else {
                *qtr = *ptr;
              }

              break;
          }
        }

        *qtr = '\0';

        string result(buffer, qtr - buffer);

        delete[] buffer;

        return result;
      }



      vector<string> split (string const& source, char delim, char quote) {
        vector<string> result;

        if (source.empty()) {
          return result;
        }

        char* buffer = new char[source.size() + 1];
        char* p = buffer;

        char const* q = source.c_str();
        char const* e = source.c_str() + source.size();

        if (quote == '\0') {
          for (;  q < e;  ++q) {
            if (*q == delim) {
              *p = '\0';
              result.push_back(string(buffer, p - buffer));
              p = buffer;
            }
            else {
              *p++ = *q;
            }
          }
        }
        else {
          for (;  q < e;  ++q) {
            if (*q == quote) {
              if (q + 1 < e) {
                *p++ = *++q;
              }
            }
            else if (*q == delim) {
              *p = '\0';
              result.push_back(string(buffer, p - buffer));
              p = buffer;
            }
            else {
              *p++ = *q;
            }
          }
        }

        *p = '\0';
        result.push_back(string(buffer, p - buffer));

        delete[] buffer;

        return result;
      }



      vector<string> split (string const& source, string const& delim, char quote) {
        vector<string> result;

        if (source.empty()) {
          return result;
        }

        char* buffer = new char[source.size() + 1];
        char* p = buffer;

        char const* q = source.c_str();
        char const* e = source.c_str() + source.size();

        if (quote == '\0') {
          for (;  q < e;  ++q) {
            if (delim.find(*q) != string::npos) {
              *p = '\0';
              result.push_back(string(buffer, p - buffer));
              p = buffer;
            }
            else {
              *p++ = *q;
            }
          }
        }
        else {
          for (;  q < e;  ++q) {
            if (*q == quote) {
              if (q + 1 < e) {
                *p++ = *++q;
              }
            }
            else if (delim.find(*q) != string::npos) {
              *p = '\0';
              result.push_back(string(buffer, p - buffer));
              p = buffer;
            }
            else {
              *p++ = *q;
            }
          }
        }

        *p = '\0';
        result.push_back(string(buffer, p - buffer));

        delete[] buffer;

        return result;
      }



      string join (vector<string> const& source, char delim) {
        string result = "";
        bool first = true;

        for (vector<string>::const_iterator i = source.begin();  i != source.end();  ++i) {
          if (first) {
            first = false;
          }
          else {
            result += delim;
          }

          result += *i;
        }

        return result;
      }



      string join (vector<string> const& source, string const& delim) {
        string result = "";
        bool first = true;

        for (vector<string>::const_iterator i = source.begin();  i != source.end();  ++i) {
          if (first) {
            first = false;
          }
          else {
            result += delim;
          }

          result += *i;
        }

        return result;
      }



      string join (set<string> const& source, char delim) {
        string result = "";
        bool first = true;

        for (set<string>::const_iterator i = source.begin();  i != source.end();  ++i) {
          if (first) {
            first = false;
          }
          else {
            result += delim;
          }

          result += *i;
        }

        return result;
      }



      string join (set<string> const& source, string const& delim) {
        string result = "";
        bool first = true;

        for (set<string>::const_iterator i = source.begin();  i != source.end();  ++i) {
          if (first) {
            first = false;
          }
          else {
            result += delim;
          }

          result += *i;
        }

        return result;
      }



      string trim (string const& sourceStr, string const& trimStr) {
        size_t s = sourceStr.find_first_not_of(trimStr);
        size_t e = sourceStr.find_last_not_of(trimStr);

        if (s == std::string::npos) {
          return string();
        }
        else {
          return string(sourceStr, s, e - s + 1);
        }
      }



      void trimInPlace (string& str, string const& trimStr) {
        size_t s = str.find_first_not_of(trimStr);
        size_t e = str.find_last_not_of(trimStr);

        if (s == std::string::npos) {
          str.clear();
        }
        else if (s == 0 && e == str.length() - 1) {
          // nothing to do
        }
        else if (s == 0) {
          str.erase(e + 1);
        }
        else {
          str = str.substr(s, e-s+1);
        }
      }



      string lTrim (string const& str, string const& trimStr) {
        size_t s = str.find_first_not_of(trimStr);

        if (s == std::string::npos) {
          return string();
        }
        else {
          return string(str, s);
        }
      }



      string rTrim (string const& sourceStr, string const& trimStr) {
        size_t e = sourceStr.find_last_not_of(trimStr);

        return string(sourceStr, 0, e + 1);
      }



      string lFill (string const& sourceStr, size_t size, char fill) {
        size_t l = sourceStr.size();

        if (l >= size) {
          return sourceStr;
        }

        return string(size - l, fill) + sourceStr;
      }



      string rFill (string const& sourceStr, size_t size, char fill) {
        size_t l = sourceStr.size();

        if (l >= size) {
          return sourceStr;
        }

        return sourceStr + string(size - l, fill);
      }



      vector<string> wrap (string const& sourceStr, size_t size, string breaks) {
        vector<string> result;
        string next = sourceStr;

        if (size > 0) {
          while (next.size() > size) {
            size_t m = next.find_last_of(breaks, size - 1);

            if (m == string::npos || m < size / 2) {
              m = size;
            }
            else {
              m += 1;
            }

            result.push_back(next.substr(0, m));
            next = next.substr(m);
          }
        }

        result.push_back(next);

        return result;
      }



/// replaces the contents of the sourceStr = "aaebbbbcce" where ever the occurence of
/// fromStr = "bb" exists with the toStr = "dd". No recursion peformed on the replaced string
/// e.g. replace("aaebbbbcce","bb","dd") = "aaeddddcce"
/// e.g. replace("aaebbbbcce","bb","bbb") = "aaebbbbbbcce"
/// e.g. replace("aaebbbbcce","bbb","bb") = "aaebbbcce"

      string replace (string const& sourceStr, string const& fromStr, string const& toStr) {
        size_t fromLength   = fromStr.length();
        size_t toLength     = toStr.length();
        size_t maxLength    = max (fromLength,toLength);
        size_t sourceLength = sourceStr.length();
        bool match;

        // cannot perform a replace if the sourceStr = "" or fromStr = ""
        if (fromLength == 0 || sourceLength == 0) {
          return (sourceStr);
        }

        // the max amount of memory is:
        size_t mt = max(static_cast<size_t>(1),toLength);

        if ((sourceLength / fromLength) + 1 >= (SIZE_MAX - toLength) / mt) {
          THROW_OUT_OF_MEMORY_ERROR();
        }

        maxLength = (((sourceLength / fromLength) + 1) * mt) + toLength;

        // the min amount of memory we have to allocate for the "replace" (new) string is length of sourceStr
        maxLength = max(maxLength, sourceLength) + 1;

        char* result = new char[maxLength];
        size_t k = 0;

        for (size_t j = 0; j < sourceLength; ++j) {

          match = true;

          for (size_t i = 0; i < fromLength; ++i) {
            if (sourceStr[j + i] != fromStr[i]) {
              match = false;
              break;
            }
          }

          if (! match) {
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

        string retStr(result);

        delete[] result;

        return retStr;
      }


      void tolowerInPlace (string* str) {
        size_t len = str->length();

        if (len == 0) {
          return;
        }

        for (string::iterator i = str->begin(); i != str->end(); ++i) {
          *i = ::tolower(*i);
        }
      }


      string tolower (string const& str) {
        size_t len = str.length();

        if (len == 0) {
          return "";
        }

        char * buffer = new char[len];
        char * qtr = buffer;
        char const * ptr = str.c_str();

        for (; 0 < len;  len--, ptr++, qtr++) {
          *qtr = static_cast<char>(::tolower(*ptr));
        }

        string result(buffer, str.size());

        delete[] buffer;

        return result;
      }


      void toupperInPlace (string* str) {
        size_t len = str->length();

        if (len == 0) {
          return;
        }

        for (string::iterator i = str->begin(); i != str->end(); ++i) {
          *i = ::toupper(*i);
        }
      }


      string toupper (string const& str) {
        size_t len = str.length();

        if (len == 0) {
          return "";
        }

        char * buffer = new char[len];
        char * qtr = buffer;
        char const * ptr = str.c_str();

        for (; 0 < len;  len--, ptr++, qtr++) {
          *qtr = static_cast<char>(::toupper(*ptr));
        }

        string result(buffer, str.size());
        delete[] buffer;
        return result;
      }



      bool isPrefix (string const& str, string const& prefix) {
        if (prefix.length() > str.length()) {
          return false;
        }
        else if (prefix.length() == str.length()) {
          return str == prefix;
        }
        else {
          return str.compare(0, prefix.length(), prefix) == 0;
        }
      }



      bool isSuffix (string const& str, string const& postfix) {
        if (postfix.length() > str.length()) {
          return false;
        }
        else if (postfix.length() == str.length()) {
          return str == postfix;
        }
        else {
          return str.compare(str.size() - postfix.length(), postfix.length(), postfix) == 0;
        }
      }



      string urlDecode (string const& str) {
        char const* src = str.c_str();
        char const* end = src + str.size();

        char* buffer = new char[str.size() + 1];
        char* ptr = buffer;

        for (;  src < end && *src != '%';  ++src) {
          if (*src == '+') {
            *ptr++ = ' ';
          }
          else {
            *ptr++ = *src;
          }
        }

        while (src < end) {
          if (src + 2 < end) {
            int h1 = hex2int(src[1], -1);
            int h2 = hex2int(src[2], -1);

            if (h1 == -1) {
              src += 1;
            }
            else {
              if (h2 == -1) {
                *ptr++ = h1;
                src += 2;
              }
              else {
                *ptr++ = h1 << 4 | h2;
                src += 3;
              }
            }
          }
          else if (src + 1 < end) {
            int h1 = hex2int(src[1], -1);

            if (h1 == -1) {
              src += 1;
            }
            else {
              *ptr++ = h1;
              src += 2;
            }
          }
          else {
            src += 1;
          }

          for (;  src < end && *src != '%';  ++src) {
            if (*src == '+') {
              *ptr++ = ' ';
            }
            else {
              *ptr++ = *src;
            }
          }
        }

        *ptr = '\0';
        string result(buffer, ptr - buffer);

        delete[] buffer;

        return result;
      }



      string urlEncode (string const& str) {
        return urlEncode(str.c_str(),str.size());
      }



      string urlEncode (const char* src) {
        if (src != 0) {
          size_t len = strlen(src);
          return urlEncode(src,len);
        }
        return "";
      }



      string urlEncode (const char* src, const size_t len) {
        static char hexChars[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

        char const* end = src + len;

        if (len >= (SIZE_MAX - 1) / 3) {
          THROW_OUT_OF_MEMORY_ERROR();
        }

        char* buffer = new char[3 * len + 1];
        char* ptr = buffer;


        for (; src < end;  ++src) {
          if ('0' <= *src && *src <= '9') {
            *ptr++ = *src;
          }

          else if ('a' <= *src && *src <= 'z') {
            *ptr++ = *src;
          }

          else if ('A' <= *src && *src <= 'Z') {
            *ptr++ = *src;
          }

          else if (*src == '-' || *src == '_' || *src == '.' || *src == '~') {
            *ptr++ = *src;
          }

          else {
            uint8_t n = (uint8_t)(*src);
            uint8_t n1 = n >> 4;
            uint8_t n2 = n & 0x0F;

            *ptr++ = '%';
            *ptr++ = hexChars[n1];
            *ptr++ = hexChars[n2];
          }
        }

        *ptr = '\0';
        string result(buffer, ptr - buffer);

        delete[] buffer;

        return result;
      }

// -----------------------------------------------------------------------------
      // CONVERT TO STRING
// -----------------------------------------------------------------------------

      string itoa (int16_t attr) {
        char buffer[7];
        char* p = buffer;

        if (attr == INT16_MIN) {
          return "-32768";
        }

        if (attr < 0) {
          *p++ = '-';
          attr = -attr;
        }

        if (     10000L <= attr) { *p++ = char((attr /      10000L) % 10 + '0'); }
        if (      1000L <= attr) { *p++ = char((attr /       1000L) % 10 + '0'); }
        if (       100L <= attr) { *p++ = char((attr /        100L) % 10 + '0'); }
        if (        10L <= attr) { *p++ = char((attr /         10L) % 10 + '0'); }

        *p++ = char(attr % 10 + '0');
        *p = '\0';

        return buffer;
      }



      string itoa (uint16_t attr) {
        char buffer[6];
        char* p = buffer;

        if (     10000L <= attr) { *p++ = char((attr /      10000L) % 10 + '0'); }
        if (      1000L <= attr) { *p++ = char((attr /       1000L) % 10 + '0'); }
        if (       100L <= attr) { *p++ = char((attr /        100L) % 10 + '0'); }
        if (        10L <= attr) { *p++ = char((attr /         10L) % 10 + '0'); }

        *p++ = char(attr % 10 + '0');
        *p = '\0';

        return buffer;
      }



      string itoa (int32_t attr) {
        char buffer[12];
        char* p = buffer;

        if (attr == INT32_MIN) {
          return "-2147483648";
        }

        if (attr < 0) {
          *p++ = '-';
          attr = -attr;
        }

        if (1000000000L <= attr) { *p++ = char((attr / 1000000000L) % 10 + '0'); }
        if ( 100000000L <= attr) { *p++ = char((attr /  100000000L) % 10 + '0'); }
        if (  10000000L <= attr) { *p++ = char((attr /   10000000L) % 10 + '0'); }
        if (   1000000L <= attr) { *p++ = char((attr /    1000000L) % 10 + '0'); }
        if (    100000L <= attr) { *p++ = char((attr /     100000L) % 10 + '0'); }
        if (     10000L <= attr) { *p++ = char((attr /      10000L) % 10 + '0'); }
        if (      1000L <= attr) { *p++ = char((attr /       1000L) % 10 + '0'); }
        if (       100L <= attr) { *p++ = char((attr /        100L) % 10 + '0'); }
        if (        10L <= attr) { *p++ = char((attr /         10L) % 10 + '0'); }

        *p++ = char(attr % 10 + '0');
        *p = '\0';

        return buffer;
      }



      string itoa (uint32_t attr) {
        char buffer[11];
        char* p = buffer;

        if (1000000000L <= attr) { *p++ = char((attr / 1000000000L) % 10 + '0'); }
        if ( 100000000L <= attr) { *p++ = char((attr /  100000000L) % 10 + '0'); }
        if (  10000000L <= attr) { *p++ = char((attr /   10000000L) % 10 + '0'); }
        if (   1000000L <= attr) { *p++ = char((attr /    1000000L) % 10 + '0'); }
        if (    100000L <= attr) { *p++ = char((attr /     100000L) % 10 + '0'); }
        if (     10000L <= attr) { *p++ = char((attr /      10000L) % 10 + '0'); }
        if (      1000L <= attr) { *p++ = char((attr /       1000L) % 10 + '0'); }
        if (       100L <= attr) { *p++ = char((attr /        100L) % 10 + '0'); }
        if (        10L <= attr) { *p++ = char((attr /         10L) % 10 + '0'); }

        *p++ = char(attr % 10 + '0');
        *p = '\0';

        return buffer;
      }



      string itoa (int64_t attr) {
        char buffer[21];
        char* p = buffer;

        if (attr == INT64_MIN) {
          return "-9223372036854775808";
        }

        if (attr < 0) {
          *p++ = '-';
          attr = -attr;
        }

        if (1000000000000000000LL <= attr) { *p++ = char((attr / 1000000000000000000LL) % 10 + '0'); }
        if ( 100000000000000000LL <= attr) { *p++ = char((attr /  100000000000000000LL) % 10 + '0'); }
        if (  10000000000000000LL <= attr) { *p++ = char((attr /   10000000000000000LL) % 10 + '0'); }
        if (   1000000000000000LL <= attr) { *p++ = char((attr /    1000000000000000LL) % 10 + '0'); }
        if (    100000000000000LL <= attr) { *p++ = char((attr /     100000000000000LL) % 10 + '0'); }
        if (     10000000000000LL <= attr) { *p++ = char((attr /      10000000000000LL) % 10 + '0'); }
        if (      1000000000000LL <= attr) { *p++ = char((attr /       1000000000000LL) % 10 + '0'); }
        if (       100000000000LL <= attr) { *p++ = char((attr /        100000000000LL) % 10 + '0'); }
        if (        10000000000LL <= attr) { *p++ = char((attr /         10000000000LL) % 10 + '0'); }
        if (         1000000000LL <= attr) { *p++ = char((attr /          1000000000LL) % 10 + '0'); }
        if (          100000000LL <= attr) { *p++ = char((attr /           100000000LL) % 10 + '0'); }
        if (           10000000LL <= attr) { *p++ = char((attr /            10000000LL) % 10 + '0'); }
        if (            1000000LL <= attr) { *p++ = char((attr /             1000000LL) % 10 + '0'); }
        if (             100000LL <= attr) { *p++ = char((attr /              100000LL) % 10 + '0'); }
        if (              10000LL <= attr) { *p++ = char((attr /               10000LL) % 10 + '0'); }
        if (               1000LL <= attr) { *p++ = char((attr /                1000LL) % 10 + '0'); }
        if (                100LL <= attr) { *p++ = char((attr /                 100LL) % 10 + '0'); }
        if (                 10LL <= attr) { *p++ = char((attr /                  10LL) % 10 + '0'); }

        *p++ = char(attr % 10 + '0');
        *p = '\0';

        return buffer;
      }



      string itoa (uint64_t attr) {
        char buffer[21];
        char* p = buffer;

        if (10000000000000000000ULL <= attr) { *p++ = char((attr / 10000000000000000000ULL) % 10 + '0'); }
        if ( 1000000000000000000ULL <= attr) { *p++ = char((attr /  1000000000000000000ULL) % 10 + '0'); }
        if (  100000000000000000ULL <= attr) { *p++ = char((attr /   100000000000000000ULL) % 10 + '0'); }
        if (   10000000000000000ULL <= attr) { *p++ = char((attr /    10000000000000000ULL) % 10 + '0'); }
        if (    1000000000000000ULL <= attr) { *p++ = char((attr /     1000000000000000ULL) % 10 + '0'); }
        if (     100000000000000ULL <= attr) { *p++ = char((attr /      100000000000000ULL) % 10 + '0'); }
        if (      10000000000000ULL <= attr) { *p++ = char((attr /       10000000000000ULL) % 10 + '0'); }
        if (       1000000000000ULL <= attr) { *p++ = char((attr /        1000000000000ULL) % 10 + '0'); }
        if (        100000000000ULL <= attr) { *p++ = char((attr /         100000000000ULL) % 10 + '0'); }
        if (         10000000000ULL <= attr) { *p++ = char((attr /          10000000000ULL) % 10 + '0'); }
        if (          1000000000ULL <= attr) { *p++ = char((attr /           1000000000ULL) % 10 + '0'); }
        if (           100000000ULL <= attr) { *p++ = char((attr /            100000000ULL) % 10 + '0'); }
        if (            10000000ULL <= attr) { *p++ = char((attr /             10000000ULL) % 10 + '0'); }
        if (             1000000ULL <= attr) { *p++ = char((attr /              1000000ULL) % 10 + '0'); }
        if (              100000ULL <= attr) { *p++ = char((attr /               100000ULL) % 10 + '0'); }
        if (               10000ULL <= attr) { *p++ = char((attr /                10000ULL) % 10 + '0'); }
        if (                1000ULL <= attr) { *p++ = char((attr /                 1000ULL) % 10 + '0'); }
        if (                 100ULL <= attr) { *p++ = char((attr /                  100ULL) % 10 + '0'); }
        if (                  10ULL <= attr) { *p++ = char((attr /                   10ULL) % 10 + '0'); }

        *p++ = char(attr % 10 + '0');
        *p = '\0';

        return buffer;
      }



      string ftoa (double i) {
        StringBuffer buffer(TRI_CORE_MEM_ZONE);

        buffer.appendDecimal(i);

        string result(buffer.c_str());

        return result;
      }

// -----------------------------------------------------------------------------
      // CONVERT FROM STRING
// -----------------------------------------------------------------------------

      bool boolean (string const& str) {
        string lower = tolower(trim(str));

        if (lower == "true" || lower == "yes" || lower == "on" || lower == "y" || lower == "1") {
          return true;
        }
        else {
          return false;
        }
      }



      int64_t int64 (string const& str) {
#ifdef TRI_HAVE_STRTOLL_R
        struct reent buffer;
        return strtoll_r(&buffer, str.c_str(), 0, 10);
#else
#ifdef TRI_HAVE__STRTOLL_R
        struct reent buffer;
        return _strtoll_r(&buffer, str.c_str(), 0, 10);
#else
#ifdef TRI_HAVE_STRTOLL
        return strtoll(str.c_str(), 0, 10);
#else
        return stoll(str, 0, 10);
#endif
#endif
#endif
      }



      int64_t int64 (char const* value, size_t size) {
        char tmp[22];

        if (value[size] != '\0') {
          if (size >= sizeof(tmp)) {
            size = sizeof(tmp) - 1;
          }

          memcpy(tmp, value, size);
          tmp[size] = '\0';
          value = tmp;
        }

#ifdef TRI_HAVE_STRTOLL_R
        struct reent buffer;
        return strtoll_r(&buffer, value, 0, 10);
#else
#ifdef TRI_HAVE__STRTOLL_R
        struct reent buffer;
        return _strtoll_r(&buffer, value, 0, 10);
#else
#ifdef TRI_HAVE_STRTOLL
        return strtoll(value, 0, 10);
#else
        return stoll(string(value, size), 0, 10);
#endif
#endif
#endif
      }



      uint64_t uint64 (string const& str) {
#ifdef TRI_HAVE_STRTOULL_R
        struct reent buffer;
        return strtoull_r(&buffer, str.c_str(), 0, 10);
#else
#ifdef TRI_HAVE__STRTOULL_R
        struct reent buffer;
        return _strtoull_r(&buffer, str.c_str(), 0, 10);
#else
#ifdef TRI_HAVE_STRTOULL
        return strtoull(str.c_str(), 0, 10);
#else
        return stoull(str, 0, 10);
#endif
#endif
#endif
      }



      uint64_t uint64 (char const* value, size_t size) {
        char tmp[22];

        if (value[size] != '\0') {
          if (size >= sizeof(tmp)) {
            size = sizeof(tmp) - 1;
          }

          memcpy(tmp, value, size);
          tmp[size] = '\0';
          value = tmp;
        }

#ifdef TRI_HAVE_STRTOULL_R
        struct reent buffer;
        return strtoull_r(&buffer, value, 0, 10);
#else
#ifdef TRI_HAVE__STRTOULL_R
        struct reent buffer;
        return _strtoull_r(&buffer, value, 0, 10);
#else
#ifdef TRI_HAVE_STRTOULL
        return strtoull(value, 0, 10);
#else
        return stoull(string(value, size), 0, 10);
#endif
#endif
#endif
      }



      int32_t int32 (string const& str) {
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



      int32_t int32 (char const* value, size_t size) {
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



      uint32_t uint32 (string const& str) {
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



      uint32_t unhexUint32 (string const& str) {
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



      uint32_t uint32 (char const* value, size_t size) {
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



      uint32_t unhexUint32 (char const* value, size_t size) {
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



      double doubleDecimal (string const& str) {
        return doubleDecimal(str.c_str(), str.size());
      }



      double doubleDecimal (char const* value, size_t size) {
        double v = 0.0;
        double e = 1.0;

        bool seenDecimalPoint = false;

        uint8_t const* ptr = reinterpret_cast<uint8_t const*>(value);
        uint8_t const* end = ptr + size;

        // check for the sign first
        if (*ptr == '-') {
          e = -e;
          ++ptr;
        }
        else if (*ptr == '+') {
          ++ptr;
        }

        for (;  ptr < end;  ++ptr) {
          uint8_t n = *ptr;


          if (n == '.' && ! seenDecimalPoint) {
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

        ++ptr; // move past the 'e' or 'E'

        int32_t expSign  = 1;
        int32_t expValue = 0;

        // is there an exponent sign?
        if (*ptr == '-') {
          expSign = -1;
          ++ptr;
        }
        else if (*ptr == '+') {
          ++ptr;
        }

        for (;  ptr < end;  ++ptr) {
          uint8_t n = *ptr;

          if ('9' < n || n < '0') {
            return 0.0;
          }

          expValue = expValue * 10 + (n - 48);
        }

        expValue = expValue * expSign;

        return (v / e) * pow(10.0, double(expValue));

      }



      float floatDecimal (string const& str) {
        return floatDecimal(str.c_str(), str.size());
      }



      float floatDecimal (char const* value, size_t size) {
        return (float) doubleDecimal(value, size);
      }



      seconds_t seconds (string const& format, string const& str) {
        int32_t hour = 0;
        int32_t minute = 0;
        int32_t second = 0;

        char const * f = format.c_str();
        char const * s = str.c_str();

        while (*f) {
          if (*f == 'H') {
            if (strncmp(f, "HH", 2) == 0) {
              hour = matchInteger(s, 2);
              f += 2;
            }
            else {
              THROW_PARSE_ERROR("unknown time format '" + string(f) + "', expecting 'HH'");
            }
          }
          else if (*f == 'M') {
            if (strncmp(f, "MI", 2) == 0) {
              minute = matchInteger(s, 2);
              f += 2;
            }
            else {
              THROW_PARSE_ERROR("unknown time format '" + string(f) + "', expecting 'MI'");
            }
          }
          else if (*f == 'S') {
            if (strncmp(f, "SSS", 3) == 0) {
              int32_t seconds = matchInteger(s);
              f += 3;

              hour   = (seconds / 3600);
              minute = (seconds /   60) % 60;
              second = (seconds       ) % 60;
            }
            else if (strncmp(f, "SS", 2) == 0) {
              second = matchInteger(s, 2);
              f += 2;
            }
            else {
              THROW_PARSE_ERROR("unknown time format '" + string(f) + "', expecting 'SS'");
            }
          }
          else if (*f == *s) {
            f++;
            s++;
          }
          else {
            THROW_PARSE_ERROR("cannot match time '" + str + "' with format '" + format + "'");
          }
        }

        if (23 < hour || hour < 0) {
          THROW_PARSE_ERROR("illegal hour '" + itoa(hour) + "'");
        }

        if (59 < minute || minute < 0) {
          THROW_PARSE_ERROR("illegal minute '" + itoa(minute) + "'");
        }

        if (59 < second || second < 0) {
          THROW_PARSE_ERROR("illegal second '" + itoa(second) + "'");
        }

        return hour * 3600 + minute * 60 + second;
      }



      string formatSeconds (seconds_t date) {
        int hour = date / 3600;
        int minute = (date / 60) % 60;
        int second = date % 60;

        StringBuffer buffer(TRI_CORE_MEM_ZONE);

        buffer.appendInteger2(hour);
        buffer.appendChar(':');
        buffer.appendInteger2(minute);
        buffer.appendChar(':');
        buffer.appendInteger2(second);

        string result(buffer.c_str());

        return result;
      }



      string formatSeconds (string const& format, seconds_t date) {
        return formatDateTime(format, 0, date);
      }



      date_t date (string const& format, string const& str) {
        int32_t year = 1970;
        int32_t month = 1;
        int32_t day = 1;

        char const * f = format.c_str();
        char const * s = str.c_str();

        while (*f) {
          if (*f == 'Y') {
            if (strncmp(f, "YYYY", 4) == 0) {
              year = matchInteger(s, 4);
              f += 4;
            }
            else {
              THROW_PARSE_ERROR("unknown date format '" + string(f) + "', expecting 'YYYY'");
            }
          }
          else if (*f == 'M') {
            if (strncmp(f, "MM", 2) == 0) {
              month = matchInteger(s, 2);
              f += 2;
            }
            else {
              THROW_PARSE_ERROR("unknown date format '" + string(f) + "', expecting 'MM'");
            }
          }
          else if (*f == 'D') {
            if (strncmp(f, "DD", 2) == 0) {
              day = matchInteger(s, 2);
              f += 2;
            }
            else {
              THROW_PARSE_ERROR("unknown date format '" + string(f) + "', expecting 'DD'");
            }
          }
          else if (*f == *s) {
            f++;
            s++;
          }
          else {
            THROW_PARSE_ERROR("cannot match date '" + str + "' with format '" + format + "'");
          }
        }

        if (12 < month || month < 1) {
          THROW_PARSE_ERROR("illegal month '" + itoa(month) + "'");
        }

        if (31 < day || day < 1) {
          THROW_PARSE_ERROR("illegal day '" + itoa(day) + "'");
        }

        struct tm t;

        t.tm_sec = 0;
        t.tm_min = 0;
        t.tm_hour = 0;
        t.tm_mday = day;
        t.tm_mon = month - 1;
        t.tm_year = year - 1900;
        t.tm_isdst = 0;

        return date_t((mktime(&t) - GLOBAL_TIMEZONE) / 86400);
      }



      string formatDate (date_t date) {
        time_t ti = date * 86400;

#ifdef TRI_HAVE_GMTIME_R
        struct tm t;
        gmtime_r(&ti, &t);
#else
#ifdef TRI_HAVE_GMTIME_S
        struct tm t;
        gmtime_s(&t, &ti);
#else
        struct tm t;
        struct tm* tp = gmtime(&ti);

        if (tp != 0) {
          memcpy(&t, tp, sizeof(struct tm));
        }
#endif
#endif

        StringBuffer buffer(TRI_CORE_MEM_ZONE);

        buffer.appendInteger4(t.tm_year + 1900);
        buffer.appendChar('-');
        buffer.appendInteger2(t.tm_mon + 1);
        buffer.appendChar('-');
        buffer.appendInteger2(t.tm_mday);

        string result(buffer.c_str());

        return result;
      }



      string formatDate (string const& format, date_t date) {
        return formatDateTime(format, date, 0);
      }



      datetime_t datetime (string const& format, string const& str) {
        int32_t year = 1970;
        int32_t month = 1;
        int32_t day = 1;
        int32_t hour = 0;
        int32_t minute = 0;
        int32_t second = 0;

        char const * f = format.c_str();
        char const * s = str.c_str();

        while (*f) {
          if (*f == 'Y') {
            if (strncmp(f, "YYYY", 4) == 0) {
              year = matchInteger(s, 4);
              f += 4;
            }
            else {
              THROW_PARSE_ERROR("unknown date format '" + string(f) + "', expecting 'YYYY'");
            }
          }
          else if (*f == 'M') {
            if (strncmp(f, "MM", 2) == 0) {
              month = matchInteger(s, 2);
              f += 2;
            }
            else if (strncmp(f, "MI", 2) == 0) {
              minute = matchInteger(s, 2);
              f += 2;
            }
            else {
              THROW_PARSE_ERROR("unknown date format '" + string(f) + "', expecting 'MM'");
            }
          }
          else if (*f == 'D') {
            if (strncmp(f, "DD", 2) == 0) {
              day = matchInteger(s, 2);
              f += 2;
            }
            else {
              THROW_PARSE_ERROR("unknown date format '" + string(f) + "', expecting 'DD'");
            }
          }
          else if (*f == 'H') {
            if (strncmp(f, "HH", 2) == 0) {
              hour = matchInteger(s, 2);
              f += 2;
            }
            else {
              THROW_PARSE_ERROR("unknown time format '" + string(f) + "', expecting 'HH'");
            }
          }
          else if (*f == 'S') {
            if (strncmp(f, "SSSSS", 5) == 0) {
              time_t seconds = matchInteger(s);
              f += 5;

#ifdef TRI_HAVE_GMTIME_R
              struct tm t;
              time_t ts = seconds;
              gmtime_r(&ts, &t);
#else
#ifdef TRI_HAVE_GMTIME_S
              struct tm t;
              time_t ts = seconds;
              gmtime_s(&t, &ts);
#else
              struct tm t;
              time_t ts = seconds;
              struct tm* tp = gmtime(&ts);

              if (tp != 0) {
                memcpy(&t, tp, sizeof(struct tm));
              }
#endif
#endif

              second = t.tm_sec;
              minute = t.tm_min;
              hour   = t.tm_hour;
              day    = t.tm_mday;
              month  = t.tm_mon + 1;
              year   = t.tm_year + 1900;
            }
            else if (strncmp(f, "SSS", 3) == 0) {
              int32_t seconds = matchInteger(s);
              f += 3;

              hour   = (seconds / 3600);
              minute = (seconds /   60) % 60;
              second = (seconds       ) % 60;
            }
            else if (strncmp(f, "SS", 2) == 0) {
              second = matchInteger(s, 2);
              f += 2;
            }
            else {
              THROW_PARSE_ERROR("unknown time format '" + string(f) + "', expecting 'SS'");
            }
          }
          else if (*f == *s) {
            f++;
            s++;
          }
          else {
            THROW_PARSE_ERROR("cannot match date '" + str + "' with format '" + format + "'");
          }
        }

        if (12 < month || month < 1) {
          THROW_PARSE_ERROR("illegal month '" + itoa(month) + "'");
        }

        if (31 < day || day < 1) {
          THROW_PARSE_ERROR("illegal day '" + itoa(day) + "'");
        }

        struct tm t;

        t.tm_sec = second;
        t.tm_min = minute;
        t.tm_hour = hour;
        t.tm_mday = day;
        t.tm_mon = month - 1;
        t.tm_year = year - 1900;
        t.tm_isdst = 0;

        return datetime_t((mktime(&t) - GLOBAL_TIMEZONE));
      }


      string formatDatetime (datetime_t datetime) {
        int64_t cnv = static_cast<int64_t>(datetime);
        date_t d = date_t(cnv / 86400LL);
        seconds_t s = seconds_t(cnv % 86400LL);
        return formatDateTime(d, s);
      }



      string formatDatetime (string const& format, datetime_t datetime) {
        int64_t cnv = static_cast<int64_t>(datetime);
        date_t d = date_t(cnv / 86400LL);
        seconds_t s = seconds_t(cnv % 86400LL);
        return formatDateTime(format, d, s);
      }



      string formatDateTime (date_t date, seconds_t time) {
        time_t ti = date * 86400;

#ifdef TRI_HAVE_GMTIME_R
        struct tm t;
        gmtime_r(&ti, &t);
#else
#ifdef TRI_HAVE_GMTIME_S
        struct tm t;
        gmtime_s(&t, &ti);
#else
        struct tm t;
        struct tm* tp = gmtime(&ti);

        if (tp != 0) {
          memcpy(&t, tp, sizeof(struct tm));
        }
#endif
#endif

        int hour = time / 3600;
        int minute = (time / 60) % 60;
        int second = time % 60;

        StringBuffer buffer(TRI_CORE_MEM_ZONE);

        buffer.appendInteger4(t.tm_year + 1900);
        buffer.appendChar('-');
        buffer.appendInteger2(t.tm_mon + 1);
        buffer.appendChar('-');
        buffer.appendInteger2(t.tm_mday);
        buffer.appendChar('T');
        buffer.appendInteger2(hour);
        buffer.appendChar(':');
        buffer.appendInteger2(minute);
        buffer.appendChar(':');
        buffer.appendInteger2(second);

        string result(buffer.c_str());

        return result;
      }



      string formatDateTime (string const& format, date_t date, seconds_t time) {
        time_t ti = date * 86400;

#ifdef TRI_HAVE_GMTIME_R
        struct tm t;
        gmtime_r(&ti, &t);
#else
#ifdef TRI_HAVE_GMTIME_S
        struct tm t;
        gmtime_s(&t, &ti);
#else
        struct tm t;
        struct tm* tp = gmtime(&ti);

        if (tp != 0) {
          memcpy(&t, tp, sizeof(struct tm));
        }
#endif
#endif

        int year = t.tm_year + 1900;
        int month = t.tm_mon + 1;
        int day = t.tm_mday;
        int wday = t.tm_wday + 1;
        int yday = t.tm_yday + 1;

        int hour = time / 3600;
        int minute = (time / 60) % 60;
        int second = time % 60;

        StringBuffer buffer(TRI_CORE_MEM_ZONE);

        char const* p = format.c_str();
        char const* e = p + format.size();

        for (;  p < e;  ++p) {

          // .............................................................................
          // quote
          // .............................................................................

          if (p[0] == '"') {
            for (++p;  p[0] != '"' && p < e;  ++p) {
              buffer.appendChar(p[0]);
            }

            continue;
          }

          // .............................................................................
          // YYYY, YYY, YY, or Y
          // .............................................................................

          if (p[0] == 'Y' || p[0] == 'y') {

            // YYYY, YYY, or YY
            if (p + 1 < e && (p[1] == 'Y' || p[1] == 'y')) {

              // YYYY or YYY
              if (p + 2 < e && (p[2] == 'Y' || p[2] == 'y')) {

                // YYYY
                if (p + 3 < e && (p[3] == 'Y' || p[3] == 'y')) {
                  buffer.appendInteger4(year);
                  p += 3;
                }

                // YYY
                else {
                  buffer.appendInteger3(year % 1000);
                  p += 2;
                }
              }

              // YY
              else {
                buffer.appendInteger2(year % 100);
                p += 1;
              }
            }

            // Y
            else {
              buffer.appendChar((year % 10) + '0');
            }

            continue;
          }

          // .............................................................................
          // MM, MONTH, MON, MI
          // .............................................................................

          if (p[0] == 'M' || p[0] == 'm') {

            // MM
            if (p + 1 < e && (p[1] == 'M' || p[1] == 'm')) {
              buffer.appendInteger2(month);

              p += 1;
              continue;
            }

            // MI
            else if (p + 1 < e && (p[1] == 'I' || p[1] == 'i')) {
              buffer.appendInteger2(minute);

              p += 1;
              continue;
            }

            // MONTH
            else if (p + 4 < e && strcasecmp(p + 1, "onth") == 0) {
              if (p[1] == 'O') {
                switch (month) {
                  case  1: buffer.appendText("JANUARY");  break;
                  case  2: buffer.appendText("FEBRUARY");  break;
                  case  3: buffer.appendText("MARCH");  break;
                  case  4: buffer.appendText("APRIL");  break;
                  case  5: buffer.appendText("MAY");  break;
                  case  6: buffer.appendText("JUNE");  break;
                  case  7: buffer.appendText("JULY");  break;
                  case  8: buffer.appendText("AUGUST");  break;
                  case  9: buffer.appendText("SEPTEMBER");  break;
                  case 10: buffer.appendText("OCTOBER");  break;
                  case 11: buffer.appendText("NOVEMBER");  break;
                  case 12: buffer.appendText("DECEMBER");  break;
                  default: buffer.appendText("UNDEFINED");  break;
                }
              }
              else {
                switch (month) {
                  case  1: buffer.appendText("January");  break;
                  case  2: buffer.appendText("February");  break;
                  case  3: buffer.appendText("March");  break;
                  case  4: buffer.appendText("April");  break;
                  case  5: buffer.appendText("May");  break;
                  case  6: buffer.appendText("June");  break;
                  case  7: buffer.appendText("July");  break;
                  case  8: buffer.appendText("August");  break;
                  case  9: buffer.appendText("September");  break;
                  case 10: buffer.appendText("October");  break;
                  case 11: buffer.appendText("November");  break;
                  case 12: buffer.appendText("December");  break;
                  default: buffer.appendText("Undefined");  break;
                }
              }

              p += 4;
              continue;
            }

            // MON
            else if (p + 2 < e && strcasecmp(p + 1, "on") == 0) {
              if (p[1] == 'O') {
                switch (month) {
                  case  1: buffer.appendText("JAN");  break;
                  case  2: buffer.appendText("FEB");  break;
                  case  3: buffer.appendText("MAR");  break;
                  case  4: buffer.appendText("APR");  break;
                  case  5: buffer.appendText("MAY");  break;
                  case  6: buffer.appendText("JUN");  break;
                  case  7: buffer.appendText("JUL");  break;
                  case  8: buffer.appendText("AUG");  break;
                  case  9: buffer.appendText("SEP");  break;
                  case 10: buffer.appendText("OCT");  break;
                  case 11: buffer.appendText("NOV");  break;
                  case 12: buffer.appendText("DEC");  break;
                  default: buffer.appendText("UDF");  break;
                }
              }
              else {
                switch (month) {
                  case  1: buffer.appendText("Jan");  break;
                  case  2: buffer.appendText("Feb");  break;
                  case  3: buffer.appendText("Mar");  break;
                  case  4: buffer.appendText("Apr");  break;
                  case  5: buffer.appendText("May");  break;
                  case  6: buffer.appendText("Jun");  break;
                  case  7: buffer.appendText("Jul");  break;
                  case  8: buffer.appendText("Aug");  break;
                  case  9: buffer.appendText("Sep");  break;
                  case 10: buffer.appendText("Oct");  break;
                  case 11: buffer.appendText("Nov");  break;
                  case 12: buffer.appendText("Dec");  break;
                  default: buffer.appendText("Udf");  break;
                }
              }

              p += 2;
              continue;
            }
          }

          // .............................................................................
          // DAY, D, DD, DDD, DY
          // .............................................................................

          if (p[0] == 'D' || p[0] == 'd') {

            // DDD, DD
            if (p + 1 < e && (p[1] == 'D' || p[1] == 'd')) {

              // DDD
              if (p + 2 < e && (p[2] == 'D' || p[2] == 'd')) {
                buffer.appendInteger3(yday);
                p += 2;
              }

              // DD
              else {
                buffer.appendInteger2(day);
                p += 1;
              }
            }

            // DY
            else if (p + 1 < e && (p[1] == 'Y' || p[1] == 'y')) {
              if (p[1] == 'Y') {
                switch (wday) {
                  case 1: buffer.appendText("SUN"); break;
                  case 2: buffer.appendText("MON"); break;
                  case 3: buffer.appendText("TUE"); break;
                  case 4: buffer.appendText("WED"); break;
                  case 5: buffer.appendText("THU"); break;
                  case 6: buffer.appendText("FRI"); break;
                  case 7: buffer.appendText("SAT"); break;
                  default: buffer.appendText("UDF");  break;
                }
              }
              else {
                switch (wday) {
                  case 1: buffer.appendText("Sun"); break;
                  case 2: buffer.appendText("Mon"); break;
                  case 3: buffer.appendText("Tue"); break;
                  case 4: buffer.appendText("Wed"); break;
                  case 5: buffer.appendText("Thu"); break;
                  case 6: buffer.appendText("Fri"); break;
                  case 7: buffer.appendText("Sat"); break;
                  default: buffer.appendText("Udf");  break;
                }
              }

              p += 1;
            }

            // DAY
            else if (p + 2 < e && (p[1] == 'A' || p[1] == 'a') && (p[2] == 'Y' || p[2] == 'y')) {
              if (p[2] == 'A') {
                switch (wday) {
                  case 1: buffer.appendText("SUNDAY"); break;
                  case 2: buffer.appendText("MONDAY"); break;
                  case 3: buffer.appendText("TUESDAY"); break;
                  case 4: buffer.appendText("WEDNESDAY"); break;
                  case 5: buffer.appendText("THURSDAY"); break;
                  case 6: buffer.appendText("FRIDAY"); break;
                  case 7: buffer.appendText("SATURDAY"); break;
                  default: buffer.appendText("UNDEFINED");  break;
                }
              }
              else {
                switch (wday) {
                  case 1: buffer.appendText("Sunday"); break;
                  case 2: buffer.appendText("Monday"); break;
                  case 3: buffer.appendText("Tuesday"); break;
                  case 4: buffer.appendText("Wednesday"); break;
                  case 5: buffer.appendText("Thursday"); break;
                  case 6: buffer.appendText("Friday"); break;
                  case 7: buffer.appendText("Saturday"); break;
                  default: buffer.appendText("Undefined");  break;
                }
              }

              p += 2;
            }

            // D
            else {
              buffer.appendInteger(wday);
            }

            continue;
          }

          // .............................................................................
          // HH, HH12, HH24
          // .............................................................................

          if (p[0] == 'H' || p[0] == 'h') {
            if (p + 1 < e && (p[1] == 'H' || p[1] == 'h')) {

              // HH12
              if (p + 3 < e && p[2] == '1' && p[3] == '2') {
                buffer.appendInteger2(hour % 12);
                p += 3;
              }

              // HH24
              else if (p + 3 < e && p[2] == '2' && p[3] == '4') {
                buffer.appendInteger2(hour);
                p += 3;
              }

              // HH
              else {
                buffer.appendInteger2(hour);
                p += 1;
              }

              continue;
            }
          }

          // .............................................................................
          // SS, SSSSS
          // .............................................................................

          if (p[0] == 'S' || p[0] == 's') {
            if (p + 1 < e && (p[1] == 'S' || p[1] == 's')) {

              // SSSSSS
              if (p + 4 < e && (p[2] == 'S' || p[2] == 's') && (p[3] == 'S' || p[3] == 's') && (p[4] == 'S' || p[4] == 's')) {
                buffer.appendInteger(hour * 3600 + minute * 60 + second);
                p += 4;
              }

              // SS
              else {
                buffer.appendInteger2(second);
              }

              continue;
            }
          }

          // .............................................................................
          // something else
          // .............................................................................

          buffer.appendChar(*p);
        }

        string result = buffer.c_str();

        return result;
      }


// -----------------------------------------------------------------------------
      // UTF8
// -----------------------------------------------------------------------------

      // this function takes a sequence of hexadecimal characters (inputStr) of a particular
      // length (len) and output 1,2,3 or 4 characters which represents the utf8 encoding
      // of the unicode representation of that character.

      bool unicodeToUTF8 (const char* inputStr, const size_t& len, string& outputStr) {
        uint32_t outputInt = 0;
        bool ok = true;
        ok = parseHexanumber(inputStr, len, &outputInt);
        if (ok == false) {
          outputStr = string(inputStr, len);
          return false;
        }
        ok = isHighSurrugate(outputInt) || isLowSurrugate(outputInt);
        if (ok == true) {
          outputStr.append("?");
          return false;
        }
        ok = toUtf8(outputInt, outputStr);
        if (ok == false) {
          outputStr = string(inputStr, len);
        }
        return ok;

      }

// -----------------------------------------------------------------------------
      // UTF16
// -----------------------------------------------------------------------------

      // this function converts unicode symbols whose code points lies in U+10000 to U+10FFFF
      //
      // the parameters of the function correspond to the UTF16 description for
      // the code points mentioned above
      //
      // high_surrogate is the high surrogate bytes sequence, valid values begin with D[89AB]
      // low_surrogate  is the low  surrogate bytes sequence, valid values begin with D[CDEF]
      // for details see: http://en.wikipedia.org/wiki/UTF-16#Code_points_U.2B10000_to_U.2B10FFFF
      //

      bool convertUTF16ToUTF8 (const char* high_surrogate, const char* low_surrogate, string& outputStr) {
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

// -----------------------------------------------------------------------------
      // BASE64
// -----------------------------------------------------------------------------

      string encodeBase64 (string const& in) {
        unsigned char charArray3[3];
        unsigned char charArray4[4];

        string ret;

        int i = 0;
        int j = 0;

        unsigned char const* bytesToEncode = reinterpret_cast<unsigned char const*>(in.c_str());
        size_t in_len = in.size();

        while (in_len--) {
          charArray3[i++] = *(bytesToEncode++);

          if (i == 3) {
            charArray4[0] =  (charArray3[0] & 0xfc) >> 2;
            charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
            charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
            charArray4[3] =   charArray3[2] & 0x3f;

            for (i = 0;  i < 4;  i++) {
              ret += BASE64_CHARS[charArray4[i]];
            }

            i = 0;
          }
        }

        if (i != 0) {
          for(j = i; j < 3; j++) {
            charArray3[j] = '\0';
          }

          charArray4[0] =  (charArray3[0] & 0xfc) >> 2;
          charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
          charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
          charArray4[3] =   charArray3[2] & 0x3f;

          for (j = 0; (j < i + 1); j++) {
            ret += BASE64_CHARS[charArray4[j]];
          }

          while ((i++ < 3)) {
            ret += '=';
          }
        }

        return ret;
      }



      string decodeBase64 (string const& source) {
        unsigned char charArray4[4];
        unsigned char charArray3[3];

        string ret;

        int i = 0;
        int j = 0;
        int inp = 0;

        int in_len = source.size();

        while (in_len-- && (source[inp] != '=') && isBase64(source[inp])) {
          charArray4[i++] = source[inp];

          inp++;

          if (i ==4) {
            for (i = 0;  i < 4;  i++) {
              charArray4[i] = BASE64_REVS[charArray4[i]];
            }

            charArray3[0] =  (charArray4[0] << 2)        + ((charArray4[1] & 0x30) >> 4);
            charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
            charArray3[2] = ((charArray4[2] & 0x3) << 6) +   charArray4[3];

            for (i = 0; (i < 3); i++) {
              ret += charArray3[i];
            }

            i = 0;
          }
        }

        if (i) {
          for (j = i;  j < 4;  j++) {
              charArray4[j] = 0;
          }

          for (j = 0;  j < 4; j++) {
            charArray4[j] = BASE64_REVS[charArray4[j]];
          }

          charArray3[0] =  (charArray4[0] << 2)        + ((charArray4[1] & 0x30) >> 4);
          charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
          charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

          for (j = 0;  j < i - 1;  j++) {
            ret += charArray3[j];
          }
        }

        return ret;
      }



      string encodeBase64U (string const& in) {
        unsigned char charArray3[3];
        unsigned char charArray4[4];

        string ret;

        int i = 0;
        int j = 0;

        unsigned char const* bytesToEncode = reinterpret_cast<unsigned char const*>(in.c_str());
        size_t in_len = in.size();

        while (in_len--) {
          charArray3[i++] = *(bytesToEncode++);

          if (i == 3) {
            charArray4[0] =  (charArray3[0] & 0xfc) >> 2;
            charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
            charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
            charArray4[3] =   charArray3[2] & 0x3f;

            for (i = 0;  i < 4;  i++) {
              ret += BASE64U_CHARS[charArray4[i]];
            }

            i = 0;
          }
        }

        if (i != 0) {
          for(j = i; j < 3; j++) {
            charArray3[j] = '\0';
          }

          charArray4[0] =  (charArray3[0] & 0xfc) >> 2;
          charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
          charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
          charArray4[3] =   charArray3[2] & 0x3f;

          for (j = 0; (j < i + 1); j++) {
            ret += BASE64U_CHARS[charArray4[j]];
          }

          while ((i++ < 3)) {
            ret += '=';
          }
        }

        return ret;
      }



      string decodeBase64U (string const& source) {
        unsigned char charArray4[4];
        unsigned char charArray3[3];

        string ret;

        int i = 0;
        int j = 0;
        int inp = 0;

        int in_len = source.size();

        while (in_len-- && (source[inp] != '=') && isBase64U(source[inp])) {
          charArray4[i++] = source[inp];

          inp++;

          if (i ==4) {
            for (i = 0;  i < 4;  i++) {
              charArray4[i] = BASE64U_REVS[charArray4[i]];
            }

            charArray3[0] =  (charArray4[0] << 2)        + ((charArray4[1] & 0x30) >> 4);
            charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
            charArray3[2] = ((charArray4[2] & 0x3) << 6) +   charArray4[3];

            for (i = 0; (i < 3); i++) {
              ret += charArray3[i];
            }

            i = 0;
          }
        }

        if (i) {
          for (j = i;  j < 4;  j++) {
              charArray4[j] = 0;
          }

          for (j = 0;  j < 4; j++) {
            charArray4[j] = BASE64U_REVS[charArray4[j]];
          }

          charArray3[0] =  (charArray4[0] << 2)        + ((charArray4[1] & 0x30) >> 4);
          charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
          charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

          for (j = 0;  j < i - 1;  j++) {
            ret += charArray3[j];
          }
        }

        return ret;
      }

// -----------------------------------------------------------------------------
      // ADDITIONAL STRING UTILITIES
// -----------------------------------------------------------------------------

      string correctPath(const string& incorrectPath) {
        #ifdef _WIN32
          return replace (incorrectPath, "/", "\\");
        #else
          return replace (incorrectPath, "\\", "/");
        #endif
      }

/// In a list str = "xx,yy,zz ...", entry(n,str,',') returns the nth entry of the list delimited
/// by ','. E.g entry(2,str,',') = 'yy'

      string entry (const size_t pos, string const& sourceStr, string const& delimiter) {
        size_t delLength    = delimiter.length();
        size_t sourceLength = sourceStr.length();

        if (pos == 0) {
          return "";
        }

        if (delLength == 0 || sourceLength == 0) {
          return sourceStr;
        }

        size_t k = 0;
        size_t delPos;
        size_t offSet = 0;

        while (true) {
          delPos = sourceStr.find(delimiter, offSet);

          if ((delPos == sourceStr.npos) || (delPos >= sourceLength) || (offSet >= sourceLength)) {
            return sourceStr.substr(offSet);
          }

          ++k;

          if (k == pos) {
            return sourceStr.substr(offSet,delPos - offSet);
          }

          offSet = delPos + delLength;
        }

        return sourceStr;
      }



/// Determines the number of entries in a list str = "xx,yyy,zz,www".
/// numEntries(str,',') = 4.

      size_t numEntries (string const& sourceStr, string const& delimiter) {
        size_t delLength    = delimiter.length();
        size_t sourceLength = sourceStr.length();
        bool match;

        if (sourceLength == 0) {
          return (0);
        }

        if (delLength == 0) {
          return (1);
        }

        size_t k = 1;

        for (size_t j = 0; j < sourceLength; ++j) {
          match = true;
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
        return(k);
      }



      string encodeHex (string const& str) {
        char* tmp;
        size_t len;

        tmp = TRI_EncodeHexString(str.c_str(), str.length(), &len);
        string result = string(tmp, len);
        TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);

        return result;
      }



      string decodeHex (string const& str) {
        char* tmp;
        size_t len;

        tmp = TRI_DecodeHexString(str.c_str(), str.length(), &len);
        string result = string(tmp, len);
        TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);

        return result;
      }
    }
  }
}
