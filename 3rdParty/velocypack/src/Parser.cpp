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

#include "velocypack/velocypack-common.h"
#include "velocypack/Parser.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"
#include "asm-functions.h"

#include <cstdlib>

using namespace arangodb::velocypack;

// The following function does the actual parse. It gets bytes
// via peek, consume and reset appends the result to the Builder
// in *_builderPtr. Errors are reported via an exception.
// Behind the scenes it runs two parses, one to collect sizes and
// check for parse errors (scan phase) and then one to actually
// build the result (build phase).

ValueLength Parser::parseInternal(bool multi) {
  // skip over optional BOM
  if (_size >= 3 && _start[0] == 0xef && _start[1] == 0xbb &&
      _start[2] == 0xbf) {
    // found UTF-8 BOM. simply skip over it
    _pos += 3;
  }

  ValueLength nr = 0;
  do {
    bool haveReported = false;
    if (!_builderPtr->_stack.empty()) {
      ValueLength const tos = _builderPtr->_stack.back();
      if (_builderPtr->_start[tos] == 0x0b || _builderPtr->_start[tos] == 0x14) {
        if (!_builderPtr->_keyWritten) {
          throw Exception(Exception::BuilderKeyMustBeString);
        }
        else {
          _builderPtr->_keyWritten = false;
        }
      }
      else {
        _builderPtr->reportAdd();
        haveReported = true;
      }
    }
    try {
      parseJson();
    }
    catch (...) {
      if (haveReported) {
        _builderPtr->cleanupAdd();
      }
      throw;
    }
    nr++;
    while (_pos < _size && isWhiteSpace(_start[_pos])) {
      ++_pos;
    }
    if (!multi && _pos != _size) {
      consume();  // to get error reporting right. return value intentionally not checked
      throw Exception(Exception::ParseError, "Expecting EOF");
    }
  } while (multi && _pos < _size);
  return nr;
}

// skips over all following whitespace tokens but does not consume the
// byte following the whitespace
int Parser::skipWhiteSpace(char const* err) {
  if (VELOCYPACK_UNLIKELY(_pos >= _size)) {
    throw Exception(Exception::ParseError, err);
  }
  uint8_t c = _start[_pos];
  if (!isWhiteSpace(c)) {
    return c;
  }
  if (c == ' ') {
    if (_pos + 1 >= _size) {
      _pos++;
      throw Exception(Exception::ParseError, err);
    }
    c = _start[_pos + 1];
    if (!isWhiteSpace(c)) {
      _pos++;
      return c;
    }
  }
  std::size_t remaining = _size - _pos;
  if (remaining >= 16) {
    std::size_t count = JSONSkipWhiteSpace(_start + _pos, remaining - 15);
    _pos += count;
  }
  do {
    if (!isWhiteSpace(_start[_pos])) {
      return static_cast<int>(_start[_pos]);
    }
    _pos++;
  } while (_pos < _size);
  throw Exception(Exception::ParseError, err);
}

// parses a number value
void Parser::parseNumber() {
  std::size_t startPos = _pos;
  ParsedNumber numberValue;
  bool negative = false;
  int i = consume();
  // We know that a character is coming, and it's a number if it
  // starts with '-' or a digit. otherwise it's invalid
  if (i == '-') {
    i = getOneOrThrow("Incomplete number");
    negative = true;
  }
  if (i < '0' || i > '9') {
    throw Exception(Exception::ParseError, "Expecting digit");
  }

  if (i != '0') {
    unconsume();
    scanDigits(numberValue);
  }
  i = consume();
  if (i < 0 || (i != '.' && i != 'e' && i != 'E')) {
    if (i >= 0) {
      unconsume();
    }
    if (!numberValue.isInteger) {
      if (negative) {
        _builderPtr->addDouble(-numberValue.doubleValue);
      } else {
        _builderPtr->addDouble(numberValue.doubleValue);
      }
    } else if (negative) {
      if (numberValue.intValue <= static_cast<uint64_t>(INT64_MAX)) {
        _builderPtr->addInt(-static_cast<int64_t>(numberValue.intValue));
      } else if (numberValue.intValue == toUInt64(INT64_MIN)) {
        _builderPtr->addInt(INT64_MIN);
      } else {
        _builderPtr->addDouble(-static_cast<double>(numberValue.intValue));
      }
    } else {
      _builderPtr->addUInt(numberValue.intValue);
    }
    return;
  }

  double fractionalPart;
  if (i == '.') {
    // fraction. skip over '.'
    i = getOneOrThrow("Incomplete number");
    if (i < '0' || i > '9') {
      throw Exception(Exception::ParseError, "Incomplete number");
    }
    unconsume();
    fractionalPart = scanDigitsFractional();
    if (negative) {
      fractionalPart = -numberValue.asDouble() - fractionalPart;
    } else {
      fractionalPart = numberValue.asDouble() + fractionalPart;
    }
    i = consume();
    if (i < 0) {
      _builderPtr->addDouble(fractionalPart);
      return;
    }
  } else {
    if (negative) {
      fractionalPart = -numberValue.asDouble();
    } else {
      fractionalPart = numberValue.asDouble();
    }
  }
  if (i != 'e' && i != 'E') {
    unconsume();
    // use conventional atof() conversion here, to avoid precision loss
    // when interpreting and multiplying the single digits of the input stream
    // _builderPtr->addDouble(fractionalPart);
    _builderPtr->addDouble(atof(reinterpret_cast<char const*>(_start) + startPos));
    return;
  }
  i = getOneOrThrow("Incomplete number");
  negative = false;
  if (i == '+' || i == '-') {
    negative = (i == '-');
    i = getOneOrThrow("Incomplete number");
  }
  if (i < '0' || i > '9') {
    throw Exception(Exception::ParseError, "Incomplete number");
  }
  unconsume();
  ParsedNumber exponent;
  scanDigits(exponent);
  if (negative) {
    fractionalPart *= pow(10, -exponent.asDouble());
  } else {
    fractionalPart *= pow(10, exponent.asDouble());
  }
  if (std::isnan(fractionalPart) || !std::isfinite(fractionalPart)) {
    throw Exception(Exception::NumberOutOfRange);
  }
  // use conventional atof() conversion here, to avoid precision loss
  // when interpreting and multiplying the single digits of the input stream
  // _builderPtr->addDouble(fractionalPart);
  _builderPtr->addDouble(atof(reinterpret_cast<char const*>(_start) + startPos));
}

void Parser::parseString() {
  // When we get here, we have seen a " character and now want to
  // find the end of the string and parse the string value to its
  // VPack representation. We assume that the string is short and
  // insert 8 bytes for the length as soon as we reach 127 bytes
  // in the VPack representation.
  ValueLength const base = _builderPtr->_pos;
  _builderPtr->appendByte(0x40); // correct this later

  bool large = false;          // set to true when we reach 128 bytes
  uint32_t highSurrogate = 0;  // non-zero if high-surrogate was seen

  while (true) {
    std::size_t remainder = _size - _pos;
    if (remainder >= 16) {
      _builderPtr->reserve(remainder);
      std::size_t count;
      // Note that the SSE4.2 accelerated string copying functions might
      // peek up to 15 bytes over the given end, because they use 128bit
      // registers. Therefore, we have to subtract 15 from remainder
      // to be on the safe side. Further bytes will be processed below.
      if (options->validateUtf8Strings) {
        count = JSONStringCopyCheckUtf8(_builderPtr->_start + _builderPtr->_pos, _start + _pos,
                                        remainder - 15);
      } else {
        count = JSONStringCopy(_builderPtr->_start + _builderPtr->_pos, _start + _pos,
                               remainder - 15);
      }
      _pos += count;
      _builderPtr->advance(count);
    }
    int i = getOneOrThrow("Unfinished string");
    if (!large && _builderPtr->_pos - (base + 1) > 126) {
      large = true;
      _builderPtr->reserve(8);
      ValueLength len = _builderPtr->_pos - (base + 1);
      memmove(_builderPtr->_start + base + 9, _builderPtr->_start + base + 1, checkOverflow(len));
      _builderPtr->advance(8);
    }
    switch (i) {
      case '"':
        ValueLength len;
        if (!large) {
          len = _builderPtr->_pos - (base + 1);
          _builderPtr->_start[base] = 0x40 + static_cast<uint8_t>(len);
          // String is ready
        } else {
          len = _builderPtr->_pos - (base + 9);
          _builderPtr->_start[base] = 0xbf;
          for (ValueLength i = 1; i <= 8; i++) {
            _builderPtr->_start[base + i] = len & 0xff;
            len >>= 8;
          }
        }
        return;
      case '\\':
        // Handle cases or throw error
        i = consume();
        if (VELOCYPACK_UNLIKELY(i < 0)) {
          throw Exception(Exception::ParseError, "Invalid escape sequence");
        }
        switch (i) {
          case '"':
          case '/':
          case '\\':
            _builderPtr->appendByte(static_cast<uint8_t>(i));
            highSurrogate = 0;
            break;
          case 'b':
            _builderPtr->appendByte('\b');
            highSurrogate = 0;
            break;
          case 'f':
            _builderPtr->appendByte('\f');
            highSurrogate = 0;
            break;
          case 'n':
            _builderPtr->appendByte('\n');
            highSurrogate = 0;
            break;
          case 'r':
            _builderPtr->appendByte('\r');
            highSurrogate = 0;
            break;
          case 't':
            _builderPtr->appendByte('\t');
            highSurrogate = 0;
            break;
          case 'u': {
            uint32_t v = 0;
            for (int j = 0; j < 4; j++) {
              i = consume();
              if (i < 0) {
                throw Exception(Exception::ParseError,
                                "Unfinished \\uXXXX escape sequence");
              }
              if (i >= '0' && i <= '9') {
                v = (v << 4) + i - '0';
              } else if (i >= 'a' && i <= 'f') {
                v = (v << 4) + i - 'a' + 10;
              } else if (i >= 'A' && i <= 'F') {
                v = (v << 4) + i - 'A' + 10;
              } else {
                throw Exception(Exception::ParseError,
                                "Illegal \\uXXXX escape sequence");
              }
            }
            if (v < 0x80) {
              _builderPtr->appendByte(static_cast<uint8_t>(v));
              highSurrogate = 0;
            } else if (v < 0x800) {
              _builderPtr->reserve(2);
              _builderPtr->appendByteUnchecked(0xc0 + (v >> 6));
              _builderPtr->appendByteUnchecked(0x80 + (v & 0x3f));
              highSurrogate = 0;
            } else if (v >= 0xdc00 && v < 0xe000 && highSurrogate != 0) {
              // Low surrogate, put the two together:
              v = 0x10000 + ((highSurrogate - 0xd800) << 10) + v - 0xdc00;
              _builderPtr->rollback(3);
              _builderPtr->reserve(4);
              _builderPtr->appendByteUnchecked(0xf0 + (v >> 18));
              _builderPtr->appendByteUnchecked(0x80 + ((v >> 12) & 0x3f));
              _builderPtr->appendByteUnchecked(0x80 + ((v >> 6) & 0x3f));
              _builderPtr->appendByteUnchecked(0x80 + (v & 0x3f));
              highSurrogate = 0;
            } else {
              if (v >= 0xd800 && v < 0xdc00) {
                // High surrogate:
                highSurrogate = v;
              } else {
                highSurrogate = 0;
              }
              _builderPtr->reserve(3);
              _builderPtr->appendByteUnchecked(0xe0 + (v >> 12));
              _builderPtr->appendByteUnchecked(0x80 + ((v >> 6) & 0x3f));
              _builderPtr->appendByteUnchecked(0x80 + (v & 0x3f));
            }
            break;
          }
          default:
            throw Exception(Exception::ParseError, "Invalid escape sequence");
        }
        break;
      default:
        if ((i & 0x80) == 0) {
          // non-UTF-8 sequence
          if (VELOCYPACK_UNLIKELY(i < 0x20)) {
            // control character
            throw Exception(Exception::UnexpectedControlCharacter);
          }
          highSurrogate = 0;
          _builderPtr->appendByte(static_cast<uint8_t>(i));
        } else {
          if (!options->validateUtf8Strings) {
            highSurrogate = 0;
            _builderPtr->appendByte(static_cast<uint8_t>(i));
          } else {
            // multi-byte UTF-8 sequence!
            int follow = 0;
            if ((i & 0xe0) == 0x80) {
              throw Exception(Exception::InvalidUtf8Sequence);
            } else if ((i & 0xe0) == 0xc0) {
              // two-byte sequence
              follow = 1;
            } else if ((i & 0xf0) == 0xe0) {
              // three-byte sequence
              follow = 2;
            } else if ((i & 0xf8) == 0xf0) {
              // four-byte sequence
              follow = 3;
            } else {
              throw Exception(Exception::InvalidUtf8Sequence);
            }

            // validate follow up characters
            _builderPtr->reserve(1 + follow);
            _builderPtr->appendByteUnchecked(static_cast<uint8_t>(i));
            for (int j = 0; j < follow; ++j) {
              i = getOneOrThrow("scanString: truncated UTF-8 sequence");
              if ((i & 0xc0) != 0x80) {
                throw Exception(Exception::InvalidUtf8Sequence);
              }
              _builderPtr->appendByteUnchecked(static_cast<uint8_t>(i));
            }
            highSurrogate = 0;
          }
        }
        break;
    }
  }
}

void Parser::parseArray() {
  _builderPtr->addArray();

  int i = skipWhiteSpace("Expecting item or ']'");
  if (i == ']') {
    // empty array
    ++_pos;  // the closing ']'
    _builderPtr->close();
    return;
  }

  increaseNesting();

  while (true) {
    // parse array element itself
    _builderPtr->reportAdd();
    parseJson();
    i = skipWhiteSpace("Expecting ',' or ']'");
    if (i == ']') {
      // end of array
      ++_pos;  // the closing ']'
      _builderPtr->close();
      decreaseNesting();
      return;
    }
    // skip over ','
    if (VELOCYPACK_UNLIKELY(i != ',')) {
      throw Exception(Exception::ParseError, "Expecting ',' or ']'");
    }
    ++_pos;  // the ','
  }

  // should never get here
  VELOCYPACK_ASSERT(false);
}

void Parser::parseObject() {
  _builderPtr->addObject();

  int i = skipWhiteSpace("Expecting item or '}'");
  if (i == '}') {
    // empty object
    consume();  // the closing '}'. return value intentionally not checked

    if (_nesting != 0 || !options->keepTopLevelOpen) {
      // only close if we've not been asked to keep top level open
      _builderPtr->close();
    }
    return;
  }

  increaseNesting();

  while (true) {
    // always expecting a string attribute name here
    if (VELOCYPACK_UNLIKELY(i != '"')) {
      throw Exception(Exception::ParseError, "Expecting '\"' or '}'");
    }
    // get past the initial '"'
    ++_pos;

    _builderPtr->reportAdd();
    auto const lastPos = _builderPtr->_pos;
    parseString();

    if (options->attributeTranslator != nullptr) {
      // check if a translation for the attribute name exists
      Slice key(_builderPtr->_start + lastPos);

      if (key.isString()) {
        ValueLength keyLength;
        char const* p = key.getString(keyLength);
        uint8_t const* translated =
            options->attributeTranslator->translate(p, keyLength);

        if (translated != nullptr) {
          // found translation... now reset position to old key position
          // and simply overwrite the existing key with the numeric translation
          // id
          _builderPtr->resetTo(lastPos);
          _builderPtr->addUInt(Slice(translated).getUInt());
        }
      }
    }

    i = skipWhiteSpace("Expecting ':'");
    // always expecting the ':' here
    if (VELOCYPACK_UNLIKELY(i != ':')) {
      throw Exception(Exception::ParseError, "Expecting ':'");
    }
    ++_pos;  // skip over the colon

    parseJson();

    i = skipWhiteSpace("Expecting ',' or '}'");
    if (i == '}') {
      // end of object
      ++_pos;  // the closing '}'
      if (_nesting != 1 || !options->keepTopLevelOpen) {
        // only close if we've not been asked to keep top level open
        _builderPtr->close();
      }
      decreaseNesting();
      return;
    }
    if (VELOCYPACK_UNLIKELY(i != ',')) {
      throw Exception(Exception::ParseError, "Expecting ',' or '}'");
    }
    // skip over ','
    ++_pos;  // the ','
    i = skipWhiteSpace("Expecting '\"' or '}'");
  }

  // should never get here
  VELOCYPACK_ASSERT(false);
}

void Parser::parseJson() {
  skipWhiteSpace("Expecting item"); // return value intentionally not checked

  int i = consume();
  if (i < 0) {
    return;
  }
  switch (i) {
    case '{':
      parseObject();  // this consumes the closing '}' or throws
      break;
    case '[':
      parseArray();  // this consumes the closing ']' or throws
      break;
    case 't':
      parseTrue();  // this consumes "rue" or throws
      break;
    case 'f':
      parseFalse();  // this consumes "alse" or throws
      break;
    case 'n':
      parseNull();  // this consumes "ull" or throws
      break;
    case '"':
      parseString();
      break;
    default: {
      // everything else must be a number or is invalid...
      // this includes '-' and '0' to '9'. scanNumber() will
      // throw if the input is non-numeric
      unconsume();
      parseNumber();  // this consumes the number or throws
      break;
    }
  }
}
