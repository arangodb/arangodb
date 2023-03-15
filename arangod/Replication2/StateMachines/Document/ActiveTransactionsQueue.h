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

#pragma once

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <deque>
#include <unordered_map>

namespace arangodb::replication2::replicated_state::document {

/**
 * Keeps track of active transactions.
 * Uses a deque instead of a set because log indices are always given in
 * increasing order.
 */
class ActiveTransactionsQueue {
  enum Status { kActive, kInactive };

 public:
  void markAsActive(TransactionId tid, LogIndex index);
  void markAsInactive(TransactionId tid);

  // These are used when we have a log index but no transaction id.
  void markAsActive(LogIndex index);
  void markAsInactive(LogIndex index);

  std::optional<LogIndex> getReleaseIndex() const;

  auto getTransactions() const
      -> std::unordered_map<TransactionId, LogIndex> const&;
  void clear();

 private:
  void popInactive();

 private:
  std::unordered_map<TransactionId, LogIndex> _transactions;
  std::deque<std::pair<LogIndex, Status>> _logIndices;
};

}  // namespace arangodb::replication2::replicated_state::document
