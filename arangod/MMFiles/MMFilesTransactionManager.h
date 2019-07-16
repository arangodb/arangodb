////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_MMFILES_TRANSACTION_MANAGER_H
#define ARANGOD_MMFILES_MMFILES_TRANSACTION_MANAGER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "StorageEngine/TransactionManager.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class MMFilesTransactionManager final : public TransactionManager {
  static constexpr size_t numBuckets = 16;

 public:
  MMFilesTransactionManager() : TransactionManager() {}
  ~MMFilesTransactionManager() {}

  // register a list of failed transactions
  void registerFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions) override;

  // unregister a list of failed transactions
  void unregisterFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions) override;

  // return the set of failed transactions
  std::unordered_set<TRI_voc_tid_t> getFailedTransactions() override;

  // register a transaction
  void registerTransaction(TRI_voc_tid_t transactionId,
                           std::unique_ptr<TransactionData> data,
                           bool isReadOnlyTransaction) override;

  // unregister a transaction
  void unregisterTransaction(TRI_voc_tid_t transactionId, bool markAsFailed, bool isReadOnlyTransaction) override;

  // iterate all the active transactions
  void iterateActiveTransactions(
      std::function<void(TRI_voc_tid_t, TransactionData const*)> const& callback) override;

  uint64_t getActiveTransactionCount() override;

 private:
  // hashes the transaction id into a bucket
  inline size_t getBucket(TRI_voc_tid_t id) const {
    return std::hash<TRI_voc_cid_t>()(id) % numBuckets;
  }

  // a lock protecting ALL buckets in _transactions
  basics::ReadWriteLock _allTransactionsLock;

  struct {
    // a lock protecting _activeTransactions and _failedTransactions
    basics::ReadWriteLock _lock;

    // currently ongoing transactions
    std::unordered_map<TRI_voc_tid_t, std::unique_ptr<TransactionData>> _activeTransactions;

    // set of failed transactions
    std::unordered_set<TRI_voc_tid_t> _failedTransactions;
  } _transactions[numBuckets];
};

}  // namespace arangodb

#endif
