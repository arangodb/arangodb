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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include <Basics/StringUtils.h>
#include <Basics/StaticStrings.h>

#include <Basics/VelocyPackHelper.h>
#include <chrono>
#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;

auto LogIndex::operator<=(LogIndex other) const -> bool {
  return value <= other.value;
}
auto LogIndex::operator+(std::uint64_t delta) const -> LogIndex {
  return LogIndex(this->value + delta);
}
LogEntry::LogEntry(LogTerm logTerm, LogIndex logIndex, LogPayload payload)
    : _logTerm{logTerm}, _logIndex{logIndex}, _payload{std::move(payload)} {}

auto LogEntry::logTerm() const noexcept -> LogTerm { return _logTerm; }

auto LogEntry::logIndex() const noexcept -> LogIndex { return _logIndex; }

auto LogEntry::logPayload() const noexcept -> LogPayload const& { return _payload; }

void LogEntry::toVelocyPack(velocypack::Builder& builder) const {
  builder.openObject();
  builder.add("logTerm", velocypack::Value(_logTerm.value));
  builder.add("logIndex", velocypack::Value(_logIndex.value));
  builder.add("payload", velocypack::Slice(_payload.dummy.data()));
  builder.close();
}

auto LogEntry::fromVelocyPack(velocypack::Slice slice) -> LogEntry {
  auto const logTerm = LogTerm{slice.get("logTerm").getNumericValue<std::size_t>()};
  auto const logIndex = LogIndex{slice.get("logIndex").getNumericValue<std::size_t>()};
  auto payload = LogPayload{slice.get("payload")};
  return LogEntry{logTerm, logIndex, std::move(payload)};
}

auto LogEntry::operator==(LogEntry const& other) const noexcept -> bool {
  return other._logIndex == _logIndex && other._logTerm == _logTerm &&
         other._payload == _payload;
}

void LogEntry::setInsertTp(clock::time_point tp) noexcept {
  _insertTp = tp;
}
auto LogEntry::insertTp() const noexcept -> clock::time_point {
  return _insertTp;
}

auto LogEntry::logTermIndexPair() const noexcept -> TermIndexPair {
  return TermIndexPair{_logTerm, _logIndex};
}

auto LogTerm::operator<=(LogTerm other) const -> bool {
  return value <= other.value;
}

auto replication2::operator==(LogPayload const& left, LogPayload const& right) -> bool {
  return arangodb::basics::VelocyPackHelper::equal(velocypack::Slice(left.dummy.data()),
                                                   velocypack::Slice(right.dummy.data()),
                                                   true);
}

auto replication2::operator!=(LogPayload const& left, LogPayload const& right) -> bool {
  return !(left == right);
}

LogPayload::LogPayload(VPackSlice slice) {
  VPackBuilder builder(dummy);
  builder.add(slice);
}

auto LogPayload::byteSize() const noexcept -> std::size_t {
  return dummy.size();
}

LogPayload::LogPayload(std::string_view dummy) {
  VPackBuilder builder(this->dummy);
  builder.add(VPackValue(dummy));
}

auto LogId::fromString(std::string_view name) noexcept -> std::optional<LogId> {
  if (std::all_of(name.begin(), name.end(), [](char c) { return isdigit(c); })) {
    using namespace basics::StringUtils;
    return LogId{uint64(name)};
  }
  return std::nullopt;
}

[[nodiscard]] LogId::operator velocypack::Value() const noexcept {
  return velocypack::Value(id());
}

auto replication2::to_string(LogId logId) -> std::string {
  return std::to_string(logId.id());
}

auto replication2::to_string(LogTerm term) -> std::string {
  return std::to_string(term.value);
}

auto replication2::to_string(LogIndex index) -> std::string {
  return std::to_string(index.value);
}

auto replication2::operator<=(replication2::TermIndexPair const& left,
                              replication2::TermIndexPair const& right) noexcept -> bool {
  if (left.term < right.term) {
    return true;
  } else if (left.term == right.term) {
    return left.index <= right.index;
  } else {
    return false;
  }
}

void replication2::TermIndexPair::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Term, VPackValue(term.value));
  builder.add(StaticStrings::Index, VPackValue(index.value));
}

auto replication2::TermIndexPair::fromVelocyPack(velocypack::Slice slice) -> TermIndexPair {
  TermIndexPair pair;
  pair.term = LogTerm{slice.get(StaticStrings::Term).getNumericValue<size_t>()};
  pair.index = LogIndex{slice.get(StaticStrings::Index).getNumericValue<size_t>()};
  return pair;
}

replication2::TermIndexPair::TermIndexPair(LogTerm term, LogIndex index) noexcept
    : term(term), index(index) {
  // Index 0 has always term 0, and it is the only index with that term.
  // FIXME this should be an if and only if
  TRI_ASSERT((index != LogIndex{0}) || (term == LogTerm{0}));
}

auto replication2::operator<<(std::ostream& os, const TermIndexPair& pair) -> std::ostream& {
    return os << '(' << pair.term << ':' << pair.index << ')';
}
