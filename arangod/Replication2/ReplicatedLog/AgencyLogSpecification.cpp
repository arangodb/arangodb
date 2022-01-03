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

#include "AgencyLogSpecification.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <type_traits>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;

auto LogPlanTermSpecification::toVelocyPack(VPackBuilder& builder) const
    -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Term, VPackValue(term.value));
  {
    VPackObjectBuilder ob2(&builder, StaticStrings::Participants);
    for (auto const& [p, l] : participants) {
      builder.add(p, VPackSlice::emptyObjectSlice());
    }
  }

  builder.add(VPackValue(StaticStrings::Config));
  config.toVelocyPack(builder);

  if (leader.has_value()) {
    VPackObjectBuilder ob2(&builder, StaticStrings::Leader);
    builder.add(StaticStrings::ServerId, VPackValue(leader->serverId));
    builder.add(StaticStrings::RebootId, VPackValue(leader->rebootId.value()));
  }
}

LogPlanTermSpecification::LogPlanTermSpecification(from_velocypack_t,
                                                   VPackSlice slice)
    : term(slice.get(StaticStrings::Term).extract<LogTerm>()),
      config(slice.get(StaticStrings::Config)) {
  for (auto const& [key, value] :
       VPackObjectIterator(slice.get(StaticStrings::Participants))) {
    TRI_ASSERT(value.isEmptyObject());
    participants.emplace(ParticipantId{key.copyString()}, Participant{});
  }
  if (auto leaders = slice.get(StaticStrings::Leader); !leaders.isNone()) {
    leader = Leader{leaders.get(StaticStrings::ServerId).copyString(),
                    leaders.get(StaticStrings::RebootId).extract<RebootId>()};
  }
}

auto LogPlanSpecification::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Id, VPackValue(id.id()));
  builder.add(VPackValue(StaticStrings::TargetConfig));
  targetConfig.toVelocyPack(builder);
  if (currentTerm.has_value()) {
    builder.add(VPackValue(StaticStrings::CurrentTerm));
    currentTerm->toVelocyPack(builder);
  }
}

LogPlanSpecification::LogPlanSpecification(from_velocypack_t, VPackSlice slice)
    : id(slice.get(StaticStrings::Id).extract<LogId>()),
      targetConfig(slice.get(StaticStrings::TargetConfig)) {
  if (auto term = slice.get(StaticStrings::CurrentTerm); !term.isNone()) {
    currentTerm = LogPlanTermSpecification{from_velocypack, term};
  }
}

LogPlanTermSpecification::LogPlanTermSpecification(
    LogTerm term, LogConfig config, std::optional<Leader> leader,
    std::unordered_map<ParticipantId, Participant> participants)
    : term(term),
      config(config),
      leader(std::move(leader)),
      participants(std::move(participants)) {}

LogPlanSpecification::LogPlanSpecification(
    LogId id, std::optional<LogPlanTermSpecification> term, LogConfig config)
    : id(id), currentTerm(std::move(term)), targetConfig(config) {}

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
  for (auto const& [key, value] :
       VPackObjectIterator(slice.get(StaticStrings::LocalStatus))) {
    localState.emplace(ParticipantId{key.copyString()},
                       LogCurrentLocalState(from_velocypack, value));
  }
  if (auto ss = slice.get("supervision"); !ss.isNone()) {
    supervision = LogCurrentSupervision{from_velocypack, ss};
  }
}

LogCurrentSupervision::LogCurrentSupervision(from_velocypack_t,
                                             VPackSlice slice) {
  if (auto es = slice.get("election"); !es.isNone()) {
    election = LogCurrentSupervisionElection{from_velocypack, es};
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
}

auto LogCurrentSupervision::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  if (election.has_value()) {
    builder.add(VPackValue("election"));
    election->toVelocyPack(builder);
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
