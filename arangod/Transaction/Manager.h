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

#ifndef ARANGOD_TRANSACTION_MANAGER_H
#define ARANGOD_TRANSACTION_MANAGER_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ReadWriteSpinLock.h"
#include "Transaction/Status.h"
#include "VocBase/voc-types.h"
#include "VocBase/AccessMode.h"

#include <atomic>
#include <vector>

namespace arangodb {
class TransactionState;
// to be derived by storage engines
struct TransactionData {};
  
namespace transaction {
class Context;

class Manager final {
  static constexpr size_t numBuckets = 16;
  static constexpr double defaultTTL = 600.0;

 public:
  Manager(bool keepData)
    : _keepTransactionData(keepData), _nrRunning(0) {}

 public:
  typedef std::function<void(TRI_voc_tid_t, TransactionData const*)> TrxCallback;

 public:
  // register a list of failed transactions
  void registerFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions);

  // unregister a list of failed transactions
  void unregisterFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions);

  // return the set of failed transactions
  std::unordered_set<TRI_voc_tid_t> getFailedTransactions() const;

  // register a transaction
  void registerTransaction(TRI_voc_tid_t, std::unique_ptr<TransactionData> data);

  // unregister a transaction
  void unregisterTransaction(TRI_voc_tid_t transactionId, bool markAsFailed);

  // iterate all the active transactions
  void iterateActiveTransactions(TrxCallback const&);

  uint64_t getActiveTransactionCount();
  
 public:

  /// @brief collect forgotten transactions
  void garbageCollect();
  
  enum class Ownership : bool { Lease = true, Move = false };
    
  /// @brief register a AQL transaction
  void registerAQLTrx(TransactionState*);
  void unregisterAQLTrx(TRI_voc_tid_t tid) noexcept;
    
  /// @brief create managed transaction
  Result createManagedTrx(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
                          velocypack::Slice const trxOpts);
  
  /// @brief lease the transaction, increases nesting
  std::shared_ptr<transaction::Context> leaseTrx(TRI_voc_tid_t, AccessMode::Type mode,
                                                 Ownership action);
  void returnManagedTrx(TRI_voc_tid_t, AccessMode::Type mode) noexcept;
  
  /// @brief get the meta transasction state
  transaction::Status getManagedTrxStatus(TRI_voc_tid_t) const;
  
 private:
  // hashes the transaction id into a bucket
  inline size_t getBucket(TRI_voc_tid_t id) const {
    return std::hash<TRI_voc_cid_t>()(id) % numBuckets;
  }
    
  enum class MetaType : uint8_t {
    Managed = 1,  /// global single shard db transaction
    StandaloneAQL = 2,  /// used for a standalone transaction (AQL standalone)
  };
  struct ManagedTrx {
    ManagedTrx(MetaType t, TransactionState* st, double ex)
      : type(t), state(st), expires(ex), rwlock() {}
    ~ManagedTrx();
    
    MetaType type;
    TransactionState* state;
    double expires; /// expiration timespamp
    mutable basics::ReadWriteSpinLock rwlock; /// usage lock
  };
  
private:
  
  const bool _keepTransactionData;

  // a lock protecting ALL buckets in _transactions
  mutable basics::ReadWriteLock _allTransactionsLock;

  struct {
    // a lock protecting _activeTransactions and _failedTransactions
    mutable basics::ReadWriteLock _lock;

    // currently ongoing transactions
    std::unordered_map<TRI_voc_tid_t, std::unique_ptr<TransactionData>> _activeTransactions;

    // set of failed transactions
    std::unordered_set<TRI_voc_tid_t> _failedTransactions;
    
    // managed transactions, seperate lifetime from above
    std::unordered_map<TRI_voc_tid_t, ManagedTrx> _managed;
  } _transactions[numBuckets];

  /// Nr of running transactions
  std::atomic<uint64_t> _nrRunning;
};
}  // namespace transaction
}  // namespace arangodb

#endif
