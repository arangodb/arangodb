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

#include <velocypack/Buffer.h>

namespace arangodb::replication2::storage::wal {

struct Buffer {
  template<typename T>
  void append(T const& v) {
    static_assert(std::is_trivially_copyable<std::decay_t<decltype(v)>>::value,
                  "T must be trivially copyable");

    _buffer.append(
        std::string_view{reinterpret_cast<char const*>(&v), sizeof(v)});
  }

  void append(void const* data, std::size_t size) {
    _buffer.append(std::string_view{reinterpret_cast<char const*>(data), size});
  }

  void resetTo(std::size_t position) noexcept { _buffer.resetTo(position); }

  void advance(std::size_t value) { _buffer.advance(value); }

  void clear() { _buffer.clear(); }

  auto data() const -> uint8_t const* { return _buffer.data(); }

  auto size() const -> std::size_t { return _buffer.size(); }

  auto buffer() -> velocypack::Buffer<uint8_t>& { return _buffer; }

 private:
  velocypack::Buffer<uint8_t> _buffer;
};

}  // namespace arangodb::replication2::storage::wal
