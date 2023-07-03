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
#include "Replication2/Exceptions/ParticipantResignedException.h"
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

auto InMemoryLogManager::appendLogEntry(
    std::variant<LogMetaPayload, LogPayload> payload, LogTerm term,
    InMemoryLogEntry::clock::time_point insertTp, bool waitForSync)
    -> LogIndex {
  return _guardedData.doUnderLock([&](auto& data) -> LogIndex {
    if (data._resigned) {
      throw ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED, ADB_HERE);
    }

    // TODO for now only waitForSync=true is supported.
    // waitForSync = true;  // by setting this to true, the waitForSync flag is
    // set
    // for all log entries, independent of the log config.

    auto const index = data._inMemoryLog.getNextIndex();
    auto const payloadSize = std::holds_alternative<LogPayload>(payload)
                                 ? std::get<LogPayload>(payload).byteSize()
                                 : 0;
    bool const isMetaLogEntry = std::holds_alternative<LogMetaPayload>(payload);

    auto logEntry = InMemoryLogEntry(
        PersistingLogEntry(TermIndexPair{term, index}, std::move(payload)),
        waitForSync);
    logEntry.setInsertTp(insertTp);
    auto size = logEntry.entry().approxByteSize();
    data._inMemoryLog.appendInPlace(_logContext, std::move(logEntry));

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

auto InMemoryLogManager::getInternalLogIterator(LogIndex firstIdx) const
    -> std::unique_ptr<TypedLogIterator<InMemoryLogEntry>> {
  return _guardedData.doUnderLock(
      [&](auto& data) -> std::unique_ptr<TypedLogIterator<InMemoryLogEntry>> {
        auto const& inMemoryLog = data._inMemoryLog;
        if (inMemoryLog.getFirstIndex() <= firstIdx) {
          auto const endIdx = inMemoryLog.getLastTermIndexPair().index + 1;
          TRI_ASSERT(firstIdx <= endIdx);
          return inMemoryLog.getMemtryIteratorFrom(firstIdx);
        }

        auto diskIter = _storageManager->getPersistedLogIterator(firstIdx);

        struct OverlayIterator : TypedLogIterator<InMemoryLogEntry> {
          explicit OverlayIterator(
              std::unique_ptr<PersistedLogIterator> diskIter,
              std::unique_ptr<TypedLogIterator<InMemoryLogEntry>> inMemoryIter,
              LogRange inMemoryRange)
              : _diskIter(std::move(diskIter)),
                _inMemoryIter(std::move(inMemoryIter)),
                _inMemoryRange(inMemoryRange) {}

          // MSVC fails to parse a trailing return type with "final" or
          // "override" here.
          std::optional<InMemoryLogEntry> next() final {
            // iterate over the disk until it is covered by the in memory part
            if (_diskIter) {
              auto entry = _diskIter->next();
              if (entry) {
                if (not _inMemoryRange.contains(entry->logIndex())) {
                  return InMemoryLogEntry{*entry};
                }
              }
              _diskIter.reset();
            }

            return _inMemoryIter->next();
          }

          std::unique_ptr<PersistedLogIterator> _diskIter;
          std::unique_ptr<TypedLogIterator<InMemoryLogEntry>> _inMemoryIter;
          LogRange _inMemoryRange;
        };

        return std::make_unique<OverlayIterator>(
            std::move(diskIter), inMemoryLog.getMemtryIteratorFrom(firstIdx),
            inMemoryLog.getIndexRange());
      });
}

auto InMemoryLogManager::getLogConsumerIterator(
    std::optional<LogRange> bounds) const -> std::unique_ptr<LogRangeIterator> {
  return _guardedData.getLockedGuard()->getLogConsumerIterator(*_storageManager,
                                                               bounds);
}

auto InMemoryLogManager::getNonEmptyLogConsumerIterator(LogIndex const firstIdx)
    const -> std::variant<std::unique_ptr<LogRangeIterator>, LogIndex> {
  return _guardedData.doUnderLock(
      [&](auto& data)
          -> std::variant<std::unique_ptr<LogRangeIterator>, LogIndex> {
        auto const commitIndex = data._commitIndex;
        auto const& inMemoryLog = data._inMemoryLog;
        TRI_ASSERT(firstIdx <= commitIndex);

        /*
         * This code here ensures that if only private log entries are present
         * we do not reply with an empty iterator but instead wait for the
         * next entry containing payload.
         */

        auto testIndex = firstIdx;
        while (testIndex <= commitIndex) {
          auto memtry = inMemoryLog.getEntryByIndex(testIndex);
          if (!memtry.has_value()) {
            break;
          }
          if (memtry->entry().hasPayload()) {
            break;
          }
          testIndex = testIndex + 1;
        }

        if (testIndex > commitIndex) {
          return testIndex;
        }

        return data.getLogConsumerIterator(
            *_storageManager, LogRange{testIndex, commitIndex + 1});
      });
}

TermIndexPair InMemoryLogManager::getSpearheadTermIndexPair() const noexcept {
  return _guardedData.doUnderLock([](auto const& data) {
    return data._inMemoryLog.getLastTermIndexPair();
  });
}

auto InMemoryLogManager::calculateCommitLag() const noexcept
    -> std::chrono::duration<double, std::milli> {
  return _guardedData.doUnderLock([&](auto& data) -> std::chrono::duration<
                                                      double, std::milli> {
    auto const commitIndex = data._commitIndex;
    auto const& inMemoryLog = data._inMemoryLog;
    auto const memtry = inMemoryLog.getEntryByIndex(commitIndex + 1);
    if (memtry.has_value()) {
      return std::chrono::duration_cast<
          std::chrono::duration<double, std::milli>>(
          std::chrono::steady_clock::now() - memtry->insertTp());
    } else {
      TRI_ASSERT(commitIndex == LogIndex{0} ||
                 commitIndex == inMemoryLog.getLastIndex())
          << "If there is no entry following the commitIndex the last index "
             "should be the commitIndex. commitIndex = "
          << commitIndex << ", lastIndex = " << inMemoryLog.getLastIndex();
      return {};
    }
  });
}

LogIndex InMemoryLogManager::getFirstInMemoryIndex() const noexcept {
  return _guardedData.getLockedGuard()->_inMemoryLog.getFirstIndex();
}

auto InMemoryLogManager::getTermOfIndex(LogIndex logIndex) const noexcept
    -> std::optional<LogTerm> {
  return _guardedData.doUnderLock(
      [this, logIndex](auto const& data) -> std::optional<LogTerm> {
        if (data._inMemoryLog.getIndexRange().contains(logIndex)) {
          return data._inMemoryLog.getEntryByIndex(logIndex)->entry().logTerm();
        } else {
          return _storageManager->getTermIndexMapping().getTermOfIndex(
              logIndex);
        }
      });
}
void InMemoryLogManager::resign() && noexcept {
  _guardedData.getLockedGuard()->_resigned = true;
}

InMemoryLogManager::GuardedData::GuardedData(LogIndex firstIndex)
    : _inMemoryLog(firstIndex) {}

auto InMemoryLogManager::GuardedData::getLogConsumerIterator(
    IStorageManager& storageManager, std::optional<LogRange> bounds) const
    -> std::unique_ptr<LogRangeIterator> {
  // Note that there can be committed log entries only in memory, because they
  // might not be persisted locally.

  auto const commitIndex = _commitIndex;
  // Intersect the range with the committed range
  auto range = LogRange{LogIndex{0}, commitIndex + 1};
  if (bounds) {
    range = intersect(range, *bounds);
  }

  // check if we can serve everything from memory
  auto const& inMemoryLog = _inMemoryLog;
  if (inMemoryLog.getIndexRange().contains(range)) {
    return inMemoryLog.getIteratorRange(range);
  }

  // server from disk
  auto diskIter = storageManager.getCommittedLogIterator(range);

  struct OverlayIterator : LogRangeIterator {
    explicit OverlayIterator(std::unique_ptr<LogRangeIterator> diskIter,
                             std::unique_ptr<LogRangeIterator> inMemoryIter,
                             LogRange inMemoryRange, LogRange range)
        : _diskIter(std::move(diskIter)),
          _inMemoryIter(std::move(inMemoryIter)),
          _inMemoryRange(inMemoryRange),
          _range(range) {}

    auto next() -> std::optional<LogEntryView> override {
      // iterate over the disk until it is covered by the in memory part
      if (_diskIter) {
        auto entry = _diskIter->next();
        if (entry) {
          if (not _inMemoryRange.contains(entry->logIndex())) {
            return entry;
          }
        }
        _diskIter.reset();
      }

      return _inMemoryIter->next();
    }

    auto range() const noexcept -> LogRange override { return _range; }

    std::unique_ptr<LogRangeIterator> _diskIter;
    std::unique_ptr<LogRangeIterator> _inMemoryIter;
    LogRange _inMemoryRange;
    LogRange _range;
  };

  return std::make_unique<OverlayIterator>(std::move(diskIter),
                                           inMemoryLog.getIteratorRange(range),
                                           inMemoryLog.getIndexRange(), range);
}

}  // namespace arangodb::replication2::replicated_log
