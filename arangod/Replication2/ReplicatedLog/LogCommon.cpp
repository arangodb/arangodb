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

#include "LogCommon.h"

#include <Basics/StaticStrings.h>
#include <Basics/StringUtils.h>
#include <Basics/VelocyPackHelper.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

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

LogIndex::operator velocypack::Value() const noexcept {
  return velocypack::Value(value);
}

auto replication2::operator<<(std::ostream& os, LogIndex idx) -> std::ostream& {
  return os << idx.value;
}

auto LogIndex::saturatedDecrement(uint64_t delta) const noexcept -> LogIndex {
  if (value > delta) {
    return LogIndex{value - delta};
  }

  return LogIndex{0};
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
    builder.add("payload", velocypack::Slice(_payload->dummy.data()));
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

auto PersistingLogEntry::operator==(
    PersistingLogEntry const& other) const noexcept -> bool {
  return other._logIndex == _logIndex && other._logTerm == _logTerm &&
         other._payload == _payload;
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

auto LogTerm::operator<=(LogTerm other) const -> bool {
  return value <= other.value;
}

LogTerm::operator velocypack::Value() const noexcept {
  return velocypack::Value(value);
}

auto replication2::operator<<(std::ostream& os, LogTerm term) -> std::ostream& {
  return os << term.value;
}

auto replication2::operator==(LogPayload const& left, LogPayload const& right)
    -> bool {
  return arangodb::basics::VelocyPackHelper::equal(
      velocypack::Slice(left.dummy.data()),
      velocypack::Slice(right.dummy.data()), true);
}

auto replication2::operator!=(LogPayload const& left, LogPayload const& right)
    -> bool {
  return !(left == right);
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

auto LogPayload::byteSize() const noexcept -> std::size_t {
  return dummy.byteSize();
}

auto LogPayload::slice() const noexcept -> velocypack::Slice {
  return VPackSlice(dummy.data());
}

auto LogId::fromString(std::string_view name) noexcept -> std::optional<LogId> {
  if (std::all_of(name.begin(), name.end(),
                  [](char c) { return isdigit(c); })) {
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

auto replication2::operator<=(replication2::TermIndexPair left,
                              replication2::TermIndexPair right) noexcept
    -> bool {
  if (left.term < right.term) {
    return true;
  } else if (left.term == right.term) {
    return left.index <= right.index;
  } else {
    return false;
  }
}

void replication2::TermIndexPair::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Term, VPackValue(term.value));
  builder.add(StaticStrings::Index, VPackValue(index.value));
}

auto replication2::TermIndexPair::fromVelocyPack(velocypack::Slice slice)
    -> TermIndexPair {
  TermIndexPair pair;
  pair.term = slice.get(StaticStrings::Term).extract<LogTerm>();
  pair.index = slice.get(StaticStrings::Index).extract<LogIndex>();
  return pair;
}

replication2::TermIndexPair::TermIndexPair(LogTerm term,
                                           LogIndex index) noexcept
    : term(term), index(index) {
  // Index 0 has always term 0, and it is the only index with that term.
  // FIXME this should be an if and only if
  TRI_ASSERT((index != LogIndex{0}) || (term == LogTerm{0}));
}

auto replication2::operator<<(std::ostream& os, TermIndexPair pair)
    -> std::ostream& {
  return os << '(' << pair.term << ':' << pair.index << ')';
}

LogConfig::LogConfig(VPackSlice slice) {
  waitForSync = slice.get(StaticStrings::WaitForSyncString).extract<bool>();
  writeConcern = slice.get(StaticStrings::WriteConcern).extract<std::size_t>();
  replicationFactor =
      slice.get(StaticStrings::ReplicationFactor).extract<std::size_t>();
}

LogConfig::LogConfig(std::size_t writeConcern, std::size_t replicationFactor,
                     bool waitForSync) noexcept
    : writeConcern(writeConcern),
      replicationFactor(replicationFactor),
      waitForSync(waitForSync) {}

auto LogConfig::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::WaitForSyncString, VPackValue(waitForSync));
  builder.add(StaticStrings::WriteConcern, VPackValue(writeConcern));
  builder.add(StaticStrings::ReplicationFactor, VPackValue(replicationFactor));
}

auto replication2::operator==(LogConfig const& left,
                              LogConfig const& right) noexcept -> bool {
  // TODO How can we make sure that we never forget a field here?
  return left.waitForSync == right.waitForSync &&
         left.writeConcern == right.writeConcern &&
         left.replicationFactor == right.replicationFactor;
}

auto replication2::operator!=(const LogConfig& left,
                              const LogConfig& right) noexcept -> bool {
  return !(left == right);
}

LogRange::LogRange(LogIndex from, LogIndex to) noexcept : from(from), to(to) {
  TRI_ASSERT(from <= to);
}

auto LogRange::empty() const noexcept -> bool { return from == to; }

auto LogRange::count() const noexcept -> std::size_t {
  return to.value - from.value;
}

auto LogRange::contains(LogIndex idx) const noexcept -> bool {
  return from <= idx && idx < to;
}

auto replication2::operator<<(std::ostream& os, LogRange const& r)
    -> std::ostream& {
  return os << "[" << r.from << ", " << r.to << ")";
}

auto replication2::intersect(LogRange a, LogRange b) noexcept -> LogRange {
  auto max_from = std::max(a.from, b.from);
  auto min_to = std::min(a.to, b.to);
  if (max_from > min_to) {
    return {LogIndex{0}, LogIndex{0}};
  } else {
    return {max_from, min_to};
  }
}

auto LogRange::end() const noexcept -> LogRange::Iterator {
  return Iterator{to};
}
auto LogRange::begin() const noexcept -> LogRange::Iterator {
  return Iterator{from};
}

auto LogRange::Iterator::operator++() noexcept -> LogRange::Iterator& {
  current = current + 1;
  return *this;
}

auto LogRange::Iterator::operator++(int) noexcept -> LogRange::Iterator {
  auto idx = current;
  current = current + 1;
  return Iterator(idx);
}

auto LogRange::Iterator::operator*() const noexcept -> LogIndex {
  return current;
}
auto LogRange::Iterator::operator->() const noexcept -> LogIndex const* {
  return &current;
}

auto replication2::operator==(LogRange a, LogRange b) noexcept -> bool {
  return a.from == b.from && a.to == b.to;
}

auto replication2::operator!=(LogRange a, LogRange b) noexcept -> bool {
  return !(a == b);
}

auto replication2::operator==(LogRange::Iterator const& a,
                              LogRange::Iterator const& b) noexcept -> bool {
  return a.current == b.current;
}

auto replication2::operator!=(LogRange::Iterator const& a,
                              LogRange::Iterator const& b) noexcept -> bool {
  return !(a == b);
}

template<typename... Args>
replicated_log::CommitFailReason::CommitFailReason(std::in_place_t,
                                                   Args&&... args) noexcept
    : value(std::forward<Args>(args)...) {}

auto replicated_log::CommitFailReason::withNothingToCommit() noexcept
    -> CommitFailReason {
  return CommitFailReason(std::in_place, NothingToCommit{});
}

auto replicated_log::CommitFailReason::withQuorumSizeNotReached() noexcept
    -> CommitFailReason {
  return CommitFailReason(std::in_place, QuorumSizeNotReached{});
}

auto replicated_log::CommitFailReason::
    withForcedParticipantNotInQuorum() noexcept -> CommitFailReason {
  return CommitFailReason(std::in_place, ForcedParticipantNotInQuorum{});
}

namespace {
inline constexpr std::string_view ReasonFieldName = "reason";
inline constexpr std::string_view NothingToCommitEnum = "NothingToCommit";
inline constexpr std::string_view QuorumSizeNotReachedEnum =
    "QuorumSizeNotReached";
inline constexpr std::string_view ForcedParticipantNotInQuorumEnum =
    "ForcedParticipantNotInQuorum";
}  // namespace

auto replicated_log::CommitFailReason::NothingToCommit::fromVelocyPack(
    velocypack::Slice s) -> NothingToCommit {
  TRI_ASSERT(s.get(ReasonFieldName).isString())
      << "Expected string, found: " << s.toJson();
  TRI_ASSERT(
      s.get(ReasonFieldName).isEqualString(VPackStringRef(NothingToCommitEnum)))
      << "Expected string `" << NothingToCommitEnum
      << "`, found: " << s.stringView();
  return {};
}

void replicated_log::CommitFailReason::NothingToCommit::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(VPackStringRef(ReasonFieldName), VPackValue(NothingToCommitEnum));
}

auto replicated_log::CommitFailReason::QuorumSizeNotReached::fromVelocyPack(
    velocypack::Slice s) -> QuorumSizeNotReached {
  TRI_ASSERT(s.get(ReasonFieldName).isString())
      << "Expected string, found: " << s.toJson();
  TRI_ASSERT(s.get(ReasonFieldName)
                 .isEqualString(VPackStringRef(QuorumSizeNotReachedEnum)))
      << "Expected string `" << QuorumSizeNotReachedEnum
      << "`, found: " << s.stringView();
  return {};
}

void replicated_log::CommitFailReason::QuorumSizeNotReached::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(VPackStringRef(ReasonFieldName),
              VPackValue(QuorumSizeNotReachedEnum));
}

auto replicated_log::CommitFailReason::ForcedParticipantNotInQuorum::
    fromVelocyPack(velocypack::Slice s) -> ForcedParticipantNotInQuorum {
  TRI_ASSERT(s.get(ReasonFieldName).isString())
      << "Expected string, found: " << s.toJson();
  TRI_ASSERT(
      s.get(ReasonFieldName)
          .isEqualString(VPackStringRef(ForcedParticipantNotInQuorumEnum)))
      << "Expected string `" << ForcedParticipantNotInQuorumEnum
      << "`, found: " << s.stringView();
  return {};
}

void replicated_log::CommitFailReason::ForcedParticipantNotInQuorum::
    toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(VPackStringRef(ReasonFieldName),
              VPackValue(ForcedParticipantNotInQuorumEnum));
}

auto replicated_log::CommitFailReason::fromVelocyPack(velocypack::Slice s)
    -> CommitFailReason {
  auto reason = s.get(ReasonFieldName).stringView();
  if (reason == NothingToCommitEnum) {
    return CommitFailReason{std::in_place, NothingToCommit::fromVelocyPack(s)};
  } else if (reason == QuorumSizeNotReachedEnum) {
    return CommitFailReason{std::in_place,
                            QuorumSizeNotReached::fromVelocyPack(s)};
  } else if (reason == ForcedParticipantNotInQuorumEnum) {
    return CommitFailReason{std::in_place,
                            ForcedParticipantNotInQuorum::fromVelocyPack(s)};
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        basics::StringUtils::concatT("CommitFailReason `", reason,
                                     "` unknown."));
  }
}

void replicated_log::CommitFailReason::toVelocyPack(
    velocypack::Builder& builder) const {
  std::visit([&](auto const& v) { v.toVelocyPack(builder); }, value);
}

auto replicated_log::to_string(CommitFailReason const& r) -> std::string {
  struct ToStringVisitor {
    auto operator()(CommitFailReason::NothingToCommit const&) -> std::string {
      return "Nothing to commit";
    }
    auto operator()(CommitFailReason::QuorumSizeNotReached const&)
        -> std::string {
      return "Required quorum size not yet reached";
    }
    auto operator()(CommitFailReason::ForcedParticipantNotInQuorum const&)
        -> std::string {
      return "Forced participant not in quorum";
    }
  };

  return std::visit(ToStringVisitor{}, r.value);
}
