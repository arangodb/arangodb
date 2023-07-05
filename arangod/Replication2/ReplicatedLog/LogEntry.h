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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogPayload.h"
#include "Replication2/ReplicatedLog/LogMetaPayload.h"
#include "Replication2/ReplicatedLog/TypedLogIterator.h"

namespace arangodb::replication2 {

class LogEntry {
 public:
  LogEntry(LogTerm term, LogIndex index, LogPayload payload)
      : LogEntry(TermIndexPair{term, index}, std::move(payload)) {}
  LogEntry(TermIndexPair, std::variant<LogMetaPayload, LogPayload>);
  LogEntry(LogIndex,
           velocypack::Slice persisted);  // RocksDB from disk constructor

  [[nodiscard]] auto logTerm() const noexcept -> LogTerm;
  [[nodiscard]] auto logIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto logPayload() const noexcept -> LogPayload const*;
  [[nodiscard]] auto logTermIndexPair() const noexcept -> TermIndexPair;
  [[nodiscard]] auto approxByteSize() const noexcept -> std::size_t;
  [[nodiscard]] auto hasPayload() const noexcept -> bool;
  [[nodiscard]] auto hasMeta() const noexcept -> bool;
  [[nodiscard]] auto meta() const noexcept -> LogMetaPayload const*;

  class OmitLogIndex {};
  constexpr static auto omitLogIndex = OmitLogIndex();
  void toVelocyPack(velocypack::Builder& builder) const;
  void toVelocyPack(velocypack::Builder& builder, OmitLogIndex) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LogEntry;

  friend auto operator==(LogEntry const&, LogEntry const&) noexcept
      -> bool = default;

 private:
  void entriesWithoutIndexToVelocyPack(velocypack::Builder& builder) const;

  TermIndexPair _termIndex;
  // TODO It seems impractical to not copy persisting log entries, so we
  // should probably make this a shared_ptr (or immer::box).
  std::variant<LogMetaPayload, LogPayload> _payload;

  // TODO this is a magic constant "measuring" the size of
  //      of the non-payload data in a LogEntry
  static inline constexpr auto approxMetaDataSize = std::size_t{42 * 2};
};

// ReplicatedLog-internal iterator over PersistingLogEntries
struct PersistedLogIterator : TypedLogIterator<LogEntry> {};

}  // namespace arangodb::replication2
