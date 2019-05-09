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

#ifndef VELOCYPACK_PARSER_H
#define VELOCYPACK_PARSER_H 1

#include <string>
#include <cmath>

#include "velocypack/velocypack-common.h"
#include "velocypack/Builder.h"
#include "velocypack/Exception.h"
#include "velocypack/Options.h"

namespace arangodb {
namespace velocypack {

class Parser {
  // This class can parse JSON very rapidly, but only from contiguous
  // blocks of memory. It builds the result using the Builder.

  struct ParsedNumber {
    ParsedNumber() : intValue(0), doubleValue(0.0), isInteger(true) {}

    void addDigit(int i) {
      if (isInteger) {
        // check if adding another digit to the int will make it overflow
        if (intValue < 1844674407370955161ULL ||
            (intValue == 1844674407370955161ULL && (i - '0') <= 5)) {
          // int won't overflow
          intValue = intValue * 10 + (i - '0');
          return;
        }
        // int would overflow
        doubleValue = static_cast<double>(intValue);
        isInteger = false;
      }

      doubleValue = doubleValue * 10.0 + (i - '0');
      if (std::isnan(doubleValue) || !std::isfinite(doubleValue)) {
        throw Exception(Exception::NumberOutOfRange);
      }
    }

    double asDouble() const {
      if (isInteger) {
        return static_cast<double>(intValue);
      }
      return doubleValue;
    }

    uint64_t intValue;
    double doubleValue;
    bool isInteger;
  };

  std::shared_ptr<Builder> _builder;
  Builder* _builderPtr;
  uint8_t const* _start;
  std::size_t _size;
  std::size_t _pos;
  int _nesting;

 public:
  Options const* options;

  Parser(Parser const&) = delete;
  Parser(Parser&&) = delete;
  Parser& operator=(Parser const&) = delete;
  Parser& operator=(Parser&&) = delete;
  ~Parser() = default;
  
  Parser()
      : _start(nullptr), 
        _size(0), 
        _pos(0), 
        _nesting(0), 
        options(&Options::Defaults) {
    _builder.reset(new Builder());
    _builderPtr = _builder.get();
    _builderPtr->options = &Options::Defaults;
  }

  explicit Parser(Options const* options) 
      : _start(nullptr), 
        _size(0), 
        _pos(0), 
        _nesting(0), 
        options(options) {
    if (VELOCYPACK_UNLIKELY(options == nullptr)) {
      throw Exception(Exception::InternalError, "Options cannot be a nullptr");
    }
    _builder.reset(new Builder());
    _builderPtr = _builder.get();
    _builderPtr->options = options;
  }

  explicit Parser(std::shared_ptr<Builder> const& builder,
                  Options const* options = &Options::Defaults)
      : _builder(builder),
        _builderPtr(_builder.get()), 
        _start(nullptr), 
        _size(0), 
        _pos(0), 
        _nesting(0),
         options(options) {
    if (VELOCYPACK_UNLIKELY(options == nullptr)) {
      throw Exception(Exception::InternalError, "Options cannot be a nullptr");
    }
  }

  // This method produces a parser that does not own the builder
  explicit Parser(Builder& builder,
                  Options const* options = &Options::Defaults)
      : _start(nullptr), 
        _size(0), 
        _pos(0), 
        _nesting(0),
         options(options) {
    if (VELOCYPACK_UNLIKELY(options == nullptr)) {
      throw Exception(Exception::InternalError, "Options cannot be a nullptr");
    }
    _builder.reset(&builder, BuilderNonDeleter());
    _builderPtr = _builder.get();
  }

  Builder const& builder() const { return *_builderPtr; }

  static std::shared_ptr<Builder> fromJson(
      std::string const& json,
      Options const* options = &Options::Defaults) {
    Parser parser(options);
    parser.parse(json);
    return parser.steal();
  }
  
  static std::shared_ptr<Builder> fromJson(
      char const* start, std::size_t size,
      Options const* options = &Options::Defaults) {
    Parser parser(options);
    parser.parse(start, size);
    return parser.steal();
  }

  static std::shared_ptr<Builder> fromJson(
      uint8_t const* start, std::size_t size,
      Options const* options = &Options::Defaults) {
    Parser parser(options);
    parser.parse(start, size);
    return parser.steal();
  }

  ValueLength parse(std::string const& json, bool multi = false) {
    return parse(reinterpret_cast<uint8_t const*>(json.data()), json.size(),
                 multi);
  }

  ValueLength parse(char const* start, std::size_t size, bool multi = false) {
    return parse(reinterpret_cast<uint8_t const*>(start), size, multi);
  }

  ValueLength parse(uint8_t const* start, std::size_t size, bool multi = false) {
    _start = start;
    _size = size;
    _pos = 0;
    if (options->clearBuilderBeforeParse) {
      _builder->clear();
    }
    return parseInternal(multi);
  }

  // We probably want a parse from stream at some stage...
  // Not with this high-performance two-pass approach. :-(

  std::shared_ptr<Builder> steal() {
    // Parser object is broken after a steal()
    std::shared_ptr<Builder> res(_builder);
    _builder.reset();
    _builderPtr = nullptr;
    return res;
  }

  // Beware, only valid as long as you do not parse more, use steal
  // to move the data out!
  uint8_t const* start() { return _builderPtr->start(); }

  // Returns the position at the time when the just reported error
  // occurred, only use when handling an exception.
  std::size_t errorPos() const { return _pos > 0 ? _pos - 1 : _pos; }

  void clear() { _builderPtr->clear(); }

 private:
  inline int peek() const {
    if (_pos >= _size) {
      return -1;
    }
    return static_cast<int>(_start[_pos]);
  }

  inline int consume() {
    if (_pos >= _size) {
      return -1;
    }
    return static_cast<int>(_start[_pos++]);
  }

  inline void unconsume() { --_pos; }

  inline void reset() { _pos = 0; }

  ValueLength parseInternal(bool multi);

  inline bool isWhiteSpace(uint8_t i) const noexcept {
    return (i == ' ' || i == '\t' || i == '\n' || i == '\r');
  }

  // skips over all following whitespace tokens but does not consume the
  // byte following the whitespace
  int skipWhiteSpace(char const*);

  void parseTrue() {
    // Called, when main mode has just seen a 't', need to see "rue" next
    if (consume() != 'r' || consume() != 'u' || consume() != 'e') {
      throw Exception(Exception::ParseError, "Expecting 'true'");
    }
    _builderPtr->addTrue();
  }

  void parseFalse() {
    // Called, when main mode has just seen a 'f', need to see "alse" next
    if (consume() != 'a' || consume() != 'l' || consume() != 's' ||
        consume() != 'e') {
      throw Exception(Exception::ParseError, "Expecting 'false'");
    }
    _builderPtr->addFalse();
  }

  void parseNull() {
    // Called, when main mode has just seen a 'n', need to see "ull" next
    if (consume() != 'u' || consume() != 'l' || consume() != 'l') {
      throw Exception(Exception::ParseError, "Expecting 'null'");
    }
    _builderPtr->addNull();
  }

  void scanDigits(ParsedNumber& value) {
    while (true) {
      int i = consume();
      if (i < 0) {
        return;
      }
      if (i < '0' || i > '9') {
        unconsume();
        return;
      }
      value.addDigit(i);
    }
  }

  double scanDigitsFractional() {
    double pot = 0.1;
    double x = 0.0;
    while (true) {
      int i = consume();
      if (i < 0) {
        return x;
      }
      if (i < '0' || i > '9') {
        unconsume();
        return x;
      }
      x = x + pot * (i - '0');
      pot /= 10.0;
    }
  }

  inline int getOneOrThrow(char const* msg) {
    int i = consume();
    if (i < 0) {
      throw Exception(Exception::ParseError, msg);
    }
    return i;
  }

  inline void increaseNesting() { ++_nesting; }

  inline void decreaseNesting() { --_nesting; }

  void parseNumber();

  void parseString();

  void parseArray();

  void parseObject();

  void parseJson();
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
