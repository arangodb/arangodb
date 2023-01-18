////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "Escaper.h"
#include "Basics/debugging.h"

namespace arangodb {

void ControlCharsSuppressor::writeCharIntoOutputBuffer(uint32_t c,
                                                       std::string& output,
                                                       int numBytes) {
  output.push_back(' ');
}

void ControlCharsEscaper::writeCharIntoOutputBuffer(uint32_t c,
                                                    std::string& output,
                                                    int numBytes) {
  switch (c) {
    case '\n':
      output.append("\\n");
      break;

    case '\r':
      output.append("\\r");
      break;

    case '\t':
      output.append("\\t");
      break;

    case '\b':
      output.append("\\b");
      break;

    case '\f':
      output.append("\\f");
      break;

    default: {
      uint8_t n1 = c >> 4;
      uint8_t n2 = c & 0x0F;

      output.append("\\x");
      output.push_back((n1 < 10) ? ('0' + n1) : ('A' + n1 - 10));
      output.push_back((n2 < 10) ? ('0' + n2) : ('A' + n2 - 10));
    }
  }
}

void UnicodeCharsRetainer::writeCharIntoOutputBuffer(uint32_t c,
                                                     std::string& output,
                                                     int numBytes) {
  if (numBytes == 2) {
    uint16_t num1 = c & 0xffff;
    output.push_back(((num1 >> 6) & 0x1f) | 0xc0);
    output.push_back((num1 & 0x3f) | 0x80);
  } else if (numBytes == 3) {
    uint16_t num1 = c & 0xffff;
    output.push_back(((num1 >> 12) & 0x0f) | 0xe0);
    output.push_back(((num1 >> 6) & 0x3f) | 0x80);
    output.push_back((num1 & 0x3f) | 0x80);
  } else if (numBytes == 4) {
    output.push_back(((c >> 18) & 0x07) | 0xF0);
    output.push_back(((c >> 12) & 0x3f) | 0x80);
    output.push_back(((c >> 6) & 0x3f) | 0x80);
    output.push_back((c & 0x3f) | 0x80);
  }
}

void UnicodeCharsEscaper::writeCharHelper(uint16_t c, std::string& output) {
  output.append("\\u");

  uint16_t i1 = (c & 0xF000) >> 12;
  uint16_t i2 = (c & 0x0F00) >> 8;
  uint16_t i3 = (c & 0x00F0) >> 4;
  uint16_t i4 = (c & 0x000F);

  output.push_back((i1 < 10) ? ('0' + i1) : ('A' + i1 - 10));
  output.push_back((i2 < 10) ? ('0' + i2) : ('A' + i2 - 10));
  output.push_back((i3 < 10) ? ('0' + i3) : ('A' + i3 - 10));
  output.push_back((i4 < 10) ? ('0' + i4) : ('A' + i4 - 10));
}

void UnicodeCharsEscaper::writeCharIntoOutputBuffer(uint32_t c,
                                                    std::string& output,
                                                    int numBytes) {
  if (numBytes == 4) {  // when the unicode requires 4 bytes for representation,
                        // its code is escaped with surrogate pairs, the highest
                        // and the lowest bytes of the character
    TRI_ASSERT(c >= 0x10000U);
    c -= 0x10000U;
    uint16_t high = (uint16_t)(((c & 0xffc00U) >> 10) + 0xd800);
    writeCharHelper(high, output);
    uint16_t low = (c & 0x3ffU) + 0xdc00U;
    writeCharHelper(low, output);
  } else {
    writeCharHelper(c, output);
  }
}

template<typename ControlCharHandler, typename UnicodeCharHandler>
void Escaper<ControlCharHandler, UnicodeCharHandler>::writeIntoOutputBuffer(
    std::string_view message, std::string& buffer) {
  unsigned char const* p =
      reinterpret_cast<unsigned char const*>(message.data());
  unsigned char const* end = p + message.size();
  while (p < end) {
    unsigned char c = *p;
    if (c < 128) {  // the character is ASCII
      if (c < 0x20 ||
          c ==
              0x7f) {  // the character is either control, which comprises codes
                       // until 32, or is DEL, which is not a visible character
        ControlCharHandler::writeCharIntoOutputBuffer(
            c, buffer, 1);  // retain or escape the control character
      } else {              // is a visible ascii character
        buffer.push_back(c);
      }
      p++;
    } else if (c < 224) {  // unicode which requires 2 bytes for representation
      if ((p + 1) >=
          end) {  // no next byte to represent it, so it's broken unicode
        buffer.push_back('?');
        p++;
        continue;
      }
      uint8_t d = (uint8_t) * (p + 1);
      if ((d & 0xC0) == 0x80) {  // is within the rules for representing unicode
                                 // characters for the second byte
        UnicodeCharHandler::writeCharIntoOutputBuffer(
            ((c & 0x1F) << 6) | (d & 0x3F), buffer,
            2);  // retain or escape the unicode character represented by 2
                 // bytes
        ++p;
      } else {  // the next byte is broken unicode
        buffer.push_back('?');
      }
      p++;
    } else if (c < 240) {  // unicode which requires 3 bytes for representation
      if ((p + 2) >= end) {  // there's no 2 other sequential bytes to represent
                             // the unicode character, so it's broken unicode
        buffer.push_back('?');
        p++;
        continue;
      }
      uint8_t d = (uint8_t) * (p + 1);
      if ((d & 0xC0) ==
          0x80) {  // second byte is within the rules for representing a unicode
                   // character that requires 3 bytes for representation
        ++p;
        uint8_t e = (uint8_t) * (p + 1);
        if ((e & 0xC0) == 0x80) {  // third byte is within the rules for
                                   // representing a unicode character that
                                   // requires 3 bytes for representation
          ++p;
          UnicodeCharHandler::writeCharIntoOutputBuffer(
              ((c & 0x0F) << 12) | ((d & 0x3F) << 6) | (e & 0x3F), buffer,
              3);  // retain or escape the unicode character represented by 3
                   // bytes
        } else {   // second byte is not within the rules for representing a
                   // unicode character
          buffer.push_back('?');
        }
      } else {  // third byte is not within the rules for representing a unicode
                // character
        buffer.push_back('?');
      }
      p++;
    } else if (c < 248) {  // unicode which requires 4 bytes for representation
      if ((p + 3) >= end) {  // there's not 3 sequential bytes for representing
                             // this unicode character, so it's broken unicode
        buffer.push_back('?');
        p++;
        continue;
      }
      uint8_t d = (uint8_t) * (p + 1);
      if ((d & 0xC0) ==
          0x80) {  // second byte is within the rules for representing a unicode
                   // character that requires 3 bytes for representation
        ++p;
        uint8_t e = (uint8_t) * (p + 1);
        if ((e & 0xC0) == 0x80) {  // third byte is within the rules for
                                   // representing a unicode character that
                                   // requires 3 bytes for representation
          ++p;
          uint8_t f = (uint8_t) * (p + 1);
          if ((f & 0xC0) == 0x80) {  // fourth byte is within the rules for
                                     // representing a unicode character that
                                     // requires 3 bytes for representation
            p++;
            UnicodeCharHandler::writeCharIntoOutputBuffer(
                ((c & 0x07) << 18) | ((d & 0x3F) << 12) | ((e & 0x3F) << 6) |
                    (f & 0x3F),
                buffer, 4);  // retain or escape the unicode character
                             // represented by 4 bytes
          } else {  // second byte is not within the rules for representing a
                    // unicode character
            buffer.push_back('?');
          }
        } else {  // third byte is not within the rules for representing a
                  // unicode character
          buffer.push_back('?');
        }
      } else {  // fourth byte is not within the rules for representing a
                // unicode character
        buffer.push_back('?');
      }
      p++;
    } else {  // broken unicode, is not ascii and not represented with 2, 3 or 4
              // bytes
      buffer.push_back('?');
      // invalid UTF-8 sequence
      break;
    }
  }
}

template struct Escaper<ControlCharsSuppressor, UnicodeCharsRetainer>;

template struct Escaper<ControlCharsSuppressor, UnicodeCharsEscaper>;

template struct Escaper<ControlCharsEscaper, UnicodeCharsRetainer>;

template struct Escaper<ControlCharsEscaper, UnicodeCharsEscaper>;

}  // namespace arangodb
