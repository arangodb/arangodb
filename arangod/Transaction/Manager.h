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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Identifier.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ReadWriteSpinLock.h"
#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Cluster/CallbackGuard.h"
#include "Logger/LogMacros.h"
#include "Metrics/Fwd.h"
#include "Scheduler/FutureLock.h"
#include "Transaction/ManagedContext.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/voc-types.h"

#include <absl/hash/hash.h>

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace arangodb {
class TransactionState;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace transaction {
class Context;
class CounterGuard;
class ManagerFeature;
class Hints;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
class History;
#endif
struct Options;

struct IManager {
  virtual ~IManager() = default;
  virtual futures::Future<Result> abortManagedTrx(
      TransactionId, std::string const& database) = 0;
};

/// @brief Tracks TransactionState instances
class Manager final : public IManager {
  friend class CounterGuard;

  static constexpr size_t numBuckets = 16;
  static constexpr double tombstoneTTL = 10.0 * 60.0;  // 10 minutes

  enum class MetaType : uint8_t {
    Managed = 1,        /// global single shard db transaction
    StandaloneAQL = 2,  /// used for a standalone transaction (AQL standalone)
    Tombstone = 3  /// used to ensure we can acknowledge double commits / aborts
  };

  struct ManagedTrx {
    // This class belongs to the Manager and its methods. The members of
    // this class are protected by the lock on the bucket in
    // Manager::_transactions.
    ManagedTrx(ManagerFeature const& feature, MetaType type, double ttl,
               std::shared_ptr<TransactionState> state,
               arangodb::cluster::CallbackGuard rGuard);
    ~ManagedTrx();

    bool hasPerformedIntermediateCommits() const noexcept;
    bool expired() const noexcept;
    void updateExpiry() noexcept;

    /// Note that all members in this class are protected by the
    /// member `rwlock` below. One must only read members whilst
    /// holding the read lock and must only change members whilst
    /// holding the write lock. Otherwise ASAN will be unhappy with us!

    /// @brief managed, AQL or tombstone
    MetaType type;
    /// @brief whether or not the transaction has performed any intermediate
    /// commits
    bool intermediateCommits;
    /// @brief whether or not the transaction did expire at least once
    bool wasExpired;

    /// @brief number of (reading) side users of the transaction. this number
    /// is currently only increased on DB servers when they handle incoming
    /// requests by the AQL document function. while this number is > 0, there
    /// are still read requests ongoing, and the transaction status cannot be
    /// changed to committed/aborted.
    std::atomic<uint32_t> sideUsers;
    /// @brief  final TRX state that is valid if this is a tombstone
    /// necessary to avoid getting error on a 'diamond' commit or accidentally
    /// repeated commit / abort messages
    transaction::Status finalStatus;
    double const timeToLive;
    double expiryTime;                        // time this expires
    std::shared_ptr<TransactionState> state;  /// Transaction, may be nullptr
    arangodb::cluster::CallbackGuard rGuard;
    std::string const user;  /// user owning the transaction
    std::string db;          /// database in which the transaction operates
    SchedulerWrapper _schedulerWrapper;
    // The following lock protects access to the above `TransactionState`
    // in the `state` member. For a read-only transaction, multiple
    // threads (or coroutines) are allowed to hold the read lock. For
    // read/write transactions, only one thread (or coroutine) must hold the
    // write lock. One acquires the lock by calling `leaseManagedTrx`
    // on the manager and returns the lock by calling `returnManagedTrx`.
    // Usually this will be done by holding a copy of the `state` shared_ptr
    // in an object of type `transaction::Context` (or any of its derived
    // classes), such that the destructor of this context objects will
    // call `returnManagedTrx`.
    mutable FutureLock rwlock;
  };

 public:
  Manager(Manager const&) = delete;
  Manager& operator=(Manager const&) = delete;

  explicit Manager(ManagerFeature& feature);
  ~Manager();

  static constexpr double idleTTLDBServer = 5 * 60.0;  //  5 minutes

  // register a transaction, note that this does **not** store the
  // transaction in the Manager, and it does not make the transaction
  // into a "managed" transaction. It only counts it in an atomic CounterGuard
  // in the Manager. The CounterGuard object will do this and will
  // automatically count down again in the destructor!
  std::shared_ptr<CounterGuard> registerTransaction(TransactionId transactionId,
                                                    bool isReadOnlyTransaction,
                                                    bool isFollowerTransaction);

  // The following concerns only transactions which are counted by the
  // Manager via CounterGuards (see registerTransaction), but will not
  // count "managed" transactions!
  uint64_t getActiveTransactionCount();

  void disallowInserts() noexcept {
    _disallowInserts.store(true, std::memory_order_release);
  }

  arangodb::cluster::CallbackGuard buildCallbackGuard(
      TransactionState const& state);

  /// @brief register a AQL transaction, this will make the transaction
  /// a managed transaction which is stored in the Manager. If this does
  /// not work, an exception is thrown.
  void registerAQLTrx(std::shared_ptr<TransactionState> const&);
  void unregisterAQLTrx(TransactionId tid) noexcept;

  /// @brief create managed transaction, also generate a tranactionId
  futures::Future<ResultT<TransactionId>> createManagedTrx(
      TRI_vocbase_t& vocbase, velocypack::Slice trxOpts,
      OperationOrigin operationOrigin, bool allowDirtyReads);

  /// @brief ensure managed transaction, either use the one on the given tid
  ///        or create a new one with the given tid
  futures::Future<Result> ensureManagedTrx(TRI_vocbase_t& vocbase,
                                           TransactionId tid,
                                           velocypack::Slice trxOpts,
                                           OperationOrigin operationOrigin,
                                           bool isFollowerTransaction);

  /// @brief ensure managed transaction, either use the one on the given tid
  ///        or create a new one with the given tid
  futures::Future<Result> ensureManagedTrx(
      TRI_vocbase_t& vocbase, TransactionId tid,
      std::vector<std::string> const& readCollections,
      std::vector<std::string> const& writeCollections,
      std::vector<std::string> const& exclusiveCollections, Options options,
      OperationOrigin operationOrigin, double ttl);

 private:
  futures::Future<Result> beginTransaction(
      transaction::Hints hints, std::shared_ptr<TransactionState>& state);

 public:
  /// @brief lease the transaction, increases nesting, this will acquire
  /// the `rwlock` in the `ManagedTrx` struct either for reading or for
  /// writing. One has to use `returnManagedTrx` to release this lock
  /// eventually. This is usually done through the destructor of the
  /// `transaction::Context` object, which holds a copy of the
  /// `shared_ptr<TransactionState>` and will automatically call
  /// `returnManagedTrx` on destruction.
  futures::Future<std::shared_ptr<transaction::Context>> leaseManagedTrx(
      TransactionId tid, AccessMode::Type mode, bool isSideUser);
  void returnManagedTrx(TransactionId, bool isSideUser) noexcept;

  /// @brief get the meta transaction state
  transaction::Status getManagedTrxStatus(TransactionId,
                                          std::string const& database) const;

  futures::Future<Result> commitManagedTrx(TransactionId,
                                           std::string const& database);
  futures::Future<Result> abortManagedTrx(TransactionId,
                                          std::string const& database) override;

  /// @brief collect forgotten transactions
  bool garbageCollect(bool abortAll);

  /// @brief abort all transactions matching
  bool abortManagedTrx(
      std::function<bool(TransactionState const&, std::string const&)>);

  /// @brief abort all managed write transactions
  Result abortAllManagedWriteTrx(std::string const& username, bool fanout);

  /// @brief convert the list of running transactions to a VelocyPack array
  /// the array must be opened already.
  /// will use database and username to fan-out the request to the other
  /// coordinators in a cluster
  void toVelocyPack(arangodb::velocypack::Builder& builder,
                    std::string const& database, std::string const& username,
                    bool fanout) const;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  /// @brief a history of currently ongoing and recently finished
  /// transactions. currently only used for testing. NOT AVAILABLE
  /// IN PRODUCTION!
  History& history() noexcept;
#endif

  // ---------------------------------------------------------------------------
  // Hotbackup Stuff
  // ---------------------------------------------------------------------------

  // temporarily block all transactions from committing, this is needed
  // on coordinators to produce a consistent hotbackup. Therefore, all
  // transactions (even the non-managed ones) must call
  // `getTransactionCommitGuard` below before they commit!
  template<typename TimeOutType>
  bool holdTransactions(TimeOutType timeout) {
    bool ret = false;
    std::unique_lock<std::mutex> guard(_hotbackupMutex);
    if (!_hotbackupCommitLockHeld) {
      LOG_TOPIC("eedda", TRACE, Logger::TRANSACTIONS)
          << "Trying to get write lock to hold transactions...";
      ret = _hotbackupCommitLock.tryLockWriteFor(timeout);
      if (ret) {
        LOG_TOPIC("eeddb", TRACE, Logger::TRANSACTIONS)
            << "Got write lock to hold transactions.";
        _hotbackupCommitLockHeld = true;
      } else {
        LOG_TOPIC("eeddc", TRACE, Logger::TRANSACTIONS)
            << "Did not get write lock to hold transactions.";
      }
    }
    return ret;
  }

  // remove the block
  void releaseTransactions() noexcept;

  using TransactionCommitGuard = basics::ReadLocker<basics::ReadWriteLock>;

  TransactionCommitGuard getTransactionCommitGuard();

  void initiateSoftShutdown() {
    _softShutdownOngoing.store(true, std::memory_order_relaxed);
  }

 private:
  /// @brief create managed transaction, also generate a tranactionId
  futures::Future<ResultT<TransactionId>> createManagedTrx(
      TRI_vocbase_t& vocbase, std::vector<std::string> const& readCollections,
      std::vector<std::string> const& writeCollections,
      std::vector<std::string> const& exclusiveCollections, Options options,
      OperationOrigin operationOrigin);

  Result prepareOptions(transaction::Options& options);
  bool isFollowerTransactionOnDBServer(
      transaction::Options const& options) const;
  futures::Future<Result> lockCollections(
      TRI_vocbase_t& vocbase, std::shared_ptr<TransactionState> state,
      std::vector<std::string> const& exclusiveCollections,
      std::vector<std::string> const& writeCollections,
      std::vector<std::string> const& readCollections);
  transaction::Hints ensureHints(transaction::Options& options) const;

  /// @brief performs a status change on a transaction using a timeout
  futures::Future<Result> statusChangeWithTimeout(TransactionId tid,
                                                  std::string const& database,
                                                  transaction::Status status);

  /// @brief hashes the transaction id into a bucket
  inline size_t getBucket(TransactionId tid) const noexcept {
    return absl::Hash<uint64_t>()(tid.id()) % numBuckets;
  }

  std::shared_ptr<ManagedContext> buildManagedContextUnderLock(
      TransactionId tid, ManagedTrx& mtrx);

  futures::Future<Result> updateTransaction(
      TransactionId tid, transaction::Status status, bool clearServers,
      std::string const& database =
          "" /* leave empty to operate across all databases */);

  /// @brief calls the callback function for each managed transaction
  void iterateManagedTrx(
      std::function<void(TransactionId, ManagedTrx const&)> const&) const;

  static double ttlForType(ManagerFeature const& feature, Manager::MetaType);

  bool transactionIdExists(TransactionId const& tid) const;

  bool storeManagedState(TransactionId const& tid,
                         std::shared_ptr<arangodb::TransactionState> state,
                         double ttl);

 private:
  ManagerFeature& _feature;

  // There is a danger of deadlock between the `_lock` in the bucket here
  // and the rwlock in the `ManagedTrx` object, if some thread tries to
  // acquire them in this order and the other in that order. Since it is
  // possible to hold the rwlock across method calls to the manager
  // (see leaseManagedTrx/returnManagedTrx), we must never ever acquire
  // ManagedTrx::rwlock whilst we are holding _lock in the bucket!
  // The other order is allowed.
  struct {
    // This is a lock protecting _managed, as well as the members of the
    // `ManagedTrx` objects behind the shared_ptrs in the map.
    // Note it is crucial to not hold this lock whilst trying
    // to lock the rwlock in the `ManagedTrx` instances behind the
    // shared_ptr! If we did this, we would run into the danger that
    // threads which need to lock _lock for writing could be blocked
    // and this could create an entirely new "all threads are blocked"
    // situation! (see also the comment above this struct about
    // deadlock!) Therefore: Always get _lock, then look up and copy
    // the shared_ptr<ManagedTrx>, release _lock, and then lock the
    // ManagedTrx!
    // If you need to recheck the members of `ManagedTrx` after that, you
    // have to re-acquire _lock!
    mutable basics::ReadWriteLock _lock;

    // managed transactions, separate lifetime from above
    std::unordered_map<TransactionId, std::shared_ptr<ManagedTrx>> _managed;
  } _transactions[numBuckets];

  // Nr of running transactions, this only counts the non-managed ones
  // which register themselves with `registerTransaction` in the Manager!
  std::atomic<uint64_t> _nrRunning;

  std::atomic<bool> _disallowInserts;

  std::mutex _hotbackupMutex;  // Makes sure that we only ever get or release
                               // the write lock and adjust _writeLockHeld at
                               // the same time.
  basics::ReadWriteLock _hotbackupCommitLock;
  bool _hotbackupCommitLockHeld;

  double _streamingLockTimeout;

  std::atomic<bool> _softShutdownOngoing;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  /// @brief a history of currently ongoing and recently finished
  /// transactions. currently only used for testing. NOT AVAILABLE
  /// IN PRODUCTION!
  std::unique_ptr<History> _history;
#endif
};

// this RAII object is responsible for increasing and decreasing the
// number of running transactions in the manager safely, so that it can't
// be forgotten.
class CounterGuard {
  CounterGuard(CounterGuard const&) = delete;
  CounterGuard& operator=(CounterGuard const&) = delete;

 public:
  explicit CounterGuard(Manager& manager);
  ~CounterGuard();

 private:
  Manager& _manager;
};

}  // namespace transaction
}  // namespace arangodb
