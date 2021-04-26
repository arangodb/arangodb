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

#ifndef ARANGODB3_REPLICATION_COMMON_H
#define ARANGODB3_REPLICATION_COMMON_H

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include <Basics/Identifier.h>

namespace arangodb::replication2 {

template <typename T, typename S = T>
struct implement_compare {
  [[nodiscard]] auto operator==(S const& other) const -> bool {
    return self() <= other && other <= self();
  }
  [[nodiscard]] auto operator!=(S const& other) const -> bool {
    return !(self() == other);
  }
  [[nodiscard]] auto operator<(S const& other) const -> bool {
    return !(other <= self());
  }
  [[nodiscard]] auto operator>=(S const& other) const -> bool {
    return other <= self();
  }
  [[nodiscard]] auto operator>(S const& other) const -> bool {
    return other < self();
  }

 private:
  [[nodiscard]] auto self() const -> T const& {
    return static_cast<T const&>(*this);
  }
};

struct LogIndex : implement_compare<LogIndex> {
  constexpr LogIndex() noexcept : value{0} {}
  constexpr explicit LogIndex(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;

  [[nodiscard]] auto operator<=(LogIndex) const -> bool;

  auto operator+(std::uint64_t delta) const -> LogIndex;
};

struct LogTerm : implement_compare<LogTerm> {
  constexpr LogTerm() noexcept : value{0} {}
  constexpr explicit LogTerm(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;
  [[nodiscard]] auto operator<=(LogTerm) const -> bool;
};

struct LogPayload {
  explicit LogPayload(std::string_view dummy) : dummy(dummy) {}

  [[nodiscard]] auto operator==(LogPayload const&) const -> bool;

  // just a placeholder for now
  std::string dummy;
};

// just a placeholder for now, must have a hash<>
using ParticipantId = std::string;
struct LogStatistics {
  LogIndex spearHead{};
  LogIndex commitIndex{};

  void toVelocyPack(velocypack::Builder& builder) const;
};

struct LeaderStatus {
  LogStatistics local;
  LogTerm term;
  std::unordered_map<ParticipantId, LogStatistics> follower;

  void toVelocyPack(velocypack::Builder& builder) const;
};

struct FollowerStatus {
  LogStatistics local;
  ParticipantId leader;
  LogTerm term;

  void toVelocyPack(velocypack::Builder& builder) const;
};

struct UnconfiguredStatus {
  void toVelocyPack(velocypack::Builder& builder) const;
};

using LogStatus = std::variant<UnconfiguredStatus, LeaderStatus, FollowerStatus>;

// TODO This should probably be moved into a separate file
class LogEntry {
 public:
  LogEntry(LogTerm, LogIndex, LogPayload);

  [[nodiscard]] auto logTerm() const -> LogTerm;
  [[nodiscard]] auto logIndex() const -> LogIndex;
  [[nodiscard]] auto logPayload() const -> LogPayload const&;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LogEntry;

 private:
  LogTerm _logTerm{};
  LogIndex _logIndex{};
  LogPayload _payload;
};

class LogId : public arangodb::basics::Identifier {
  using arangodb::basics::Identifier::Identifier;
};

struct LogIterator {
  virtual ~LogIterator() = default;
  virtual auto next() -> std::optional<LogEntry> = 0;
};

}  // namespace arangodb::replication2

template<>
struct std::hash<arangodb::replication2::LogId> {
  auto operator()(arangodb::replication2::LogId const& v) const noexcept -> std::size_t {
    return std::hash<arangodb::basics::Identifier>{}(v);
  }
};

#endif  // ARANGODB3_REPLICATION_COMMON_H
