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

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "TestHelper.h"

#include "Replication2/ReplicatedLog/Algorithms.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::algorithms;
using namespace arangodb::replication2::replicated_log;

struct CalcCommitIndexTest : ::testing::Test {};

TEST_F(CalcCommitIndexTest, write_concern_1_single_participant) {
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {}},
  };

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{1, 1, 1},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, LogIndex{50});
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  auto expectedQuorum = std::vector<ParticipantId>{"A"};
  std::sort(std::begin(expectedQuorum), std::end(expectedQuorum));
  std::sort(std::begin(quorum), std::end(quorum));

  EXPECT_EQ(quorum, expectedQuorum);
}

TEST_F(CalcCommitIndexTest, write_concern_2_3_participants) {
  auto participants = std::vector{ParticipantStateTuple{LogIndex{50}, "A", {}},
                                  ParticipantStateTuple{LogIndex{25}, "B", {}},
                                  ParticipantStateTuple{LogIndex{35}, "C", {}}};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, LogIndex{35});
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  auto expectedQuorum = std::vector<ParticipantId>{"A", "C"};
  std::sort(std::begin(expectedQuorum), std::end(expectedQuorum));
  std::sort(std::begin(quorum), std::end(quorum));

  EXPECT_EQ(quorum, expectedQuorum);
}

TEST_F(CalcCommitIndexTest, write_concern_3_3_participants) {
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {}},
      ParticipantStateTuple{LogIndex{25}, "B", {}},
      ParticipantStateTuple{LogIndex{35}, "C", {}},
  };

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{3, 3, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, LogIndex{25});
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  auto expectedQuorum = std::vector<ParticipantId>{"A", "C", "B"};
  std::sort(std::begin(expectedQuorum), std::end(expectedQuorum));
  std::sort(std::begin(quorum), std::end(quorum));

  EXPECT_EQ(quorum, expectedQuorum);
}

TEST_F(CalcCommitIndexTest, includes_less_quorum_size) {
  // Three participants but only two are included

  auto participants = std::vector{ParticipantStateTuple{LogIndex{50}, "A", {ParticipantFlag::Excluded}},
                                  ParticipantStateTuple{LogIndex{25}, "B", {}},
                                  ParticipantStateTuple{LogIndex{35}, "C", {}}};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{3, 3, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, LogIndex{1});
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  auto expectedQuorum = std::vector<ParticipantId>{};
  std::sort(std::begin(expectedQuorum), std::end(expectedQuorum));
  std::sort(std::begin(quorum), std::end(quorum));

  EXPECT_EQ(quorum, expectedQuorum);
}

TEST_F(CalcCommitIndexTest, excluded_and_forced) {
  // One participant is excluded *and* forced, which means we cannot make any progress

  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {}},
      ParticipantStateTuple{LogIndex{25}, "B", {ParticipantFlag::Excluded, ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{35}, "C", {}},
  };

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, LogIndex{1});
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::ForcedParticipantNotInQuorum>(reason.value));

  auto expectedQuorum = std::vector<ParticipantId>{};
  std::sort(std::begin(expectedQuorum), std::end(expectedQuorum));
  std::sort(std::begin(quorum), std::end(quorum));

  EXPECT_EQ(quorum, expectedQuorum);
}

TEST_F(CalcCommitIndexTest, all_excluded) {
  // all participants are excluded.
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {ParticipantFlag::Excluded}},
      ParticipantStateTuple{LogIndex{25}, "B", {ParticipantFlag::Excluded}},
      ParticipantStateTuple{LogIndex{35}, "C", {ParticipantFlag::Excluded}},
  };

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{3, 3, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, LogIndex{1});
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  auto expectedQuorum = std::vector<ParticipantId>{};
  std::sort(std::begin(expectedQuorum), std::end(expectedQuorum));
  std::sort(std::begin(quorum), std::end(quorum));

  EXPECT_EQ(quorum, expectedQuorum);
}

TEST_F(CalcCommitIndexTest, all_forced) {
  // all participants are excluded.
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{25}, "B", {ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{35}, "C", {ParticipantFlag::Forced}},
  };

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{3, 3, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, LogIndex{25});
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  auto expectedQuorum = std::vector<ParticipantId>{"A", "C", "B"};
  std::sort(std::begin(expectedQuorum), std::end(expectedQuorum));
  std::sort(std::begin(quorum), std::end(quorum));

  EXPECT_EQ(quorum, expectedQuorum);
}

TEST_F(CalcCommitIndexTest, not_enough_eligible) {
  // all participants are excluded.
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {}},
      ParticipantStateTuple{LogIndex{35}, "B", {}},
      ParticipantStateTuple{LogIndex{50}, "C", {ParticipantFlag::Excluded}},
      ParticipantStateTuple{LogIndex{35}, "D", {}},
      ParticipantStateTuple{LogIndex{15}, "E", {}},
  };

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 3, 5},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, LogIndex{35});
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  auto expectedQuorum = std::vector<ParticipantId>{"B", "A", "D"};
  std::sort(std::begin(expectedQuorum), std::end(expectedQuorum));
  std::sort(std::begin(quorum), std::end(quorum));

  EXPECT_EQ(quorum, expectedQuorum);
}

TEST_F(CalcCommitIndexTest, nothing_to_commit) {
  // all participants are excluded.
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{15}, "A", {}},
      ParticipantStateTuple{LogIndex{15}, "B", {}},
      ParticipantStateTuple{LogIndex{15}, "C", {ParticipantFlag::Excluded}},
      ParticipantStateTuple{LogIndex{15}, "D", {}},
      ParticipantStateTuple{LogIndex{15}, "E", {}},
  };

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 3, 5},
                                       LogIndex{15}, LogIndex{15});
  EXPECT_EQ(index, LogIndex{15});
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));
  auto expectedQuorum = std::vector<ParticipantId>{"D", "E", "A"};
  std::sort(std::begin(expectedQuorum), std::end(expectedQuorum));
  std::sort(std::begin(quorum), std::end(quorum));

  EXPECT_EQ(quorum, expectedQuorum);
}
