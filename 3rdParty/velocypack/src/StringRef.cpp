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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <iostream>

#include "velocypack/Exception.h"
#include "velocypack/HashedStringRef.h"
#include "velocypack/Slice.h"
#include "velocypack/StringRef.h"

using namespace arangodb::velocypack;

namespace arangodb {
namespace velocypack {
extern void* memrchr(void const* block, int c, std::size_t size);
}
}

StringRef::StringRef(Slice slice) {
  VELOCYPACK_ASSERT(slice.isString());
  ValueLength l;
  _data = slice.getString(l);
  _length = l;
}

StringRef::StringRef(HashedStringRef const& other) noexcept 
  : _data(other.data()), _length(other.size()) {}
  
/// @brief create a StringRef from another HashedStringRef
StringRef& StringRef::operator=(HashedStringRef const& other) noexcept {
  _data = other.data();
  _length = other.size();
  return *this;
}
  
/// @brief create a StringRef from a VPack slice of type String
StringRef& StringRef::operator=(Slice slice) {
  VELOCYPACK_ASSERT(slice.isString());
  ValueLength l;
  _data = slice.getString(l);
  _length = l;
  return *this;
}
  
StringRef StringRef::substr(std::size_t pos, std::size_t count) const {
  if (VELOCYPACK_UNLIKELY(pos > _length)) {
    throw Exception(Exception::IndexOutOfBounds, "substr index out of bounds");
  }
  if (count == std::string::npos || (count + pos >= _length)) {
    count = _length - pos;
  }
  return StringRef(_data + pos, count);
}

char StringRef::at(std::size_t index) const {
  if (index >= _length) {
    throw Exception(Exception::IndexOutOfBounds, "index out of bounds");
  }
  return operator[](index);
}
  
std::size_t StringRef::find(char c, std::size_t offset) const noexcept {
  if (offset > _length) {
    offset = _length;
  }

  char const* p =
      static_cast<char const*>(std::memchr(static_cast<void const*>(_data + offset), c, _length - offset));

  if (p == nullptr) {
    return std::string::npos;
  }

  return (p - _data);
}
  
std::size_t StringRef::rfind(char c, std::size_t offset) const noexcept {
  std::size_t length;
  if (offset >= _length + 1) {
    length = _length; 
  } else {
    length = offset + 1;
  }

  char const* p =
      static_cast<char const*>(arangodb::velocypack::memrchr(static_cast<void const*>(_data), c, length));

  if (p == nullptr) {
    return std::string::npos;
  }

  return (p - _data);
}
  
int StringRef::compare(StringRef const& other) const noexcept {
  int res = std::memcmp(_data, other._data, (std::min)(_length, other._length));

  if (res != 0) {
    return res;
  }

  return static_cast<int>(_length) - static_cast<int>(other._length);
}

bool StringRef::equals(StringRef const& other) const noexcept {
  return (size() == other.size() && std::memcmp(data(), other.data(), size()) == 0);
}

#ifdef VELOCYPACK_64BIT
static_assert(sizeof(StringRef) == 16, "unexpected size of StringRef");
#endif

namespace arangodb {
namespace velocypack {

std::ostream& operator<<(std::ostream& stream, StringRef const& ref) {
  stream.write(ref.data(), ref.length());
  return stream;
}

}
}
