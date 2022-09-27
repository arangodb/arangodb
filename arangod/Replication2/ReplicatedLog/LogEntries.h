////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include <string_view>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb::replication2 {

struct LogPayload {
  using BufferType = std::basic_string<std::uint8_t>;

  explicit LogPayload(BufferType dummy);

  // Named constructors, have to make copies.
  [[nodiscard]] static auto createFromSlice(velocypack::Slice slice)
      -> LogPayload;
  [[nodiscard]] static auto createFromString(std::string_view string)
      -> LogPayload;

  friend auto operator==(LogPayload const&, LogPayload const&) -> bool;

  [[nodiscard]] auto byteSize() const noexcept -> std::size_t;
  [[nodiscard]] auto slice() const noexcept -> velocypack::Slice;
  [[nodiscard]] auto copyBuffer() const -> velocypack::UInt8Buffer;

 private:
  BufferType buffer;
};

auto operator==(LogPayload const&, LogPayload const&) -> bool;

struct LogMetaPayload {
  struct FirstEntryOfTerm {
    ParticipantId leader;
    agency::ParticipantsConfig participants;

    static auto fromVelocyPack(velocypack::Slice) -> FirstEntryOfTerm;
    void toVelocyPack(velocypack::Builder&) const;

    friend auto operator==(FirstEntryOfTerm const&,
                           FirstEntryOfTerm const&) noexcept -> bool = default;
  };

  static auto withFirstEntryOfTerm(ParticipantId leader,
                                   agency::ParticipantsConfig config)
      -> LogMetaPayload;

  struct UpdateParticipantsConfig {
    agency::ParticipantsConfig participants;

    static auto fromVelocyPack(velocypack::Slice) -> UpdateParticipantsConfig;
    void toVelocyPack(velocypack::Builder&) const;

    friend auto operator==(UpdateParticipantsConfig const&,
                           UpdateParticipantsConfig const&) noexcept
        -> bool = default;
  };

  static auto withUpdateParticipantsConfig(agency::ParticipantsConfig config)
      -> LogMetaPayload;

  static auto fromVelocyPack(velocypack::Slice) -> LogMetaPayload;
  void toVelocyPack(velocypack::Builder&) const;

  std::variant<FirstEntryOfTerm, UpdateParticipantsConfig> info;

  friend auto operator==(LogMetaPayload const&, LogMetaPayload const&) noexcept
      -> bool = default;
};

class PersistingLogEntry {
 public:
  PersistingLogEntry(LogTerm term, LogIndex index, LogPayload payload)
      : PersistingLogEntry(TermIndexPair{term, index}, std::move(payload)) {}
  PersistingLogEntry(TermIndexPair, std::variant<LogMetaPayload, LogPayload>);
  PersistingLogEntry(
      LogIndex, velocypack::Slice persisted);  // RocksDB from disk constructor

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
  static auto fromVelocyPack(velocypack::Slice slice) -> PersistingLogEntry;

  friend auto operator==(PersistingLogEntry const&,
                         PersistingLogEntry const&) noexcept -> bool = default;

 private:
  void entriesWithoutIndexToVelocyPack(velocypack::Builder& builder) const;

  TermIndexPair _termIndex;
  // TODO It seems impractical to not copy persisting log entries, so we should
  //      probably make this a shared_ptr (or immer::box).
  std::variant<LogMetaPayload, LogPayload> _payload;

  // TODO this is a magic constant "measuring" the size of
  //      of the non-payload data in a PersistingLogEntry
  static inline constexpr auto approxMetaDataSize = std::size_t{42 * 2};
};

// A log entry, enriched with non-persisted metadata, to be stored in an
// InMemoryLog.
class InMemoryLogEntry {
 public:
  using clock = std::chrono::steady_clock;

  explicit InMemoryLogEntry(PersistingLogEntry entry, bool waitForSync = false);

  [[nodiscard]] auto insertTp() const noexcept -> clock::time_point;
  void setInsertTp(clock::time_point) noexcept;
  [[nodiscard]] auto entry() const noexcept -> PersistingLogEntry const&;
  [[nodiscard]] bool getWaitForSync() const noexcept { return _waitForSync; }

 private:
  bool _waitForSync;
  // Immutable box that allows sharing, i.e. cheap copying.
  ::immer::box<PersistingLogEntry, ::arangodb::immer::arango_memory_policy>
      _logEntry;
  // Timepoint at which the insert was started (not the point in time where it
  // was committed)
  clock::time_point _insertTp{};
};

// A log entry as visible to the user of a replicated log.
// Does thus always contain a payload: only internal log entries are without
// payload, which aren't visible to the user. User-defined log entries always
// contain a payload.
// The term is not of interest, and therefore not part of this struct.
// Note that when these entries are visible, they are already committed.
// It does not own the payload, so make sure it is still valid when using it.
class LogEntryView {
 public:
  LogEntryView() = delete;
  LogEntryView(LogIndex index, LogPayload const& payload) noexcept;
  LogEntryView(LogIndex index, velocypack::Slice payload) noexcept;

  [[nodiscard]] auto logIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto logPayload() const noexcept -> velocypack::Slice;
  [[nodiscard]] auto clonePayload() const -> LogPayload;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LogEntryView;

 private:
  LogIndex _index;
  velocypack::Slice _payload;
};

template<typename T>
struct TypedLogIterator {
  virtual ~TypedLogIterator() = default;
  // The returned view is guaranteed to stay valid until a successive next()
  // call (only).
  virtual auto next() -> std::optional<T> = 0;
};

template<typename T>
struct TypedLogRangeIterator : TypedLogIterator<T> {
  // returns the index interval [from, to)
  // Note that this does not imply that all indexes in the range [from, to)
  // are returned. Hence (to - from) is only an upper bound on the number of
  // entries returned.
  [[nodiscard]] virtual auto range() const noexcept -> LogRange = 0;
};

using LogIterator = TypedLogIterator<LogEntryView>;
using LogRangeIterator = TypedLogRangeIterator<LogEntryView>;

// ReplicatedLog-internal iterator over PersistingLogEntries
struct PersistedLogIterator : TypedLogIterator<PersistingLogEntry> {};
}  // namespace arangodb::replication2
