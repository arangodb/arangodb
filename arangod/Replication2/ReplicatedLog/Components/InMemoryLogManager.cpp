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

#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Metrics/Histogram.h"
#include "Metrics/LogScale.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"

namespace arangodb::replication2::replicated_log {

InMemoryLogManager::InMemoryLogManager(
    LoggerContext logContext, std::shared_ptr<ReplicatedLogMetrics> metrics,
    LogIndex firstIndex, std::shared_ptr<IStorageManager> storage)
    : _logContext(std::move(logContext)),
      _metrics(std::move(metrics)),
      _storageManager(std::move(storage)),
      _guardedData(firstIndex) {}

auto InMemoryLogManager::getCommitIndex() const noexcept -> LogIndex {
  return _guardedData.getLockedGuard()->_commitIndex;
}

void InMemoryLogManager::updateCommitIndex(LogIndex newCommitIndex) noexcept {
  return _guardedData.doUnderLock([&](auto& data) {
    auto oldCommitIndex = data._commitIndex;

    TRI_ASSERT(oldCommitIndex < newCommitIndex)
        << "oldCommitIndex == " << oldCommitIndex
        << ", newCommitIndex == " << newCommitIndex;
    data._commitIndex = newCommitIndex;

    _metrics->replicatedLogNumberCommittedEntries->count(newCommitIndex.value -
                                                         oldCommitIndex.value);

    // Update commit time _metrics
    auto const commitTp = InMemoryLogEntry::clock::now();

    auto const newlyCommittedLogEntries =
        data._inMemoryLog.slice(oldCommitIndex, newCommitIndex + 1);
    for (auto const& it : newlyCommittedLogEntries) {
      using namespace std::chrono_literals;
      auto const entryDuration = commitTp - it.insertTp();
      _metrics->replicatedLogInsertsRtt->count(entryDuration / 1us);
    }

    // potentially evict parts of the inMemoryLog.
    auto const maxDiskIndex =
        _storageManager->getTermIndexMapping().getLastIndex().value_or(
            TermIndexPair{});
    auto const evictStopIndex = std::min(newCommitIndex, maxDiskIndex.index);
    auto const logEntriesToEvict =
        data._inMemoryLog.slice(LogIndex{0}, evictStopIndex);
    auto releasedMemory = std::size_t{0};
    auto numEntriesEvicted = std::size_t{0};
    for (auto const& memtry : logEntriesToEvict) {
      releasedMemory += memtry.entry().approxByteSize();
      numEntriesEvicted += 1;
    }
    // remove upto commit index, but keep non-locally persisted log
    data._inMemoryLog = data._inMemoryLog.removeFront(evictStopIndex);
    _metrics->leaderNumInMemoryEntries->fetch_sub(numEntriesEvicted);
    _metrics->leaderNumInMemoryBytes->fetch_sub(releasedMemory);
  });
}

auto InMemoryLogManager::getInMemoryLog() const noexcept -> InMemoryLog {
  return _guardedData.getLockedGuard()->_inMemoryLog;
}

auto InMemoryLogManager::appendLogEntry(
    std::variant<LogMetaPayload, LogPayload> payload, LogTerm term,
    InMemoryLogEntry::clock::time_point insertTp, bool waitForSync)
    -> LogIndex {
  return _guardedData.doUnderLock([&](auto& data) -> LogIndex {
    auto const index = data._inMemoryLog.getNextIndex();

    auto logEntry = InMemoryLogEntry(
        PersistingLogEntry(TermIndexPair{term, index}, std::move(payload)),
        waitForSync);
    logEntry.setInsertTp(insertTp);
    auto size = logEntry.entry().approxByteSize();

    data._inMemoryLog.appendInPlace(_logContext, std::move(logEntry));

    bool const isMetaLogEntry = std::holds_alternative<LogMetaPayload>(payload);
    auto const payloadSize = std::holds_alternative<LogPayload>(payload)
                                 ? std::get<LogPayload>(payload).byteSize()
                                 : 0;

    _metrics->replicatedLogInsertsBytes->count(payloadSize);
    _metrics->leaderNumInMemoryEntries->fetch_add(1);
    _metrics->leaderNumInMemoryBytes->fetch_add(size);
    if (isMetaLogEntry) {
      _metrics->replicatedLogNumberMetaEntries->count(1);
    } else {
      _metrics->replicatedLogNumberAcceptedEntries->count(1);
    }

    return index;
  });
}

InMemoryLogManager::GuardedData::GuardedData(LogIndex firstIndex)
    : _inMemoryLog(firstIndex) {}

}  // namespace arangodb::replication2::replicated_log
