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
/// @author Achim Brandt
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Escaper.h"

namespace arangodb {

void ControlCharsSuppressor::writeCharIntoOutputBuffer(uint32_t c, char*& output, int numBytes) {
  *output++ = ' ';
}

void ControlCharsEscaper::writeCharIntoOutputBuffer(uint32_t c, char*& output, int numBytes) {
  switch (c) {
    case '\n':
      *output++ = '\\';
      *output++ = 'n';
      break;

    case '\r':
      *output++ = '\\';
      *output++ = 'r';
      break;

    case '\t':
      *output++ = '\\';
      *output++ = 't';
      break;

    default: {
      uint8_t n1 = c >> 4;
      uint8_t n2 = c & 0x0F;

      *output++ = '\\';
      *output++ = 'x';
      *output++ = (n1 < 10) ? ('0' + n1) : ('A' + n1 - 10);
      *output++ = (n2 < 10) ? ('0' + n2) : ('A' + n2 - 10);
    }
  }
}

void UnicodeCharsRetainer::writeCharIntoOutputBuffer(uint32_t c, char*& output, int numBytes) {
  if (numBytes == 2) {
    uint16_t num1 = c & 0xffff;
    *output++ = ((num1 >> 6) & 0x1f) | 0xc0;
    *output++ = (num1 & 0x3f) | 0x80;
  } else if (numBytes == 3) {
    uint16_t num1 = c & 0xffff;
    *output++ = ((num1 >> 12) & 0x0f) | 0xe0;
    *output++ = ((num1 >> 6) & 0x3f) | 0x80;
    *output++ = (num1 & 0x3f) | 0x80;
  } else if (numBytes == 4) {
    *output++ = ((c >> 18) & 0x07) | 0xF0;
    *output++ = ((c >> 12) & 0x3f) | 0x80;
    *output++ = ((c >> 6) & 0x3f) | 0x80;
    *output++ = (c & 0x3f) | 0x80;
  }
}

void UnicodeCharsEscaper::writeCharHelper(uint16_t c, char*& output) {
  *output++ = '\\';
  *output++ = 'u';

  uint16_t i1 = (c & 0xF000) >> 12;
  uint16_t i2 = (c & 0x0F00) >> 8;
  uint16_t i3 = (c & 0x00F0) >> 4;
  uint16_t i4 = (c & 0x000F);

  *output++ = (i1 < 10) ? ('0' + i1) : ('A' + i1 - 10);
  *output++ = (i2 < 10) ? ('0' + i2) : ('A' + i2 - 10);
  *output++ = (i3 < 10) ? ('0' + i3) : ('A' + i3 - 10);
  *output++ = (i4 < 10) ? ('0' + i4) : ('A' + i4 - 10);
}

void UnicodeCharsEscaper::writeCharIntoOutputBuffer(uint32_t c, char*& output, int numBytes) {
  if (numBytes == 4) {
    c -= 0x10000U;
    uint16_t high = (uint16_t) (((c & 0xffc00U) >> 10) + 0xd800);
    writeCharHelper(high, output);
    uint16_t low = (c & 0x3ffU) + 0xdc00U;
    writeCharHelper(low, output);
  } else {
    writeCharHelper(c, output);
  }
}

template <typename ControlCharHandler, typename UnicodeCharHandler>
size_t Escaper<ControlCharHandler, UnicodeCharHandler>::determineOutputBufferSize(
    std::string const& message) const {
  return message.size() * std::max(this->_controlHandler.maxCharLength(),
                                   this->_unicodeHandler.maxCharLength());
}

template <typename ControlCharHandler, typename UnicodeCharHandler>
void Escaper<ControlCharHandler, UnicodeCharHandler>::writeIntoOutputBuffer(
    std::string const& message, char*& buffer) {
  unsigned char const* p = reinterpret_cast<unsigned char const*>(message.data());
  unsigned char const* end = p + message.length();
  while (p < end) {
    unsigned char c = *p;
    if (c < 128) {
      if (c < 0x20) {
        this->_controlHandler.writeCharIntoOutputBuffer(c, buffer, 1);
      } else {
        *buffer++ = c;
      }
      // single byte
      p++;
    } else if (c < 224) {
      if ((p + 1) >= end) {
        *buffer++ = '?';
        break;
      }
      uint8_t d = (uint8_t) * (p + 1);
      if ((d & 0xC0) == 0x80) {
        this->_unicodeHandler.writeCharIntoOutputBuffer(((c & 0x1F) << 6) | (d & 0x3F),
                                                        buffer, 2);
        ++p;
      } else {
        *buffer++ = '?';
      }
      p++;
    } else if (c < 240) {
      if ((p + 2) >= end) {
        *buffer++ = '?';
        p++;
        continue;
      }
      uint8_t d = (uint8_t) * (p + 1);
      if ((d & 0xC0) == 0x80) {
        ++p;
        uint8_t e = (uint8_t) * (p + 1);
        if ((e & 0xC0) == 0x80) {
          ++p;
          this->_unicodeHandler.writeCharIntoOutputBuffer(
              ((c & 0x0F) << 12) | ((d & 0x3F) << 6) | (e & 0x3F), buffer, 3);
        } else {
          *buffer++ = '?';
        }
      } else {
        *buffer++ = '?';
      }
      p++;
    } else if (c < 248) {
      if ((p + 3) >= end) {
        *buffer++ = '?';
        p++;
        continue;
      }
      uint8_t d = (uint8_t) * (p + 1);
      if ((d & 0xC0) == 0x80) {
        ++p;
        uint8_t e = (uint8_t) * (p + 1);
        if ((e & 0xC0) == 0x80) {
          ++p;
          uint8_t f = (uint8_t) * (p + 1);
          if ((f & 0xC0) == 0x80) {
            p++;
            this->_unicodeHandler.writeCharIntoOutputBuffer(((c & 0x07) << 18) |
                                                                ((d & 0x3F) << 12) |
                                                                ((e & 0x3F) << 6) |
                                                                (f & 0x3F),
                                                            buffer, 4);
          } else {
            *buffer++ = '?';
          }
        } else {
          *buffer++ = '?';
        }
      } else {
        *buffer++ = '?';
      }
      p++;
    } else {
      *buffer++ = '?';
      // invalid UTF-8 sequence
      break;
    }
  }
}

template class Escaper<ControlCharsSuppressor, UnicodeCharsRetainer>;

template class Escaper<ControlCharsSuppressor, UnicodeCharsEscaper>;

template class Escaper<ControlCharsEscaper, UnicodeCharsRetainer>;

template class Escaper<ControlCharsEscaper, UnicodeCharsEscaper>;

}  // namespace arangodb
