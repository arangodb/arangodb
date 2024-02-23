////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb::replication2::storage {

struct IteratorPosition {
  IteratorPosition() = default;

  static IteratorPosition fromLogIndex(LogIndex index) {
    return IteratorPosition(index);
  }

  static IteratorPosition withFileOffset(LogIndex index,
                                         std::uint64_t fileOffset) {
    return IteratorPosition(index, fileOffset);
  }

  [[nodiscard]] auto index() const noexcept -> LogIndex { return _logIndex; }

  [[nodiscard]] auto fileOffset() const noexcept -> std::uint64_t {
    return _fileOffset;
  }

 private:
  explicit IteratorPosition(LogIndex index) : _logIndex(index) {}
  IteratorPosition(LogIndex index, std::uint64_t fileOffset)
      : _logIndex(index), _fileOffset(fileOffset) {}
  LogIndex _logIndex{0};
  std::uint64_t _fileOffset{0};
};

}  // namespace arangodb::replication2::storage
