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

auto LogPlanTermSpecification::Leader::toVelocyPack(VPackBuilder& builder) const
    -> void {
  serialize(builder, *this);
}

auto LogPlanTermSpecification::toVelocyPack(VPackBuilder& builder) const
    -> void {
  serialize(builder, *this);
}

LogPlanTermSpecification::LogPlanTermSpecification(from_velocypack_t,
                                                   VPackSlice slice) {
  *this = deserialize<LogPlanTermSpecification>(slice);
}

LogPlanSpecification::LogPlanSpecification() = default;
auto LogPlanSpecification::toVelocyPack(VPackBuilder& builder) const -> void {
  serialize(builder, *this);
}

LogPlanSpecification::LogPlanSpecification(from_velocypack_t,
                                           VPackSlice slice) {
  *this = deserialize<LogPlanSpecification>(slice);
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
  *this = deserialize<LogCurrentLocalState>(slice);
}

LogCurrentLocalState::LogCurrentLocalState(LogTerm term,
                                           TermIndexPair spearhead) noexcept
    : term(term), spearhead(spearhead) {}

auto LogCurrentLocalState::toVelocyPack(VPackBuilder& builder) const -> void {
  serialize(builder, *this);
}

LogCurrent::LogCurrent(from_velocypack_t, VPackSlice slice) {
  *this = deserialize<LogCurrent>(slice);
}

LogCurrentSupervision::LogCurrentSupervision(from_velocypack_t,
                                             VPackSlice slice) {
  *this = deserialize<LogCurrentSupervision>(slice);
}

auto LogCurrentSupervision::toVelocyPack(VPackBuilder& builder) const -> void {
  serialize(builder, *this);
}

LogCurrentSupervisionElection::LogCurrentSupervisionElection(from_velocypack_t,
                                                             VPackSlice slice) {
  *this = deserialize<LogCurrentSupervisionElection>(slice);
}

auto LogCurrent::toVelocyPack(VPackBuilder& builder) const -> void {
  serialize(builder, *this);
}

auto LogCurrent::fromVelocyPack(VPackSlice s) -> LogCurrent {
  return LogCurrent(from_velocypack, s);
}

auto LogCurrentSupervisionElection::toVelocyPack(VPackBuilder& builder) const
    -> void {
  serialize(builder, *this);
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

auto agency::to_string(LogCurrentSupervisionError error) noexcept
    -> std::string_view {
  switch (error) {
    case LogCurrentSupervisionError::GENERAL_ERROR:
      return "generic error.";
    case LogCurrentSupervisionError::TARGET_LEADER_INVALID:
      return "the leader selected in target is invalid";
    case LogCurrentSupervisionError::TARGET_LEADER_EXCLUDED:
      return "the leader selected in target is excluded";
    case LogCurrentSupervisionError::TARGET_NOT_ENOUGH_PARTICIPANTS:
      return "not enough participants to create the log safely";
  }
  LOG_TOPIC("7eee2", FATAL, arangodb::Logger::REPLICATION2)
      << "Invalid LogCurrentSupervisionError "
      << static_cast<std::underlying_type_t<decltype(error)>>(error);
  FATAL_ERROR_ABORT();
}

auto LogCurrent::Leader::toVelocyPack(VPackBuilder& builder) const -> void {
  serialize(builder, *this);
}

auto LogCurrent::Leader::fromVelocyPack(VPackSlice s) -> Leader {
  return deserialize<Leader>(s);
}

auto LogTarget::fromVelocyPack(velocypack::Slice s) -> LogTarget {
  return LogTarget(from_velocypack_t{}, s);
}

void LogTarget::toVelocyPack(velocypack::Builder& builder) const {
  serialize(builder, *this);
}

LogTarget::LogTarget(from_velocypack_t, VPackSlice slice) {
  *this = deserialize<LogTarget>(slice);
}

LogTarget::LogTarget(LogId id, ParticipantsFlagsMap const& participants,
                     LogConfig const& config)
    : id{id}, participants{participants}, config(config) {}
