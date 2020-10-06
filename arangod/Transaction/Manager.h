////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Identifier.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ReadWriteSpinLock.h"
#include "Basics/Result.h"
#include "Logger/LogMacros.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/voc-types.h"

#include <atomic>
#include <functional>
#include <unordered_map>
#include <vector>

namespace arangodb {
class TransactionState;
// to be derived by storage engines
struct TransactionData {
  virtual ~TransactionData() = default;
};

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace transaction {
class Context;
class ManagerFeature;
struct Options;

/// @brief Tracks TransasctionState instances
class Manager final {
  static constexpr size_t numBuckets = 16;
  static constexpr double idleTTL = 10.0;                          // 10 seconds
  static constexpr double idleTTLDBServer = 3 * 60.0;              //  3 minutes
  static constexpr double tombstoneTTL = 10.0 * 60.0;              // 10 minutes
  static constexpr size_t maxTransactionSize = 128 * 1024 * 1024;  // 128 MiB

  enum class MetaType : uint8_t {
    Managed = 1,        /// global single shard db transaction
    StandaloneAQL = 2,  /// used for a standalone transaction (AQL standalone)
    Tombstone = 3  /// used to ensure we can acknowledge double commits / aborts
  };

  struct ManagedTrx {
    ManagedTrx(MetaType t, double ttl,
               std::shared_ptr<TransactionState> st);
    ~ManagedTrx();

    bool hasPerformedIntermediateCommits() const;
    bool expired() const;
    void updateExpiry();

   public:
    /// @brief managed, AQL or tombstone
    MetaType type;            
    /// @brief whether or not the transaction has performed any intermediate
    /// commits
    bool intermediateCommits;
    /// @brief  final TRX state that is valid if this is a tombstone
    /// necessary to avoid getting error on a 'diamond' commit or accidentally
    /// repeated commit / abort messages
    transaction::Status finalStatus;
    const double timeToLive;
    double expiryTime;  // time this expires
    std::shared_ptr<TransactionState> state;  /// Transaction, may be nullptr
    std::string user;         /// user owning the transaction
    std::string db;           /// database in which the transaction operates
    /// cheap usage lock for _state
    mutable basics::ReadWriteSpinLock rwlock;
  };

 public:
  typedef std::function<void(TransactionId, TransactionData const*)> TrxCallback;

  Manager(Manager const&) = delete;
  Manager& operator=(Manager const&) = delete;

  explicit Manager(ManagerFeature& feature);

  // register a transaction
  void registerTransaction(TransactionId transactionId, bool isReadOnlyTransaction,
                           bool isFollowerTransaction);

  // unregister a transaction
  void unregisterTransaction(TransactionId transactionId, bool isReadOnlyTransaction,
                             bool isFollowerTransaction);

  uint64_t getActiveTransactionCount();

  void disallowInserts() {
    _disallowInserts.store(true, std::memory_order_release);
  }

  /// @brief register a AQL transaction
  void registerAQLTrx(std::shared_ptr<TransactionState> const&);
  void unregisterAQLTrx(TransactionId tid) noexcept;

  /// @brief create managed transaction
  Result createManagedTrx(TRI_vocbase_t& vocbase, TransactionId tid,
                          velocypack::Slice trxOpts);

  /// @brief create managed transaction
  Result createManagedTrx(TRI_vocbase_t& vocbase, TransactionId tid,
                          std::vector<std::string> const& readCollections,
                          std::vector<std::string> const& writeCollections,
                          std::vector<std::string> const& exclusiveCollections,
                          transaction::Options options,
                          double ttl = 0.0);

  /// @brief lease the transaction, increases nesting
  std::shared_ptr<transaction::Context> leaseManagedTrx(TransactionId tid,
                                                        AccessMode::Type mode);
  void returnManagedTrx(TransactionId) noexcept;

  /// @brief get the meta transasction state
  transaction::Status getManagedTrxStatus(TransactionId, std::string const& database) const;

  Result commitManagedTrx(TransactionId, std::string const& database);
  Result abortManagedTrx(TransactionId, std::string const& database);

  /// @brief collect forgotten transactions
  bool garbageCollect(bool abortAll);

  /// @brief abort all transactions matching
  bool abortManagedTrx(std::function<bool(TransactionState const&, std::string const&)>);

  /// @brief abort all managed write transactions
  Result abortAllManagedWriteTrx(std::string const& username, bool fanout);

  /// @brief convert the list of running transactions to a VelocyPack array
  /// the array must be opened already.
  /// will use database and username to fan-out the request to the other
  /// coordinators in a cluster
  void toVelocyPack(arangodb::velocypack::Builder& builder, std::string const& database,
                    std::string const& username, bool fanout) const;

  // ---------------------------------------------------------------------------
  // Hotbackup Stuff
  // ---------------------------------------------------------------------------

  // temporarily block all new transactions
  template <typename TimeOutType>
  bool holdTransactions(TimeOutType timeout) {
    bool ret = false;
    std::unique_lock<std::mutex> guard(_mutex);
    if (!_writeLockHeld) {
      LOG_TOPIC("eedda", TRACE, Logger::TRANSACTIONS) << "Trying to get write lock to hold transactions...";
      ret = _rwLock.lockWrite(timeout);
      if (ret) {
        LOG_TOPIC("eeddb", TRACE, Logger::TRANSACTIONS) << "Got write lock to hold transactions.";
        _writeLockHeld = true;
      } else {
        LOG_TOPIC("eeddc", TRACE, Logger::TRANSACTIONS) << "Did not get write lock to hold transactions.";
      }
    }
    return ret;
  }

  // remove the block
  void releaseTransactions() {
    std::unique_lock<std::mutex> guard(_mutex);
    if (_writeLockHeld) {
      LOG_TOPIC("eeddd", TRACE, Logger::TRANSACTIONS) << "Releasing write lock to hold transactions.";
      _rwLock.unlockWrite();
      _writeLockHeld = false;
    }
  }

 private:
  /// @brief performs a status change on a transaction using a timeout
  Result statusChangeWithTimeout(TransactionId tid, std::string const& database,
                                 transaction::Status status);

  /// @brief hashes the transaction id into a bucket
  inline size_t getBucket(TransactionId tid) const {
    return std::hash<TransactionId>()(tid) % numBuckets;
  }

  Result updateTransaction(TransactionId tid, transaction::Status status,
                           bool clearServers, std::string const& database = "" /* leave empty to operate across all databases */);

  /// @brief calls the callback function for each managed transaction
  void iterateManagedTrx(std::function<void(TransactionId, ManagedTrx const&)> const&) const;
  
  static double ttlForType(Manager::MetaType);
  
 private:

  ManagerFeature& _feature;

  struct {
    // a lock protecting _managed
    mutable basics::ReadWriteLock _lock;

    // managed transactions, seperate lifetime from above
    std::unordered_map<TransactionId, ManagedTrx> _managed;
  } _transactions[numBuckets];

  /// Nr of running transactions
  std::atomic<uint64_t> _nrRunning;
  std::atomic<uint64_t> _nrReadLocked;

  std::atomic<bool> _disallowInserts;

  std::mutex _mutex;  // Makes sure that we only ever get or release the
                      // write lock and adjust _writeLockHeld at the same
                      // time.
  basics::ReadWriteLock _rwLock;
  bool _writeLockHeld;

  double _streamingLockTimeout;
};
}  // namespace transaction
}  // namespace arangodb

#endif
