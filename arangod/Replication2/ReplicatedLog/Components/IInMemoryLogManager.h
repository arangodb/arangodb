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

#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

#include <chrono>
#include <variant>

namespace arangodb::replication2 {
struct LogIndex;
struct TermIndexPair;
}  // namespace arangodb::replication2

namespace arangodb::replication2::replicated_log {
struct InMemoryLog;
struct ReplicatedLogMetrics;
inline namespace comp {

struct IInMemoryLogManager {
  virtual ~IInMemoryLogManager() = default;

  [[nodiscard]] virtual auto getCommitIndex() const noexcept -> LogIndex = 0;
  // Sets the new index, returns the old one. The new index is expected to be
  // larger than the old one.
  virtual void updateCommitIndex(LogIndex newIndex) noexcept = 0;

  [[nodiscard]] virtual auto calculateCommitLag() const noexcept
      -> std::chrono::duration<double, std::milli> = 0;

  [[nodiscard]] virtual auto getFirstInMemoryIndex() const noexcept
      -> LogIndex = 0;
  [[nodiscard]] virtual auto getSpearheadTermIndexPair() const noexcept
      -> TermIndexPair = 0;
  [[nodiscard]] virtual auto getTermOfIndex(LogIndex) const noexcept
      -> std::optional<LogTerm> = 0;

  [[nodiscard]] virtual auto appendLogEntry(
      std::variant<LogMetaPayload, LogPayload> payload, LogTerm term,
      InMemoryLogEntry::clock::time_point insertTp, bool waitForSync)
      -> LogIndex = 0;

  [[nodiscard]] virtual auto getInternalLogIterator(LogIndex firstIdx) const
      -> std::unique_ptr<InMemoryLogIterator> = 0;

  [[nodiscard]] virtual auto getLogConsumerIterator(
      std::optional<LogRange> bounds) const
      -> std::unique_ptr<LogRangeIterator> = 0;

  // If there's at least one log entry with a payload on the given range,
  // returns a LogRangeIterator. Otherwise the next index to wait for.
  [[nodiscard]] virtual auto getNonEmptyLogConsumerIterator(LogIndex firstIdx)
      const -> std::variant<std::unique_ptr<LogRangeIterator>, LogIndex> = 0;
};

}  // namespace comp
}  // namespace arangodb::replication2::replicated_log
