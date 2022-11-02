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
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"

#include <velocypack/Iterator.h>

#include <type_traits>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;

LogPlanConfig::LogPlanConfig(std::size_t effectiveWriteConcern,
                             bool waitForSync) noexcept
    : effectiveWriteConcern(effectiveWriteConcern), waitForSync(waitForSync) {}

LogPlanTermSpecification::LogPlanTermSpecification(LogTerm term,
                                                   std::optional<Leader> leader)
    : term(term), leader(std::move(leader)) {}

LogPlanSpecification::LogPlanSpecification(
    LogId id, std::optional<LogPlanTermSpecification> term)
    : id(id), currentTerm(std::move(term)) {}

LogPlanSpecification::LogPlanSpecification(
    LogId id, std::optional<LogPlanTermSpecification> term,
    ParticipantsConfig participantsConfig)
    : id(id),
      currentTerm(std::move(term)),
      participantsConfig(std::move(participantsConfig)) {}

LogCurrentLocalState::LogCurrentLocalState(LogTerm term,
                                           TermIndexPair spearhead) noexcept
    : term(term), spearhead(spearhead) {}

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

LogTargetConfig::LogTargetConfig(std::size_t writeConcern,
                                 std::size_t softWriteConcern,
                                 bool waitForSync) noexcept
    : writeConcern(writeConcern),
      softWriteConcern(softWriteConcern),
      waitForSync(waitForSync) {}

LogTarget::LogTarget(LogId id, ParticipantsFlagsMap const& participants,
                     LogTargetConfig const& config)
    : id{id}, participants{participants}, config(config) {}
