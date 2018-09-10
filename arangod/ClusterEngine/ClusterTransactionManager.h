////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_TRANSACTION_MANAGER_H
#define ARANGOD_CLUSTER_CLUSTER_TRANSACTION_MANAGER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "StorageEngine/TransactionManager.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class ClusterTransactionManager final : public TransactionManager {
 public:
  ClusterTransactionManager() : TransactionManager(), _nrRunning(0) {}
  ~ClusterTransactionManager() {}

  // register a list of failed transactions
  void registerFailedTransactions(
      std::unordered_set<TRI_voc_tid_t> const& failedTransactions) override {}

  // unregister a list of failed transactions
  void unregisterFailedTransactions(
      std::unordered_set<TRI_voc_tid_t> const& failedTransactions) override {}

  // return the set of failed transactions
  std::unordered_set<TRI_voc_tid_t> getFailedTransactions() override {
    return std::unordered_set<TRI_voc_tid_t>();
  }

  // register a transaction
  void registerTransaction(TRI_voc_tid_t transactionId,
                           std::unique_ptr<TransactionData> data) override {
    TRI_ASSERT(data == nullptr);
    ++_nrRunning;
  }

  // unregister a transaction
  void unregisterTransaction(TRI_voc_tid_t transactionId,
                             bool markAsFailed) override {
    --_nrRunning;
  }

  // iterate all the active transactions
  void iterateActiveTransactions(
      std::function<void(TRI_voc_tid_t, TransactionData const*)> const&
          callback) override {}

  uint64_t getActiveTransactionCount() override { return _nrRunning; }

 private:
  std::atomic<uint64_t> _nrRunning;
};
}

#endif
