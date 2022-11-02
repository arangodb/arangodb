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

#include "LogEntries.h"
#include "Inspection/VPack.h"

#include <Basics/StaticStrings.h>
#include <Basics/StringUtils.h>

#include <Basics/VelocyPackHelper.h>

using namespace arangodb;
using namespace arangodb::replication2;

auto replication2::operator==(LogPayload const& left, LogPayload const& right)
    -> bool {
  return arangodb::basics::VelocyPackHelper::equal(left.slice(), right.slice(),
                                                   true);
}

LogPayload::LogPayload(BufferType buffer) : buffer(std::move(buffer)) {}

auto LogPayload::createFromSlice(velocypack::Slice slice) -> LogPayload {
  return LogPayload(BufferType{slice.start(), slice.byteSize()});
}

auto LogPayload::createFromString(std::string_view string) -> LogPayload {
  VPackBuilder builder;
  builder.add(VPackValue(string));
  return LogPayload::createFromSlice(builder.slice());
}

auto LogPayload::copyBuffer() const -> velocypack::UInt8Buffer {
  velocypack::UInt8Buffer result;
  result.append(buffer.data(), buffer.size());
  return result;
}

auto LogPayload::byteSize() const noexcept -> std::size_t {
  return buffer.size();
}

auto LogPayload::slice() const noexcept -> velocypack::Slice {
  return VPackSlice(buffer.data());
}

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
  builder.add("logIndex", velocypack::Value(_termIndex.index.value));
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
  builder.add("logTerm", velocypack::Value(_termIndex.term.value));
  if (std::holds_alternative<LogPayload>(_payload)) {
    builder.add("payload", std::get<LogPayload>(_payload).slice());
  } else {
    TRI_ASSERT(std::holds_alternative<LogMetaPayload>(_payload));
    builder.add(velocypack::Value("meta"));
    std::get<LogMetaPayload>(_payload).toVelocyPack(builder);
  }
}

auto PersistingLogEntry::fromVelocyPack(velocypack::Slice slice)
    -> PersistingLogEntry {
  auto const logTerm = slice.get("logTerm").extract<LogTerm>();
  auto const logIndex = slice.get("logIndex").extract<LogIndex>();
  auto const termIndex = TermIndexPair{logTerm, logIndex};

  if (auto payload = slice.get("payload"); !payload.isNone()) {
    return {termIndex, LogPayload::createFromSlice(payload)};
  } else {
    auto meta = slice.get("meta");
    TRI_ASSERT(!meta.isNone());
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
  _termIndex.term = persisted.get("logTerm").extract<LogTerm>();
  if (auto payload = persisted.get("payload"); !payload.isNone()) {
    _payload = LogPayload::createFromSlice(payload);
  } else {
    auto meta = persisted.get("meta");
    TRI_ASSERT(!meta.isNone());
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

InMemoryLogEntry::InMemoryLogEntry(PersistingLogEntry entry, bool waitForSync)
    : _waitForSync(waitForSync), _logEntry(std::move(entry)) {}

void InMemoryLogEntry::setInsertTp(clock::time_point tp) noexcept {
  _insertTp = tp;
}

auto InMemoryLogEntry::insertTp() const noexcept -> clock::time_point {
  return _insertTp;
}

auto InMemoryLogEntry::entry() const noexcept -> PersistingLogEntry const& {
  // Note that while get() isn't marked as noexcept, it actually is.
  return _logEntry.get();
}

LogEntryView::LogEntryView(LogIndex index, LogPayload const& payload) noexcept
    : _index(index), _payload(payload.slice()) {}

LogEntryView::LogEntryView(LogIndex index, velocypack::Slice payload) noexcept
    : _index(index), _payload(payload) {}

auto LogEntryView::logIndex() const noexcept -> LogIndex { return _index; }

auto LogEntryView::logPayload() const noexcept -> velocypack::Slice {
  return _payload;
}

void LogEntryView::toVelocyPack(velocypack::Builder& builder) const {
  auto og = velocypack::ObjectBuilder(&builder);
  builder.add("logIndex", velocypack::Value(_index));
  builder.add("payload", _payload);
}

auto LogEntryView::fromVelocyPack(velocypack::Slice slice) -> LogEntryView {
  return LogEntryView(slice.get("logIndex").extract<LogIndex>(),
                      slice.get("payload"));
}

auto LogEntryView::clonePayload() const -> LogPayload {
  return LogPayload::createFromSlice(_payload);
}

namespace {
constexpr std::string_view StringFirstIndexOfTerm = "FirstIndexOfTerm";
constexpr std::string_view StringUpdateParticipantsConfig =
    "UpdateParticipantsConfig";
}  // namespace

auto LogMetaPayload::fromVelocyPack(velocypack::Slice s) -> LogMetaPayload {
  auto typeSlice = s.get(StaticStrings::IndexType);
  if (typeSlice.isEqualString(StringFirstIndexOfTerm)) {
    return {FirstEntryOfTerm::fromVelocyPack(s)};
  } else if (typeSlice.isEqualString(StringUpdateParticipantsConfig)) {
    return {UpdateParticipantsConfig::fromVelocyPack(s)};
  } else {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
}

void LogMetaPayload::toVelocyPack(velocypack::Builder& builder) const {
  std::visit([&](auto const& v) { v.toVelocyPack(builder); }, info);
}

void LogMetaPayload::FirstEntryOfTerm::toVelocyPack(
    velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(StaticStrings::IndexType, velocypack::Value(StringFirstIndexOfTerm));
  b.add(StaticStrings::Leader, velocypack::Value(leader));
  b.add(velocypack::Value(StaticStrings::Participants));
  velocypack::serialize(b, participants);
}

void LogMetaPayload::UpdateParticipantsConfig::toVelocyPack(
    velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(StaticStrings::IndexType,
        velocypack::Value(StringUpdateParticipantsConfig));
  b.add(velocypack::Value(StaticStrings::Participants));
  velocypack::serialize(b, participants);
}

auto LogMetaPayload::UpdateParticipantsConfig::fromVelocyPack(
    velocypack::Slice s) -> UpdateParticipantsConfig {
  TRI_ASSERT(s.get(StaticStrings::IndexType)
                 .isEqualString(StringUpdateParticipantsConfig));
  auto participants = velocypack::deserialize<agency::ParticipantsConfig>(
      s.get(StaticStrings::Participants));
  return UpdateParticipantsConfig{std::move(participants)};
}

auto LogMetaPayload::FirstEntryOfTerm::fromVelocyPack(velocypack::Slice s)
    -> FirstEntryOfTerm {
  TRI_ASSERT(
      s.get(StaticStrings::IndexType).isEqualString(StringFirstIndexOfTerm));
  auto leader = s.get(StaticStrings::Leader).copyString();
  auto participants = velocypack::deserialize<agency::ParticipantsConfig>(
      s.get(StaticStrings::Participants));
  return FirstEntryOfTerm{std::move(leader), std::move(participants)};
}

auto LogMetaPayload::withFirstEntryOfTerm(ParticipantId leader,
                                          agency::ParticipantsConfig config)
    -> LogMetaPayload {
  return LogMetaPayload{FirstEntryOfTerm{.leader = std::move(leader),
                                         .participants = std::move(config)}};
}

auto LogMetaPayload::withUpdateParticipantsConfig(
    agency::ParticipantsConfig config) -> LogMetaPayload {
  return LogMetaPayload{
      UpdateParticipantsConfig{.participants = std::move(config)}};
}
