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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////


#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace arangodb {

struct ControlCharsSuppressor { //control chars that will not be escaped
  size_t maxCharLength() const { return 1; }
  void writeCharIntoOutputBuffer(uint32_t c, char*& output, int numBytes);
};
struct ControlCharsEscaper { //\x07 worst case
  size_t maxCharLength() const { return 4; }
  void writeCharIntoOutputBuffer(uint32_t c, char*& output, int numBytes);
};
struct UnicodeCharsRetainer { //worst case 4 digits
  size_t maxCharLength() const { return 4; }
  void writeCharIntoOutputBuffer(uint32_t c, char*& output, int numBytes);
};
struct UnicodeCharsEscaper { //\u +4 digits
  size_t maxCharLength() const { return 6; }
  void writeCharIntoOutputBuffer(uint32_t c, char*& output, int numBytes);
  void writeCharHelper(uint16_t c, char*& output);
};

class GeneralEscaper {
 public:
  virtual ~GeneralEscaper() = default;
  virtual size_t determineOutputBufferSize(std::string const& message) const = 0;
  virtual void writeIntoOutputBuffer(std::string const& message, char*& buffer) = 0;
};

template <typename ControlCharHandler, typename UnicodeCharHandler>
class Escaper : public GeneralEscaper {
 private:
  ControlCharHandler _controlHandler;
  UnicodeCharHandler _unicodeHandler;
 public:

  size_t determineOutputBufferSize(std::string const& message) const override;

  void writeIntoOutputBuffer(std::string const& message, char*& buffer) override;
};

}

