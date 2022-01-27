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
  auto formatParticipantsAndQuorum(
      std::vector<ParticipantStateTuple> const& participants,
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

  auto createDefaultTermIndexPair(decltype(LogIndex::value) value)
      -> TermIndexPair {
    return {LogTerm{1}, LogIndex{value}};
  }

  auto verifyQuorum(std::vector<ParticipantStateTuple> const& participants,
                    std::vector<ParticipantId> const& quorum,
                    LogIndex const& expectedLogIndex,
                    LogTerm term = LogTerm{1}) {
    SCOPED_TRACE(formatParticipantsAndQuorum(participants, quorum));
    // every member of the quorum needs to have at least the expectedLogIndex,
    // must not be failed or excluded.
    auto minIndex =
        LogIndex{std::numeric_limits<decltype(LogIndex::value)>::max()};
    for (auto const& participantId : quorum) {
      SCOPED_TRACE(formatParticipantId(participantId));
      auto participant = std::find_if(
          std::begin(participants), std::end(participants),
          [participantId](auto& pst) { return pst.id == participantId; });
      ASSERT_NE(participant, std::end(participants));
      EXPECT_GE(participant->lastIndex(), expectedLogIndex);
      EXPECT_FALSE(participant->isFailed());
      EXPECT_FALSE(participant->isExcluded());
      EXPECT_EQ(participant->lastTerm(), term);
      minIndex = std::min(minIndex, participant->lastIndex());
    }
    if (!quorum.empty()) {
      ASSERT_EQ(minIndex, expectedLogIndex);
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
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .failed = false,
                            .flags = {}},
  };
  auto expectedLogIndex = LogIndex{50};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{1, 1}, LogIndex{1},
      createDefaultTermIndexPair(50));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_2_3_participants) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .failed = false,
                            .flags = {}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .failed = false,
                            .flags = {}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C",
                            .failed = false,
                            .flags = {}}};

  auto expectedLogIndex = LogIndex{35};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 2}, LogIndex{1},
      createDefaultTermIndexPair(50));

  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));

  EXPECT_EQ(quorum.size(), 2);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_0_3_participants) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .failed = false,
                            .flags = {}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .failed = false,
                            .flags = {}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C",
                            .failed = false,
                            .flags = {}}};
  auto expectedLogIndex = LogIndex{50};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{0, 0}, LogIndex{1},
      createDefaultTermIndexPair(50));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_3_3_participants) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .failed = false,
                            .flags = {}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .failed = false,
                            .flags = {}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C",
                            .failed = false,
                            .flags = {}}};
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{3, 3}, LogIndex{1},
      createDefaultTermIndexPair(50));

  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, includes_less_quorum_size) {
  // Three participants but only two are included

  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .flags = {.excluded = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C"}};
  auto expectedLogIndex = LogIndex{1};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{3, 3}, LogIndex{1},
      createDefaultTermIndexPair(50));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<
          CommitFailReason::NonEligibleServerRequiredForQuorum>(reason.value));
  auto const& details =
      std::get<CommitFailReason::NonEligibleServerRequiredForQuorum>(
          reason.value);
  EXPECT_EQ(details.candidates.size(), 1);
  EXPECT_EQ(details.candidates.at("A"),
            CommitFailReason::NonEligibleServerRequiredForQuorum::kExcluded);

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, excluded_and_forced) {
  // One participant is excluded *and* forced, which means we cannot make any
  // progress beyond LogIndex{25} (Note that participants "A" and "C" can still
  // form a quorum for LogIndex{25}!)

  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .flags = {.forced = true, .excluded = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C"},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 2}, LogIndex{1},
      createDefaultTermIndexPair(50));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::ForcedParticipantNotInQuorum>(
          reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, all_excluded) {
  // all participants are excluded.
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .flags = {.excluded = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .flags = {.excluded = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C",
                            .flags = {.excluded = true}},
  };
  auto expectedLogIndex = LogIndex{1};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{3, 3}, LogIndex{1},
      createDefaultTermIndexPair(50));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<
          CommitFailReason::NonEligibleServerRequiredForQuorum>(reason.value));
  auto const& details =
      std::get<CommitFailReason::NonEligibleServerRequiredForQuorum>(
          reason.value);
  EXPECT_EQ(details.candidates.size(), 3);

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, all_forced) {
  // all participants are forced.
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .flags = {.forced = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .flags = {.forced = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C",
                            .flags = {.forced = true}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{3, 3}, LogIndex{1},
      createDefaultTermIndexPair(50));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, not_enough_eligible) {
  // Cannot reach quorum size, as participant "C" with
  // LogIndex{50} is excluded
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "C",
                            .flags = {.excluded = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "D"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "E"},
  };
  auto expectedLogIndex = LogIndex{35};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 3}, LogIndex{1},
      createDefaultTermIndexPair(50));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, nothing_to_commit) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "A"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "C",
                            .flags = {.excluded = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "D"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "E"},
  };
  auto expectedLogIndex = LogIndex{15};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 3}, LogIndex{15},
      createDefaultTermIndexPair(15));
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
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .failed = true},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C"},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 2}, LogIndex{1},
      createDefaultTermIndexPair(50));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));

  EXPECT_EQ(quorum.size(), 2);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, failed_and_forced) {
  // One participant is failed *and* forced, which means we cannot make any
  // progress beyond LogIndex{25} (Note that participants "A" and "C" can still
  // form a quorum for LogIndex{25}!)

  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .flags = {.forced = true, .excluded = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C"},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 2}, LogIndex{1},
      createDefaultTermIndexPair(50));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::ForcedParticipantNotInQuorum>(
          reason.value));

  EXPECT_EQ(quorum.size(), 0);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, failed_with_soft_write_concern) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(55),
                            .id = "A",
                            .failed = true},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "C",
                            .failed = true},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(5),
                            .id = "D"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(17),
                            .id = "E"},
  };
  auto expectedLogIndex = LogIndex{5};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 3}, LogIndex{15},
      createDefaultTermIndexPair(15));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));
  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, smallest_failed) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(55),
                            .id = "A",
                            .failed = true},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "C"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(5),
                            .id = "D",
                            .failed = true},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(17),
                            .id = "E"},
  };
  auto expectedLogIndex = LogIndex{15};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 4}, LogIndex{15},
      createDefaultTermIndexPair(55));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));
  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, nothing_to_commit_failed) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(55),
                            .id = "A",
                            .failed = true},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "C"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(5),
                            .id = "D",
                            .failed = true},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(17),
                            .id = "E"},
  };
  auto expectedLogIndex = LogIndex{15};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 4}, LogIndex{15},
      createDefaultTermIndexPair(15));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 3);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_0_forced_flag) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "A",
                            .flags = {.forced = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(55),
                            .id = "C"},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{0, 0}, LogIndex{15},
      createDefaultTermIndexPair(55));
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
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "A",
                            .flags = {.forced = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .flags = {.forced = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "C"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "D",
                            .flags = {.forced = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "E",
                            .flags = {.forced = true}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 4}, LogIndex{15},
      createDefaultTermIndexPair(25));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 4);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, who_quorum_size_not_reached) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C"}};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 2}, LogIndex{1},
      createDefaultTermIndexPair(50));

  EXPECT_EQ(reason, CommitFailReason::withQuorumSizeNotReached("C"));
}

TEST_F(CalcCommitIndexTest, who_quorum_size_not_reached_multiple) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "A"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "C"}};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 2}, LogIndex{1},
      createDefaultTermIndexPair(50));

  EXPECT_TRUE(reason == CommitFailReason::withQuorumSizeNotReached("A") ||
              reason == CommitFailReason::withQuorumSizeNotReached("B") ||
              reason == CommitFailReason::withQuorumSizeNotReached("C"));
}

TEST_F(CalcCommitIndexTest, who_forced_participant_not_in_quorum) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A"},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .flags = {.forced = true, .excluded = true}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(35),
                            .id = "C"},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 2}, LogIndex{1},
      createDefaultTermIndexPair(50));

  EXPECT_EQ(reason, CommitFailReason::withForcedParticipantNotInQuorum("B"));
  EXPECT_EQ(index, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, who_all_failed_excluded) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .failed = true},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .flags = {.excluded = true}}};

  auto expectedLogIndex = LogIndex{1};
  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{1, 1}, LogIndex{1},
      createDefaultTermIndexPair(50));

  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(
      reason,
      (CommitFailReason::withNonEligibleServerRequiredForQuorum(
          {{"A", CommitFailReason::NonEligibleServerRequiredForQuorum::kFailed},
           {"B", CommitFailReason::NonEligibleServerRequiredForQuorum::
                     kExcluded}})));
}

TEST_F(CalcCommitIndexTest, write_concern_too_big) {
  auto participants = std::vector{
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(50),
                            .id = "A",
                            .failed = false},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(25),
                            .id = "B",
                            .flags = {.excluded = false}},
      ParticipantStateTuple{.lastAckedEntry = createDefaultTermIndexPair(15),
                            .id = "C",
                            .flags = {.excluded = false}}};

  auto expectedLogIndex = LogIndex{1};
  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{100, 100}, LogIndex{1},
      createDefaultTermIndexPair(50));

  EXPECT_TRUE(reason == CommitFailReason::withQuorumSizeNotReached("C"));
  EXPECT_EQ(index, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, who_forced_participant_in_wrong_term) {
  auto participants = std::vector{
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}), .id = "A"},
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair(LogTerm{1}, LogIndex{200}),
          .id = "B",
          .flags = {.forced = true, .excluded = false}},
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}), .id = "C"},
  };
  auto expectedLogIndex = LogIndex{1};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 2}, LogIndex{1},
      TermIndexPair(LogTerm{2}, LogIndex{50}));

  EXPECT_EQ(reason, CommitFailReason::withForcedParticipantNotInQuorum("B"));
  EXPECT_EQ(index, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, non_eligible_participant_in_wrong_term) {
  auto participants = std::vector{
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}), .id = "A"},
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair(LogTerm{1}, LogIndex{25}), .id = "B"},
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}), .id = "C"},
  };
  auto expectedLogIndex = LogIndex{50};

  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 2}, LogIndex{1},
      TermIndexPair(LogTerm{2}, LogIndex{50}));

  EXPECT_EQ(reason, CommitFailReason::withNothingToCommit());
  verifyQuorum(participants, quorum, expectedLogIndex, LogTerm{2});
}

TEST_F(CalcCommitIndexTest, who_non_eligible_required) {
  auto participants = std::vector{
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair{LogTerm{2}, LogIndex{50}},
          .id = "A",
          .failed = true},
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair{LogTerm{2}, LogIndex{25}},
          .id = "B",
          .flags = {.excluded = true}},
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair{LogTerm{1}, LogIndex{15}}, .id = "C"},
      ParticipantStateTuple{
          .lastAckedEntry = TermIndexPair{LogTerm{2}, LogIndex{15}}, .id = "D"},
  };

  auto expectedLogIndex = LogIndex{1};
  auto [index, reason, quorum] = algorithms::calculateCommitIndex(
      participants, CalculateCommitIndexOptions{2, 1}, LogIndex{1},
      TermIndexPair{LogTerm{2}, LogIndex{50}});

  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(
      reason,
      (CommitFailReason::withNonEligibleServerRequiredForQuorum(
          {{"A", CommitFailReason::NonEligibleServerRequiredForQuorum::kFailed},
           {"B",
            CommitFailReason::NonEligibleServerRequiredForQuorum::kExcluded},
           {"C", CommitFailReason::NonEligibleServerRequiredForQuorum::
                     kWrongTerm}})));
}
