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

#include <gtest/gtest.h>

#include <utility>

#include "Replication2/ReplicatedLog/Algorithms.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::algorithms;

struct CheckLogsAlgorithmTest : ::testing::Test {
  static auto makePlanSpecification(LogId id, ParticipantInfo const& info)
      -> agency::LogPlanSpecification {
    auto spec = agency::LogPlanSpecification{};
    spec.id = id;
    std::transform(info.begin(), info.end(),
                   std::inserter(spec.participantsConfig.participants,
                                 spec.participantsConfig.participants.end()),
                   [](auto const& info) {
                     return std::make_pair(info.first, ParticipantFlags{});
                   });
    return spec;
  }

  static auto makeLeader(ParticipantId leader, RebootId rebootId)
      -> agency::LogPlanTermSpecification::Leader {
    return {std::move(leader), rebootId};
  }

  static auto makeTermSpecification(LogTerm term,
                                    agency::LogPlanConfig const& config)
      -> agency::LogPlanTermSpecification {
    auto termSpec = agency::LogPlanTermSpecification{};
    termSpec.term = term;
    return termSpec;
  }

  static auto makeLogCurrent() -> agency::LogCurrent {
    auto current = agency::LogCurrent{};
    return current;
  }

  static auto makeLogCurrentReportAll(ParticipantInfo const& info, LogTerm term,
                                      LogIndex spearhead,
                                      LogTerm spearheadTerm) {
    auto current = agency::LogCurrent{};
    std::transform(info.begin(), info.end(),
                   std::inserter(current.localState, current.localState.end()),
                   [&](auto const& info) {
                     auto state = agency::LogCurrentLocalState{
                         term, TermIndexPair{spearheadTerm, spearhead}};
                     return std::make_pair(info.first, state);
                   });
    return current;
  }
};
