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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedTransactionState.h"

#include "Basics/IndexIter.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/LogMacros.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogLeader.h"

using namespace arangodb;
using namespace replication2;


// Take a snapshot, and *after* that write a "begin" entry in each involved
// replicated log.
Result ReplicatedTransactionState::beginTransaction(transaction::Hints hints) {
  LOG_TOPIC("60049", FATAL, Logger::REPLICATION2) << "Not implemented";
  FATAL_ERROR_ABORT();

  // Take a RocksDB snapshot. This has to be done before any entry is written to
  // the replicated log.
  auto result = RocksDBTransactionState::beginTransaction(hints);

  if (result.fail()) {
    return result;
  }

  for (auto const& log : getUniqueLogs()) {
    using namespace std::string_literals;
    // TODO this is a stub
    //      log shouldn't even be a LogLeader, plus we need some proper
    //      "BeginTransaction" type for a log entry to insert.
    //      Possibly this must either be tagged for the LogMultiplexer so it
    //      touches all streams (i.e. for all participating collections/shards),
    //      and/or a list of participating collections (in this particular log).
    std::ignore = log->insert(LogPayload::createFromString(
                                  "begin transaction "s + std::to_string(id().id())),
                              waitForSync());
  }
}

// Write a "commit" entry in each involved replicated log.
// Wait for the entry to be committed (in the replicated log sense, in
// all participating replicated logs), *then* commit the transaction in
// RocksDB.
Result ReplicatedTransactionState::commitTransaction(transaction::Methods* trx) {
  LOG_TOPIC("60050", FATAL, Logger::REPLICATION2) << "Not implemented";
  FATAL_ERROR_ABORT();

  // TODO Write a "commit" entry in each involved replicated log.
  //      Wait for the entry to be committed (in the replicated log sense, in
  //      all participating replicated logs), then commit the transaction in
  //      RocksDB.

  auto const logs = getUniqueLogs();
  auto indexes = std::vector<LogIndex>();
  indexes.reserve(logs.size());
  auto futures =
      std::vector<std::remove_pointer_t<decltype(logs)::value_type>::WaitForFuture>();
  futures.reserve(indexes.size());

  std::transform(logs.cbegin(), logs.cend(), std::back_inserter(indexes), [&](auto const& log) {
    using namespace std::string_literals;
    // TODO this is a stub
    //      log shouldn't even be a LogLeader, plus we need some proper
    //      "CommitTransaction" type for a log entry to insert.
    return log->insert(LogPayload::createFromString("commit transaction "s +
                                                    std::to_string(id().id())),
                       waitForSync());
  });

  auto const n = logs.size();
  for (auto i = std::size_t(0); i < n; ++i) {
    auto const& log = logs[i];
    auto const logIdx = indexes[i];
    futures.emplace_back(log->waitFor(logIdx));
  }

  FATAL_ERROR_EXIT();
  // TODO Take care of ownership here. While transaction::Methods has (shared)
  //      ownership of this TransactionState, that part's fine, but I'm not sure
  //      whether responsibility for the Methods is or must be taken care of.
  // auto future = futures::collectAll(futures).thenFinal(
  //     [this,trx](auto&&) noexcept {
  //       return RocksDBTransactionState::commitTransaction(trx);
  //     });

  // TODO return a Future instead of waiting

  // return future.get();
}

// Write an "abort" entry in each involved replicated log and abort the local
// RocksDB transaction.
Result ReplicatedTransactionState::abortTransaction(transaction::Methods* trx) {
  LOG_TOPIC("60051", FATAL, Logger::REPLICATION2) << "Not implemented";
  FATAL_ERROR_ABORT();

  // TODO Write an "abort" entry in each involved replicated log, and abort the
  //      transaction locally.
  for (auto const& log : getUniqueLogs()) {
    using namespace std::string_literals;
    // TODO this is a stub
    //      log shouldn't even be a LogLeader, plus we need some proper
    //      "CommitTransaction" type for a log entry to insert.
    std::ignore = log->insert(LogPayload::createFromString("commit transaction "s +
                                                           std::to_string(id().id())),
                              waitForSync());
  }

  return RocksDBTransactionState::abortTransaction(trx);
}

ReplicatedTransactionState::ReplicatedTransactionState(TRI_vocbase_t& vocbase,
                                                       TransactionId tid,
                                                       transaction::Options const& options)
    : RocksDBTransactionState(vocbase, tid, options) {}

void ReplicatedTransactionState::insertCollectionAt(CollectionNotFound position,
                                                    std::unique_ptr<TransactionCollection> trxColl) {
  auto replicatedLog = std::optional<std::shared_ptr<replicated_log::LogLeader>>();
  auto& ci = vocbase().server().getFeature<ClusterFeature>().clusterInfo();
  TRI_ASSERT(isShardName(trxColl->collectionName()));
  auto const logId = ci.getLogIdOfShard(vocbase().name(), trxColl->collectionName());
  if (!logId.has_value()) {
    basics::abortOrThrow(
        TRI_ERROR_INTERNAL,
        basics::StringUtils::concatT("No replicated log for shard ",
                                     trxColl->collectionName(), " found"),
        ADB_HERE);
  }
  replicatedLog = vocbase().getReplicatedLogLeaderById(*logId);
  TransactionState::insertCollectionAt(position, std::move(trxColl));
  _replicatedLogs.insert(std::next(this->_replicatedLogs.begin(), position.lowerBound),
                         *replicatedLog);
  TRI_ASSERT(_collections.size() == _replicatedLogs.size());
}

auto ReplicatedTransactionState::getUniqueLogs() const
    -> std::vector<replicated_log::LogLeader*> {
  auto logs = std::vector<replicated_log::LogLeader*>();
  logs.reserve(_replicatedLogs.size());
  std::transform(_replicatedLogs.cbegin(), _replicatedLogs.cend(),
                 std::back_inserter(logs),
                 [&](auto const& logPtr) { return logPtr.get(); });
  std::sort(logs.begin(), logs.end());
  auto last = std::unique(logs.begin(), logs.end());
  logs.erase(last, logs.end());
  return logs;
}
