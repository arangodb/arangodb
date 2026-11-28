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
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Agency/CommitIndex.h"

#include <vector>

using namespace arangodb::consensus;
using namespace arangodb::consensus::commit_index;

namespace {

// Log with the given terms, logTerms[i] is the term of the entry with
// index i + 1. Returns a predicate like State::has.
auto logWithTerms(std::vector<term_t> logTerms) {
  return [logTerms = std::move(logTerms)](index_t idx, term_t term) {
    return idx > 0 && idx <= logTerms.size() && logTerms[idx - 1] == term;
  };
}

}  // namespace

TEST(AgencyCommitIndexTest, leader_advances_to_quorum_index_of_current_term) {
  auto logHas = logWithTerms({1, 1, 2, 2, 2});
  // Three agents, quorum 2: leader at 5, followers at 4 and 1.
  EXPECT_EQ(4u, leaderCommitIndex({5, 4, 1}, 2, 1, 2, logHas));
  // Five agents, quorum 3:
  EXPECT_EQ(3u, leaderCommitIndex({5, 4, 3, 1, 0}, 3, 1, 2, logHas));
}

TEST(AgencyCommitIndexTest, leader_never_decreases_commit_index) {
  auto logHas = logWithTerms({1, 1, 2, 2, 2});
  EXPECT_EQ(4u, leaderCommitIndex({5, 3, 1}, 2, 4, 2, logHas));
  EXPECT_EQ(4u, leaderCommitIndex({5, 4, 1}, 2, 4, 2, logHas));
}

TEST(AgencyCommitIndexTest, leader_needs_enough_acknowledgements) {
  auto logHas = logWithTerms({1, 1, 1});
  EXPECT_EQ(0u, leaderCommitIndex({3}, 2, 0, 1, logHas));
  EXPECT_EQ(0u, leaderCommitIndex({}, 2, 0, 1, logHas));
}

TEST(AgencyCommitIndexTest, single_agent_commits_own_entries) {
  auto logHas = logWithTerms({1, 1, 1});
  EXPECT_EQ(3u, leaderCommitIndex({3}, 1, 0, 1, logHas));
}

// Figure 8 of the Raft paper: S1 is leader in term 4 and has log terms
// [1, 2, 4]. The entry at index 2 (term 2) is replicated on S1, S2, S3,
// i.e. on a majority of five servers. It must nevertheless not be
// committed, since S5 (with log terms [1, 3]) could still be elected by
// S2, S3, S4 and overwrite it. Only once the entry at index 3 from the
// current term 4 is on a majority, everything up to index 3 is committed.
TEST(AgencyCommitIndexTest, leader_does_not_commit_old_term_entries_figure_8) {
  auto logHas = logWithTerms({1, 2, 4});
  // S1 has 3, S2 and S3 have 2, S4 and S5 have 1:
  EXPECT_EQ(1u, leaderCommitIndex({3, 2, 2, 1, 1}, 3, 1, 4, logHas));
  // S2 and S3 have received the entry of term 4 as well:
  EXPECT_EQ(3u, leaderCommitIndex({3, 3, 3, 1, 1}, 3, 1, 4, logHas));
}

// A new leader starts with its commit index at its last compaction and
// re-replicates its log in batches, entries from earlier terms getting
// acknowledged by a majority must not be committed before the first entry
// of the new term is.
TEST(AgencyCommitIndexTest, leader_catching_up_does_not_commit_older_batches) {
  std::vector<term_t> logTerms(300, 3);
  logTerms.push_back(4);  // index 301, written when becoming leader in term 4
  auto logHas = logWithTerms(logTerms);
  EXPECT_EQ(100u, leaderCommitIndex({301, 199, 199}, 2, 100, 4, logHas));
  EXPECT_EQ(100u, leaderCommitIndex({301, 298, 300}, 2, 100, 4, logHas));
  EXPECT_EQ(301u, leaderCommitIndex({301, 298, 301}, 2, 100, 4, logHas));
}

TEST(AgencyCommitIndexTest, leader_does_not_commit_unknown_entries) {
  auto logHas = logWithTerms({1, 1});
  EXPECT_EQ(1u, leaderCommitIndex({5, 5, 5}, 2, 1, 1, logHas));
}

TEST(AgencyCommitIndexTest, follower_takes_minimum_of_leader_commit_and_new) {
  EXPECT_EQ(7u, followerCommitIndex(3, 7, 10));
  EXPECT_EQ(10u, followerCommitIndex(3, 12, 10));
  // Never decreases:
  EXPECT_EQ(5u, followerCommitIndex(5, 4, 10));
  EXPECT_EQ(5u, followerCommitIndex(5, 12, 2));
}

// A follower has the log 1..30, of which 21..30 are stale entries from an
// earlier term. The leader sends entries 11..20 (which agree) with
// leaderCommit 25. The follower must only commit up to 20, the last entry
// of the request, and not up to 25, which would include stale entries.
TEST(AgencyCommitIndexTest, follower_does_not_commit_stale_tail_of_its_log) {
  index_t const ourLastIndex = 30;
  index_t const lastEntryInRequest = 20;
  index_t const lastVerified = std::min(ourLastIndex, lastEntryInRequest);
  EXPECT_EQ(20u, followerCommitIndex(10, 25, lastVerified));
}

TEST(AgencyCommitIndexTest, verification_starts_empty) {
  FollowerLogVerification v;
  EXPECT_EQ(0u, v.verifiedIndex(0));
  EXPECT_EQ(0u, v.verifiedIndex(1));
}

TEST(AgencyCommitIndexTest, verification_only_valid_in_same_term) {
  FollowerLogVerification v;
  v.noteVerified(3, 20);
  EXPECT_EQ(20u, v.verifiedIndex(3));
  EXPECT_EQ(0u, v.verifiedIndex(2));
  EXPECT_EQ(0u, v.verifiedIndex(4));
}

TEST(AgencyCommitIndexTest, verification_is_monotonic_within_term) {
  FollowerLogVerification v;
  v.noteVerified(3, 20);
  // A request delivered out of order with fewer entries:
  v.noteVerified(3, 15);
  EXPECT_EQ(20u, v.verifiedIndex(3));
  v.noteVerified(3, 25);
  EXPECT_EQ(25u, v.verifiedIndex(3));
}

TEST(AgencyCommitIndexTest, verification_resets_on_new_term) {
  FollowerLogVerification v;
  v.noteVerified(3, 20);
  v.noteVerified(4, 8);
  EXPECT_EQ(8u, v.verifiedIndex(4));
  EXPECT_EQ(0u, v.verifiedIndex(3));
  // A late request from the old term is ignored:
  v.noteVerified(3, 30);
  EXPECT_EQ(8u, v.verifiedIndex(4));
}

// A follower with stale entries 5..8 from term 2 gets a heartbeat from the
// new leader of term 3 with leaderCommit 20 before any non-empty request
// of term 3. It must not mark its stale entries as committed.
TEST(AgencyCommitIndexTest, heartbeat_does_not_commit_unverified_entries) {
  FollowerLogVerification v;
  v.noteVerified(2, 8);  // verified by the old leader of term 2
  index_t ci = 4;
  ci = followerCommitIndex(ci, 20, v.verifiedIndex(3));
  EXPECT_EQ(4u, ci);
  // Now the leader of term 3 overwrites our log up to index 20:
  v.noteVerified(3, 20);
  ci = followerCommitIndex(ci, 20, v.verifiedIndex(3));
  EXPECT_EQ(20u, ci);
  // A later heartbeat with a higher leaderCommit does not go beyond what
  // has been verified:
  ci = followerCommitIndex(ci, 30, v.verifiedIndex(3));
  EXPECT_EQ(20u, ci);
}
