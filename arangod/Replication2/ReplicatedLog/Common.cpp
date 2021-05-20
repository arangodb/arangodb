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
  builder.add("payload", velocypack::Value(_payload.dummy));
  builder.close();
}

auto LogEntry::fromVelocyPack(velocypack::Slice slice) -> LogEntry {
  auto const logTerm = LogTerm{slice.get("logTerm").getNumericValue<std::size_t>()};
  auto const logIndex = LogIndex{slice.get("logIndex").getNumericValue<std::size_t>()};
  auto payload = LogPayload{slice.get("payload").copyString()};
  return LogEntry{logTerm, logIndex, std::move(payload)};
}

auto LogEntry::operator==(LogEntry const& other) const noexcept -> bool {
  return other._logIndex == _logIndex && other._logTerm == _logTerm &&
         other._payload == _payload;
}

auto LogTerm::operator<=(LogTerm other) const -> bool {
  return value <= other.value;
}
auto LogPayload::operator==(LogPayload const& other) const -> bool {
  return dummy == other.dummy;
}

auto LogId::fromShardName(std::string_view name) noexcept -> std::optional<LogId> {
  using namespace basics::StringUtils;
  constexpr auto isShardName = [](auto const& name) {
    return !name.empty() && name[0] == 's' &&
           std::all_of(name.begin() + 1, name.end(),
                       [](char c) { return isdigit(c); });
  };
  if (isShardName(name)) {
    auto const shardId = uint64({name.begin() + 1, name.end()});
    if (shardId > 0) {
      return LogId{shardId};
    }
  }
  return std::nullopt;
}
