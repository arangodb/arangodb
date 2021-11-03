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
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "TestHelper.h"

#include "Replication2/ReplicatedLog/Algorithms.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::algorithms;
using namespace arangodb::replication2::replicated_log;

struct CalcCommitIndexTest : ::testing::Test {

  auto verifyQuorum(std::vector<ParticipantStateTuple> const& participants, std::vector<ParticipantId> const& quorum, LogIndex const& expectedLogIndex) {

    // every member of the quorum needs to have at least the expectedLogIndex,
    // must not be failed or excluded.
    for(auto const& pid : quorum) {
      auto p = std::find_if(std::begin(participants), std::end(participants), [pid](auto& pst) { return pst.id == pid; });
      ASSERT_NE(p, std::end(participants));
      EXPECT_GE(p->index, expectedLogIndex);
      EXPECT_FALSE(p->isFailed());
      EXPECT_FALSE(p->isExcluded());
    }

    // Check that every forced participant is part of the quorum.
    // At the moment we don't care that we're not including all
    // forced participants in the quorum returned by
    // calculateCommitIndex, so this test is disabled
    /*
     for(auto const& part : participants) {
      if(part.isForced() and !(part.isExcluded() or part.isFailed())) {
        auto p = std::find(std::begin(quorum), std::end(quorum), part.id);
        EXPECT_NE(p, std::end(quorum));
      }
    }
    */
  }
};

TEST_F(CalcCommitIndexTest, write_concern_1_single_participant) {
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {}},
  };
  auto expectedLogIndex = LogIndex{50};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{1, 1, 1},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_2_3_participants) {
  auto participants = std::vector{ParticipantStateTuple{LogIndex{50}, "A", {}},
                                  ParticipantStateTuple{LogIndex{25}, "B", {}},
                                  ParticipantStateTuple{LogIndex{35}, "C", {}}};

  auto expectedLogIndex = LogIndex{35};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});


  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  EXPECT_EQ(quorum.size(), 2);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_0_3_participants) {
  auto participants = std::vector{ParticipantStateTuple{LogIndex{50}, "A", {}},
                                  ParticipantStateTuple{LogIndex{25}, "B", {}},
                                  ParticipantStateTuple{LogIndex{35}, "C", {}}};
  auto expectedLogIndex = LogIndex{50};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{0, 0, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_3_3_participants) {
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {}},
      ParticipantStateTuple{LogIndex{25}, "B", {}},
      ParticipantStateTuple{LogIndex{35}, "C", {}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{3, 3, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, includes_less_quorum_size) {
  // Three participants but only two are included

  auto participants = std::vector{ParticipantStateTuple{LogIndex{50}, "A", {ParticipantFlag::Excluded}},
                                  ParticipantStateTuple{LogIndex{25}, "B", {}},
                                  ParticipantStateTuple{LogIndex{35}, "C", {}}};
  auto expectedLogIndex = LogIndex{1};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{3, 3, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
 }

TEST_F(CalcCommitIndexTest, excluded_and_forced) {
  // One participant is excluded *and* forced, which means we cannot make any progress
  // beyond LogIndex{25}
  // (Note that participants "A" and "C" can still form a quorum for LogIndex{25}!)

  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {}},
      ParticipantStateTuple{LogIndex{25}, "B", {ParticipantFlag::Excluded, ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{35}, "C", {}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::ForcedParticipantNotInQuorum>(reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, all_excluded) {
  // all participants are excluded.
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {ParticipantFlag::Excluded}},
      ParticipantStateTuple{LogIndex{25}, "B", {ParticipantFlag::Excluded}},
      ParticipantStateTuple{LogIndex{35}, "C", {ParticipantFlag::Excluded}},
  };
  auto expectedLogIndex = LogIndex{1};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{3, 3, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, all_forced) {
  // all participants are forced.
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{25}, "B", {ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{35}, "C", {ParticipantFlag::Forced}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{3, 3, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, not_enough_eligible) {
  // Cannot reach quorum size, as participant "C" with
  // LogIndex{50} is excluded
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {}},
      ParticipantStateTuple{LogIndex{35}, "B", {}},
      ParticipantStateTuple{LogIndex{50}, "C", {ParticipantFlag::Excluded}},
      ParticipantStateTuple{LogIndex{35}, "D", {}},
      ParticipantStateTuple{LogIndex{15}, "E", {}},
  };
  auto expectedLogIndex = LogIndex{35};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 3, 5},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, nothing_to_commit) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{15}, "A", {}},
      ParticipantStateTuple{LogIndex{15}, "B", {}},
      ParticipantStateTuple{LogIndex{15}, "C", {ParticipantFlag::Excluded}},
      ParticipantStateTuple{LogIndex{15}, "D", {}},
      ParticipantStateTuple{LogIndex{15}, "E", {}},
  };
  auto expectedLogIndex = LogIndex{15};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 3, 5},
                                       LogIndex{15}, LogIndex{15});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, failed_participant) {
  // One participant has failed, which means we cannot make any progress
  // beyond LogIndex{25}

  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {ParticipantFlag::Failed}},
      ParticipantStateTuple{LogIndex{25}, "B", {}},
      ParticipantStateTuple{LogIndex{35}, "C", {}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
    std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(reason.value));

  EXPECT_EQ(quorum.size(), 2);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, failed_and_forced) {
  // One participant is failed *and* forced, which means we cannot make any progress
  // beyond LogIndex{25}
  // (Note that participants "A" and "C" can still form a quorum for LogIndex{25}!)

  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{50}, "A", {}},
      ParticipantStateTuple{LogIndex{25}, "B", {ParticipantFlag::Failed, ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{35}, "C", {}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::ForcedParticipantNotInQuorum>(reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, failed_with_soft_write_concern) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{55}, "A", {ParticipantFlag::Failed}},
      ParticipantStateTuple{LogIndex{15}, "B", {}},
      ParticipantStateTuple{LogIndex{25}, "C", {ParticipantFlag::Failed}},
      ParticipantStateTuple{LogIndex{5}, "D", {}},
      ParticipantStateTuple{LogIndex{17}, "E", {}},
  };
  auto expectedLogIndex = LogIndex{5};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 4, 5},
                                       LogIndex{15}, LogIndex{15});
  EXPECT_EQ(index, expectedLogIndex);

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, smallest_failed) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{55}, "A", {ParticipantFlag::Failed}},
      ParticipantStateTuple{LogIndex{15}, "B", {}},
      ParticipantStateTuple{LogIndex{25}, "C", {}},
      ParticipantStateTuple{LogIndex{5}, "D", {ParticipantFlag::Failed}},
      ParticipantStateTuple{LogIndex{17}, "E", {}},
  };
  auto expectedLogIndex = LogIndex{15};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 4, 5},
                                       LogIndex{15}, LogIndex{55});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, nothing_to_commit_failed) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{55}, "A", {ParticipantFlag::Failed}},
      ParticipantStateTuple{LogIndex{15}, "B", {}},
      ParticipantStateTuple{LogIndex{25}, "C", {}},
      ParticipantStateTuple{LogIndex{5}, "D", {ParticipantFlag::Failed}},
      ParticipantStateTuple{LogIndex{17}, "E", {}},
  };
  auto expectedLogIndex = LogIndex{15};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 4, 5},
                                       LogIndex{15}, LogIndex{15});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_0_forced_flag) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{25}, "A", {ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{15}, "B", {}},
      ParticipantStateTuple{LogIndex{55}, "C", {}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{0, 0, 3},
                                       LogIndex{15}, LogIndex{55});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, DISABLED_more_forced_than_quorum_size) {
  // There are more forced participants than writeConcern
  //
  // At the moment we don't care that we're not including all
  // forced participants in the quorum returned by
  // calculateCommitIndex, so this test is disabled
  auto participants = std::vector{
      ParticipantStateTuple{LogIndex{25}, "A", {ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{25}, "B", {ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{25}, "C", {}},
      ParticipantStateTuple{LogIndex{25}, "D", {ParticipantFlag::Forced}},
      ParticipantStateTuple{LogIndex{25}, "E", {ParticipantFlag::Forced}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 4, 5},
                                       LogIndex{15}, LogIndex{25});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 4);
  verifyQuorum(participants, quorum, expectedLogIndex);
}
