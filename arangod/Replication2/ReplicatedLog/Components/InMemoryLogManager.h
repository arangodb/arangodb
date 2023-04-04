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

#include "Basics/Guarded.h"
#include "Replication2/ReplicatedLog/Components/IInMemoryLogManager.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/LoggerContext.h"

#include <memory>

namespace arangodb::replication2::replicated_log {
struct ReplicatedLogMetrics;
inline namespace comp {
struct IStorageManager;

struct InMemoryLogManager : IInMemoryLogManager {
  explicit InMemoryLogManager(LoggerContext logContext, LogIndex firstIndex,
                              std::shared_ptr<IStorageManager> storage);

  auto getCommitIndex() const noexcept -> LogIndex override;
  void updateCommitIndex(ReplicatedLogMetrics&,
                         LogIndex newIndex) noexcept override;

  auto getInMemoryLog() const noexcept -> InMemoryLog override;
  auto appendLogEntry(std::variant<LogMetaPayload, LogPayload> payload,
                      LogTerm term,
                      InMemoryLogEntry::clock::time_point insertTp,
                      bool waitForSync) -> InsertLogEntryResult override;

 private:
  struct GuardedData {
    explicit GuardedData(LogIndex firstIndex);
    InMemoryLog _inMemoryLog;
    LogIndex _commitIndex{0};
  };
  LoggerContext _logContext;
  std::shared_ptr<IStorageManager> const _storageManager;
  Guarded<GuardedData> _guardedData;
};

}  // namespace comp
}  // namespace arangodb::replication2::replicated_log
