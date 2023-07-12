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

#include "LogPayload.h"

#include "Basics/Exceptions.h"

#include <velocypack/Builder.h>

namespace arangodb::replication2 {

auto operator==(LogPayload const& left, LogPayload const& right) -> bool {
  if (left.slice().isString() and right.slice().isString()) {
    return left.slice().stringView() == right.slice().stringView();
  } else {
    // We do not use velocypack compare here, since this is used only in tests
    // and velocypack compare always has ICU as dependency.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}

LogPayload::LogPayload(BufferType const& buffer) : buffer(buffer) {}
LogPayload::LogPayload(BufferType&& buffer) : buffer(std::move(buffer)) {}

auto LogPayload::createFromSlice(velocypack::Slice slice) -> LogPayload {
  BufferType buffer(slice.byteSize());
  buffer.append(slice.start(), slice.byteSize());
  return LogPayload{std::move(buffer)};
}

auto LogPayload::createFromString(std::string_view string) -> LogPayload {
  VPackBuilder builder;
  builder.add(VPackValue(string));
  return LogPayload{*builder.steal()};
}

auto LogPayload::copyBuffer() const -> velocypack::UInt8Buffer {
  velocypack::UInt8Buffer result;
  result.append(buffer.data(), buffer.size());
  return result;
}

auto LogPayload::stealBuffer() -> velocypack::UInt8Buffer&& {
  return std::move(buffer);
}

auto LogPayload::byteSize() const noexcept -> std::size_t {
  return buffer.size();
}

auto LogPayload::slice() const noexcept -> velocypack::Slice {
  return VPackSlice(buffer.data());
}
}  // namespace arangodb::replication2
