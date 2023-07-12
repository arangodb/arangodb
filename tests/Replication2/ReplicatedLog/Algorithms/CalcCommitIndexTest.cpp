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

#include "Basics/VelocyPackHelper.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::algorithms;
using namespace arangodb::replication2::replicated_log;

struct CalcCommitIndexTest : ::testing::Test {
  auto formatParticipantsAndQuorum(
      std::vector<ParticipantState> const& participants,
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

  auto verifyQuorum(std::vector<ParticipantState> const& participants,
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
      EXPECT_TRUE(participant->isAllowedInQuorum());
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
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{50}},
  };
  auto expectedLogIndex = LogIndex{50};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 1, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{0});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_2_3_participants) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{35}}};

  auto expectedLogIndex = LogIndex{35};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});

  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));

  EXPECT_EQ(quorum.size(), 2U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_0_3_participants) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{35}}};
  auto expectedLogIndex = LogIndex{50};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 0, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 0U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_3_3_participants) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{35}}};
  auto expectedLogIndex = LogIndex{25};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 3, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});

  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));

  EXPECT_EQ(quorum.size(), 3U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, includes_less_quorum_size) {
  // Three participants but only two are included

  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {.allowedInQuorum = false},
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{35}}};
  auto expectedLogIndex = LogIndex{1};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 3, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<
          CommitFailReason::NonEligibleServerRequiredForQuorum>(reason.value));
  auto const& details =
      std::get<CommitFailReason::NonEligibleServerRequiredForQuorum>(
          reason.value);
  EXPECT_EQ(details.candidates.size(), 1U);
  EXPECT_EQ(details.candidates.at("A"),
            CommitFailReason::NonEligibleServerRequiredForQuorum::
                kNotAllowedInQuorum);

  EXPECT_EQ(quorum.size(), 0U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, excluded_and_forced) {
  // One participant is excluded *and* forced, which means we cannot make any
  // progress beyond LogIndex{25} (Note that participants "A" and "C" can still
  // form a quorum for LogIndex{25}!)

  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {.forced = true, .allowedInQuorum = false},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{35}}};

  auto expectedLogIndex = LogIndex{25};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::ForcedParticipantNotInQuorum>(
          reason.value));

  EXPECT_EQ(quorum.size(), 0U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, all_excluded) {
  // all participants are excluded.
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .flags = {.allowedInQuorum = false},
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .flags = {.allowedInQuorum = false},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .flags = {.allowedInQuorum = false},
                       .syncIndex = LogIndex{35}}};

  auto expectedLogIndex = LogIndex{1};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 3, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<
          CommitFailReason::NonEligibleServerRequiredForQuorum>(reason.value));
  auto const& details =
      std::get<CommitFailReason::NonEligibleServerRequiredForQuorum>(
          reason.value);
  EXPECT_EQ(details.candidates.size(), 3U);

  EXPECT_EQ(quorum.size(), 0U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, all_forced) {
  // all participants are forced.
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {.forced = true},
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {.forced = true},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .flags = {.forced = true},
                       .syncIndex = LogIndex{35}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 3, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));

  EXPECT_EQ(quorum.size(), 3U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, not_enough_eligible) {
  // Cannot reach quorum size, as participant "C" with
  // LogIndex{50} is excluded
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "B",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{35}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "C",
                       .snapshotAvailable = true,
                       .flags = {.allowedInQuorum = false},
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "D",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{35}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(15),
                       .id = "E",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{15}},
  };
  auto expectedLogIndex = LogIndex{35};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(std::holds_alternative<CommitFailReason::QuorumSizeNotReached>(
      reason.value));

  EXPECT_EQ(quorum.size(), 2U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, nothing_to_commit) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(15),
                       .id = "A",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{15}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(15),
                       .id = "B",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{15}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(15),
                       .id = "C",
                       .snapshotAvailable = true,
                       .flags = {.allowedInQuorum = false},
                       .syncIndex = LogIndex{15}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(15),
                       .id = "D",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{15}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(15),
                       .id = "E",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{15}}};
  auto expectedLogIndex = LogIndex{15};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{15},
                                       createDefaultTermIndexPair(15),
                                       LogIndex{15});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 2U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, failed_and_forced) {
  // One participant is failed *and* forced, which means we cannot make any
  // progress beyond LogIndex{25} (Note that participants "A" and "C" can still
  // form a quorum for LogIndex{25}!)

  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {.forced = true, .allowedInQuorum = false},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{35}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::ForcedParticipantNotInQuorum>(
          reason.value));

  EXPECT_EQ(quorum.size(), 0U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, write_concern_0_forced_flag) {
  // Everyone is at LogIndex{15}, so there is nothing to do
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {.forced = true},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(15),
                       .id = "B",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{15}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(55),
                       .id = "C",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{55}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 0, LogIndex{15},
                                       createDefaultTermIndexPair(55),
                                       LogIndex{15});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 0U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, DISABLED_more_forced_than_quorum_size) {
  // There are more forced participants than writeConcern
  //
  // At the moment we don't care that we're not including all
  // forced participants in the quorum returned by
  // calculateCommitIndex, so this test is disabled
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {.forced = true},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {.forced = true},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "C",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "D",
                       .snapshotAvailable = true,
                       .flags = {.forced = true},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "E",
                       .snapshotAvailable = true,
                       .flags = {.forced = true},
                       .syncIndex = LogIndex{25}},
  };
  auto expectedLogIndex = LogIndex{25};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{15},
                                       createDefaultTermIndexPair(25),
                                       LogIndex{15});
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_TRUE(
      std::holds_alternative<CommitFailReason::NothingToCommit>(reason.value));

  EXPECT_EQ(quorum.size(), 4U);
  verifyQuorum(participants, quorum, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, who_quorum_size_not_reached) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{35}}};

  auto const spearhead = createDefaultTermIndexPair(50);
  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1}, spearhead,
                                       LogIndex{1});

  auto who = CommitFailReason::QuorumSizeNotReached::who_type();
  who["B"] = {.isAllowedInQuorum = true,
              .snapshotAvailable = true,
              .lastAcknowledged = participants[1].lastAckedEntry};
  who["C"] = {.isAllowedInQuorum = true,
              .snapshotAvailable = true,
              .lastAcknowledged = participants[2].lastAckedEntry};
  auto const expected =
      CommitFailReason::withQuorumSizeNotReached(std::move(who), spearhead);
  EXPECT_EQ(reason, expected)
      << "Actual: " << basics::velocypackhelper::toJson(reason) << "\n"
      << "Expected: " << basics::velocypackhelper::toJson(expected);
}

TEST_F(CalcCommitIndexTest, who_quorum_size_not_reached_multiple) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "A",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "C",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{25}}};

  auto const spearhead = createDefaultTermIndexPair(50);
  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1}, spearhead,
                                       LogIndex{1});

  auto who = CommitFailReason::QuorumSizeNotReached::who_type();
  who["A"] = {.isAllowedInQuorum = true,
              .snapshotAvailable = true,
              .lastAcknowledged = participants[0].lastAckedEntry};
  who["B"] = {.isAllowedInQuorum = true,
              .snapshotAvailable = true,
              .lastAcknowledged = participants[1].lastAckedEntry};
  who["C"] = {.isAllowedInQuorum = true,
              .snapshotAvailable = true,
              .lastAcknowledged = participants[2].lastAckedEntry};
  auto const expected =
      CommitFailReason::withQuorumSizeNotReached(std::move(who), spearhead);
  EXPECT_EQ(reason, expected)
      << "Actual: " << basics::velocypackhelper::toJson(reason) << "\n"
      << "Expected: " << basics::velocypackhelper::toJson(expected);
}

TEST_F(CalcCommitIndexTest, who_forced_participant_not_in_quorum) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {.forced = true, .allowedInQuorum = false},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{35}}};
  auto expectedLogIndex = LogIndex{25};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});

  EXPECT_EQ(reason, CommitFailReason::withForcedParticipantNotInQuorum("B"));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, who_excluded) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "A",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {.allowedInQuorum = false},
                       .syncIndex = LogIndex{50}}};

  auto expectedLogIndex = LogIndex{25};
  auto const spearhead = createDefaultTermIndexPair(50);
  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 1, LogIndex{1}, spearhead,
                                       LogIndex{1});

  auto who = CommitFailReason::QuorumSizeNotReached::who_type();
  who["A"] = {.isAllowedInQuorum = true,
              .snapshotAvailable = true,
              .lastAcknowledged = participants[0].lastAckedEntry};
  who["B"] = {.isAllowedInQuorum = false,
              .snapshotAvailable = true,
              .lastAcknowledged = participants[1].lastAckedEntry};
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  auto const expected =
      CommitFailReason::withQuorumSizeNotReached(std::move(who), spearhead);
  EXPECT_EQ(reason, expected)
      << "Actual: " << basics::velocypackhelper::toJson(reason) << "\n"
      << "Expected: " << basics::velocypackhelper::toJson(expected);
}

TEST_F(CalcCommitIndexTest, who_all_excluded) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {.allowedInQuorum = false},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {.allowedInQuorum = false},
                       .syncIndex = LogIndex{50}}};

  auto expectedLogIndex = LogIndex{1};
  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 1, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});

  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_EQ(reason,
            (CommitFailReason::withNonEligibleServerRequiredForQuorum(
                {{"A", CommitFailReason::NonEligibleServerRequiredForQuorum::
                           kNotAllowedInQuorum},
                 {"B", CommitFailReason::NonEligibleServerRequiredForQuorum::
                           kNotAllowedInQuorum}})))
      << "Actual: " << basics::velocypackhelper::toJson(reason);
}

TEST_F(CalcCommitIndexTest, who_all_excluded_wrong_term) {
  auto participants =
      std::vector{ParticipantState{
                      .lastAckedEntry = TermIndexPair{LogTerm{1}, LogIndex{25}},
                      .id = "A",
                      .snapshotAvailable = true,
                      .syncIndex = LogIndex{25}},
                  ParticipantState{
                      .lastAckedEntry = TermIndexPair{LogTerm{2}, LogIndex{50}},
                      .id = "B",
                      .snapshotAvailable = true,
                      .flags = {.allowedInQuorum = false},
                      .syncIndex = LogIndex{50}}};

  auto expectedLogIndex = LogIndex{1};
  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 1, LogIndex{1},
                                       TermIndexPair{LogTerm{2}, LogIndex{50}},
                                       LogIndex{1});

  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_EQ(
      reason,
      (CommitFailReason::withNonEligibleServerRequiredForQuorum(
          {{"A",
            CommitFailReason::NonEligibleServerRequiredForQuorum::kWrongTerm},
           {"B", CommitFailReason::NonEligibleServerRequiredForQuorum::
                     kNotAllowedInQuorum}})))
      << "Actual: " << basics::velocypackhelper::toJson(reason);
}

TEST_F(CalcCommitIndexTest, write_concern_too_big) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .syncIndex = LogIndex{50}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {.allowedInQuorum = true},
                       .syncIndex = LogIndex{25}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(15),
                       .id = "C",
                       .snapshotAvailable = true,
                       .flags = {.allowedInQuorum = true},
                       .syncIndex = LogIndex{15}}};

  auto expectedLogIndex = LogIndex{1};
  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 4, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});

  EXPECT_EQ(reason, CommitFailReason::withFewerParticipantsThanWriteConcern(
                        {.effectiveWriteConcern = 4, .numParticipants = 3}))
      << "Actual: " << basics::velocypackhelper::toJson(reason);
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, who_forced_participant_in_wrong_term) {
  auto participants = std::vector{
      ParticipantState{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}),
          .id = "A",
          .snapshotAvailable = true,
          .syncIndex = LogIndex{50}},
      ParticipantState{
          .lastAckedEntry = TermIndexPair(LogTerm{1}, LogIndex{200}),
          .id = "B",
          .snapshotAvailable = true,
          .flags = {.forced = true, .allowedInQuorum = true},
          .syncIndex = LogIndex{200}},
      ParticipantState{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}),
          .id = "C",
          .snapshotAvailable = true,
          .syncIndex = LogIndex{50}},
  };
  auto expectedLogIndex = LogIndex{1};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1},
                                       TermIndexPair(LogTerm{2}, LogIndex{50}),
                                       LogIndex{1});

  EXPECT_EQ(reason, CommitFailReason::withForcedParticipantNotInQuorum("B"));
  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
}

TEST_F(CalcCommitIndexTest, non_eligible_participant_in_wrong_term) {
  auto participants = std::vector{
      ParticipantState{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}),
          .id = "A",
          .snapshotAvailable = true,
          .syncIndex = LogIndex{50}},
      ParticipantState{
          .lastAckedEntry = TermIndexPair(LogTerm{1}, LogIndex{25}),
          .id = "B",
          .snapshotAvailable = true,
          .syncIndex = LogIndex{25}},
      ParticipantState{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}),
          .id = "C",
          .snapshotAvailable = true,
          .syncIndex = LogIndex{50}},
  };
  auto expectedLogIndex = LogIndex{50};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1},
                                       TermIndexPair(LogTerm{2}, LogIndex{50}),
                                       LogIndex{1});

  EXPECT_EQ(reason, CommitFailReason::withNothingToCommit());
  verifyQuorum(participants, quorum, expectedLogIndex, LogTerm{2});
}

TEST_F(CalcCommitIndexTest, who_non_eligible_required) {
  auto participants = std::vector{
      ParticipantState{
          .lastAckedEntry = TermIndexPair{LogTerm{2}, LogIndex{50}},
          .id = "A",
          .snapshotAvailable = true,
          .flags = {.allowedInQuorum = false},
          .syncIndex = LogIndex{50}},
      ParticipantState{
          .lastAckedEntry = TermIndexPair{LogTerm{2}, LogIndex{25}},
          .id = "B",
          .snapshotAvailable = true,
          .flags = {.allowedInQuorum = false},
          .syncIndex = LogIndex{25}},
      ParticipantState{
          .lastAckedEntry = TermIndexPair{LogTerm{1}, LogIndex{15}},
          .id = "C",
          .snapshotAvailable = true,
          .syncIndex = LogIndex{15}},
      ParticipantState{
          .lastAckedEntry = TermIndexPair{LogTerm{2}, LogIndex{15}},
          .id = "D",
          .snapshotAvailable = true,
          .syncIndex = LogIndex{15}},
  };

  auto expectedLogIndex = LogIndex{1};
  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1},
                                       TermIndexPair{LogTerm{2}, LogIndex{50}},
                                       LogIndex{1});

  EXPECT_EQ(index, expectedLogIndex);
  EXPECT_EQ(syncCommitIndex, expectedLogIndex);
  EXPECT_EQ(reason,
            (CommitFailReason::withNonEligibleServerRequiredForQuorum(
                {{"A", CommitFailReason::NonEligibleServerRequiredForQuorum::
                           kNotAllowedInQuorum},
                 {"B", CommitFailReason::NonEligibleServerRequiredForQuorum::
                           kNotAllowedInQuorum},
                 {"C", CommitFailReason::NonEligibleServerRequiredForQuorum::
                           kWrongTerm}})))
      << "Actual: " << basics::velocypackhelper::toJson(reason);
}

TEST_F(CalcCommitIndexTest, no_snapshot_is_non_eligible) {
  auto participants = std::vector{
      ParticipantState{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}),
          .id = "A",
          .snapshotAvailable = true,
          .syncIndex = LogIndex{50}},
      ParticipantState{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{25}),
          .id = "B",
          .snapshotAvailable = true,
          .syncIndex = LogIndex{25}},
      ParticipantState{
          .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}),
          .id = "C",
          .snapshotAvailable = false,
          .syncIndex = LogIndex{50}},
  };
  auto expectedCommitIndex = TermIndexPair(LogTerm{2}, LogIndex{25});
  auto who = CommitFailReason::QuorumSizeNotReached::who_type();
  who["B"] = {.isAllowedInQuorum = true,
              .snapshotAvailable = true,
              .lastAcknowledged = participants[1].lastAckedEntry};
  who["C"] = {.isAllowedInQuorum = true,
              .snapshotAvailable = false,
              .lastAcknowledged = participants[2].lastAckedEntry};
  auto const expected = CommitFailReason::withQuorumSizeNotReached(
      std::move(who), TermIndexPair(LogTerm{2}, LogIndex{50}));
  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 2, LogIndex{1},
                                       TermIndexPair(LogTerm{2}, LogIndex{50}),
                                       LogIndex{1});

  EXPECT_EQ(reason, expected)
      << basics::velocypackhelper::toJson(reason) << " vs "
      << basics::velocypackhelper::toJson(expected);
  verifyQuorum(participants, quorum, expectedCommitIndex.index, LogTerm{2});
}

TEST_F(CalcCommitIndexTest, no_snapshot_is_non_eligible_but_required) {
  auto participants =
      std::vector{ParticipantState{
                      .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}),
                      .id = "A",
                      .snapshotAvailable = true,
                      .syncIndex = LogIndex{50}},
                  ParticipantState{
                      .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{25}),
                      .id = "B",
                      .snapshotAvailable = true,
                      .syncIndex = LogIndex{25}},
                  ParticipantState{
                      .lastAckedEntry = TermIndexPair(LogTerm{2}, LogIndex{50}),
                      .id = "C",
                      .snapshotAvailable = false,
                      .syncIndex = LogIndex{50}}};
  auto expectedCommitIndex = TermIndexPair(LogTerm{2}, LogIndex{25});
  auto who =
      CommitFailReason::NonEligibleServerRequiredForQuorum::CandidateMap{};
  who["C"] =
      CommitFailReason::NonEligibleServerRequiredForQuorum::kSnapshotMissing;
  auto const expected =
      CommitFailReason::withNonEligibleServerRequiredForQuorum(std::move(who));
  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 3, LogIndex{1},
                                       TermIndexPair(LogTerm{2}, LogIndex{50}),
                                       LogIndex{1});

  EXPECT_EQ(reason, expected)
      << basics::velocypackhelper::toJson(reason) << " vs "
      << basics::velocypackhelper::toJson(expected);
  verifyQuorum(participants, quorum, expectedCommitIndex.index, LogTerm{2});
}

TEST_F(CalcCommitIndexTest, sync_commit_index_different_from_commit_index) {
  auto participants = std::vector{
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(50),
                       .id = "A",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{3}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(25),
                       .id = "B",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{3}},
      ParticipantState{.lastAckedEntry = createDefaultTermIndexPair(35),
                       .id = "C",
                       .snapshotAvailable = true,
                       .flags = {},
                       .syncIndex = LogIndex{5}}};
  auto expectedCommitIndex = LogIndex{25};
  auto expectedSyncCommitIndex = LogIndex{3};

  auto [index, syncCommitIndex, reason, quorum] =
      algorithms::calculateCommitIndex(participants, 3, LogIndex{1},
                                       createDefaultTermIndexPair(50),
                                       LogIndex{1});

  EXPECT_EQ(index, expectedCommitIndex);
  EXPECT_EQ(syncCommitIndex, expectedSyncCommitIndex);
  EXPECT_EQ(quorum.size(), 3U);
}
