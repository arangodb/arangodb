////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace arangodb::velocypack {
class ArrayIterator;
}

namespace arangodb::replication2::replicated_log {

class VPackArrayToLogPayloadIterator : public TypedLogIterator<LogPayload> {
 public:
  auto next() -> std::optional<LogPayload> override {
    if (_iter.valid()) {
      return LogPayload::createFromSlice(*_iter++);
    }

    return std::nullopt;
  }

  explicit VPackArrayToLogPayloadIterator(VPackSlice slice) : _iter(slice) {}

 private:
  velocypack::ArrayIterator _iter;
};

}  // namespace arangodb::replication2::replicated_log