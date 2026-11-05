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

#pragma once

#include "Agency/AgencyCommon.h"

#include <algorithm>
#include <functional>
#include <vector>

/// Pure helpers implementing the Raft rules for advancing the commit index.
/// They are kept free of any Agent state such that they can be unit tested.
namespace arangodb::consensus::commit_index {

/// @brief Leader side (Raft paper, Figure 2, "Rules for Servers / Leaders"):
/// If there exists an N such that N > commitIndex, a majority of
/// matchIndex[i] >= N, and log[N].term == currentTerm, set commitIndex = N.
/// `ackedIndexes` contains the last acknowledged index of every agent
/// including the leader itself, `logHas(index, term)` returns whether the
/// leader's log contains an entry with index `index` and term `term`. Returns
/// the new commit index, which is never smaller than `currentCommitIndex`. The
/// term check is essential (Raft paper, section 5.4.2 and Figure 8): entries
/// from earlier terms must never be committed by counting replicas. Since terms
/// in the log are non-decreasing, if the entry at the quorum index is not from
/// the current term, no smaller index can be either, so it suffices to look at
/// the quorum index alone.
inline index_t leaderCommitIndex(
    std::vector<index_t> ackedIndexes, size_t quorum,
    index_t currentCommitIndex, term_t currentTerm,
    std::function<bool(index_t, term_t)> const& logHas) {
  if (quorum == 0 || ackedIndexes.size() < quorum) {
    return currentCommitIndex;
  }
  std::sort(ackedIndexes.begin(), ackedIndexes.end());
  index_t index = ackedIndexes[ackedIndexes.size() - quorum];
  if (index <= currentCommitIndex) {
    return currentCommitIndex;
  }
  if (!logHas(index, currentTerm)) {
    return currentCommitIndex;
  }
  return index;
}

/// @brief Follower side (Raft paper, Figure 2, AppendEntries RPC, step 5):
/// If leaderCommit > commitIndex, set
/// commitIndex = min(leaderCommit, index of last new entry).
/// `lastVerifiedIndex` must be an index up to which our log is known to
/// agree with the leader's log, i.e. the last entry of the AppendEntries
/// request just processed, and *not* simply the last index of our own log,
/// which might contain stale entries from an earlier term beyond it.
inline index_t followerCommitIndex(index_t currentCommitIndex,
                                   index_t leaderCommitIndex,
                                   index_t lastVerifiedIndex) {
  return std::max(currentCommitIndex,
                  std::min(leaderCommitIndex, lastVerifiedIndex));
}

/// @brief Remembers on a follower up to which index its log is known to
/// agree with the log of the leader of a given term. Within one term there
/// is only one leader and its log only grows, so a verified prefix stays
/// valid for the whole term and can safely be used to advance the commit
/// index when an empty AppendEntries (heartbeat) arrives, which carries no
/// log consistency check of its own. Not thread-safe, the caller must
/// protect it.
class FollowerLogVerification {
 public:
  /// @brief Note that our log agrees with the leader of `term` up to and
  /// including `index`.
  void noteVerified(term_t term, index_t index) {
    if (term > _term) {
      _term = term;
      _index = index;
    } else if (term == _term) {
      _index = std::max(_index, index);
    }
    // A late request from an older term is ignored.
  }

  /// @brief Index up to which our log is known to agree with the log of the
  /// leader of `term`, 0 if nothing has been verified in this term.
  index_t verifiedIndex(term_t term) const {
    return term == _term ? _index : 0;
  }

 private:
  term_t _term = 0;
  index_t _index = 0;
};

}  // namespace arangodb::consensus::commit_index
