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
#include <cstddef>
#include <cstdint>
#include <string>
#include <optional>

#include <Basics/Identifier.h>

namespace arangodb::replication2 {

struct LogIndex {
  LogIndex() : value{0} {}
  explicit LogIndex(std::uint64_t value) : value{value} {}
  std::uint64_t value;

  [[nodiscard]] auto operator==(LogIndex) const -> bool;
  [[nodiscard]] auto operator<=(LogIndex) const -> bool;
  [[nodiscard]] auto operator<(LogIndex) const -> bool;
};
struct LogTerm {
  LogTerm() : value{0} {}
  explicit LogTerm(std::uint64_t value) : value{value} {}
  std::uint64_t value;
  [[nodiscard]] auto operator==(LogTerm) const -> bool;
};
struct LogPayload {
  explicit LogPayload(std::string_view dummy) : dummy(dummy) {}

  [[nodiscard]] auto operator==(LogPayload const&) const -> bool;

  // just a placeholder for now
  std::string dummy;
};
// just a placeholder for now, must have a hash<>
using ParticipantId = std::size_t;
struct LogStatistics {
  LogIndex spearHead{};
  LogIndex commitIndex{};
};

// TODO This should probably be moved into a separate file
class LogEntry {
 public:
  LogEntry(LogTerm, LogIndex, LogPayload);

  [[nodiscard]] LogTerm logTerm() const;
  [[nodiscard]] LogIndex logIndex() const;
  [[nodiscard]] LogPayload const& logPayload() const;

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

#endif  // ARANGODB3_REPLICATION_COMMON_H
