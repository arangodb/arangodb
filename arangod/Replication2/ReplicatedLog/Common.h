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

#include <Basics/Identifier.h>
#include <Basics/ResultT.h>

#include <ostream>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

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
};

struct LogTerm : implement_compare<LogTerm> {
  constexpr LogTerm() noexcept : value{0} {}
  constexpr explicit LogTerm(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;
  [[nodiscard]] auto operator<=(LogTerm) const -> bool;

  friend auto operator<<(std::ostream& os, LogTerm const& term) -> std::ostream& {
    return os << term.value;
  }
};

struct LogPayload {
  explicit LogPayload(std::string_view dummy) : dummy(dummy) {}

  [[nodiscard]] auto operator==(LogPayload const&) const -> bool;
  [[nodiscard]] auto operator!=(LogPayload const& other) const -> bool {
    return !operator==(other);
  }

  // just a placeholder for now
  std::string dummy;
};

// just a placeholder for now, must have a hash<>
using ParticipantId = std::string;

// TODO This should probably be moved into a separate file
class LogEntry {
 public:
  LogEntry(LogTerm, LogIndex, LogPayload);

  [[nodiscard]] auto logTerm() const noexcept -> LogTerm;
  [[nodiscard]] auto logIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto logPayload() const noexcept -> LogPayload const&;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LogEntry;

  auto operator==(LogEntry const&) const noexcept -> bool;

 private:
  LogTerm _logTerm{};
  LogIndex _logIndex{};
  LogPayload _payload;
};

class LogId : public arangodb::basics::Identifier {
 public:
  using arangodb::basics::Identifier::Identifier;

  static auto fromShardName(std::string_view) noexcept -> std::optional<LogId>;
};

struct LogIterator {
  virtual ~LogIterator() = default;
  virtual auto next() -> std::optional<LogEntry> = 0;
};

template <typename I>
struct ContainerIterator : LogIterator {
  static_assert(std::is_same_v<typename I::value_type, LogEntry>);

  ContainerIterator(I begin, I end)
      : _current(std::move(begin)), _end(std::move(end)) {}

  auto next() -> std::optional<LogEntry> override {
    if (_current == _end) {
      return std::nullopt;
    }
    return *(_current++);
  }

  I _current;
  I _end;
};

}  // namespace arangodb::replication2

template <>
struct std::hash<arangodb::replication2::LogId> {
  auto operator()(arangodb::replication2::LogId const& v) const noexcept -> std::size_t {
    return std::hash<arangodb::basics::Identifier>{}(v);
  }
};

#endif  // ARANGODB3_REPLICATION_COMMON_H
