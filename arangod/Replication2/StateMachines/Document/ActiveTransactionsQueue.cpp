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
 * @brief Marks a transaction as being active, thus preventing it from being
 * released.
 */
void ActiveTransactionsQueue::markAsActive(TransactionId tid, LogIndex index) {
  if (_transactions.try_emplace(tid, index).second) {
    markAsActive(index);
  }
}

void ActiveTransactionsQueue::markAsActive(LogIndex index) {
  if (!(_logIndices.empty() || index > _logIndices.back().first)) {
    // Workaround: in some settings, gcc doesn't seem to find the appropriate
    // operator<< via ADL.
    auto stream = std::stringstream{};
    arangodb::operator<<(stream, _logIndices);
    ADB_PROD_CRASH() << "Trying to add index " << index << " after "
                     << stream.str();
  }
  _logIndices.emplace_back(index, Status::kActive);
}

/**
 * @brief Remove a transaction from the active transactions map and update the
 * log indices list.
 */
void ActiveTransactionsQueue::markAsInactive(TransactionId tid) {
  // Fetch the log index indicating when tid was first marked as active. Then,
  // mark it as inactive. We assume that the log indices are always given in
  // increasing order.
  auto it = _transactions.find(tid);
  ADB_PROD_ASSERT(it != std::end(_transactions))
      << "Could not find transaction " << tid;
  markAsInactive(it->second);
  _transactions.erase(it);
}

void ActiveTransactionsQueue::markAsInactive(LogIndex index) {
  auto deactivateIdx =
      std::lower_bound(std::begin(_logIndices), std::end(_logIndices),
                       std::make_pair(index, Status::kActive));
  ADB_PROD_ASSERT(deactivateIdx != std::end(_logIndices) &&
                  deactivateIdx->first == index)
      << "Could not find log index " << index
      << " in the active transactions queue";
  deactivateIdx->second = Status::kInactive;
  popInactive();
}

/**
 * Returns the first index that can be released without discarding
 * any actively ongoing operations.
 */
std::optional<LogIndex> ActiveTransactionsQueue::getReleaseIndex() const {
  if (_logIndices.empty()) {
    return std::nullopt;
  }
  auto const& idx = _logIndices.front();
  ADB_PROD_ASSERT(idx.second == Status::kActive)
      << "An inactive index was found at the front of the dequeue: "
      << idx.first;
  return idx.first.saturatedDecrement();
}

auto ActiveTransactionsQueue::getTransactions() const
    -> std::unordered_map<TransactionId, LogIndex> const& {
  return _transactions;
}

void ActiveTransactionsQueue::clear() {
  _transactions.clear();
  _logIndices.clear();
}

/**
 * @brief Remove all inactive transactions from the front of the dequeue.
 */
void ActiveTransactionsQueue::popInactive() {
  while (!_logIndices.empty() && _logIndices.front().second == kInactive) {
    _logIndices.pop_front();
  }
}

}  // namespace arangodb::replication2::replicated_state::document
