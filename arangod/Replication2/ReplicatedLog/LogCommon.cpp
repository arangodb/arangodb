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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "LogCommon.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include <Basics/StaticStrings.h>
#include <Basics/StringUtils.h>
#include <Basics/VelocyPackHelper.h>
#include <Basics/debugging.h>

#include <chrono>
#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;

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

LogTerm::operator velocypack::Value() const noexcept {
  return velocypack::Value(value);
}

auto replication2::operator<<(std::ostream& os, LogTerm term) -> std::ostream& {
  return os << term.value;
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

void replication2::TermIndexPair::toVelocyPack(
    velocypack::Builder& builder) const {
  serialize(builder, *this);
}

auto replication2::TermIndexPair::fromVelocyPack(velocypack::Slice slice)
    -> TermIndexPair {
  return deserialize<TermIndexPair>(slice);
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
  if (auto sw = slice.get(StaticStrings::SoftWriteConcern); !sw.isNone()) {
    softWriteConcern = sw.extract<std::size_t>();
  } else {
    softWriteConcern = writeConcern;
  }
  replicationFactor =
      slice.get(StaticStrings::ReplicationFactor).extract<std::size_t>();
}

LogConfig::LogConfig(std::size_t writeConcern, std::size_t softWriteConcern,
                     std::size_t replicationFactor, bool waitForSync) noexcept
    : writeConcern(writeConcern),
      softWriteConcern(softWriteConcern),
      replicationFactor(replicationFactor),
      waitForSync(waitForSync) {}

auto LogConfig::toVelocyPack(VPackBuilder& builder) const -> void {
  serialize(builder, *this);
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

auto replication2::to_string(LogRange const& r) -> std::string {
  return basics::StringUtils::concatT("[", r.from, ", ", r.to, ")");
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

template<typename... Args>
replicated_log::CommitFailReason::CommitFailReason(std::in_place_t,
                                                   Args&&... args) noexcept
    : value(std::forward<Args>(args)...) {}

auto replicated_log::CommitFailReason::withNothingToCommit() noexcept
    -> CommitFailReason {
  return CommitFailReason(std::in_place, NothingToCommit{});
}

auto replicated_log::CommitFailReason::withQuorumSizeNotReached(
    QuorumSizeNotReached::who_type who, TermIndexPair spearhead) noexcept
    -> CommitFailReason {
  return CommitFailReason(std::in_place,
                          QuorumSizeNotReached{std::move(who), spearhead});
}

auto replicated_log::CommitFailReason::withForcedParticipantNotInQuorum(
    ParticipantId who) noexcept -> CommitFailReason {
  return CommitFailReason(std::in_place,
                          ForcedParticipantNotInQuorum{std::move(who)});
}

auto replicated_log::CommitFailReason::withNonEligibleServerRequiredForQuorum(
    NonEligibleServerRequiredForQuorum::CandidateMap candidates) noexcept
    -> CommitFailReason {
  return CommitFailReason(
      std::in_place, NonEligibleServerRequiredForQuorum{std::move(candidates)});
}

namespace {
inline constexpr std::string_view ReasonFieldName = "reason";
inline constexpr std::string_view NothingToCommitEnum = "NothingToCommit";
inline constexpr std::string_view QuorumSizeNotReachedEnum =
    "QuorumSizeNotReached";
inline constexpr std::string_view ForcedParticipantNotInQuorumEnum =
    "ForcedParticipantNotInQuorum";
inline constexpr std::string_view NonEligibleServerRequiredForQuorumEnum =
    "NonEligibleServerRequiredForQuorum";
inline constexpr std::string_view WhoFieldName = "who";
inline constexpr std::string_view CandidatesFieldName = "candidates";
inline constexpr std::string_view NonEligibleNotAllowedInQuorum =
    "notAllowedInQuorum";
inline constexpr std::string_view NonEligibleWrongTerm = "wrongTerm";
inline constexpr std::string_view IsFailedFieldName = "isFailed";
inline constexpr std::string_view IsAllowedInQuorumFieldName =
    "isAllowedInQuorum";
inline constexpr std::string_view LastAcknowledgedFieldName =
    "lastAcknowledged";
inline constexpr std::string_view SpearheadFieldName = "spearhead";
}  // namespace

auto replicated_log::CommitFailReason::NothingToCommit::fromVelocyPack(
    velocypack::Slice s) -> NothingToCommit {
  TRI_ASSERT(s.get(ReasonFieldName).isString())
      << "Expected string, found: " << s.toJson();
  TRI_ASSERT(s.get(ReasonFieldName).isEqualString(NothingToCommitEnum))
      << "Expected string `" << NothingToCommitEnum
      << "`, found: " << s.stringView();
  return {};
}

void replicated_log::CommitFailReason::NothingToCommit::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(std::string_view(ReasonFieldName),
              VPackValue(NothingToCommitEnum));
}

auto replicated_log::CommitFailReason::QuorumSizeNotReached::fromVelocyPack(
    velocypack::Slice s) -> QuorumSizeNotReached {
  TRI_ASSERT(s.get(ReasonFieldName).isString())
      << "Expected string, found: " << s.toJson();
  TRI_ASSERT(s.get(ReasonFieldName).isEqualString(QuorumSizeNotReachedEnum))
      << "Expected string `" << QuorumSizeNotReachedEnum
      << "`, found: " << s.stringView();
  TRI_ASSERT(s.get(WhoFieldName).isObject())
      << "Expected object, found: " << s.toJson();
  auto result = QuorumSizeNotReached();
  for (auto const& [participantIdSlice, participantInfoSlice] :
       VPackObjectIterator(s.get(WhoFieldName))) {
    auto const participantId = participantIdSlice.stringView();
    result.who.try_emplace(
        participantId, ParticipantInfo::fromVelocyPack(participantInfoSlice));
  }
  result.spearhead = deserialize<TermIndexPair>(s.get(SpearheadFieldName));
  return result;
}

void replicated_log::CommitFailReason::QuorumSizeNotReached::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(ReasonFieldName, VPackValue(QuorumSizeNotReachedEnum));
  {
    builder.add(VPackValue(WhoFieldName));
    VPackObjectBuilder objWho(&builder);

    for (auto const& [participantId, participantInfo] : who) {
      builder.add(VPackValue(participantId));
      participantInfo.toVelocyPack(builder);
    }
  }
  {
    builder.add(VPackValue(SpearheadFieldName));
    serialize(builder, spearhead);
  }
}

auto replicated_log::CommitFailReason::QuorumSizeNotReached::ParticipantInfo::
    fromVelocyPack(velocypack::Slice s) -> ParticipantInfo {
  TRI_ASSERT(s.get(IsFailedFieldName).isBool())
      << "Expected bool in field `" << IsFailedFieldName << "` in "
      << s.toJson();
  return {
      .isFailed = s.get(IsFailedFieldName).getBool(),
      .isAllowedInQuorum = s.get(IsAllowedInQuorumFieldName).getBool(),
      .lastAcknowledged =
          deserialize<TermIndexPair>(s.get(LastAcknowledgedFieldName)),
  };
}

void replicated_log::CommitFailReason::QuorumSizeNotReached::ParticipantInfo::
    toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(IsFailedFieldName, isFailed);
  builder.add(IsAllowedInQuorumFieldName, isAllowedInQuorum);
  {
    builder.add(VPackValue(LastAcknowledgedFieldName));
    serialize(builder, lastAcknowledged);
  }
}

auto replicated_log::operator<<(
    std::ostream& ostream,
    CommitFailReason::QuorumSizeNotReached::ParticipantInfo pInfo)
    -> std::ostream& {
  ostream << "{ ";
  ostream << std::boolalpha;
  if (pInfo.isAllowedInQuorum) {
    ostream << "isAllowedInQuorum: " << pInfo.isAllowedInQuorum;
  } else {
    ostream << "lastAcknowledgedEntry: " << pInfo.lastAcknowledged;
    if (pInfo.isFailed) {
      ostream << ", isFailed: " << pInfo.isFailed;
    }
  }
  ostream << " }";
  return ostream;
}

auto replicated_log::CommitFailReason::ForcedParticipantNotInQuorum::
    fromVelocyPack(velocypack::Slice s) -> ForcedParticipantNotInQuorum {
  TRI_ASSERT(s.get(ReasonFieldName).isString())
      << "Expected string, found: " << s.toJson();
  TRI_ASSERT(
      s.get(ReasonFieldName).isEqualString(ForcedParticipantNotInQuorumEnum))
      << "Expected string `" << ForcedParticipantNotInQuorumEnum
      << "`, found: " << s.stringView();
  TRI_ASSERT(s.get(WhoFieldName).isString())
      << "Expected string, found: " << s.toJson();
  return {s.get(WhoFieldName).toString()};
}

void replicated_log::CommitFailReason::ForcedParticipantNotInQuorum::
    toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(std::string_view(ReasonFieldName),
              VPackValue(ForcedParticipantNotInQuorumEnum));
  builder.add(std::string_view(WhoFieldName), VPackValue(who));
}

void replicated_log::CommitFailReason::NonEligibleServerRequiredForQuorum::
    toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(ReasonFieldName,
              VPackValue(NonEligibleServerRequiredForQuorumEnum));
  builder.add(VPackValue(CandidatesFieldName));
  VPackObjectBuilder canObject(&builder);
  for (auto const& [p, why] : candidates) {
    builder.add(p, VPackValue(to_string(why)));
  }
}

auto replicated_log::CommitFailReason::NonEligibleServerRequiredForQuorum::
    to_string(replicated_log::CommitFailReason::
                  NonEligibleServerRequiredForQuorum::Why why) noexcept
    -> std::string_view {
  switch (why) {
    case kNotAllowedInQuorum:
      return NonEligibleNotAllowedInQuorum;
    case kWrongTerm:
      return NonEligibleWrongTerm;
    default:
      TRI_ASSERT(false);
      return "(unknown)";
  }
}

auto replicated_log::CommitFailReason::NonEligibleServerRequiredForQuorum::
    fromVelocyPack(velocypack::Slice s) -> NonEligibleServerRequiredForQuorum {
  TRI_ASSERT(s.get(ReasonFieldName)
                 .isEqualString(NonEligibleServerRequiredForQuorumEnum))
      << "Expected string `" << NonEligibleServerRequiredForQuorumEnum
      << "`, found: " << s.stringView();
  CandidateMap candidates;
  for (auto const& [key, value] :
       velocypack::ObjectIterator(s.get(CandidatesFieldName))) {
    if (value.isEqualString(NonEligibleNotAllowedInQuorum)) {
      candidates[key.copyString()] = kNotAllowedInQuorum;
    } else if (value.isEqualString(NonEligibleWrongTerm)) {
      candidates[key.copyString()] = kWrongTerm;
    }
  }
  return NonEligibleServerRequiredForQuorum{std::move(candidates)};
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
  } else if (reason == NonEligibleServerRequiredForQuorumEnum) {
    return CommitFailReason{
        std::in_place, NonEligibleServerRequiredForQuorum::fromVelocyPack(s)};
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

auto replicated_log::CommitFailReason::withFewerParticipantsThanWriteConcern(
    FewerParticipantsThanWriteConcern fewerParticipantsThanWriteConcern)
    -> replicated_log::CommitFailReason {
  auto result = CommitFailReason();
  result.value = fewerParticipantsThanWriteConcern;
  return result;
}

auto replicated_log::to_string(CommitFailReason const& r) -> std::string {
  struct ToStringVisitor {
    auto operator()(CommitFailReason::NothingToCommit const&) -> std::string {
      return "Nothing to commit";
    }
    auto operator()(CommitFailReason::QuorumSizeNotReached const& reason)
        -> std::string {
      auto stream = std::stringstream();
      stream << "Required quorum size not yet reached. ";
      stream << "The leader's spearhead is at " << reason.spearhead << ". ";
      stream << "Participants who aren't currently contributing to the "
                "spearhead are ";
      // ADL cannot find this operator here.
      arangodb::operator<<(stream, reason.who);
      return stream.str();
    }
    auto operator()(
        CommitFailReason::ForcedParticipantNotInQuorum const& reason)
        -> std::string {
      return "Forced participant not in quorum. Participant " + reason.who;
    }
    auto operator()(
        CommitFailReason::NonEligibleServerRequiredForQuorum const& reason)
        -> std::string {
      auto result =
          std::string{"A non-eligible server is required to reach a quorum: "};
      for (auto const& [pid, why] : reason.candidates) {
        result += basics::StringUtils::concatT(" ", pid, ": ", why);
      }
      return result;
    }
    auto operator()(
        CommitFailReason::FewerParticipantsThanWriteConcern const& reason) {
      using namespace basics::StringUtils;
      return concatT("Fewer participants than write concern. Have ",
                     reason.numParticipants,
                     " participants and effectiveWriteConcern=",
                     reason.effectiveWriteConcern,
                     ". With writeConcern=", reason.writeConcern,
                     " and softWriteConcern=", reason.softWriteConcern, ".");
    }
  };

  return std::visit(ToStringVisitor{}, r.value);
}

void replication2::ParticipantFlags::toVelocyPack(
    velocypack::Builder& builder) const {
  serialize(builder, *this);
}

auto replication2::ParticipantFlags::fromVelocyPack(velocypack::Slice s)
    -> ParticipantFlags {
  return deserialize<ParticipantFlags>(s);
}

auto replication2::operator<<(std::ostream& os, ParticipantFlags const& f)
    -> std::ostream& {
  os << "{ ";
  if (f.forced) {
    os << "forced ";
  }
  if (f.allowedAsLeader) {
    os << "allowedAsLeader ";
  }
  if (f.allowedInQuorum) {
    os << "allowedInQuorum ";
  }
  return os << "}";
}

void replication2::ParticipantsConfig::toVelocyPack(
    velocypack::Builder& builder) const {
  serialize(builder, *this);
}

auto replication2::ParticipantsConfig::fromVelocyPack(velocypack::Slice s)
    -> ParticipantsConfig {
  return deserialize<ParticipantsConfig>(s);
}

auto replicated_log::CommitFailReason::FewerParticipantsThanWriteConcern::
    fromVelocyPack(velocypack::Slice)
        -> replicated_log::CommitFailReason::FewerParticipantsThanWriteConcern {
  auto result =
      replicated_log::CommitFailReason::FewerParticipantsThanWriteConcern();

  return result;
}

void replicated_log::CommitFailReason::FewerParticipantsThanWriteConcern::
    toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(StaticStrings::WriteConcern, writeConcern);
  builder.add(StaticStrings::SoftWriteConcern, softWriteConcern);
  builder.add(StaticStrings::EffectiveWriteConcern, effectiveWriteConcern);
}

GlobalLogIdentifier::GlobalLogIdentifier(std::string database, LogId id)
    : database(std::move(database)), id(id) {}
