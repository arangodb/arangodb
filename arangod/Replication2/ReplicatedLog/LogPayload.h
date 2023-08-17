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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string_view>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

namespace arangodb::replication2 {

struct LogPayload {
  using BufferType = velocypack::UInt8Buffer;

  explicit LogPayload(BufferType dummy);

  // Named constructors, have to make copies.
  [[nodiscard]] static auto createFromSlice(velocypack::Slice slice)
      -> LogPayload;
  [[nodiscard]] static auto createFromString(std::string_view string)
      -> LogPayload;

  friend auto operator==(LogPayload const&, LogPayload const&) -> bool;

  [[nodiscard]] auto byteSize() const noexcept -> std::size_t;
  [[nodiscard]] auto slice() const noexcept -> velocypack::Slice;
  [[nodiscard]] auto copyBuffer() const -> velocypack::UInt8Buffer;
  [[nodiscard]] auto stealBuffer() -> velocypack::UInt8Buffer&&;

 private:
  BufferType buffer;
};

auto operator==(LogPayload const&, LogPayload const&) -> bool;

}  // namespace arangodb::replication2
