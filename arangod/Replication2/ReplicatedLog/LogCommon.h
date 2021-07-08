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

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

#include <Basics/Identifier.h>

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

  [[nodiscard]] auto operator<=(LogIndex) const -> bool;

  auto operator+(std::uint64_t delta) const -> LogIndex;

  friend auto operator<<(std::ostream& os, LogIndex const& idx) -> std::ostream& {
    return os << idx.value;
  }

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

struct LogTerm : implement_compare<LogTerm> {
  constexpr LogTerm() noexcept : value{0} {}
  constexpr explicit LogTerm(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;
  [[nodiscard]] auto operator<=(LogTerm) const -> bool;

  friend auto operator<<(std::ostream& os, LogTerm const& term) -> std::ostream& {
    return os << term.value;
  }

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto to_string(LogTerm term) -> std::string;
auto to_string(LogIndex index) -> std::string;

struct TermIndexPair : implement_compare<TermIndexPair> {
  LogTerm term{};
  LogIndex index{};

  friend auto operator<=(TermIndexPair const& left, TermIndexPair const& right) noexcept
      -> bool;

  TermIndexPair(LogTerm term, LogIndex index) noexcept;
  TermIndexPair() = default;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice) -> TermIndexPair;

  friend auto operator<<(std::ostream& os, TermIndexPair const& pair) -> std::ostream&;
};

auto operator<=(TermIndexPair const& left, TermIndexPair const& right) noexcept -> bool;
auto operator<<(std::ostream& os, TermIndexPair const& pair) -> std::ostream&;

struct LogPayload {
  explicit LogPayload(velocypack::UInt8Buffer dummy) : dummy(std::move(dummy)) {}
  explicit LogPayload(velocypack::Slice slice);
  explicit LogPayload(std::string_view dummy);

  friend auto operator==(LogPayload const&, LogPayload const&) -> bool;
  friend auto operator!=(LogPayload const&, LogPayload const&) -> bool;

  [[nodiscard]] auto byteSize() const noexcept -> std::size_t;

  // just a placeholder for now
  velocypack::UInt8Buffer dummy;
};

auto operator==(LogPayload const&, LogPayload const&) -> bool;
auto operator!=(LogPayload const&, LogPayload const&) -> bool;

// just a placeholder for now, must have a hash<>
using ParticipantId = std::string;

// TODO This should probably be moved into a separate file
class LogEntry {
 public:
  using clock = std::chrono::steady_clock;

  LogEntry(LogTerm, LogIndex, LogPayload);

  [[nodiscard]] auto logTerm() const noexcept -> LogTerm;
  [[nodiscard]] auto logIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto logPayload() const noexcept -> LogPayload const&;
  [[nodiscard]] auto logTermIndexPair() const noexcept -> TermIndexPair;
  [[nodiscard]] auto insertTp() const noexcept -> clock::time_point;
  void setInsertTp(clock::time_point) noexcept;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LogEntry;

  auto operator==(LogEntry const&) const noexcept -> bool;

 private:
  LogTerm _logTerm{};
  LogIndex _logIndex{};
  LogPayload _payload;
  // Timepoint at which the insert was started (not the point in time where it
  // was committed)
  clock::time_point _insertTp{};
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
  virtual auto next() -> std::optional<LogEntry> = 0;
};

struct LogConfig {
  std::size_t writeConcern = 1;
  bool waitForSync = false;

  auto toVelocyPack(velocypack::Builder&) const -> void;
  LogConfig(velocypack::Slice);
  LogConfig() noexcept = default;
  LogConfig(std::size_t writeConcern, bool waitForSync) noexcept;

  friend auto operator==(LogConfig const& left, LogConfig const& right) noexcept -> bool;
  friend auto operator!=(LogConfig const& left, LogConfig const& right) noexcept -> bool;
};

auto operator==(LogConfig const& left, LogConfig const& right) noexcept -> bool;
auto operator!=(LogConfig const& left, LogConfig const& right) noexcept -> bool;

}  // namespace arangodb::replication2

template <>
struct std::hash<arangodb::replication2::LogIndex> {
  auto operator()(arangodb::replication2::LogIndex const& v) const noexcept -> std::size_t {
    return std::hash<uint64_t>{}(v.value);
  }
};

template <>
struct std::hash<arangodb::replication2::LogTerm> {
  auto operator()(arangodb::replication2::LogTerm const& v) const noexcept -> std::size_t {
    return std::hash<uint64_t>{}(v.value);
  }
};

template <>
struct std::hash<arangodb::replication2::LogId> {
  auto operator()(arangodb::replication2::LogId const& v) const noexcept -> std::size_t {
    return std::hash<arangodb::basics::Identifier>{}(v);
  }
};

