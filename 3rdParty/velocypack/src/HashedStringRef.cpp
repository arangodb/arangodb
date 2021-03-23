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

#include <cstring>
#include <iostream>
#include <limits>

#include "velocypack/Exception.h"
#include "velocypack/Slice.h"
#include "velocypack/HashedStringRef.h"
#include "velocypack/StringRef.h"

using namespace arangodb::velocypack;

namespace arangodb {
namespace velocypack {
extern void* memrchr(void const* block, int c, std::size_t size);
}
}

HashedStringRef::HashedStringRef(Slice slice) {
  VELOCYPACK_ASSERT(slice.isString());
  ValueLength l;
  _data = slice.getString(l);
  if (l > std::numeric_limits<uint32_t>::max()) {
    throw Exception(Exception::IndexOutOfBounds, "string value too long for HashedStringRef");
  }
  _length = static_cast<uint32_t>(l);
  _hash = hash(_data, _length);
}

/// @brief create a HashedStringRef from a VPack slice of type String
HashedStringRef& HashedStringRef::operator=(Slice slice) {
  VELOCYPACK_ASSERT(slice.isString());
  ValueLength l;
  _data = slice.getString(l);
  if (l > std::numeric_limits<uint32_t>::max()) {
    throw Exception(Exception::IndexOutOfBounds, "string value too long for HashedStringRef");
  }
  _length = static_cast<uint32_t>(l);
  _hash = hash(_data, _length);
  return *this;
}

HashedStringRef HashedStringRef::substr(std::size_t pos, std::size_t count) const {
  if (VELOCYPACK_UNLIKELY(pos > _length)) {
    throw Exception(Exception::IndexOutOfBounds, "substr index out of bounds");
  }
  if (count == std::string::npos || (count + pos >= _length)) {
    count = _length - pos;
  }
  return HashedStringRef(_data + pos, static_cast<uint32_t>(count));
}

char HashedStringRef::at(std::size_t index) const {
  if (index >= _length) {
    throw Exception(Exception::IndexOutOfBounds, "index out of bounds");
  }
  return operator[](index);
}
  
std::size_t HashedStringRef::find(char c, std::size_t offset) const noexcept {
  if (offset > _length) {
    offset = _length;
  }

  char const* p =
      static_cast<char const*>(memchr(static_cast<void const*>(_data + offset), c, _length - offset));

  if (p == nullptr) {
    return std::string::npos;
  }

  return (p - _data);
}
  
std::size_t HashedStringRef::rfind(char c, std::size_t offset) const noexcept {
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
  
int HashedStringRef::compare(HashedStringRef const& other) const noexcept {
  int res = std::memcmp(_data, other._data, (std::min)(_length, other._length));

  if (res != 0) {
    return res;
  }

  return static_cast<int>(_length) - static_cast<int>(other._length);
}

int HashedStringRef::compare(char const* data, std::size_t length) const noexcept {
  int res = std::memcmp(_data, data, (std::min)(static_cast<std::size_t>(_length), length));

  if (res != 0) {
    return res;
  }

  if (VELOCYPACK_UNLIKELY(length > std::numeric_limits<uint32_t>::max())) {
    return -1;
  }
  return static_cast<int>(_length) - static_cast<int>(length);
}

bool HashedStringRef::equals(HashedStringRef const& other) const noexcept {
  // if the tag is equal, then the size is equal too!
  return (tag() == other.tag() && std::memcmp(data(), other.data(), size()) == 0);
}
  
bool HashedStringRef::equals(char const* data, std::size_t length) const noexcept {
  // it does not matter that the std::string can be longer in size here,
  // as we are upcasting our own size to size_t and compare. only for
  // equal lengths we do the memory comparison
  return (static_cast<std::size_t>(size()) == length && std::memcmp(_data, data, length) == 0);
}

StringRef HashedStringRef::stringRef() const noexcept {
  return StringRef(data(), size());
}

#ifdef VELOCYPACK_64BIT
static_assert(sizeof(HashedStringRef) == 16, "unexpected size of HashedStringRef");
#endif

namespace arangodb {
namespace velocypack {

std::ostream& operator<<(std::ostream& stream, HashedStringRef const& ref) {
  stream.write(ref.data(), ref.length());
  return stream;
}

}
}
