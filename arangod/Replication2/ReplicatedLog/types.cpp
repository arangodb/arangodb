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

#include "types.h"

#include <Basics/Exceptions.h>
#include <Basics/debugging.h>
#include <Basics/StaticStrings.h>
#include <Basics/overload.h>
#include <Basics/voc-errors.h>
#include <Containers/ImmerMemoryPolicy.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <cstddef>
#include <functional>
#include <utility>

#include "Replication2/ReplicatedLog/NetworkMessages.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

replicated_log::QuorumData::QuorumData(LogIndex index, LogTerm term,
                                       std::vector<ParticipantId> quorum)
    : index(index), term(term), quorum(std::move(quorum)) {}

replicated_log::QuorumData::QuorumData(LogIndex index, LogTerm term)
    : QuorumData(index, term, {}) {}

replicated_log::QuorumData::QuorumData(VPackSlice slice) {
  index = LogIndex{slice.get(StaticStrings::Index).extract<std::size_t>()};
  term = LogTerm{slice.get("term").extract<std::size_t>()};
  for (auto part : VPackArrayIterator(slice.get("quorum"))) {
    quorum.push_back(part.copyString());
  }
}

void replicated_log::QuorumData::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Index, VPackValue(index.value));
  builder.add("term", VPackValue(term.value));
  {
    VPackArrayBuilder ab(&builder, "quorum");
    for (auto const& part : quorum) {
      builder.add(VPackValue(part));
    }
  }
}


void replicated_log::LogStatistics::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("commitIndex", VPackValue(commitIndex.value));
  builder.add(VPackValue(StaticStrings::Spearhead));
  spearHead.toVelocyPack(builder);
}

auto replicated_log::LogStatistics::fromVelocyPack(velocypack::Slice slice) -> LogStatistics {
  LogStatistics stats;
  stats.commitIndex = LogIndex{slice.get("commitIndex").getNumericValue<size_t>()};
  stats.spearHead = TermIndexPair::fromVelocyPack(slice.get(StaticStrings::Spearhead));
  return stats;
}

void replicated_log::UnconfiguredStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue("unconfigured"));
}

auto replicated_log::UnconfiguredStatus::fromVelocyPack(velocypack::Slice slice) -> UnconfiguredStatus {
  TRI_ASSERT(slice.get("role").isEqualString("unconfigured"));
  return UnconfiguredStatus();
}

void replicated_log::FollowerStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue(StaticStrings::Follower));
  if (leader.has_value()) {
    builder.add(StaticStrings::Leader, VPackValue(*leader));
  }
  builder.add(StaticStrings::Term, VPackValue(term.value));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
}

auto replicated_log::FollowerStatus::fromVelocyPack(velocypack::Slice slice) -> FollowerStatus {
  TRI_ASSERT(slice.get("role").isEqualString(StaticStrings::Follower));
  FollowerStatus status;
  status.term = LogTerm{slice.get(StaticStrings::Term).getNumericValue<std::size_t>()};
  status.local = LogStatistics::fromVelocyPack(slice);
  if (auto leader = slice.get(StaticStrings::Leader); !leader.isNone()) {
    status.leader = leader.copyString();
  }
  return status;
}

void replicated_log::LeaderStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue(StaticStrings::Leader));
  builder.add(StaticStrings::Term, VPackValue(term.value));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
  {
    VPackObjectBuilder ob2(&builder, StaticStrings::Follower);
    for (auto const& [id, stat] : follower) {
      builder.add(VPackValue(id));
      stat.toVelocyPack(builder);
    }
  }
}

auto replicated_log::LeaderStatus::fromVelocyPack(velocypack::Slice slice) -> LeaderStatus {
  TRI_ASSERT(slice.get("role").isEqualString(StaticStrings::Leader));
  LeaderStatus status;
  status.term = LogTerm{slice.get(StaticStrings::Term).getNumericValue<std::size_t>()};
  status.local = LogStatistics::fromVelocyPack(slice.get("local"));
  for (auto [key, value] : VPackObjectIterator(slice.get(StaticStrings::Follower))) {
    auto id = ParticipantId{key.copyString()};
    auto stat = FollowerStatistics::fromVelocyPack(value);
    status.follower.emplace(id, stat);
  }
  return status;
}

void replicated_log::LeaderStatus::FollowerStatistics::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("commitIndex", VPackValue(commitIndex.value));
  builder.add(VPackValue(StaticStrings::Spearhead));
  spearHead.toVelocyPack(builder);
  builder.add("lastErrorReason", VPackValue(int(lastErrorReason)));
  builder.add("lastErrorReasonMessage", VPackValue(to_string(lastErrorReason)));
  builder.add("lastRequestLatencyMS", VPackValue(lastRequestLatencyMS));
}

auto replicated_log::LeaderStatus::FollowerStatistics::fromVelocyPack(velocypack::Slice slice) -> FollowerStatistics {
  FollowerStatistics stats;
  stats.commitIndex = LogIndex{slice.get("commitIndex").getNumericValue<size_t>()};
  stats.spearHead = TermIndexPair::fromVelocyPack(slice.get(StaticStrings::Spearhead));
  stats.lastErrorReason = AppendEntriesErrorReason{slice.get("lastErrorReason").getNumericValue<int>()};
  stats.lastRequestLatencyMS = slice.get("lastRequestLatencyMS").getDouble();
  return stats;
}

std::string arangodb::replication2::replicated_log::to_string(replicated_log::AppendEntriesErrorReason reason) {
  switch (reason) {
    case replicated_log::AppendEntriesErrorReason::NONE:
      return {};
    case replicated_log::AppendEntriesErrorReason::INVALID_LEADER_ID:
      return "leader id was invalid";
    case replicated_log::AppendEntriesErrorReason::LOST_LOG_CORE:
      return "term has changed and an internal state was lost";
    case replicated_log::AppendEntriesErrorReason::WRONG_TERM:
      return "current term is different from leader term";
    case replicated_log::AppendEntriesErrorReason::NO_PREV_LOG_MATCH:
      return "previous log index did not match";
    case AppendEntriesErrorReason::MESSAGE_OUTDATED:
      return "message was outdated";
    case AppendEntriesErrorReason::PERSISTENCE_FAILURE:
      return "persisting the log entries failed";
    case AppendEntriesErrorReason::COMMUNICATION_ERROR:
      return "communicating with participant failed - network error";
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

auto LogStatus::getCurrentTerm() const noexcept -> std::optional<LogTerm> {
  return std::visit(
      overload{[&](replicated_log::UnconfiguredStatus) -> std::optional<LogTerm> {
                 return std::nullopt;
               },
               [&](replicated_log::LeaderStatus const& s) -> std::optional<LogTerm> {
                 return s.term;
               },
               [&](replicated_log::FollowerStatus const& s) -> std::optional<LogTerm> {
                 return s.term;
               }},
      _variant);
}

auto LogStatus::getLocalStatistics() const noexcept
    -> std::optional<LogStatistics> {
  return std::visit(
      overload{[&](replicated_log::UnconfiguredStatus const& s) -> std::optional<LogStatistics> {
                 return std::nullopt;
               },
               [&](replicated_log::LeaderStatus const& s) -> std::optional<LogStatistics> {
                 return s.local;
               },
               [&](replicated_log::FollowerStatus const& s) -> std::optional<LogStatistics> {
                 return s.local;
               }},
      _variant);
}

auto LogStatus::fromVelocyPack(VPackSlice slice) -> LogStatus {
  auto role = slice.get("role");
  if (role.isEqualString(StaticStrings::Leader)) {
    return LogStatus{LeaderStatus::fromVelocyPack(slice)};
  } else if (role.isEqualString(StaticStrings::Follower)) {
    return LogStatus{FollowerStatus::fromVelocyPack(slice)};
  } else {
    return LogStatus{UnconfiguredStatus::fromVelocyPack(slice)};
  }
}
