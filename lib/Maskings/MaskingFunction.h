////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_MASKINGS_MASKING_FUNCTION_H
#define ARANGODB_MASKINGS_MASKING_FUNCTION_H 1

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace maskings {
class Maskings;

class MaskingFunction {
 public:
  static bool isNameChar(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
           ('0' <= c && c <= '9') || c == '_' || c == '-';
  }

  static bool utf8Length(uint8_t c) {
    if ((c & 0x80) == 0) {
      return 1;
    } else if ((c & 0xE0) == 0xC0) {
      return 2;
    } else if ((c & 0xF0) == 0xE0) {
      return 3;
    } else if ((c & 0xF8) == 0xF0) {
      return 4;
    }

    return 1;
  }

 public:
  explicit MaskingFunction(Maskings* maskings) : _maskings(maskings) {}
  virtual ~MaskingFunction() {}

 public:
  virtual VPackValue mask(bool) const = 0;
  virtual VPackValue mask(std::string const&, std::string& buffer) const = 0;
  virtual VPackValue mask(int64_t) const = 0;
  virtual VPackValue mask(double) const = 0;

 protected:
  Maskings* _maskings;
};
}  // namespace maskings
}  // namespace arangodb

#endif
