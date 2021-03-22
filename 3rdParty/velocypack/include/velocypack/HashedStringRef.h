////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef VELOCYPACK_HASHED_STRINGREF_H
#define VELOCYPACK_HASHED_STRINGREF_H 1

#include <cstring>
#include <functional>
#include <algorithm>
#include <string>
#include <iosfwd>

#include "velocypack/velocypack-common.h"

namespace arangodb {
namespace velocypack {
class Slice;
class StringRef;

/// a non-owning string reference with a computed hash value.
/// it can be used for fast equality comparisons, for example as keys
/// in unordered containers.
/// the HashedStringRef does not own the data it points to. 
/// it is the caller's responsibility to keep the pointed-to string 
/// data valid while the HashedStringRef points to it.
/// the length of the pointed-to string is restricted to (2 ^ 32) - 1.
/// there are intentionally no constructors to create HashedStringRefs
/// from std::string, char const* or (char const*, size_t) pairs,
/// so that the caller must explicitly pass the string length as a
/// uint32_t and must handle potential string truncation at the call
/// site.
class HashedStringRef {
 public:
  /// @brief create an empty HashedStringRef
  HashedStringRef() noexcept 
    : _data(""), 
      _length(0), 
      _hash(hash("", 0)) {}

  /// @brief create a HashedStringRef from a string plus length.
  /// the length is intentionally restricted to a uint32_t here.
  HashedStringRef(char const* data, uint32_t length) noexcept 
    : _data(data), 
      _length(length),
      _hash(hash(_data, _length)) {}
  
  /// @brief create a HashedStringRef from a VPack slice (must be of type String)
  explicit HashedStringRef(Slice slice);
  
  /// @brief create a HashedStringRef from another HashedStringRef
  constexpr HashedStringRef(HashedStringRef const& other) noexcept
      : _data(other._data), 
        _length(other._length), 
        _hash(other._hash) {}
  
  /// @brief move a HashedStringRef from another HashedStringRef
  constexpr HashedStringRef(HashedStringRef&& other) noexcept
      : _data(other._data), 
        _length(other._length), 
        _hash(other._hash) {}
  
  /// @brief create a HashedStringRef from another HashedStringRef
  HashedStringRef& operator=(HashedStringRef const& other) noexcept {
    _data = other._data;
    _length = other._length;
    _hash = other._hash;
    return *this;
  }
  
  /// @brief move a HashedStringRef from another HashedStringRef
  HashedStringRef& operator=(HashedStringRef&& other) noexcept {
    _data = other._data;
    _length = other._length;
    _hash = other._hash;
    return *this;
  }
  
  /// @brief create a HashedStringRef from a VPack slice of type String.
  /// if the slice if not of type String, this method will throw an exception.
  HashedStringRef& operator=(Slice slice);
  
  HashedStringRef substr(std::size_t pos = 0, std::size_t count = std::string::npos) const;
  
  char at(std::size_t index) const;
  
  std::size_t find(char c, std::size_t offset = 0) const noexcept;
  
  std::size_t rfind(char c, std::size_t offset = std::string::npos) const noexcept;

  int compare(HashedStringRef const& other) const noexcept;
  
  int compare(char const* data, std::size_t length) const noexcept;
  
  int compare(std::string const& other) const noexcept {
    return compare(other.data(), other.size());
  }
  
  int compare(char const* data) const noexcept {
    return compare(data, std::strlen(data));
  }
   
  bool equals(HashedStringRef const& other) const noexcept;
  
  bool equals(char const* data, std::size_t length) const noexcept;
  
  bool equals(std::string const& other) const noexcept {
    return equals(other.data(), other.size());
  }
  
  bool equals(char const* data) const noexcept {
    return equals(data, std::strlen(data));
  }
  
  std::string toString() const {
    return std::string(_data, _length);
  }

  StringRef stringRef() const noexcept;

  constexpr inline bool empty() const noexcept {
    return (_length == 0);
  }
 
  inline char const* begin() const noexcept {
    return _data;
  }
  
  inline char const* cbegin() const noexcept {
    return _data;
  }
 
  inline char const* end() const noexcept {
    return _data + _length;
  }
  
  inline char const* cend() const noexcept {
    return _data + _length;
  }

  inline char front() const noexcept { return _data[0]; }

  inline char back() const noexcept { return _data[_length - 1]; }

  inline void pop_back() { --_length; }
  
  inline char operator[](std::size_t index) const noexcept { 
    return _data[index];
  }
  
  constexpr inline char const* data() const noexcept {
    return _data;
  }

  constexpr inline std::size_t size() const noexcept {
    return _length;
  }

  constexpr inline std::size_t length() const noexcept {
    return _length;
  }
  
  constexpr inline uint32_t hash() const noexcept {
    return _hash;
  }

  // return a combined hash/length value for comparisons with less branching
  constexpr inline uint64_t tag() const noexcept {
    return (static_cast<uint64_t>(_hash) << 32) | static_cast<uint64_t>(_length);
  }

 private:
  inline uint32_t hash(char const* data, uint32_t length) const noexcept {
    return VELOCYPACK_HASH32(data, length, 0xdeadbeef);
  }

 private:
  char const* _data;
  uint32_t _length;
  uint32_t _hash;
};

std::ostream& operator<<(std::ostream& stream, HashedStringRef const& ref);
} // namespace velocypack
} // namespace arangodb

inline bool operator==(arangodb::velocypack::HashedStringRef const& lhs, arangodb::velocypack::HashedStringRef const& rhs) noexcept {
  // equal tags imply equal length/size
  return (lhs.tag() == rhs.tag() && memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
}

inline bool operator!=(arangodb::velocypack::HashedStringRef const& lhs, arangodb::velocypack::HashedStringRef const& rhs) noexcept {
  return !(lhs == rhs);
}

inline bool operator==(arangodb::velocypack::HashedStringRef const& lhs, std::string const& rhs) noexcept {
  return (lhs.size() == rhs.size() && memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
}

inline bool operator!=(arangodb::velocypack::HashedStringRef const& lhs, std::string const& rhs) noexcept {
  return !(lhs == rhs);
}

inline bool operator<(arangodb::velocypack::HashedStringRef const& lhs, arangodb::velocypack::HashedStringRef const& rhs) noexcept {
  return (lhs.compare(rhs) < 0);
}

inline bool operator>(arangodb::velocypack::HashedStringRef const& lhs, arangodb::velocypack::HashedStringRef const& rhs) noexcept {
  return (lhs.compare(rhs) > 0);
}

namespace std {

template <>
struct hash<arangodb::velocypack::HashedStringRef> {
  std::size_t operator()(arangodb::velocypack::HashedStringRef const& value) const noexcept {
    return static_cast<std::size_t>(value.hash());
  }
};

template <>
struct equal_to<arangodb::velocypack::HashedStringRef> {
  bool operator()(arangodb::velocypack::HashedStringRef const& lhs,
                  arangodb::velocypack::HashedStringRef const& rhs) const noexcept {
    return (lhs.tag() == rhs.tag() && memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
  }
};

}

#endif
