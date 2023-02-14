////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/ActiveTransactionsQueue.h"

#include <Basics/debugging.h>

#include <algorithm>

namespace arangodb::replication2::replicated_state::document {

/**
 * Returns the first index which can be released without discarding
 * any active transactions.
 */
LogIndex ActiveTransactionsQueue::getReleaseIndex(LogIndex current) const {
  if (_logIndices.empty()) {
    return current;
  }
  auto const& idx = _logIndices.front();
  TRI_ASSERT(idx.second == Status::kActive);
  return idx.first.saturatedDecrement();
}

/**
 * @brief Remove a transaction from the active transactions map and update the
 * log indices list.
 * @return true if the transaction was found and removed, false otherwise
 */
bool ActiveTransactionsQueue::erase(TransactionId const& tid) {
  auto it = _transactions.find(tid);
  if (it == std::end(_transactions)) {
    return false;
  }

  // Locate the transaction with that specific log index, corresponding to when
  // tid was first created. Then, mark it as inactive.
  // This assumes that the log indices are always given in increasing order.
  auto deactivateIdx =
      std::lower_bound(std::begin(_logIndices), std::end(_logIndices),
                       std::make_pair(it->second, Status::kActive));
  TRI_ASSERT(deactivateIdx != std::end(_logIndices) &&
             deactivateIdx->first == it->second);
  deactivateIdx->second = Status::kInactive;
  _transactions.erase(it);

  popInactive();

  return true;
}

/**
 * @brief Try to insert an entry into the active transactions map. If the entry
 * is new, mark the log index as active.
 */
void ActiveTransactionsQueue::emplace(TransactionId tid, LogIndex index) {
  if (_transactions.try_emplace(tid, index).second) {
    if (!_logIndices.empty()) {
      TRI_ASSERT(index > _logIndices.back().first);
    }
    _logIndices.emplace_back(index, Status::kActive);
  }
}

/**
 * We should not leave the deque empty, even if the last transaction is
 * inactive. This ensures that we always have a release index to report.
 */
void ActiveTransactionsQueue::popInactive() {
  while (!_logIndices.empty() && _logIndices.front().second == kInactive) {
    _logIndices.pop_front();
  }
}

std::size_t ActiveTransactionsQueue::size() const noexcept {
  return _transactions.size();
}

auto ActiveTransactionsQueue::getTransactions() const
    -> std::unordered_map<TransactionId, LogIndex> const& {
  return _transactions;
}

void ActiveTransactionsQueue::clear() {
  _transactions.clear();
  _logIndices.clear();
}

}  // namespace arangodb::replication2::replicated_state::document
