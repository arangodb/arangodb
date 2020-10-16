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

namespace arangodb {
namespace velocypack {

void* memrchr(void const* block, int c, std::size_t size) {
#ifdef __linux__
  return const_cast<void*>(::memrchr(block, c, size));
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

} // namespace velocypack
} // namespace arangodb
