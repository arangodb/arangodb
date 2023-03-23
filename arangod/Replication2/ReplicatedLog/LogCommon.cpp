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
#include <Inspection/VPack.h>

#include <chrono>
#include <utility>
#include <fmt/core.h>

using namespace arangodb;
using namespace arangodb::replication2;

auto LogIndex::operator+(std::uint64_t delta) const -> LogIndex {
  return LogIndex(this->value + delta);
}

auto LogIndex::operator+=(std::uint64_t delta) -> LogIndex& {
  this->value += delta;
  return *this;
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

auto LogRange::contains(LogRange other) const noexcept -> bool {
  return from <= other.from && other.to <= to;
}

auto replication2::operator<<(std::ostream& os, LogRange const& r)
    -> std::ostream& {
  return os << "[" << r.from << ", " << r.to << ")";
}

auto replication2::intersect(LogRange a, LogRange b) noexcept -> LogRange {
  auto max_from = std::max(a.from, b.from);
  auto min_to = std::min(a.to, b.to);
  if (max_from > min_to) {
    return {};
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

auto replication2::operator==(LogRange left, LogRange right) noexcept -> bool {
  // Two ranges compare equal iff either both are empty or _from_ and _to_ agree
  return (left.empty() && right.empty()) ||
         (left.from == right.from && left.to == right.to);
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
inline constexpr std::string_view NonEligibleNotAllowedInQuorum =
    "notAllowedInQuorum";
inline constexpr std::string_view NonEligibleWrongTerm = "wrongTerm";
inline constexpr std::string_view NonEligibleSnapshotMissing =
    "snapshotMissing";
}  // namespace

auto replicated_log::operator<<(
    std::ostream& ostream,
    CommitFailReason::QuorumSizeNotReached::ParticipantInfo pInfo)
    -> std::ostream& {
  ostream << "{ ";
  ostream << std::boolalpha;
  if (not pInfo.snapshotAvailable) {
    ostream << "snapshot: " << pInfo.snapshotAvailable << ", ";
  }
  if (pInfo.isAllowedInQuorum) {
    ostream << "isAllowedInQuorum: " << pInfo.isAllowedInQuorum;
  } else {
    ostream << "lastAcknowledgedEntry: " << pInfo.lastAcknowledged;
  }
  ostream << " }";
  return ostream;
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
    case kSnapshotMissing:
      return NonEligibleSnapshotMissing;
    default:
      TRI_ASSERT(false);
      return "(unknown)";
  }
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
      return fmt::format(
          "Fewer participants than effective write concern. Have {} ",
          "participants and effectiveWriteConcern={}.", reason.numParticipants,
          reason.effectiveWriteConcern);
    }
  };

  return std::visit(ToStringVisitor{}, r.value);
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

GlobalLogIdentifier::GlobalLogIdentifier(std::string database, LogId id)
    : database(std::move(database)), id(id) {}

auto replication2::to_string(GlobalLogIdentifier const& gid) -> std::string {
  VPackBuilder b;
  velocypack::serialize(b, gid);
  return b.toJson();
}

auto replication2::operator<<(std::ostream& os, GlobalLogIdentifier const& gid)
    -> std::ostream& {
  return os << to_string(gid);
}

auto replicated_log::CompactionResponse::fromResult(
    ResultT<CompactionResult> res) -> CompactionResponse {
  if (res.fail()) {
    return CompactionResponse{
        Error{res.errorNumber(), std::string{res.errorMessage()}}};
  } else {
    return CompactionResponse{std::move(res).get()};
  }
}

auto replicated_log::operator<<(std::ostream& os,
                                CompactionStopReason const& csr)
    -> std::ostream& {
  return os << to_string(csr);
}

auto replicated_log::to_string(CompactionStopReason const& csr) -> std::string {
  struct ToStringVisitor {
    auto operator()(CompactionStopReason::LeaderBlocksReleaseEntry const&)
        -> std::string {
      return "Leader prevents release of more log entries";
    }
    auto operator()(CompactionStopReason::NothingToCompact const&)
        -> std::string {
      return "Nothing to compact";
    }
    auto operator()(
        CompactionStopReason::NotReleasedByStateMachine const& reason)
        -> std::string {
      return fmt::format("Statemachine release index is at {}",
                         reason.releasedIndex.value);
    }
    auto operator()(
        CompactionStopReason::CompactionThresholdNotReached const& reason)
        -> std::string {
      return fmt::format(
          "Automatic compaction threshold not reached, next compaction at {}",
          reason.nextCompactionAt.value);
    }
    auto operator()(
        CompactionStopReason::ParticipantMissingEntries const& reason)
        -> std::string {
      return fmt::format(
          "Compaction waiting for participant {} to receive all log entries",
          reason.who);
    }
  };

  return std::visit(ToStringVisitor{}, csr.value);
}
