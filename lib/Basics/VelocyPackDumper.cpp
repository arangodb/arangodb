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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include "VelocyPackDumper.h"

#include "Basics/Exceptions.h"
#include "Basics/fpconv.h"
#include "Logger/Logger.h"

#include <velocypack/velocypack-common.h>

#include <velocypack/AttributeTranslator.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;

static size_t const MinReserveValue = 32;

void VelocyPackDumper::handleUnsupportedType(VPackSlice const* slice) {
  TRI_string_buffer_t* buffer = _buffer->stringBuffer();

  if (options->unsupportedTypeBehavior == VPackOptions::NullifyUnsupportedType) {
    TRI_AppendStringUnsafeStringBuffer(buffer, "null", 4);
    return;
  } else if (options->unsupportedTypeBehavior == VPackOptions::ConvertUnsupportedType) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(strlen("\"(non-representable type)\"") + 1 < MinReserveValue);
#endif
    TRI_AppendStringUnsafeStringBuffer(buffer, "\"(non-representable type ");
    TRI_AppendStringUnsafeStringBuffer(buffer, slice->typeName());
    TRI_AppendStringUnsafeStringBuffer(buffer, ")\"");

    return;
  }

  throw VPackException(VPackException::NoJsonEquivalent);
}

void VelocyPackDumper::appendUInt(uint64_t v) {
  TRI_string_buffer_t* buffer = _buffer->stringBuffer();

  TRI_ASSERT(MinReserveValue > 20);
  int res = TRI_ReserveStringBuffer(buffer, MinReserveValue);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  if (10000000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000000000ULL) % 10);
  }
  if (1000000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000000000ULL) % 10);
  }
  if (100000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000000000ULL) % 10);
  }
  if (10000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000000ULL) % 10);
  }
  if (1000000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000000ULL) % 10);
  }
  if (100000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000000ULL) % 10);
  }
  if (10000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000ULL) % 10);
  }
  if (1000000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000ULL) % 10);
  }
  if (100000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000ULL) % 10);
  }
  if (10000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000ULL) % 10);
  }
  if (1000000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000ULL) % 10);
  }
  if (100000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000ULL) % 10);
  }
  if (10000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000ULL) % 10);
  }
  if (1000000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000ULL) % 10);
  }
  if (100000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000ULL) % 10);
  }
  if (10000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000ULL) % 10);
  }
  if (1000ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000ULL) % 10);
  }
  if (100ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100ULL) % 10);
  }
  if (10ULL <= v) {
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10ULL) % 10);
  }

  TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v % 10));
}

void VelocyPackDumper::appendDouble(double v) {
  char temp[24];
  int len = fpconv_dtoa(v, &temp[0]);

  TRI_string_buffer_t* buffer = _buffer->stringBuffer();

  int res = TRI_ReserveStringBuffer(buffer, static_cast<size_t>(len));

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_AppendStringUnsafeStringBuffer(buffer, &temp[0], static_cast<size_t>(len));
}

void VelocyPackDumper::dumpInteger(VPackSlice const* slice) {
  if (slice->isType(VPackValueType::UInt)) {
    uint64_t v = slice->getUInt();

    appendUInt(v);
  } else if (slice->isType(VPackValueType::Int)) {
    TRI_string_buffer_t* buffer = _buffer->stringBuffer();

    TRI_ASSERT(MinReserveValue > 20);
    int res = TRI_ReserveStringBuffer(buffer, MinReserveValue);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    int64_t v = slice->getInt();
    if (v == INT64_MIN) {
      TRI_AppendStringUnsafeStringBuffer(buffer, "-9223372036854775808", 20);
      return;
    }

    if (v < 0) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '-');
      v = -v;
    }

    if (1000000000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000000000LL) % 10);
    }
    if (100000000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000000000LL) % 10);
    }
    if (10000000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000000LL) % 10);
    }
    if (1000000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000000LL) % 10);
    }
    if (100000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000000LL) % 10);
    }
    if (10000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000000LL) % 10);
    }
    if (1000000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000000LL) % 10);
    }
    if (100000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000000LL) % 10);
    }
    if (10000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000000LL) % 10);
    }
    if (1000000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000000LL) % 10);
    }
    if (100000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000000LL) % 10);
    }
    if (10000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000000LL) % 10);
    }
    if (1000000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000000LL) % 10);
    }
    if (100000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100000LL) % 10);
    }
    if (10000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10000LL) % 10);
    }
    if (1000LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 1000LL) % 10);
    }
    if (100LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 100LL) % 10);
    }
    if (10LL <= v) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v / 10LL) % 10);
    }

    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + (v % 10));
  } else if (slice->isType(VPackValueType::SmallInt)) {
    TRI_string_buffer_t* buffer = _buffer->stringBuffer();

    TRI_ASSERT(MinReserveValue > 20);
    int res = TRI_ReserveStringBuffer(buffer, MinReserveValue);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    int64_t v = slice->getSmallInt();
    if (v < 0) {
      TRI_AppendCharUnsafeStringBuffer(buffer, '-');
      v = -v;
    }
    TRI_AppendCharUnsafeStringBuffer(buffer, '0' + static_cast<char>(v));
  }
}

void VelocyPackDumper::appendUnicodeCharacter(uint16_t value) {
  TRI_string_buffer_t* buffer = _buffer->stringBuffer();

  TRI_AppendCharUnsafeStringBuffer(buffer, '\\');
  TRI_AppendCharUnsafeStringBuffer(buffer, 'u');

  uint16_t p;
  p = (value & 0xf000U) >> 12;
  TRI_AppendCharUnsafeStringBuffer(buffer, (p < 10) ? ('0' + p) : ('A' + p - 10));

  p = (value & 0x0f00U) >> 8;
  TRI_AppendCharUnsafeStringBuffer(buffer, (p < 10) ? ('0' + p) : ('A' + p - 10));

  p = (value & 0x00f0U) >> 4;
  TRI_AppendCharUnsafeStringBuffer(buffer, (p < 10) ? ('0' + p) : ('A' + p - 10));

  p = (value & 0x000fU);
  TRI_AppendCharUnsafeStringBuffer(buffer, (p < 10) ? ('0' + p) : ('A' + p - 10));
}

void VelocyPackDumper::appendString(char const* src, VPackValueLength len) {
  static char const EscapeTable[256] = {
      // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E
      // F
      'u',  'u', 'u', 'u', 'u', 'u', 'u', 'u', 'b', 't', 'n', 'u', 'f', 'r',
      'u',
      'u',  // 00
      'u',  'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
      'u',
      'u',  // 10
      0,    0,   '"', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,
      '/',  // 20
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,
      0,  // 30~4F
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      '\\', 0,   0,   0,  // 50
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,
      0,  // 60~FF
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,    0,   0,   0};

  TRI_string_buffer_t* buffer = _buffer->stringBuffer();
  // reserve enough room for the whole string at once
  // each character is at most 6 bytes (if we ignore surrogate pairs here)
  // plus we need two bytes for the enclosing double quotes
  int res = TRI_ReserveStringBuffer(buffer, 6 * len + 2);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_AppendCharUnsafeStringBuffer(buffer, '"');

  uint8_t const* p = reinterpret_cast<uint8_t const*>(src);
  uint8_t const* e = p + len;
  while (p < e) {
    uint8_t c = *p;

    if ((c & 0x80U) == 0) {
      // check for control characters
      char esc = EscapeTable[c];

      if (esc) {
        if (c != '/' || options->escapeForwardSlashes) {
          // escape forward slashes only when requested
          TRI_AppendCharUnsafeStringBuffer(buffer, '\\');
        }
        TRI_AppendCharUnsafeStringBuffer(buffer, static_cast<char>(esc));

        if (esc == 'u') {
          // control character
          TRI_AppendCharUnsafeStringBuffer(buffer, '0');
          TRI_AppendCharUnsafeStringBuffer(buffer, '0');

          uint16_t value;
          value = (((uint16_t)c) & 0xf0U) >> 4;
          TRI_AppendCharUnsafeStringBuffer(
              buffer, static_cast<char>((value < 10) ? ('0' + value) : ('A' + value - 10)));
          value = (((uint16_t)c) & 0x0fU);
          TRI_AppendCharUnsafeStringBuffer(
              buffer, static_cast<char>((value < 10) ? ('0' + value) : ('A' + value - 10)));
        }
      } else {
        TRI_AppendCharUnsafeStringBuffer(buffer, static_cast<char>(c));
      }
    } else if ((c & 0xe0U) == 0xc0U) {
      // two-byte sequence
      if (p + 1 >= e) {
        throw velocypack::Exception(velocypack::Exception::InvalidUtf8Sequence);
      }
      if (options->escapeUnicode) {
        uint16_t value = ((((uint16_t)*p & 0x1fU) << 6) | ((uint16_t) * (p + 1) & 0x3fU));
        appendUnicodeCharacter(value);
      } else {
        TRI_AppendStringUnsafeStringBuffer(buffer, reinterpret_cast<char const*>(p), 2);
      }
      ++p;
    } else if ((c & 0xf0U) == 0xe0U) {
      // three-byte sequence
      if (p + 2 >= e) {
        throw velocypack::Exception(velocypack::Exception::InvalidUtf8Sequence);
      }
      if (options->escapeUnicode) {
        uint16_t value = ((((uint16_t)*p & 0x0fU) << 12) |
                          (((uint16_t) * (p + 1) & 0x3fU) << 6) |
                          ((uint16_t) * (p + 2) & 0x3fU));
        appendUnicodeCharacter(value);
      } else {
        TRI_AppendStringUnsafeStringBuffer(buffer, reinterpret_cast<char const*>(p), 3);
      }
      p += 2;
    } else if ((c & 0xf8U) == 0xf0U) {
      // four-byte sequence
      if (p + 3 >= e) {
        throw velocypack::Exception(velocypack::Exception::InvalidUtf8Sequence);
      }
      if (options->escapeUnicode) {
        // must now reserve more memory
        if (TRI_ReserveStringBuffer(buffer, 6 * (p - reinterpret_cast<uint8_t const*>(src) +
                                                 2)) != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }

        uint32_t value =
            ((((uint32_t)*p & 0x0fU) << 18) | (((uint32_t) * (p + 1) & 0x3fU) << 12) |
             (((uint32_t) * (p + 2) & 0x3fU) << 6) | ((uint32_t) * (p + 3) & 0x3fU));

        // construct the surrogate pairs
        value -= 0x10000U;
        uint16_t high = (uint16_t)(((value & 0xffc00U) >> 10) + 0xd800);
        appendUnicodeCharacter(high);
        uint16_t low = (value & 0x3ffU) + 0xdc00U;
        appendUnicodeCharacter(low);
      } else {
        TRI_AppendStringUnsafeStringBuffer(buffer, reinterpret_cast<char const*>(p), 4);
      }
      p += 3;
    }

    ++p;
  }

  TRI_AppendCharUnsafeStringBuffer(buffer, '"');
}

void VelocyPackDumper::dumpValue(VPackSlice const* slice, VPackSlice const* base) {
  if (base == nullptr) {
    base = slice;
  }

  TRI_string_buffer_t* buffer = _buffer->stringBuffer();

  // alloc at least 32 bytes
  int res = TRI_ReserveStringBuffer(buffer, 32);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  switch (slice->type()) {
    case VPackValueType::Null: {
      TRI_AppendStringUnsafeStringBuffer(buffer, "null", 4);
      break;
    }

    case VPackValueType::Bool: {
      if (slice->getBool()) {
        TRI_AppendStringUnsafeStringBuffer(buffer, "true", 4);
      } else {
        TRI_AppendStringUnsafeStringBuffer(buffer, "false", 5);
      }
      break;
    }

    case VPackValueType::Array: {
      VPackArrayIterator it(*slice);
      TRI_AppendCharUnsafeStringBuffer(buffer, '[');
      while (it.valid()) {
        if (!it.isFirst()) {
          if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
        dumpValue(it.value(), slice);
        it.next();
      }
      if (TRI_AppendCharStringBuffer(buffer, ']') != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
      break;
    }

    case VPackValueType::Object: {
      VPackObjectIterator it(*slice, true);
      TRI_AppendCharUnsafeStringBuffer(buffer, '{');
      while (it.valid()) {
        if (!it.isFirst()) {
          if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
        dumpValue(it.key().makeKey(), slice);
        if (TRI_AppendCharStringBuffer(buffer, ':') != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
        dumpValue(it.value(), slice);
        it.next();
      }
      if (TRI_AppendCharStringBuffer(buffer, '}') != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
      break;
    }

    case VPackValueType::Double: {
      double const v = slice->getDouble();
      if (std::isnan(v) || !std::isfinite(v)) {
        handleUnsupportedType(slice);
      } else {
        appendDouble(v);
      }
      break;
    }

    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt: {
      dumpInteger(slice);
      break;
    }

    case VPackValueType::String: {
      VPackValueLength len;
      char const* p = slice->getString(len);
      appendString(p, len);
      break;
    }

    case VPackValueType::External: {
      VPackSlice const external(reinterpret_cast<uint8_t const*>(slice->getExternal()));
      dumpValue(&external, base);
      break;
    }

    case VPackValueType::Custom: {
      if (options->customTypeHandler == nullptr) {
        throw VPackException(VPackException::NeedCustomTypeHandler);
      } else {
        std::string v = options->customTypeHandler->toString(*slice, nullptr, *base);
        appendString(v.c_str(), v.size());
      }
      break;
    }

    case VPackValueType::UTCDate:
    case VPackValueType::None:
    case VPackValueType::Binary:
    case VPackValueType::Illegal:
    case VPackValueType::MinKey:
    case VPackValueType::MaxKey:
    case VPackValueType::BCD: 
    case VPackValueType::Tagged: {
      handleUnsupportedType(slice);
      break;
    }
  }
}
