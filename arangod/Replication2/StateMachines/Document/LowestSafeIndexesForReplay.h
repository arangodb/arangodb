////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Basics/MaintainerMode.h>

#include <map>

namespace arangodb {
struct ShardID;
}
namespace arangodb::replication2 {
struct LogIndex;
}

namespace arangodb::replication2::replicated_state::document {
struct DocumentStateMetadata;

struct LowestSafeIndexesForReplay {
  explicit LowestSafeIndexesForReplay(DocumentStateMetadata const& metadata);
  bool isSafeForReplay(ShardID, LogIndex);
  void setFromMetadata(DocumentStateMetadata const& metadata);

  // only used for a maintainer-mode check
  constexpr auto getMap() const noexcept -> std::map<ShardID, LogIndex> const& {
    static_assert(maintainerMode);
    return _map;
  }

 private:
  std::map<ShardID, LogIndex> _map;
};

}  // namespace arangodb::replication2::replicated_state::document
