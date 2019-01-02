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

#ifndef VELOCYPACK_STRINGREF_H
#define VELOCYPACK_STRINGREF_H 1

#include <cstdint>
#include <cstring>

#include "velocypack/velocypack-common.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace velocypack {

class StringRef {
 public:
  /// @brief create an empty StringRef
  constexpr StringRef() noexcept : _data(""), _length(0) {}

  /// @brief create a StringRef from an std::string
  explicit StringRef(std::string const& str) noexcept : StringRef(str.c_str(), str.size()) {}
  
  /// @brief create a StringRef from a null-terminated C string
  explicit StringRef(char const* data) noexcept : StringRef(data, strlen(data)) {}
  
  /// @brief create a StringRef from a VPack slice (must be of type String)
  explicit StringRef(arangodb::velocypack::Slice const& slice) : StringRef() {
    VELOCYPACK_ASSERT(slice.isString());
    arangodb::velocypack::ValueLength l;
    _data = slice.getString(l);
    _length = l;
  }
  
  /// @brief create a StringRef from a C string plus length
  StringRef(char const* data, size_t length) noexcept : _data(data), _length(length) {}
  
  /// @brief create a StringRef from another StringRef
  StringRef(StringRef const& other) noexcept
      : _data(other._data), _length(other._length) {}
  
  /// @brief create a StringRef from another StringRef
  StringRef& operator=(StringRef const& other) noexcept {
    _data = other._data;
    _length = other._length;
    return *this;
  }
  
  /// @brief create a StringRef from an std::string
  StringRef& operator=(std::string const& other) noexcept {
    _data = other.c_str();
    _length = other.size();
    return *this;
  }
  
  /// @brief create a StringRef from a null-terminated C string
  StringRef& operator=(char const* other) noexcept {
    _data = other;
    _length = strlen(other);
    return *this;
  }
  
  /// @brief create a StringRef from a VPack slice of type String
  StringRef& operator=(arangodb::velocypack::Slice const& slice) noexcept {
    arangodb::velocypack::ValueLength l;
    _data = slice.getString(l);
    _length = l;
    return *this;
  }

  int compare(std::string const& other) const noexcept {
    int res = memcmp(_data, other.c_str(), (std::min)(_length, other.size()));
    if (res != 0) {
      return res;
    }
    return static_cast<int>(_length) - static_cast<int>(other.size());
  }
  
  int compare(StringRef const& other) const noexcept {
    int res = memcmp(_data, other._data, (std::min)(_length, other._length));
    if (res != 0) {
      return res;
    }
    return static_cast<int>(_length) - static_cast<int>(other._length);
  }

  inline std::string toString() const {
    return std::string(_data, _length);
  }

  inline bool empty() const noexcept {
    return (_length == 0);
  }
  
  inline char const* begin() const noexcept {
    return _data;
  }
  
  inline char const* end() const noexcept {
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
}

/*
inline bool operator==(arangodb::velocypack::StringRef const& lhs, arangodb::velocypack::StringRef const& rhs) {
  return (lhs.size() == rhs.size() && memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
}

inline bool operator!=(arangodb::velocypack::StringRef const& lhs, arangodb::velocypack::StringRef const& rhs) {
  return !(lhs == rhs);
}

inline bool operator==(arangodb::velocypack::StringRef const& lhs, std::string const& rhs) {
  return (lhs.size() == rhs.size() && memcmp(lhs.data(), rhs.c_str(), lhs.size()) == 0);
}

inline bool operator!=(arangodb::velocypack::StringRef const& lhs, std::string const& rhs) {
  return !(lhs == rhs);
}

inline bool operator==(arangodb::velocypack::StringRef const& lhs, char const* rhs) {
  size_t const len = strlen(rhs);
  return (lhs.size() == len && memcmp(lhs.data(), rhs, lhs.size()) == 0);
}

inline bool operator!=(arangodb::velocypack::StringRef const& lhs, char const* rhs) {
  return !(lhs == rhs);
}

inline bool operator<(arangodb::StringRef const& lhs, arangodb::StringRef const& rhs) {
  return (lhs.compare(rhs) < 0);
}

inline bool operator>(arangodb::StringRef const& lhs, arangodb::StringRef const& rhs) {
  return (lhs.compare(rhs) > 0);
}
*/

namespace std {

template <>
struct hash<arangodb::velocypack::StringRef> {
  size_t operator()(arangodb::velocypack::StringRef const& value) const noexcept {
    return VELOCYPACK_HASH(value.data(), value.size(), 0xdeadbeef); 
  }
};

template <>
struct equal_to<arangodb::velocypack::StringRef> {
  bool operator()(arangodb::velocypack::StringRef const& lhs,
                  arangodb::velocypack::StringRef const& rhs) const noexcept {
    return (lhs.size() == rhs.size() &&
            (memcmp(lhs.data(), rhs.data(), lhs.size()) == 0));
  }
};

}

#endif
