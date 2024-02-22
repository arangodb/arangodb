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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/InMemoryLogEntry.h"
#include "Replication2/ReplicatedLog/LogEntry.h"
#include "Replication2/ReplicatedLog/LogEntryView.h"

#include <Containers/ImmerMemoryPolicy.h>
#include <velocypack/Builder.h>

#include <initializer_list>
#include <optional>

#include <immer/flex_vector.hpp>

namespace arangodb::replication2::storage {
struct IStorageEngineMethods;
}

namespace arangodb::replication2::replicated_log {
struct TermIndexMapping;
class ReplicatedLogIterator;

/**
 * @brief The ephemeral part of the replicated log held in memory. Can hold more
 * recent entries than the corresponding persisted log, while the latter is
 * catching up. On startup (or currently, on creation of a leader or follower
 * instance), this is restored from the persisted log.
 */
struct InMemoryLog {
 public:
  template<typename T>
  using log_type_t =
      ::immer::flex_vector<T, arangodb::immer::arango_memory_policy>;
  using log_type = log_type_t<InMemoryLogEntry>;
  using log_type_persisted = log_type_t<LogEntry>;

 private:
  log_type _log{};
  LogIndex _first{1};

 public:
  InMemoryLog() = default;
  explicit InMemoryLog(log_type log);
  explicit InMemoryLog(LogIndex first);

  InMemoryLog(InMemoryLog&& other) noexcept;
  InMemoryLog(InMemoryLog const&) = default;

  auto operator=(InMemoryLog&& other) noexcept -> InMemoryLog&;
  auto operator=(InMemoryLog const&) -> InMemoryLog& = default;

  ~InMemoryLog() noexcept = default;

  [[nodiscard]] auto getLastTermIndexPair() const noexcept -> TermIndexPair;
  [[nodiscard]] auto getLastIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto getLastTerm() const noexcept -> LogTerm;
  [[nodiscard]] auto getLastEntry() const noexcept
      -> std::optional<InMemoryLogEntry>;
  [[nodiscard]] auto getFirstEntry() const noexcept
      -> std::optional<InMemoryLogEntry>;
  [[nodiscard]] auto getFirstIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto getNextIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto getEntryByIndex(LogIndex idx) const noexcept
      -> std::optional<InMemoryLogEntry>;
  [[nodiscard]] auto slice(LogIndex from, LogIndex to) const -> log_type;

  [[nodiscard]] auto getFirstIndexOfTerm(LogTerm term) const noexcept
      -> std::optional<LogIndex>;
  [[nodiscard]] auto getLastIndexOfTerm(LogTerm term) const noexcept
      -> std::optional<LogIndex>;

  [[nodiscard]] auto getIndexRange() const noexcept -> LogRange;

  // @brief Unconditionally accesses the last element
  [[nodiscard]] auto back() const noexcept -> decltype(_log)::const_reference;
  [[nodiscard]] auto empty() const noexcept -> bool;

  [[nodiscard]] auto release(LogIndex stop) const -> InMemoryLog;

  void appendInPlace(LoggerContext const& logContext, InMemoryLogEntry entry);

  [[nodiscard]] auto append(
      std::initializer_list<InMemoryLogEntry> entries) const -> InMemoryLog;
  [[nodiscard]] auto append(InMemoryLog const& entries) const -> InMemoryLog;
  [[nodiscard]] auto append(log_type entries) const -> InMemoryLog;
  [[nodiscard]] auto append(log_type_persisted const& entries) const
      -> InMemoryLog;

  [[nodiscard]] auto getIteratorFrom(LogIndex fromIdx) const
      -> std::unique_ptr<LogViewIterator>;
  [[nodiscard]] auto getRangeIteratorFrom(LogIndex fromIdx) const
      -> std::unique_ptr<LogViewRangeIterator>;
  [[nodiscard]] auto getLogIterator() const -> std::unique_ptr<LogIterator>;
  [[nodiscard]] auto getMemtryIteratorFrom(LogIndex fromIdx) const
      -> std::unique_ptr<InMemoryLogIterator>;
  [[nodiscard]] auto getMemtryIteratorRange(LogIndex fromIdx,
                                            LogIndex toIdx) const
      -> std::unique_ptr<InMemoryLogIterator>;
  [[nodiscard]] auto getMemtryIteratorRange(LogRange) const
      -> std::unique_ptr<InMemoryLogIterator>;
  // get an iterator for range [from, to).
  [[nodiscard]] auto getIteratorRange(LogIndex fromIdx, LogIndex toIdx) const
      -> std::unique_ptr<LogViewRangeIterator>;
  [[nodiscard]] auto getIteratorRange(LogRange bounds) const
      -> std::unique_ptr<LogViewRangeIterator>;

  [[nodiscard]] auto takeSnapshotUpToAndIncluding(LogIndex until) const
      -> InMemoryLog;
  [[nodiscard]] auto removeBack(LogIndex start) const -> InMemoryLog;
  [[nodiscard]] auto removeFront(LogIndex stop) const -> InMemoryLog;

  [[nodiscard]] auto copyFlexVector() const -> log_type;
  [[nodiscard]] auto computeTermIndexMap() const -> TermIndexMapping;

  // helpful for debugging
  [[nodiscard]] static auto dump(log_type const& log) -> std::string;
  [[nodiscard]] auto dump() const -> std::string;

 protected:
  explicit InMemoryLog(log_type log, LogIndex first);
};

}  // namespace arangodb::replication2::replicated_log
