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

#include "Replication2/ReplicatedLog/Algorithms.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::algorithms;

struct CheckLogsAlgorithmTest : ::testing::Test {

  auto makePlanSpecification(LogId id) -> agency::LogPlanSpecification {
    auto spec = agency::LogPlanSpecification{};
    spec.id = id;
    spec.targetConfig.writeConcern = 1;
    spec.targetConfig.waitForSync = false;
    return spec;
  }

  auto makeTermSpecification(LogTerm term, agency::LogPlanConfig const& config,
                             ParticipantInfo const& info, std::optional<std::string> leader)
      -> agency::LogPlanTermSpecification {
    auto termSpec = agency::LogPlanTermSpecification{};
    termSpec.term = term;
    termSpec.config = config;
    std::transform(info.begin(), info.end(),
                   std::inserter(termSpec.participants, termSpec.participants.end()),
                   [](auto const& info) {
                     return std::make_pair(info.first,
                                           agency::LogPlanTermSpecification::Participant{});
                   });
    if (leader) {
      termSpec.leader =
          agency::LogPlanTermSpecification::Leader{*leader, info.at(*leader).rebootId};
    }

    return termSpec;
  }

  auto makeLogCurrent() -> agency::LogCurrent {
    auto current = agency::LogCurrent{};
    return current;
  }

};

TEST_F(CheckLogsAlgorithmTest, check_do_nothing_if_all_good) {


  auto const participants = ParticipantInfo{
      {"A", ParticipantRecord{RebootId{1}, true}},
      {"B", ParticipantRecord{RebootId{1}, true}},
      {"C", ParticipantRecord{RebootId{1}, true}},
  };

  auto spec = makePlanSpecification(LogId{1});
  spec.currentTerm = makeTermSpecification(LogTerm{1}, {}, participants, "A");

  auto current = makeLogCurrent();


  auto result = checkReplicatedLog("db", spec, current, participants);




}
