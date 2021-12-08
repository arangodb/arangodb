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

#include <Basics/debugging.h>
#include <Basics/StaticStrings.h>
#include <Basics/overload.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "LogStatus.h"

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;


void UnconfiguredStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue("unconfigured"));
}

auto UnconfiguredStatus::fromVelocyPack(velocypack::Slice slice) -> UnconfiguredStatus {
  TRI_ASSERT(slice.get("role").isEqualString("unconfigured"));
  return {};
}

void FollowerStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue(StaticStrings::Follower));
  if (leader.has_value()) {
    builder.add(StaticStrings::Leader, VPackValue(*leader));
  }
  builder.add(StaticStrings::Term, VPackValue(term.value));
  builder.add("largestCommonIndex", VPackValue(largestCommonIndex.value));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
}

auto FollowerStatus::fromVelocyPack(velocypack::Slice slice) -> FollowerStatus {
  TRI_ASSERT(slice.get("role").isEqualString(StaticStrings::Follower));
  FollowerStatus status;
  status.term = slice.get(StaticStrings::Term).extract<LogTerm>();
  status.largestCommonIndex = slice.get("largestCommonIndex").extract<LogIndex>();
  status.local = LogStatistics::fromVelocyPack(slice.get("local"));
  if (auto leader = slice.get(StaticStrings::Leader); !leader.isNone()) {
    status.leader = leader.copyString();
  }
  return status;
}

void LeaderStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue(StaticStrings::Leader));
  builder.add(StaticStrings::Term, VPackValue(term.value));
  builder.add("largestCommonIndex", VPackValue(largestCommonIndex.value));
  builder.add("commitLagMS", VPackValue(commitLagMS.count()));
  builder.add("leadershipEstablished", VPackValue(leadershipEstablished));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
  builder.add(VPackValue("lastCommitStatus"));
  lastCommitStatus.toVelocyPack(builder);
  {
    VPackObjectBuilder ob2(&builder, StaticStrings::Follower);
    for (auto const& [id, stat] : follower) {
      builder.add(VPackValue(id));
      stat.toVelocyPack(builder);
    }
  }
}

auto LeaderStatus::fromVelocyPack(velocypack::Slice slice) -> LeaderStatus {
  TRI_ASSERT(slice.get("role").isEqualString(StaticStrings::Leader));
  LeaderStatus status;
  status.term = slice.get(StaticStrings::Term).extract<LogTerm>();
  status.local = LogStatistics::fromVelocyPack(slice.get("local"));
  status.largestCommonIndex = slice.get("largestCommonIndex").extract<LogIndex>();
  status.leadershipEstablished = slice.get("leadershipEstablished").isTrue();
  status.commitLagMS = std::chrono::duration<double, std::milli>{
      slice.get("commitLagMS").extract<double>()};
  status.lastCommitStatus =
      CommitFailReason::fromVelocyPack(slice.get("lastCommitStatus"));
  for (auto [key, value] : VPackObjectIterator(slice.get(StaticStrings::Follower))) {
    auto id = ParticipantId{key.copyString()};
    auto stat = FollowerStatistics::fromVelocyPack(value);
    status.follower.emplace(std::move(id), stat);
  }
  return status;
}

auto replicated_log::operator==(LeaderStatus const& left,
                                LeaderStatus const& right) -> bool {
  bool result = left.local == right.local &&
                left.term == right.term &&
                left.largestCommonIndex == right.largestCommonIndex &&
                left.commitLagMS == right.commitLagMS &&
                left.lastCommitStatus == right.lastCommitStatus
                && left.follower.size() == right.follower.size();
  if (!result) {
    return false;
  }
  for (auto const& [participantId, followerStatistics] : left.follower) {
    auto search = right.follower.find(participantId);
    if (search == right.follower.end() || !(search->second == followerStatistics)) {
      result = false;
      break;
    }
  }
  return result;
}

auto replicated_log::operator!=(LeaderStatus const& left,
                                LeaderStatus const& right) -> bool {
  return !(left == right);
}

void FollowerStatistics::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::CommitIndex, VPackValue(commitIndex.value));
  builder.add(VPackValue(StaticStrings::Spearhead));
  spearHead.toVelocyPack(builder);
  builder.add(VPackValue("lastErrorReason"));
  lastErrorReason.toVelocyPack(builder);
  builder.add("lastRequestLatencyMS", VPackValue(lastRequestLatencyMS.count()));
  builder.add(VPackValue("state"));
  internalState.toVelocyPack(builder);
}

auto FollowerStatistics::fromVelocyPack(velocypack::Slice slice) -> FollowerStatistics {
  FollowerStatistics stats;
  stats.commitIndex = slice.get(StaticStrings::CommitIndex).extract<LogIndex>();
  stats.spearHead = TermIndexPair::fromVelocyPack(slice.get(StaticStrings::Spearhead));
  stats.lastErrorReason = AppendEntriesErrorReason::fromVelocyPack(slice.get("lastErrorReason"));
  stats.lastRequestLatencyMS = std::chrono::duration<double, std::milli>{
      slice.get("lastRequestLatencyMS").getDouble()};
  stats.internalState = FollowerState::fromVelocyPack(slice.get("state"));
  return stats;
}

auto replicated_log::operator==(FollowerStatistics const& left,
                FollowerStatistics const& right) noexcept -> bool {
  return left.lastErrorReason == right.lastErrorReason &&
      left.lastRequestLatencyMS == right.lastRequestLatencyMS &&
      left.internalState.value.index() == right.internalState.value.index();
}

auto replicated_log::operator!=(FollowerStatistics const& left,
                FollowerStatistics const& right) noexcept -> bool {
  return !(left == right);
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

auto LogStatus::getLocalStatistics() const noexcept -> std::optional<LogStatistics> {
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

LogStatus::LogStatus(UnconfiguredStatus status) noexcept : _variant(status) {}
LogStatus::LogStatus(LeaderStatus status) noexcept
    : _variant(std::move(status)) {}
LogStatus::LogStatus(FollowerStatus status) noexcept
    : _variant(std::move(status)) {}

auto LogStatus::getVariant() const noexcept -> VariantType const& {
  return _variant;
}

auto LogStatus::toVelocyPack(velocypack::Builder& builder) const -> void {
  std::visit([&](auto const& s) { s.toVelocyPack(builder); }, _variant);
}
