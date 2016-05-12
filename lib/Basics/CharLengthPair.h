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

#ifndef ARANGODB_BASICS_CHAR_LENGTH_PAIR_H
#define ARANGODB_BASICS_CHAR_LENGTH_PAIR_H 1

#include "Basics/Common.h"
#include "Basics/fasthash.h"

namespace arangodb {

/// @brief a struct describing a C character array
/// not responsible for memory management!
struct CharLengthPair {
  CharLengthPair() = delete;
  explicit CharLengthPair(std::string const& str) : data(str.c_str()), length(str.size()) {}
  explicit CharLengthPair(char const* data) : data(data), length(strlen(data)) {}
  CharLengthPair(char const* data, size_t length) : data(data), length(length) {}

  char const* data;
  size_t length;
};

}

namespace std {

template <>
struct hash<arangodb::CharLengthPair> {
  size_t operator()(arangodb::CharLengthPair const& value) const noexcept {
    return fasthash64(value.data, value.length, 0xdeadbeef) ^ std::hash<size_t>()(value.length);
  }
};

template <>
struct equal_to<arangodb::CharLengthPair> {
  bool operator()(arangodb::CharLengthPair const& lhs,
                  arangodb::CharLengthPair const& rhs) const {
    if (lhs.length != rhs.length) {
      return false;
    }
    return (memcmp(lhs.data, rhs.data, lhs.length) == 0);
  }
};

}

#endif
