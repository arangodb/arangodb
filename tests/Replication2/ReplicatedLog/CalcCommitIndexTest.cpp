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

#include "Replication2/ReplicatedLog/Algorithms.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::algorithms;
using namespace arangodb::replication2::replicated_log;

struct CalcCommitIndexTest : ::testing::Test {
  auto formatParticipantsAndQuorum(std::vector<ParticipantStateTuple> const& participants,
                                   std::vector<ParticipantId> const& quorum) -> std::string {
    std::stringstream buf;
    buf << "participants: ";
    for (auto const& p : participants) {
      buf << p << ", ";
    }
    buf << "quorum: ";
    for (auto const& p : quorum) {
      buf << p << ", ";
    }
    return buf.str();
  }

  auto formatParticipantId(ParticipantId const& pid) {
    std::stringstream buf;
    buf << "participantId: " << pid;
    return buf.str();
  }

  auto verifyQuorum(std::vector<ParticipantStateTuple> const& participants,
                    std::vector<ParticipantId> const& quorum,
                    LogIndex const& expectedLogIndex) {
    SCOPED_TRACE(formatParticipantsAndQuorum(participants, quorum));
    // every member of the quorum needs to have at least the expectedLogIndex,
    // must not be failed or excluded.
    for (auto const& participantId : quorum) {
      SCOPED_TRACE(formatParticipantId(participantId));
      auto participant = std::find_if(std::begin(participants), std::end(participants),
                                      [participantId](auto& pst) {
                                        return pst.id == participantId;
                                      });
      ASSERT_NE(participant, std::end(participants));
      EXPECT_GE(participant->index, expectedLogIndex);
      EXPECT_FALSE(participant->isFailed());
      EXPECT_FALSE(participant->isExcluded());
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
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A", .failed = false, .flags = {}},
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
  auto participants = std::vector{
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A", .failed = false, .flags = {}},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "B", .failed = false, .flags = {}},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "C", .failed = false, .flags = {}}};

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
  auto participants = std::vector{
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A", .failed = false, .flags = {}},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "B", .failed = false, .flags = {}},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "C", .failed = false, .flags = {}}};
  auto expectedLogIndex = LogIndex{50};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{0, 0, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_3_3_participants) {
  auto participants = std::vector{
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A", .flags = {}},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "B", .flags = {}},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "C", .flags = {}},
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

  auto participants =
      std::vector{ParticipantStateTuple{.index = LogIndex{50}, .id = "A", .flags = {.excluded = true}},
                  ParticipantStateTuple{.index = LogIndex{25}, .id = "B"},
                  ParticipantStateTuple{.index = LogIndex{35}, .id = "C"}};
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
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A"},
      ParticipantStateTuple{.index = LogIndex{25},
                            .id = "B",
                            .flags = {.forced = true, .excluded = true}},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "C"},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::ForcedParticipantNotInQuorum>(
      reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, all_excluded) {
  // all participants are excluded.
  auto participants = std::vector{
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A", .flags = {.excluded = true}},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "B", .flags = {.excluded = true}},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "C", .flags = {.excluded = true}},
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
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A", .flags = {.forced = true}},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "B", .flags = {.forced = true}},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "C", .flags = {.forced = true}},
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
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A"},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "B"},
      ParticipantStateTuple{.index = LogIndex{50}, .id = "C", .flags = {.excluded = true}},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "D"},
      ParticipantStateTuple{.index = LogIndex{15}, .id = "E"},
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
      ParticipantStateTuple{.index = LogIndex{15}, .id = "A"},
      ParticipantStateTuple{.index = LogIndex{15}, .id = "B"},
      ParticipantStateTuple{.index = LogIndex{15}, .id = "C", .flags = {.excluded = true}},
      ParticipantStateTuple{.index = LogIndex{15}, .id = "D"},
      ParticipantStateTuple{.index = LogIndex{15}, .id = "E"},
  };
  auto expectedLogIndex = LogIndex{15};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 3, 5},
                                       LogIndex{15}, LogIndex{15});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, failed_participant) {
  // One participant has failed, which means we cannot make any progress
  // beyond LogIndex{25}

  auto participants = std::vector{
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A", .failed = true},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "B"},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "C"},
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
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A"},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "B", .flags = {.forced = true, .excluded = true}},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "C"},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::ForcedParticipantNotInQuorum>(
      reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, failed_with_soft_write_concern) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantStateTuple{.index = LogIndex{55}, .id = "A", .failed = true},
      ParticipantStateTuple{.index = LogIndex{15}, .id = "B"},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "C", .failed = true},
      ParticipantStateTuple{.index = LogIndex{5}, .id = "D"},
      ParticipantStateTuple{.index = LogIndex{17}, .id = "E"},
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
      ParticipantStateTuple{.index = LogIndex{55}, .id = "A", .failed = true},
      ParticipantStateTuple{.index = LogIndex{15}, .id = "B"},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "C"},
      ParticipantStateTuple{.index = LogIndex{5}, .id = "D", .failed = true},
      ParticipantStateTuple{.index = LogIndex{17}, .id = "E"},
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
      ParticipantStateTuple{.index = LogIndex{55}, .id = "A", .failed = true},
      ParticipantStateTuple{.index = LogIndex{15}, .id = "B"},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "C"},
      ParticipantStateTuple{.index = LogIndex{5}, .id = "D", .failed = true},
      ParticipantStateTuple{.index = LogIndex{17}, .id = "E"},
  };
  auto expectedLogIndex = LogIndex{15};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 4, 5},
                                       LogIndex{15}, LogIndex{15});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_0_forced_flag) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantStateTuple{.index = LogIndex{25}, .id = "A", .flags = {.forced = true}},
      ParticipantStateTuple{.index = LogIndex{15}, .id = "B"},
      ParticipantStateTuple{.index = LogIndex{55}, .id = "C"},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{0, 0, 3},
                                       LogIndex{15}, LogIndex{55});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

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
      ParticipantStateTuple{.index = LogIndex{25}, .id = "A", .flags = {.forced = true}},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "B", .flags = {.forced = true}},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "C"},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "D", .flags = {.forced = true}},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "E", .flags = {.forced = true}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 4, 5},
                                       LogIndex{15}, LogIndex{25});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 4);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, who_quorum_size_not_reached) {
  auto participants = std::vector{ParticipantStateTuple{.index = LogIndex{50}, .id = "A"},
                                  ParticipantStateTuple{.index = LogIndex{25}, .id = "B"},
                                  ParticipantStateTuple{.index = LogIndex{35}, .id = "C"}};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});

  EXPECT_EQ(reason, CommitFailReason::withQuorumSizeNotReached("C"));
}

TEST_F(CalcCommitIndexTest, who_quorum_size_not_reached_multiple) {
  auto participants = std::vector{ParticipantStateTuple{.index = LogIndex{25}, .id = "A"},
                                  ParticipantStateTuple{.index = LogIndex{25}, .id = "B"},
                                  ParticipantStateTuple{.index = LogIndex{25}, .id = "C"}};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});

  EXPECT_TRUE(reason == CommitFailReason::withQuorumSizeNotReached("A") ||
              reason == CommitFailReason::withQuorumSizeNotReached("B") ||
              reason == CommitFailReason::withQuorumSizeNotReached("C"));
}

TEST_F(CalcCommitIndexTest, who_forced_participant_not_in_quorum) {
  auto participants = std::vector{
      ParticipantStateTuple{.index = LogIndex{50}, .id = "A"},
      ParticipantStateTuple{.index = LogIndex{25}, .id = "B", .flags = {.forced = true, .excluded = true}},
      ParticipantStateTuple{.index = LogIndex{35}, .id = "C"},
  };

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{2, 2, 3},
                                       LogIndex{1}, LogIndex{50});

  EXPECT_EQ(reason, CommitFailReason::withForcedParticipantNotInQuorum("B"));
}

TEST_F(CalcCommitIndexTest, who_all_failed_excluded) {
  auto participants =
      std::vector{ParticipantStateTuple{.index = LogIndex{50}, .id = "A", .failed = true},
                  ParticipantStateTuple{.index = LogIndex{25}, .id = "B", .flags = {.excluded = true}}};

  auto [index, reason, quorum] =
      algorithms::calculateCommitIndex(participants,
                                       CalculateCommitIndexOptions{1, 1, 2},
                                       LogIndex{1}, LogIndex{50});

  EXPECT_TRUE(reason == CommitFailReason::withQuorumSizeNotReached("A") ||
              reason == CommitFailReason::withQuorumSizeNotReached("B"));
}
