////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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

namespace arangodb::replication2 {
struct LogIndex;
}

namespace arangodb::replication2::replicated_log {
inline namespace comp {

struct IInMemoryLogManager {
  virtual ~IInMemoryLogManager() = default;

  virtual auto getCommitIndex() const noexcept -> LogIndex = 0;
  // Sets the new index, returns the old one. The new index is expected to be
  // larger than the old one.
  virtual auto updateCommitIndex(LogIndex newIndex) noexcept -> LogIndex = 0;
};

}  // namespace comp
}  // namespace arangodb::replication2::replicated_log
