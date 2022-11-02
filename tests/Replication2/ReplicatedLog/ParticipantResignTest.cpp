////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "TestHelper.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

enum class ParticipantTesteeRole { Leader, Follower, UnconfiguredParticipant };

auto allRoles = {ParticipantTesteeRole::UnconfiguredParticipant,
                 ParticipantTesteeRole::Follower,
                 ParticipantTesteeRole::Leader};

auto to_string(ParticipantTesteeRole role) -> std::string {
  switch (role) {
    case ParticipantTesteeRole::Leader:
      return "ParticipantTesteeRole::Leader";
    case ParticipantTesteeRole::Follower:
      return "ParticipantTesteeRole::Follower";
    case ParticipantTesteeRole::UnconfiguredParticipant:
      return "ParticipantTesteeRole::UnconfiguredParticipant";
  }
  std::abort();
}

auto operator<<(std::ostream& ostream, ParticipantTesteeRole role)
    -> std::ostream& {
  return ostream << to_string(role);
}

struct ParticipantResignTest
    : ReplicatedLogTest,
      ::testing::WithParamInterface<ParticipantTesteeRole> {};

TEST_P(ParticipantResignTest, participant_resign) {
  auto log = makeReplicatedLog(LogId{1});

  switch (GetParam()) {
    case ParticipantTesteeRole::Leader:
      log->becomeLeader({}, LogTerm{1}, {}, 1);
      break;
    case ParticipantTesteeRole::Follower:
      log->becomeFollower({}, {}, {});
      break;
    case ParticipantTesteeRole::UnconfiguredParticipant:
      break;
  }

  auto participant = log->getParticipant();

  bool alpha = false;
  bool beta = false;

  {  // install first callback
    auto future = participant->waitForResign();

    std::move(future).thenFinal([&](auto) noexcept { alpha = true; });

    EXPECT_FALSE(alpha);
    // resign
    log.reset();
    EXPECT_TRUE(alpha);
  }

  {  // install second callback
    auto future = participant->waitForResign();

    EXPECT_FALSE(beta);
    std::move(future).thenFinal([&](auto) noexcept { beta = true; });
    EXPECT_TRUE(beta);
  }
}

INSTANTIATE_TEST_CASE_P(ParticipantResign, ParticipantResignTest,
                        testing::ValuesIn(allRoles));
