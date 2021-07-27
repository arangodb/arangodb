////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/box.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include <velocypack/Buffer.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>

#include <Basics/Identifier.h>
#include <Containers/ImmerMemoryPolicy.h>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2 {

template <typename T, typename S = T>
struct implement_compare {
  [[nodiscard]] friend auto operator==(T const& left, S const& right) -> bool {
    return left <= right && right <= left;
  }
  [[nodiscard]] friend auto operator!=(T const& left, S const& right) -> bool {
    return !(left == right);
  }
  [[nodiscard]] friend auto operator<(T const& left, S const& right) -> bool {
    return !(right <= left);
  }
  [[nodiscard]] friend auto operator>=(T const& left, S const& right) -> bool {
    return right <= left;
  }
  [[nodiscard]] friend auto operator>(T const& left, S const& right) -> bool {
    return right < left;
  }
};

struct LogIndex : implement_compare<LogIndex> {
  constexpr LogIndex() noexcept : value{0} {}
  constexpr explicit LogIndex(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;

  [[nodiscard]] auto saturatedDecrement() const noexcept -> LogIndex;

  [[nodiscard]] auto operator<=(LogIndex) const -> bool;

  [[nodiscard]] auto operator+(std::uint64_t delta) const -> LogIndex;

  friend auto operator<<(std::ostream& os, LogIndex const& idx) -> std::ostream&;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto operator<<(std::ostream& os, LogIndex const& idx) -> std::ostream&;

struct LogTerm : implement_compare<LogTerm> {
  constexpr LogTerm() noexcept : value{0} {}
  constexpr explicit LogTerm(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;
  [[nodiscard]] auto operator<=(LogTerm) const -> bool;

  friend auto operator<<(std::ostream& os, LogTerm const& term) -> std::ostream&;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto operator<<(std::ostream& os, LogTerm const& term) -> std::ostream&;

[[nodiscard]] auto to_string(LogTerm term) -> std::string;
[[nodiscard]] auto to_string(LogIndex index) -> std::string;

struct TermIndexPair : implement_compare<TermIndexPair> {
  LogTerm term{};
  LogIndex index{};

  friend auto operator<=(TermIndexPair const& left, TermIndexPair const& right) noexcept
      -> bool;

  TermIndexPair(LogTerm term, LogIndex index) noexcept;
  TermIndexPair() = default;

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> TermIndexPair;

  friend auto operator<<(std::ostream& os, TermIndexPair const& pair) -> std::ostream&;
};

[[nodiscard]] auto operator<=(TermIndexPair const& left, TermIndexPair const& right) noexcept -> bool;
auto operator<<(std::ostream& os, TermIndexPair const& pair) -> std::ostream&;

struct LogPayload {
  LogPayload() = delete;
  explicit LogPayload(velocypack::UInt8Buffer dummy);

  // Named constructors, have to make copies.
  [[nodiscard]] static auto createFromSlice(velocypack::Slice slice) -> LogPayload;
  [[nodiscard]] static auto createFromString(std::string_view string) -> LogPayload;

  friend auto operator==(LogPayload const&, LogPayload const&) -> bool;
  friend auto operator!=(LogPayload const&, LogPayload const&) -> bool;

  [[nodiscard]] auto byteSize() const noexcept -> std::size_t;
  [[nodiscard]] auto slice() const noexcept -> velocypack::Slice;

  // just a placeholder for now
  velocypack::UInt8Buffer dummy;
};

[[nodiscard]] auto operator==(LogPayload const&, LogPayload const&) -> bool;
[[nodiscard]] auto operator!=(LogPayload const&, LogPayload const&) -> bool;

// just a placeholder for now, must have a hash<>
using ParticipantId = std::string;

class PersistingLogEntry {
 public:
  PersistingLogEntry(LogTerm, LogIndex, std::optional<LogPayload>);
  PersistingLogEntry(TermIndexPair, std::optional<LogPayload>);
  PersistingLogEntry(LogIndex, velocypack::Slice persisted);


  [[nodiscard]] auto logTerm() const noexcept -> LogTerm;
  [[nodiscard]] auto logIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto logPayload() const noexcept -> std::optional<LogPayload> const&;
  [[nodiscard]] auto logTermIndexPair() const noexcept -> TermIndexPair;

  class OmitLogIndex {};
  constexpr static auto omitLogIndex = OmitLogIndex();
  void toVelocyPack(velocypack::Builder& builder) const;
  void toVelocyPack(velocypack::Builder& builder, OmitLogIndex) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> PersistingLogEntry;

  [[nodiscard]] auto operator==(PersistingLogEntry const&) const noexcept -> bool;

 private:
  void entriesWithoutIndexToVelocyPack(velocypack::Builder& builder) const;

  LogTerm _logTerm{};
  LogIndex _logIndex{};
  // TODO It seems impractical to not copy persisting log entries, so we should
  //      probably make this a shared_ptr (or immer::box).
  std::optional<LogPayload> _payload;
};

// A log entry, enriched with non-persisted metadata, to be stored in an
// InMemoryLog.
class InMemoryLogEntry {
 public:
  using clock = std::chrono::steady_clock;

  explicit InMemoryLogEntry(PersistingLogEntry entry);

  [[nodiscard]] auto insertTp() const noexcept -> clock::time_point;
  void setInsertTp(clock::time_point) noexcept;
  [[nodiscard]] auto entry() const noexcept -> PersistingLogEntry const&;

 private:
  // Immutable box that allows sharing, i.e. cheap copying.
  ::immer::box<PersistingLogEntry, ::arangodb::immer::arango_memory_policy> _logEntry;
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

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LogEntryView;

 private:
  LogIndex _index;
  velocypack::Slice _payload;
};

class LogId : public arangodb::basics::Identifier {
 public:
  using arangodb::basics::Identifier::Identifier;

  static auto fromString(std::string_view) noexcept -> std::optional<LogId>;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto to_string(LogId logId) -> std::string;

struct LogIterator {
  virtual ~LogIterator() = default;
  // The returned view is guaranteed to stay valid until a successive next()
  // call (only).
  virtual auto next() -> std::optional<LogEntryView> = 0;
};

struct LogConfig {
  std::size_t writeConcern = 1;
  bool waitForSync = false;

  auto toVelocyPack(velocypack::Builder&) const -> void;
  explicit LogConfig(velocypack::Slice);
  LogConfig() noexcept = default;
  LogConfig(std::size_t writeConcern, bool waitForSync) noexcept;

  friend auto operator==(LogConfig const& left, LogConfig const& right) noexcept -> bool;
  friend auto operator!=(LogConfig const& left, LogConfig const& right) noexcept -> bool;
};

[[nodiscard]] auto operator==(LogConfig const& left, LogConfig const& right) noexcept -> bool;
[[nodiscard]] auto operator!=(LogConfig const& left, LogConfig const& right) noexcept -> bool;

}  // namespace arangodb::replication2

template <>
struct std::hash<arangodb::replication2::LogIndex> {
  [[nodiscard]] auto operator()(arangodb::replication2::LogIndex const& v) const noexcept -> std::size_t {
    return std::hash<uint64_t>{}(v.value);
  }
};

template <>
struct std::hash<arangodb::replication2::LogTerm> {
  [[nodiscard]] auto operator()(arangodb::replication2::LogTerm const& v) const noexcept -> std::size_t {
    return std::hash<uint64_t>{}(v.value);
  }
};

template <>
struct std::hash<arangodb::replication2::LogId> {
  [[nodiscard]] auto operator()(arangodb::replication2::LogId const& v) const noexcept -> std::size_t {
    return std::hash<arangodb::basics::Identifier>{}(v);
  }
};

