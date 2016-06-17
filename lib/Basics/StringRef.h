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

#ifndef ARANGODB_BASICS_STRING_REF_H
#define ARANGODB_BASICS_STRING_REF_H 1

#include "Basics/Common.h"
#include "Basics/xxhash.h"

namespace arangodb {

/// @brief a struct describing a C character array
/// not responsible for memory management!
struct StringRef {
  StringRef() : data(""), length(0) {}
  explicit StringRef(std::string const& str) : data(str.c_str()), length(str.size()) {}
  explicit StringRef(char const* data) : data(data), length(strlen(data)) {}
  StringRef(char const* data, size_t length) : data(data), length(length) {}

  bool operator==(StringRef const& other) const {
    return (length == other.length && memcmp(data, other.data, length) == 0);
  }
  
  bool operator==(std::string const& other) const {
    return (length == other.size() && memcmp(data, other.c_str(), length) == 0);
  }

  char const* data;
  size_t length;
};

}

namespace std {

template <>
struct hash<arangodb::StringRef> {
  size_t operator()(arangodb::StringRef const& value) const noexcept {
    return XXH64(value.data, value.length, 0xdeadbeef); 
  }
};

template <>
struct equal_to<arangodb::StringRef> {
  bool operator()(arangodb::StringRef const& lhs,
                  arangodb::StringRef const& rhs) const {
    if (lhs.length != rhs.length) {
      return false;
    }
    return (memcmp(lhs.data, rhs.data, lhs.length) == 0);
  }
};

}

#endif
