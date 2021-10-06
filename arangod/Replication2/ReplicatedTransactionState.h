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

#pragma once

#include "StorageEngine/TransactionState.h"
#include "RocksDBEngine/RocksDBTransactionState.h"

namespace arangodb::replication2 {

// TODO We need one RocksDBTransactionState per shard, rather than one for all
//      shards.
class ReplicatedTransactionState final : public ::arangodb::RocksDBTransactionState {
 public:
  ReplicatedTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                             transaction::Options const& options);
  ~ReplicatedTransactionState() override = default;

  // TODO commitTransaction must be changed to return a Future. This of course
  //      means changing the TransactionState API and their usages.
  [[nodiscard]] Result beginTransaction(transaction::Hints hints) override;
  [[nodiscard]] Result commitTransaction(transaction::Methods* trx) override;
  [[nodiscard]] Result abortTransaction(transaction::Methods* trx) override;

 protected:
  void insertCollectionAt(CollectionNotFound position,
                          std::unique_ptr<TransactionCollection> trxColl) override;

 private:

  // Make a unique list of logs. This is necessary because multiple collections
  // might share the same log (in case of distributeShardsLike), but each log
  // must get only one entry to begin the transaction.
  auto getUniqueLogs() const -> std::vector<replicated_log::LogLeader*>;

  // replicated logs; if existent, must be the same size as _collections, and
  // for each i, _replicatedLogs[i] must be the replicated log used by the
  // collection _collections[i].
  // TODO This should not be LogLeader, but some other interface to the log,
  //      at least a LogMultiplexer, but rather some interface provided for
  //      state machines.
  std::vector<std::shared_ptr<arangodb::replication2::replicated_log::LogLeader>> _replicatedLogs;
};

}  // namespace arangodb::replication2
