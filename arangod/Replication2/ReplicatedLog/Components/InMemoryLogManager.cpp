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

#include "InMemoryLogManager.h"

namespace arangodb::replication2::replicated_log {

InMemoryLogManager::InMemoryLogManager(LogIndex firstIndex,
                                       std::shared_ptr<IStorageManager> storage)
    : _storageManager(std::move(storage)), _guardedData(firstIndex) {}
LogIndex InMemoryLogManager::getCommitIndex() const noexcept {
  return _guardedData.getLockedGuard()->_commitIndex;
}
LogIndex InMemoryLogManager::updateCommitIndex(LogIndex newIndex) noexcept {
  return _guardedData.doUnderLock([newIndex](auto& data) {
    auto oldCommitIndex = data._commitIndex;

    TRI_ASSERT(oldCommitIndex < newIndex)
        << "_commitIndex == " << oldCommitIndex << ", newIndex == " << newIndex;
    data._commitIndex = newIndex;
    return oldCommitIndex;
  });
}

InMemoryLogManager::GuardedData::GuardedData(LogIndex firstIndex)
    : _inMemoryLog(firstIndex) {}

}  // namespace arangodb::replication2::replicated_log
