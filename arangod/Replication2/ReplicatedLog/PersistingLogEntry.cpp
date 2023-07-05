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

#include "PersistingLogEntry.h"

namespace arangodb::replication2 {

PersistingLogEntry::PersistingLogEntry(
    TermIndexPair termIndexPair,
    std::variant<LogMetaPayload, LogPayload> payload)
    : _termIndex(termIndexPair), _payload(std::move(payload)) {}

auto PersistingLogEntry::logTerm() const noexcept -> LogTerm {
  return _termIndex.term;
}

auto PersistingLogEntry::logIndex() const noexcept -> LogIndex {
  return _termIndex.index;
}

auto PersistingLogEntry::logPayload() const noexcept -> LogPayload const* {
  return std::get_if<LogPayload>(&_payload);
}

void PersistingLogEntry::toVelocyPack(velocypack::Builder& builder) const {
  builder.openObject();
  builder.add(StaticStrings::LogIndex,
              velocypack::Value(_termIndex.index.value));
  entriesWithoutIndexToVelocyPack(builder);
  builder.close();
}

void PersistingLogEntry::toVelocyPack(velocypack::Builder& builder,
                                      PersistingLogEntry::OmitLogIndex) const {
  builder.openObject();
  entriesWithoutIndexToVelocyPack(builder);
  builder.close();
}

void PersistingLogEntry::entriesWithoutIndexToVelocyPack(
    velocypack::Builder& builder) const {
  builder.add(StaticStrings::LogTerm, velocypack::Value(_termIndex.term.value));
  if (std::holds_alternative<LogPayload>(_payload)) {
    builder.add(StaticStrings::Payload, std::get<LogPayload>(_payload).slice());
  } else {
    TRI_ASSERT(std::holds_alternative<LogMetaPayload>(_payload));
    builder.add(velocypack::Value(StaticStrings::Meta));
    std::get<LogMetaPayload>(_payload).toVelocyPack(builder);
  }
}

auto PersistingLogEntry::fromVelocyPack(velocypack::Slice slice)
    -> PersistingLogEntry {
  auto const logTerm = slice.get(StaticStrings::LogTerm).extract<LogTerm>();
  auto const logIndex = slice.get(StaticStrings::LogIndex).extract<LogIndex>();
  auto const termIndex = TermIndexPair{logTerm, logIndex};

  if (auto payload = slice.get(StaticStrings::Payload); !payload.isNone()) {
    return {termIndex, LogPayload::createFromSlice(payload)};
  } else {
    auto meta = slice.get(StaticStrings::Meta);
    TRI_ASSERT(!meta.isNone()) << slice.toJson();
    return {termIndex, LogMetaPayload::fromVelocyPack(meta)};
  }
}

auto PersistingLogEntry::logTermIndexPair() const noexcept -> TermIndexPair {
  return _termIndex;
}

auto PersistingLogEntry::approxByteSize() const noexcept -> std::size_t {
  auto size = approxMetaDataSize;

  if (std::holds_alternative<LogPayload>(_payload)) {
    return std::get<LogPayload>(_payload).byteSize();
  }

  return size;
}

PersistingLogEntry::PersistingLogEntry(LogIndex index,
                                       velocypack::Slice persisted) {
  _termIndex.index = index;
  _termIndex.term = persisted.get(StaticStrings::LogTerm).extract<LogTerm>();
  if (auto payload = persisted.get(StaticStrings::Payload); !payload.isNone()) {
    _payload = LogPayload::createFromSlice(payload);
  } else {
    auto meta = persisted.get(StaticStrings::Meta);
    TRI_ASSERT(!meta.isNone()) << persisted.toJson();
    _payload = LogMetaPayload::fromVelocyPack(meta);
  }
}

auto PersistingLogEntry::hasPayload() const noexcept -> bool {
  return std::holds_alternative<LogPayload>(_payload);
}
auto PersistingLogEntry::hasMeta() const noexcept -> bool {
  return std::holds_alternative<LogMetaPayload>(_payload);
}

auto PersistingLogEntry::meta() const noexcept -> const LogMetaPayload* {
  return std::get_if<LogMetaPayload>(&_payload);
}

}  // namespace arangodb::replication2
