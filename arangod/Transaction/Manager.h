////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/ReadWriteLock.h"
#include "Basics/ReadWriteSpinLock.h"
#include "Basics/Result.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"

#include <atomic>
#include <functional>
#include <map>
#include <set>
#include <vector>

namespace arangodb {
class TransactionState;
// to be derived by storage engines
struct TransactionData {
  virtual ~TransactionData() = default;
};

namespace transaction {
class Context;
struct Options;

/// @bried Tracks TransasctionState instances 
class Manager final {
  static constexpr size_t numBuckets = 16;
  static constexpr double defaultTTL = 10.0 * 60.0;   // 10 minutes
  static constexpr double tombstoneTTL = 5.0 * 60.0;  // 5 minutes

 public:
  explicit Manager(bool keepData) : _keepTransactionData(keepData), _nrRunning(0) {}

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
  
  /// @brief register a AQL transaction
  void registerAQLTrx(TransactionState*);
  void unregisterAQLTrx(TRI_voc_tid_t tid) noexcept;
  
  /// @brief create managed transaction
  Result createManagedTrx(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
                          velocypack::Slice const trxOpts);
  
  /// @brief create managed transaction
  Result createManagedTrx(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
                          std::vector<std::string> const& readCollections,
                          std::vector<std::string> const& writeCollections,
                          std::vector<std::string> const& exclusiveCollections,
                          transaction::Options const& options);
  
  /// @brief lease the transaction, increases nesting
  std::shared_ptr<transaction::Context> leaseManagedTrx(TRI_voc_tid_t tid,
                                                        AccessMode::Type mode);
  void returnManagedTrx(TRI_voc_tid_t, AccessMode::Type mode) noexcept;
  
  /// @brief get the meta transasction state
  transaction::Status getManagedTrxStatus(TRI_voc_tid_t) const;
    
  Result commitManagedTrx(TRI_voc_tid_t);
  Result abortManagedTrx(TRI_voc_tid_t);
  
  /// @brief collect forgotten transactions
  bool garbageCollect(bool abortAll);
  
  /// @brief abort all transactions matching
  bool abortManagedTrx(std::function<bool(TransactionState const&)>);
  
 private:
  // hashes the transaction id into a bucket
  inline size_t getBucket(TRI_voc_tid_t tid) const {
    return std::hash<TRI_voc_cid_t>()(tid) % numBuckets;
  }
  
  Result updateTransaction(TRI_voc_tid_t tid, transaction::Status status,
                           bool clearServers);
  
 private:
    
  enum class MetaType : uint8_t {
    Managed = 1,  /// global single shard db transaction
    StandaloneAQL = 2,  /// used for a standalone transaction (AQL standalone)
    Tombstone = 3  /// used to ensure we can acknowledge double commits / aborts
  };
  struct ManagedTrx {
    ManagedTrx(MetaType t, TransactionState* st, double ex)
      : type(t), expires(ex), state(st), finalStatus(Status::UNDEFINED),
        rwlock() {}
    ~ManagedTrx();
    
    MetaType type;
    double expires; /// expiration timestamp, if 0 it expires immediately
    TransactionState* state; /// Transaction, may be nullptr
    /// @brief  final TRX state that is valid if this is a tombstone
    /// necessary to avoid getting error on a 'diamond' commit or accidantally
    /// repeated commit / abort messages
    transaction::Status finalStatus;
    /// cheap usage lock for *state
    mutable basics::ReadWriteSpinLock rwlock;
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
