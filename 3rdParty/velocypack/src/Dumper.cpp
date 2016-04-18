////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include "velocypack/velocypack-common.h"
#include "velocypack/Dumper.h"
#include "velocypack/Iterator.h"
#include "velocypack/ValueType.h"

using namespace arangodb::velocypack;

// forward for fpconv function declared elsewhere
namespace arangodb {
namespace velocypack {
int fpconv_dtoa(double fp, char dest[24]);
}
};

void Dumper::appendUInt(uint64_t v) {
  if (10000000000000000000ULL <= v) {
    _sink->push_back('0' + (v / 10000000000000000000ULL) % 10);
  }
  if (1000000000000000000ULL <= v) {
    _sink->push_back('0' + (v / 1000000000000000000ULL) % 10);
  }
  if (100000000000000000ULL <= v) {
    _sink->push_back('0' + (v / 100000000000000000ULL) % 10);
  }
  if (10000000000000000ULL <= v) {
    _sink->push_back('0' + (v / 10000000000000000ULL) % 10);
  }
  if (1000000000000000ULL <= v) {
    _sink->push_back('0' + (v / 1000000000000000ULL) % 10);
  }
  if (100000000000000ULL <= v) {
    _sink->push_back('0' + (v / 100000000000000ULL) % 10);
  }
  if (10000000000000ULL <= v) {
    _sink->push_back('0' + (v / 10000000000000ULL) % 10);
  }
  if (1000000000000ULL <= v) {
    _sink->push_back('0' + (v / 1000000000000ULL) % 10);
  }
  if (100000000000ULL <= v) {
    _sink->push_back('0' + (v / 100000000000ULL) % 10);
  }
  if (10000000000ULL <= v) {
    _sink->push_back('0' + (v / 10000000000ULL) % 10);
  }
  if (1000000000ULL <= v) {
    _sink->push_back('0' + (v / 1000000000ULL) % 10);
  }
  if (100000000ULL <= v) {
    _sink->push_back('0' + (v / 100000000ULL) % 10);
  }
  if (10000000ULL <= v) {
    _sink->push_back('0' + (v / 10000000ULL) % 10);
  }
  if (1000000ULL <= v) {
    _sink->push_back('0' + (v / 1000000ULL) % 10);
  }
  if (100000ULL <= v) {
    _sink->push_back('0' + (v / 100000ULL) % 10);
  }
  if (10000ULL <= v) {
    _sink->push_back('0' + (v / 10000ULL) % 10);
  }
  if (1000ULL <= v) {
    _sink->push_back('0' + (v / 1000ULL) % 10);
  }
  if (100ULL <= v) {
    _sink->push_back('0' + (v / 100ULL) % 10);
  }
  if (10ULL <= v) {
    _sink->push_back('0' + (v / 10ULL) % 10);
  }

  _sink->push_back('0' + (v % 10));
}

void Dumper::appendDouble(double v) {
  char temp[24];
  int len = fpconv_dtoa(v, &temp[0]);
  _sink->append(&temp[0], static_cast<ValueLength>(len));
}

void Dumper::dumpInteger(Slice const* slice) {
  VELOCYPACK_ASSERT(slice->isInteger());

  if (slice->isType(ValueType::UInt)) {
    uint64_t v = slice->getUInt();

    appendUInt(v);
  } else if (slice->isType(ValueType::Int)) {
    int64_t v = slice->getInt();
    if (v == INT64_MIN) {
      _sink->append("-9223372036854775808", 20);
      return;
    }
    if (v < 0) {
      _sink->push_back('-');
      v = -v;
    }

    if (1000000000000000000LL <= v) {
      _sink->push_back('0' + (v / 1000000000000000000LL) % 10);
    }
    if (100000000000000000LL <= v) {
      _sink->push_back('0' + (v / 100000000000000000LL) % 10);
    }
    if (10000000000000000LL <= v) {
      _sink->push_back('0' + (v / 10000000000000000LL) % 10);
    }
    if (1000000000000000LL <= v) {
      _sink->push_back('0' + (v / 1000000000000000LL) % 10);
    }
    if (100000000000000LL <= v) {
      _sink->push_back('0' + (v / 100000000000000LL) % 10);
    }
    if (10000000000000LL <= v) {
      _sink->push_back('0' + (v / 10000000000000LL) % 10);
    }
    if (1000000000000LL <= v) {
      _sink->push_back('0' + (v / 1000000000000LL) % 10);
    }
    if (100000000000LL <= v) {
      _sink->push_back('0' + (v / 100000000000LL) % 10);
    }
    if (10000000000LL <= v) {
      _sink->push_back('0' + (v / 10000000000LL) % 10);
    }
    if (1000000000LL <= v) {
      _sink->push_back('0' + (v / 1000000000LL) % 10);
    }
    if (100000000LL <= v) {
      _sink->push_back('0' + (v / 100000000LL) % 10);
    }
    if (10000000LL <= v) {
      _sink->push_back('0' + (v / 10000000LL) % 10);
    }
    if (1000000LL <= v) {
      _sink->push_back('0' + (v / 1000000LL) % 10);
    }
    if (100000LL <= v) {
      _sink->push_back('0' + (v / 100000LL) % 10);
    }
    if (10000LL <= v) {
      _sink->push_back('0' + (v / 10000LL) % 10);
    }
    if (1000LL <= v) {
      _sink->push_back('0' + (v / 1000LL) % 10);
    }
    if (100LL <= v) {
      _sink->push_back('0' + (v / 100LL) % 10);
    }
    if (10LL <= v) {
      _sink->push_back('0' + (v / 10LL) % 10);
    }

    _sink->push_back('0' + (v % 10));
  } else if (slice->isType(ValueType::SmallInt)) {
    int64_t v = slice->getSmallInt();
    if (v < 0) {
      _sink->push_back('-');
      v = -v;
    }
    _sink->push_back('0' + static_cast<char>(v));
  }
}

void Dumper::dumpString(char const* src, ValueLength len) {
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

  uint8_t const* p = reinterpret_cast<uint8_t const*>(src);
  uint8_t const* e = p + len;
  while (p < e) {
    uint8_t c = *p;

    if ((c & 0x80) == 0) {
      // check for control characters
      char esc = EscapeTable[c];

      if (esc) {
        if (c != '/' || options->escapeForwardSlashes) {
          // escape forward slashes only when requested
          _sink->push_back('\\');
        }
        _sink->push_back(static_cast<char>(esc));

        if (esc == 'u') {
          uint16_t i1 = (((uint16_t)c) & 0xf0) >> 4;
          uint16_t i2 = (((uint16_t)c) & 0x0f);

          _sink->append("00", 2);
          _sink->push_back(
              static_cast<char>((i1 < 10) ? ('0' + i1) : ('A' + i1 - 10)));
          _sink->push_back(
              static_cast<char>((i2 < 10) ? ('0' + i2) : ('A' + i2 - 10)));
        }
      } else {
        _sink->push_back(static_cast<char>(c));
      }
    } else if ((c & 0xe0) == 0xc0) {
      // two-byte sequence
      if (p + 1 >= e) {
        throw Exception(Exception::InvalidUtf8Sequence);
      }

      _sink->append(reinterpret_cast<char const*>(p), 2);
      ++p;
    } else if ((c & 0xf0) == 0xe0) {
      // three-byte sequence
      if (p + 2 >= e) {
        throw Exception(Exception::InvalidUtf8Sequence);
      }

      _sink->append(reinterpret_cast<char const*>(p), 3);
      p += 2;
    } else if ((c & 0xf8) == 0xf0) {
      // four-byte sequence
      if (p + 3 >= e) {
        throw Exception(Exception::InvalidUtf8Sequence);
      }

      _sink->append(reinterpret_cast<char const*>(p), 4);
      p += 3;
    }

    ++p;
  }
}

void Dumper::dumpValue(Slice const* slice, Slice const* base) {
  if (base == nullptr) {
    base = slice;
    VELOCYPACK_ASSERT(base != nullptr);
  }

  switch (slice->type()) {
    case ValueType::None: {
      handleUnsupportedType(slice);
      break;
    }

    case ValueType::Null: {
      _sink->append("null", 4);
      break;
    }

    case ValueType::Bool: {
      if (slice->getBool()) {
        _sink->append("true", 4);
      } else {
        _sink->append("false", 5);
      }
      break;
    }

    case ValueType::Array: {
      ArrayIterator it(*slice);
      _sink->push_back('[');
      if (options->prettyPrint) {
        _sink->push_back('\n');
        ++_indentation;
        while (it.valid()) {
          indent();
          dumpValue(it.value(), slice);
          if (!it.isLast()) {
            _sink->push_back(',');
          }
          _sink->push_back('\n');
          it.next();
        }
        --_indentation;
        indent();
      } else {
        while (it.valid()) {
          if (!it.isFirst()) {
            _sink->push_back(',');
          }
          dumpValue(it.value(), slice);
          it.next();
        }
      }
      _sink->push_back(']');
      break;
    }

    case ValueType::Object: {
      ObjectIterator it(*slice);
      _sink->push_back('{');
      if (options->prettyPrint) {
        _sink->push_back('\n');
        ++_indentation;
        while (it.valid()) {
          indent();
          dumpValue(it.key().makeKey(), slice);
          _sink->append(" : ", 3);
          dumpValue(it.value(), slice);
          if (!it.isLast()) {
            _sink->push_back(',');
          }
          _sink->push_back('\n');
          it.next();
        }
        --_indentation;
        indent();
      } else {
        while (it.valid()) {
          if (!it.isFirst()) {
            _sink->push_back(',');
          }
          dumpValue(it.key().makeKey(), slice);
          _sink->push_back(':');
          dumpValue(it.value(), slice);
          it.next();
        }
      }
      _sink->push_back('}');
      break;
    }

    case ValueType::Double: {
      double const v = slice->getDouble();
      if (std::isnan(v) || !std::isfinite(v)) {
        handleUnsupportedType(slice);
      } else {
        appendDouble(v);
      }
      break;
    }

    case ValueType::UTCDate: {
      handleUnsupportedType(slice);
      break;
    }

    case ValueType::External: {
      Slice const external(slice->getExternal());
      dumpValue(&external, base);
      break;
    }

    case ValueType::Illegal:
    case ValueType::MinKey:
    case ValueType::MaxKey: {
      handleUnsupportedType(slice);
      break;
    }

    case ValueType::Int:
    case ValueType::UInt:
    case ValueType::SmallInt: {
      dumpInteger(slice);
      break;
    }

    case ValueType::String: {
      ValueLength len;
      char const* p = slice->getString(len);
      _sink->reserve(2 + len);
      _sink->push_back('"');
      dumpString(p, len);
      _sink->push_back('"');
      break;
    }

    case ValueType::Binary: {
      handleUnsupportedType(slice);
      break;
    }

    case ValueType::BCD: {
      // TODO
      throw Exception(Exception::NotImplemented);
    }

    case ValueType::Custom: {
      if (options->customTypeHandler == nullptr) {
        throw Exception(Exception::NeedCustomTypeHandler);
      } else {
        options->customTypeHandler->dump(*slice, this, *base);
      }
      break;
    }
  }
}
