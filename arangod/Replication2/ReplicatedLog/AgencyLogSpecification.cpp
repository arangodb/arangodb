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

#include "AgencyLogSpecification.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <velocypack/Iterator.h>

#include <type_traits>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;

namespace {
auto constexpr StringCommittedParticipantsConfig =
    std::string_view{"committedParticipantsConfig"};
}

auto LogPlanTermSpecification::Leader::toVelocyPack(VPackBuilder& builder) const
    -> void {
  VPackObjectBuilder ob2(&builder);
  builder.add(StaticStrings::ServerId, VPackValue(serverId));
  builder.add(StaticStrings::RebootId, VPackValue(rebootId.value()));
}

LogPlanTermSpecification::Leader::Leader(from_velocypack_t, VPackSlice slice)
    : serverId(slice.get(StaticStrings::ServerId).copyString()),
      rebootId(slice.get(StaticStrings::RebootId).extract<RebootId>()) {}

auto LogPlanTermSpecification::toVelocyPack(VPackBuilder& builder) const
    -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Term, VPackValue(term.value));

  builder.add(VPackValue(StaticStrings::Config));
  config.toVelocyPack(builder);

  if (leader.has_value()) {
    builder.add(VPackValue(StaticStrings::Leader));
    leader->toVelocyPack(builder);
  }
}

LogPlanTermSpecification::LogPlanTermSpecification(from_velocypack_t,
                                                   VPackSlice slice)
    : term(slice.get(StaticStrings::Term).extract<LogTerm>()),
      config(slice.get(StaticStrings::Config)) {
  // Participants were moved to LogPlanSpecification. This assertion can be
  // removed after the transition is complete.
  TRI_ASSERT(slice.get(StaticStrings::Participants).isNone());
  if (auto leaders = slice.get(StaticStrings::Leader); !leaders.isNone()) {
    leader = Leader(from_velocypack_t{}, leaders);
  }
}

LogPlanSpecification::LogPlanSpecification() = default;
auto LogPlanSpecification::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Id, VPackValue(id.id()));
  if (currentTerm.has_value()) {
    builder.add(VPackValue(StaticStrings::CurrentTerm));
    currentTerm->toVelocyPack(builder);
  }
  builder.add(VPackValue("participantsConfig"));
  participantsConfig.toVelocyPack(builder);
}

LogPlanSpecification::LogPlanSpecification(from_velocypack_t, VPackSlice slice)
    : id(slice.get(StaticStrings::Id).extract<LogId>()) {
  if (auto term = slice.get(StaticStrings::CurrentTerm); !term.isNone()) {
    currentTerm = LogPlanTermSpecification{from_velocypack, term};
  }

  if (auto partConfig = slice.get("participantsConfig"); !partConfig.isNone()) {
    participantsConfig = ParticipantsConfig::fromVelocyPack(partConfig);
  }
}

LogPlanTermSpecification::LogPlanTermSpecification(LogTerm term,
                                                   LogConfig config,
                                                   std::optional<Leader> leader)
    : term(term), config(config), leader(std::move(leader)) {}

LogPlanSpecification::LogPlanSpecification(
    LogId id, std::optional<LogPlanTermSpecification> term)
    : id(id), currentTerm(std::move(term)) {}

LogPlanSpecification::LogPlanSpecification(
    LogId id, std::optional<LogPlanTermSpecification> term,
    ParticipantsConfig participantsConfig)
    : id(id),
      currentTerm(std::move(term)),
      participantsConfig(std::move(participantsConfig)) {}

auto LogPlanSpecification::fromVelocyPack(velocypack::Slice slice)
    -> LogPlanSpecification {
  return LogPlanSpecification(from_velocypack, slice);
}

LogCurrentLocalState::LogCurrentLocalState(from_velocypack_t,
                                           VPackSlice slice) {
  auto spearheadSlice = slice.get(StaticStrings::Spearhead);
  spearhead.term = spearheadSlice.get(StaticStrings::Term).extract<LogTerm>();
  spearhead.index =
      spearheadSlice.get(StaticStrings::Index).extract<LogIndex>();
  term = slice.get(StaticStrings::Term).extract<LogTerm>();
}

LogCurrentLocalState::LogCurrentLocalState(LogTerm term,
                                           TermIndexPair spearhead) noexcept
    : term(term), spearhead(spearhead) {}

auto LogCurrentLocalState::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Term, VPackValue(term.value));
  builder.add(VPackValue(StaticStrings::Spearhead));
  spearhead.toVelocyPack(builder);
}

LogCurrent::LogCurrent(from_velocypack_t, VPackSlice slice) {
  if (auto ls = slice.get(StaticStrings::LocalStatus); !ls.isNone()) {
    for (auto const& [key, value] : VPackObjectIterator(ls)) {
      localState.emplace(ParticipantId{key.copyString()},
                         LogCurrentLocalState(from_velocypack, value));
    }
  }
  if (auto ss = slice.get("supervision"); !ss.isNone()) {
    supervision = LogCurrentSupervision{from_velocypack, ss};
  }
  if (auto ls = slice.get("leader"); !ls.isNone()) {
    leader = Leader::fromVelocyPack(ls);
  }
}

LogCurrentSupervision::LogCurrentSupervision(from_velocypack_t,
                                             VPackSlice slice) {
  if (auto es = slice.get("election"); !es.isNone()) {
    election = LogCurrentSupervisionElection{from_velocypack, es};
  }
  if (auto es = slice.get("error"); !es.isNone()) {
    if (es.isObject()) {
      error = es.get("code").getNumericValue<LogCurrentSupervisionError>();
    }
  }
  if (auto es = slice.get("statusMessage"); !es.isNone()) {
    statusMessage = es.copyString();
  }
}

LogCurrentSupervisionElection::LogCurrentSupervisionElection(from_velocypack_t,
                                                             VPackSlice slice)
    : term(slice.get(StaticStrings::Term).extract<LogTerm>()),
      participantsRequired(
          slice.get("participantsRequired").getNumericValue<std::size_t>()),
      participantsAvailable(
          slice.get("participantsAvailable").getNumericValue<std::size_t>()) {
  for (auto [key, value] : VPackObjectIterator(slice.get("details"))) {
    detail.emplace(key.copyString(),
                   value.get("code").getNumericValue<ErrorCode>());
  }
}

auto LogCurrent::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  VPackObjectBuilder localStatusBuilder(&builder, StaticStrings::LocalStatus);
  for (auto const& [key, value] : localState) {
    builder.add(VPackValue(key));
    value.toVelocyPack(builder);
  }
  if (supervision.has_value()) {
    builder.add(VPackValue("supervision"));
    supervision->toVelocyPack(builder);
  }
  if (leader.has_value()) {
    builder.add(VPackValue("leader"));
    leader->toVelocyPack(builder);
  }
}

auto LogCurrent::fromVelocyPack(VPackSlice s) -> LogCurrent {
  return LogCurrent(from_velocypack, s);
}

auto LogCurrentSupervision::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  if (election) {
    builder.add(VPackValue("election"));
    election->toVelocyPack(builder);
  }
  if (error) {
    builder.add(VPackValue("error"));
    ::toVelocyPack(*error, builder);
  }
  if (statusMessage) {
    builder.add("statusMessage", statusMessage.value());
  }
}

auto LogCurrentSupervisionElection::toVelocyPack(VPackBuilder& builder) const
    -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Term, VPackValue(term.value));

  builder.add("participantsRequired", VPackValue(participantsRequired));
  builder.add("participantsAvailable", VPackValue(participantsAvailable));
  {
    VPackObjectBuilder db(&builder, "details");
    for (auto const& [server, error] : detail) {
      builder.add(VPackValue(server));
      ::toVelocyPack(error, builder);
    }
  }
}

auto agency::toVelocyPack(LogCurrentSupervisionElection::ErrorCode ec,
                          VPackBuilder& builder) -> void {
  VPackObjectBuilder ob(&builder);
  builder.add("code", VPackValue(static_cast<int>(ec)));
  builder.add("message", VPackValue(to_string(ec)));
}

auto agency::to_string(LogCurrentSupervisionElection::ErrorCode ec) noexcept
    -> std::string_view {
  switch (ec) {
    case LogCurrentSupervisionElection::ErrorCode::OK:
      return "the server is ok";
    case LogCurrentSupervisionElection::ErrorCode::SERVER_NOT_GOOD:
      return "the server is not reported as good in Supervision/Health";
    case LogCurrentSupervisionElection::ErrorCode::TERM_NOT_CONFIRMED:
      return "the server has not (yet) confirmed the current term";
    case LogCurrentSupervisionElection::ErrorCode::SERVER_EXCLUDED:
      return "the server is configured as excluded";
  }
  LOG_TOPIC("7e572", FATAL, arangodb::Logger::REPLICATION2)
      << "Invalid LogCurrentSupervisionElection::ErrorCode "
      << static_cast<std::underlying_type_t<decltype(ec)>>(ec);
  FATAL_ERROR_ABORT();
}

auto agency::operator==(const LogCurrentSupervisionElection& left,
                        const LogCurrentSupervisionElection& right) noexcept
    -> bool {
  return left.term == right.term &&
         left.participantsAvailable == right.participantsAvailable &&
         left.participantsRequired == right.participantsRequired &&
         left.detail == right.detail;
}

auto agency::toVelocyPack(LogCurrentSupervisionError error,
                          VPackBuilder& builder) -> void {
  VPackObjectBuilder ob(&builder);
  builder.add("code", VPackValue(static_cast<int>(error)));
  builder.add("message", VPackValue(to_string(error)));
}

auto agency::to_string(LogCurrentSupervisionError error) noexcept
    -> std::string_view {
  switch (error) {
    case LogCurrentSupervisionError::GENERAL_ERROR:
      return "generic error.";
    case LogCurrentSupervisionError::TARGET_LEADER_INVALID:
      return "the leader selected in target is invalid";
    case LogCurrentSupervisionError::TARGET_LEADER_EXCLUDED:
      return "the leader selected in target is excluded";
  }
  LOG_TOPIC("7eee2", FATAL, arangodb::Logger::REPLICATION2)
      << "Invalid LogCurrentSupervisionError "
      << static_cast<std::underlying_type_t<decltype(error)>>(error);
  FATAL_ERROR_ABORT();
}

auto LogCurrent::Leader::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Term, VPackValue(term));
  builder.add(StaticStrings::ServerId, VPackValue(serverId));
  if (committedParticipantsConfig) {
    builder.add(VPackValue(StringCommittedParticipantsConfig));
    committedParticipantsConfig->toVelocyPack(builder);
  }
  builder.add("leadershipEstablished", VPackValue(leadershipEstablished));
  if (commitStatus) {
    builder.add(VPackValue("commitStatus"));
    commitStatus->toVelocyPack(builder);
  }
}

auto LogCurrent::Leader::fromVelocyPack(VPackSlice s) -> Leader {
  auto leader = LogCurrent::Leader{};
  leader.term = s.get(StaticStrings::Term).extract<LogTerm>();
  leader.serverId = s.get(StaticStrings::ServerId).copyString();
  leader.leadershipEstablished = s.get("leadershipEstablished").isTrue();
  if (auto commitStatusSlice = s.get("commitStatus");
      !commitStatusSlice.isNone()) {
    leader.commitStatus =
        replicated_log::CommitFailReason::fromVelocyPack(commitStatusSlice);
  }
  if (auto configSlice = s.get(StringCommittedParticipantsConfig);
      !configSlice.isNone()) {
    leader.committedParticipantsConfig =
        ParticipantsConfig::fromVelocyPack(configSlice);
  }
  return leader;
}

auto LogTarget::fromVelocyPack(velocypack::Slice s) -> LogTarget {
  return LogTarget(from_velocypack_t{}, s);
}

void LogTarget::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);

  builder.add(StaticStrings::Id, VPackValue(id));

  builder.add(VPackValue(StaticStrings::Config));

  config.toVelocyPack(builder);

  if (leader.has_value()) {
    builder.add(StaticStrings::Leader, VPackValue(leader.value()));
  }

  builder.add(VPackValue(StaticStrings::Participants));
  {
    velocypack::ObjectBuilder pb(&builder);
    for (auto const& [pid, flags] : participants) {
      builder.add(VPackValue(pid));
      flags.toVelocyPack(builder);
    }
  }

  builder.add(VPackValue("properties"));
  properties.toVelocyPack(builder);

  if (supervision.has_value()) {
    builder.add(VPackValue("supervision"));
    supervision->toVelocyPack(builder);
  }
}

void LogTarget::Properties::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
}

auto LogTarget::Properties::fromVelocyPack(velocypack::Slice s)
    -> LogTarget::Properties {
  return {};
}

auto LogTarget::Supervision::fromVelocyPack(velocypack::Slice s)
    -> Supervision {
  Supervision result;
  if (auto slice = s.get("maxActionsTraceLength"); !slice.isNone()) {
    result.maxActionsTraceLength = slice.extract<std::size_t>();
  }
  return result;
}

auto LogTarget::Supervision::toVelocyPack(velocypack::Builder& b) const
    -> void {
  velocypack::ObjectBuilder ob(&b);
  b.add("maxActionsTraceLength", velocypack::Value(maxActionsTraceLength));
}

LogTarget::LogTarget(from_velocypack_t, VPackSlice slice) {
  id = slice.get(StaticStrings::Id).extract<LogId>();

  config = LogConfig(slice.get(StaticStrings::Config));

  if (auto leaderSlice = slice.get(StaticStrings::Leader);
      leaderSlice.isString()) {
    leader = leaderSlice.copyString();
  }

  if (auto participantsSlice = slice.get("participants");
      participantsSlice.isObject()) {
    for (auto const& [pid, flags] :
         velocypack::ObjectIterator(participantsSlice)) {
      participants.emplace(pid.copyString(),
                           ParticipantFlags::fromVelocyPack(flags));
    }
  }

  if (auto propSlice = slice.get("properties"); !propSlice.isNone()) {
    properties = Properties::fromVelocyPack(propSlice);
  }

  if (auto supSlice = slice.get("supervision"); !supSlice.isNone()) {
    supervision = Supervision::fromVelocyPack(supSlice);
  }
}

LogTarget::LogTarget(LogId id, ParticipantsFlagsMap const& participants,
                     LogConfig const& config)
    : id{id}, participants{participants}, config(config) {}
