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

#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include <iosfwd>
#include <functional>

namespace arangodb {

/// @brief a struct describing a C character array
/// not responsible for memory management!
class StringRef {
 public:
  /// @brief create an empty StringRef
  constexpr StringRef() : _data(""), _length(0) {}

  /// @brief create a StringRef from an std::string
  explicit StringRef(std::string const& str) noexcept : _data(str.data()), _length(str.size()) {}
  
  /// @brief create a StringRef from a null-terminated C string
  explicit StringRef(char const* data) : _data(data), _length(strlen(data)) {}
  
  /// @brief create a StringRef from a VPack slice (must be of type String)
  explicit StringRef(arangodb::velocypack::Slice const& slice) : StringRef() {
    arangodb::velocypack::ValueLength l;
    _data = slice.getString(l);
    _length = l;
  }
  
  /// @brief create a StringRef from a C string plus length
  StringRef(char const* data, size_t length) : _data(data), _length(length) {}
  
  /// @brief create a StringRef from another StringRef
  StringRef(StringRef const& other) 
      : _data(other._data), _length(other._length) {}
  
  /// @brief create a StringRef from another StringRef
  StringRef& operator=(StringRef const& other) {
    _data = other._data;
    _length = other._length;
    return *this;
  }
  
  /// @brief create a StringRef from an std::string
  StringRef& operator=(std::string const& other) {
    _data = other.c_str();
    _length = other.size();
    return *this;
  }
  
  /// @brief create a StringRef from a null-terminated C string
  StringRef& operator=(char const* other) {
    _data = other;
    _length = strlen(other);
    return *this;
  }
  
  /// @brief create a StringRef from a VPack slice of type String
  StringRef& operator=(arangodb::velocypack::Slice const& slice) {
    arangodb::velocypack::ValueLength l;
    _data = slice.getString(l);
    _length = l;
    return *this;
  }

  size_t find(char c) const {
    char const* p = static_cast<char const*>(memchr(static_cast<void const*>(_data), c, _length));

    if (p == nullptr) {
      return std::string::npos;
    }

    return (p - _data);
  }
  
  StringRef substr(size_t pos = 0, size_t count = std::string::npos) const {
    if (pos >= _length) {
      throw std::out_of_range("substr index out of bounds");
    }
    if (count == std::string::npos || (count + pos >= _length)) {
      count = _length - pos;
    }
    return StringRef(_data + pos, count);
  }

  int compare(std::string const& other) const {
    int res = memcmp(_data, other.c_str(), (std::min)(_length, other.size()));
    if (res != 0) {
      return res;
    }
    return static_cast<int>(_length) - static_cast<int>(other.size());
  }
  
  int compare(StringRef const& other) const {
    int res = memcmp(_data, other._data, (std::min)(_length, other._length));
    if (res != 0) {
      return res;
    }
    return static_cast<int>(_length) - static_cast<int>(other._length);
  }

  inline std::string toString() const {
    return std::string(_data, _length);
  }

  inline bool empty() const {
    return (_length == 0);
  }
  
  char at(size_t index) const { 
    if (index >= _length) {
      throw std::out_of_range("StringRef index out of bounds");
    }
    return operator[](index);
  }

  inline char const* begin() const {
    return _data;
  }
  
  inline char const* end() const {
    return _data + _length;
  }

  inline char front() const { return _data[0]; }

  inline char back() const { return _data[_length - 1]; }
  
  inline char operator[](size_t index) const noexcept { 
    return _data[index];
  }
  
  inline char const* data() const noexcept {
    return _data;
  }

  inline size_t size() const noexcept {
    return _length;
  }

  inline size_t length() const noexcept {
    return _length;
  }

 private:
  char const* _data;
  size_t _length;
};

}

std::ostream& operator<<(std::ostream&, arangodb::StringRef const&);

inline bool operator==(arangodb::StringRef const& lhs, arangodb::StringRef const& rhs) {
  return (lhs.size() == rhs.size() && memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
}

inline bool operator!=(arangodb::StringRef const& lhs, arangodb::StringRef const& rhs) {
  return !(lhs == rhs);
}

inline bool operator==(arangodb::StringRef const& lhs, std::string const& rhs) {
  return (lhs.size() == rhs.size() && memcmp(lhs.data(), rhs.c_str(), lhs.size()) == 0);
}

inline bool operator!=(arangodb::StringRef const& lhs, std::string const& rhs) {
  return !(lhs == rhs);
}

inline bool operator==(arangodb::StringRef const& lhs, char const* rhs) {
  size_t const len = strlen(rhs);
  return (lhs.size() == len && memcmp(lhs.data(), rhs, lhs.size()) == 0);
}

inline bool operator!=(arangodb::StringRef const& lhs, char const* rhs) {
  return !(lhs == rhs);
}

inline bool operator<(arangodb::StringRef const& lhs, arangodb::StringRef const& rhs) {
  return (lhs.compare(rhs) < 0);
}

inline bool operator>(arangodb::StringRef const& lhs, arangodb::StringRef const& rhs) {
  return (lhs.compare(rhs) > 0);
}

namespace std {

template <>
struct hash<arangodb::StringRef> {
  size_t operator()(arangodb::StringRef const& value) const noexcept {
    return XXH64(value.data(), value.size(), 0xdeadbeef); 
  }
};

template <>
struct equal_to<arangodb::StringRef> {
  bool operator()(arangodb::StringRef const& lhs,
                  arangodb::StringRef const& rhs) const noexcept {
    return (lhs.size() == rhs.size() &&
            (memcmp(lhs.data(), rhs.data(), lhs.size()) == 0));
  }
};

}


#endif
