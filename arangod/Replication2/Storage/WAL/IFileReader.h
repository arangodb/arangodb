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
#include <string>
#include <type_traits>

#include "Basics/Result.h"

namespace arangodb::replication2::storage::wal {

struct IFileReader {
  virtual ~IFileReader() = default;

  virtual auto path() const -> std::string = 0;

  template<typename T>
  [[nodiscard]] auto read(T& result) -> Result {
    static_assert(std::is_trivially_copyable_v<T>);
    return read(&result, sizeof(T));
  }

  [[nodiscard]] virtual auto read(void* buffer, std::size_t n) -> Result = 0;

  virtual void seek(std::uint64_t pos) = 0;

  [[nodiscard]] virtual auto position() const -> std::uint64_t = 0;

  [[nodiscard]] virtual auto size() const -> std::uint64_t = 0;
};

}  // namespace arangodb::replication2::storage::wal
