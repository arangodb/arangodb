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

#include <Basics/StaticStrings.h>
#include <Basics/StringUtils.h>

#include <Basics/VelocyPackHelper.h>

using namespace arangodb;
using namespace arangodb::replication2;

auto replication2::operator==(LogPayload const& left, LogPayload const& right)
    -> bool {
  return arangodb::basics::VelocyPackHelper::equal(
      velocypack::Slice(left.dummy.data()),
      velocypack::Slice(right.dummy.data()), true);
}

LogPayload::LogPayload(velocypack::UInt8Buffer dummy)
    : dummy(std::move(dummy)) {}

auto LogPayload::createFromSlice(velocypack::Slice slice) -> LogPayload {
  VPackBufferUInt8 buffer;
  VPackBuilder builder(buffer);
  builder.add(slice);
  return LogPayload(std::move(buffer));
}

auto LogPayload::createFromString(std::string_view string) -> LogPayload {
  VPackBufferUInt8 buffer;
  VPackBuilder builder(buffer);
  builder.add(VPackValue(string));
  return LogPayload(std::move(buffer));
}

auto LogPayload::copyBuffer() const -> velocypack::UInt8Buffer {
  return dummy;
}

auto LogPayload::byteSize() const noexcept -> std::size_t {
  return dummy.byteSize();
}

auto LogPayload::slice() const noexcept -> velocypack::Slice {
  return VPackSlice(dummy.data());
}

PersistingLogEntry::PersistingLogEntry(LogTerm logTerm, LogIndex logIndex,
                                       std::optional<LogPayload> payload)
    : _logTerm{logTerm}, _logIndex{logIndex}, _payload{std::move(payload)} {}

PersistingLogEntry::PersistingLogEntry(TermIndexPair termIndexPair,
                                       std::optional<LogPayload> payload)
    : _logTerm(termIndexPair.term),
      _logIndex(termIndexPair.index),
      _payload(std::move(payload)) {}

auto PersistingLogEntry::logTerm() const noexcept -> LogTerm {
  return _logTerm;
}

auto PersistingLogEntry::logIndex() const noexcept -> LogIndex {
  return _logIndex;
}

auto PersistingLogEntry::logPayload() const noexcept
    -> std::optional<LogPayload> const& {
  return _payload;
}

void PersistingLogEntry::toVelocyPack(velocypack::Builder& builder) const {
  builder.openObject();
  builder.add("logIndex", velocypack::Value(_logIndex.value));
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
  builder.add("logTerm", velocypack::Value(_logTerm.value));
  if (_payload) {
    builder.add("payload", _payload->slice());
  }
}

auto PersistingLogEntry::fromVelocyPack(velocypack::Slice slice)
    -> PersistingLogEntry {
  auto const logTerm = slice.get("logTerm").extract<LogTerm>();
  auto const logIndex = slice.get("logIndex").extract<LogIndex>();
  auto payload = std::invoke([&]() -> std::optional<LogPayload> {
    if (auto payloadSlice = slice.get("payload"); !payloadSlice.isNone()) {
      return LogPayload::createFromSlice(payloadSlice);
    } else {
      return std::nullopt;
    }
  });
  return PersistingLogEntry(logTerm, logIndex, std::move(payload));
}

auto PersistingLogEntry::logTermIndexPair() const noexcept -> TermIndexPair {
  return TermIndexPair{_logTerm, _logIndex};
}

auto PersistingLogEntry::approxByteSize() const noexcept -> std::size_t {
  auto size = approxMetaDataSize;

  if (_payload.has_value()) {
    size += _payload->byteSize();
  }

  return size;
}

PersistingLogEntry::PersistingLogEntry(LogIndex index,
                                       velocypack::Slice persisted) {
  _logIndex = index;
  _logTerm = persisted.get("logTerm").extract<LogTerm>();
  if (auto payload = persisted.get("payload"); !payload.isNone()) {
    _payload = LogPayload::createFromSlice(payload);
  }
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
