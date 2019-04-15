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

#include <iostream>

#include "velocypack/StringRef.h"
#include "velocypack/Slice.h"

using namespace arangodb::velocypack;

namespace {

void* memrchrSwitch(void const* block, int c, size_t size) {
#ifdef __linux__
  return const_cast<void*>(memrchr(block, c, size));
#else
/// naive memrchr overlay for Windows or other platforms, which don't implement it
  if (size) {
    unsigned char const* p = static_cast<unsigned char const*>(block);

    for (p += size - 1; size; p--, size--) {
      if (*p == c) {
        return const_cast<void*>(static_cast<void const*>(p));
      }
    }
  }
  return nullptr;
#endif
}

} // namespace
  
StringRef::StringRef(Slice slice) {
  VELOCYPACK_ASSERT(slice.isString());
  ValueLength l;
  _data = slice.getString(l);
  _length = l;
}
  
/// @brief create a StringRef from a VPack slice of type String
StringRef& StringRef::operator=(Slice slice) {
  VELOCYPACK_ASSERT(slice.isString());
  ValueLength l;
  _data = slice.getString(l);
  _length = l;
  return *this;
}
  
size_t StringRef::find(char c) const {
  char const* p =
      static_cast<char const*>(memchr(static_cast<void const*>(_data), c, _length));

  if (p == nullptr) {
    return std::string::npos;
  }

  return (p - _data);
}
  
size_t StringRef::rfind(char c) const {
  char const* p =
      static_cast<char const*>(::memrchrSwitch(static_cast<void const*>(_data), c, _length));

  if (p == nullptr) {
    return std::string::npos;
  }

  return (p - _data);
}

namespace arangodb {
namespace velocypack {

std::ostream& operator<<(std::ostream& stream, StringRef const& ref) {
  stream.write(ref.data(), ref.length());
  return stream;
}

}
}
