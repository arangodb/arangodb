////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_STORAGE_ENGINE_TRANSACTION_MANAGER_H
#define ARANGOD_STORAGE_ENGINE_TRANSACTION_MANAGER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/voc-types.h"

#include <atomic>
#include <vector>

namespace arangodb {

class TransactionState;
// to be derived by storage engines
struct TransactionData {
  virtual ~TransactionData() = default;
  TransactionState* _state = nullptr;
  /// @brief expiry time
  double _expires;
};

class TransactionManager {
  static constexpr size_t numBuckets = 16;
  static constexpr double defaultTTL = 60.0;

 public:
  TransactionManager() : _nrRunning(0) {}
  virtual ~TransactionManager() {}

 public:
  typedef std::function<void(TRI_voc_tid_t, TransactionData const*)> TrxCallback;
  enum class Ownership : bool { Lease = true, Move = false };

 public:
  static bool isChildTransactionId(TRI_voc_tid_t);
  static bool isCoordinatorTransactionId(TRI_voc_tid_t);
  static bool isFollowerTransactionId(TRI_voc_tid_t);
  static bool isLeaderTransactionId(TRI_voc_tid_t);
  static bool isLegacyTransactionId(TRI_voc_tid_t);

 public:
  // register a list of failed transactions
  void registerFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions);

  // unregister a list of failed transactions
  void unregisterFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions);

  // return the set of failed transactions
  std::unordered_set<TRI_voc_tid_t> getFailedTransactions() const;

  // register a transaction
  void registerTransaction(TransactionState&, std::unique_ptr<TransactionData> data);

  // unregister a transaction
  void unregisterTransaction(TRI_voc_tid_t transactionId, bool markAsFailed);

  // iterate all the active transactions
  void iterateActiveTransactions(TrxCallback const&);

  uint64_t getActiveTransactionCount();

  /// @brief lease the transaction, increases nesting
  TransactionState* lookup(TRI_voc_tid_t, Ownership action) const;

  /// @brief collect forgotten transactions
  void garbageCollect();

 protected:
  virtual bool keepTransactionData(TransactionState const&) const = 0;

 private:
  // hashes the transaction id into a bucket
  inline size_t getBucket(TRI_voc_tid_t id) const {
    return std::hash<TRI_voc_cid_t>()(id) % numBuckets;
  }

  // a lock protecting ALL buckets in _transactions
  mutable basics::ReadWriteLock _allTransactionsLock;

  struct {
    // a lock protecting _activeTransactions and _failedTransactions
    mutable basics::ReadWriteLock _lock;

    // currently ongoing transactions
    std::unordered_map<TRI_voc_tid_t, std::unique_ptr<TransactionData>> _activeTransactions;

    // set of failed transactions
    std::unordered_set<TRI_voc_tid_t> _failedTransactions;
  } _transactions[numBuckets];

  /// Nr of running transactions
  std::atomic<uint64_t> _nrRunning;
};
}  // namespace arangodb

#endif
