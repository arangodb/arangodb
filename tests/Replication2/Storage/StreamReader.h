////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <type_traits>

#include "Assertions/Assert.h"

namespace arangodb::replication2::storage::wal {

struct StreamReader {
  StreamReader(void const* data, std::size_t size)
      : _data(static_cast<uint8_t const*>(data)), _end(_data + size) {}

  auto data() const -> void const* { return _data; }
  auto size() const -> std::size_t { return _end - _data; }

  template<typename T>
  T read() {
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable");
    TRI_ASSERT(size() >= sizeof(T));

    T result;
    memcpy(&result, _data, sizeof(T));
    _data += sizeof(T);
    return result;
  }

  void skip(std::size_t size) {
    TRI_ASSERT(this->size() >= size);
    _data += size;
  }

 private:
  uint8_t const* _data;
  uint8_t const* _end;
};

}  // namespace arangodb::replication2::storage::wal
