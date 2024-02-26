////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <variant>

#include "fmt/core.h"

#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"

#include "Inspection/VPack.h"
#include "velocypack/Parser.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;
using namespace arangodb::replication2::replicated_log;

struct ParticipantsHealthTest : ::testing::Test {};
TEST_F(ParticipantsHealthTest, test_empty_and_does_not_crash) {
  auto const health = ParticipantsHealth{};

  ASSERT_EQ(health.notIsFailed("A"), false);
  ASSERT_EQ(health.validRebootId("A", RebootId{0}), false);
  ASSERT_EQ(health.getRebootId("A"), std::nullopt);
  ASSERT_EQ(health.contains("A"), false);
  ASSERT_EQ(health.numberNotIsFailedOf(ParticipantsFlagsMap{}), 0U);
  ASSERT_EQ(health.numberNotIsFailedOf(ParticipantsFlagsMap{{"A", {}}}), 0U);
}

TEST_F(ParticipantsHealthTest, test_participants_health) {
  auto const& health = ParticipantsHealth{
      ._health = {
          {"A",
           ParticipantHealth{.rebootId = RebootId{42}, .notIsFailed = true}},
          {"B",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"C",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = true}},
          {"D",
           ParticipantHealth{.rebootId = RebootId{14}, .notIsFailed = false}}}};

  ASSERT_EQ(health.notIsFailed("A"), true);
  ASSERT_EQ(health.notIsFailed("E"), false);
  ASSERT_EQ(health.validRebootId("A", RebootId{0}), false);
  ASSERT_EQ(health.validRebootId("A", RebootId{42}), true);
  ASSERT_EQ(health.getRebootId("C"), RebootId{14});
  ASSERT_EQ(health.contains("A"), true);
  ASSERT_EQ(health.contains("F"), false);
  ASSERT_EQ(health.numberNotIsFailedOf(ParticipantsFlagsMap{}), 0U);
  ASSERT_EQ(
      health.numberNotIsFailedOf(ParticipantsFlagsMap{{"A", {}}, {"D", {}}}),
      1U);
}
