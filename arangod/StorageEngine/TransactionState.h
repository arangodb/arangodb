////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Basics/ReadWriteLock.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/Result.h"
#include "Basics/ResourceUsage.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/Utils/ShardID.h"
#include "Cluster/ServerState.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "Containers/SmallVector.h"
#include "Futures/Future.h"
#include "Metrics/MetricsFeature.h"
#include "Transaction/Hints.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/Options.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/voc-types.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <variant>

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#define LOG_TRX(logid, llevel, trx)                        \
  LOG_TOPIC(logid, llevel, arangodb::Logger::TRANSACTIONS) \
      << "#" << trx->id().id() << " ("                     \
      << transaction::statusString(trx->status()) << "): "

#else

#define LOG_TRX(logid, llevel, ...) \
  while (0) LOG_TOPIC(logid, llevel, arangodb::Logger::TRANSACTIONS)
#endif

struct TRI_vocbase_t;

namespace arangodb {
class CollectionNameResolver;
struct ResourceMonitor;

namespace transaction {
class CounterGuard;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
class HistoryEntry;
#endif
class Methods;
struct Options;
}  // namespace transaction

class TransactionCollection;
struct TransactionStatistics;

/// @brief transaction type
class TransactionState : public std::enable_shared_from_this<TransactionState> {
 public:
  /// @brief an implementation-dependent structure for storing runtime data
  struct Cookie {
    typedef std::unique_ptr<Cookie> ptr;
    virtual ~Cookie() = default;
  };

  [[nodiscard]] static bool ServerIdLessThan(ServerID const& lhs,
                                             ServerID const& rhs) {
    return lhs < rhs;
  }

  typedef std::function<void(TransactionState& state)> StatusChangeCallback;

  TransactionState() = delete;
  TransactionState(TransactionState const&) = delete;
  TransactionState& operator=(TransactionState const&) = delete;

  TransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                   transaction::Options const& options,
                   transaction::OperationOrigin operationOrigin);
  virtual ~TransactionState();

  /// @return a cookie associated with the specified key, nullptr if none
  [[nodiscard]] Cookie* cookie(void const* key) const noexcept;

  /// @brief associate the specified cookie with the specified key
  /// @return the previously associated cookie, if any
  Cookie::ptr cookie(void const* key, Cookie::ptr&& cookie);

  [[nodiscard]] bool isRunningInCluster() const noexcept {
    return ServerState::isRunningInCluster(_serverRole);
  }
  [[nodiscard]] bool isDBServer() const noexcept {
    return ServerState::isDBServer(_serverRole);
  }
  [[nodiscard]] bool isCoordinator() const noexcept {
    return ServerState::isCoordinator(_serverRole);
  }
  [[nodiscard]] ServerState::RoleEnum serverRole() const noexcept {
    return _serverRole;
  }

  [[nodiscard]] transaction::Options& options() noexcept { return _options; }
  [[nodiscard]] transaction::Options const& options() const noexcept {
    return _options;
  }
  [[nodiscard]] TRI_vocbase_t& vocbase() const noexcept { return _vocbase; }
  [[nodiscard]] TransactionId id() const noexcept { return _id; }
  [[nodiscard]] transaction::Status status() const noexcept { return _status; }
  [[nodiscard]] bool isRunning() const noexcept {
    return _status == transaction::Status::RUNNING;
  }
  /// @brief returns the name of the actor the transaction runs on:
  /// - leader
  /// - follower
  /// - coordinator
  /// - single
  [[nodiscard]] std::string_view actorName() const noexcept;

  /// @brief return a reference to the global transaction statistics/counters
  TransactionStatistics& statistics() const noexcept;

  [[nodiscard]] double lockTimeout() const noexcept {
    return _options.lockTimeout;
  }
  void lockTimeout(double value) noexcept {
    if (value > 0.0) {
      _options.lockTimeout = value;
    }
  }

  [[nodiscard]] bool waitForSync() const noexcept {
    return _options.waitForSync;
  }
  void waitForSync(bool value) noexcept { _options.waitForSync = value; }

  [[nodiscard]] bool allowImplicitCollectionsForRead() const noexcept {
    return _options.allowImplicitCollectionsForRead;
  }
  void allowImplicitCollectionsForRead(bool value) noexcept {
    _options.allowImplicitCollectionsForRead = value;
  }

  /// @brief return the collection from a transaction
  [[nodiscard]] TransactionCollection* collection(
      DataSourceId cid, AccessMode::Type accessType) const;

  /// @brief return the collection from a transaction
  [[nodiscard]] TransactionCollection* collection(
      std::string_view name, AccessMode::Type accessType) const;

  /// @brief add a collection to a transaction
  [[nodiscard]] futures::Future<Result> addCollection(
      DataSourceId cid, std::string_view cname, AccessMode::Type accessType,
      bool lockUsage);

  /// @brief use all participating collections of a transaction
  [[nodiscard]] futures::Future<Result> useCollections();

  /// @brief run a callback on all collections of the transaction
  template<typename F>
  void allCollections(F&& cb) {
    RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
    for (auto& trxCollection : _collections) {
      TRI_ASSERT(trxCollection);  // ensured by addCollection(...)
      if (!std::forward<F>(cb)(*trxCollection)) {  // abort early
        return;
      }
    }
  }

  /// @brief return the number of collections in the transaction
  [[nodiscard]] size_t numCollections() const noexcept {
    RECURSIVE_READ_LOCKER(_collectionsLock, _collectionsLockOwner);
    return _collections.size();
  }

  /// @brief whether or not a transaction consists of a single operation
  [[nodiscard]] bool isSingleOperation() const noexcept {
    return hasHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  /// @brief update the status of a transaction
  void updateStatus(transaction::Status status) noexcept;

  /// @brief whether or not a specific hint is set for the transaction
  [[nodiscard]] bool hasHint(transaction::Hints::Hint hint) const noexcept {
    return _hints.has(hint);
  }

  using CommitCallback = std::function<void(TransactionState&)>;
  using BeforeCommitCallback = CommitCallback;
  using AfterCommitCallback = CommitCallback;

  void addBeforeCommitCallback(BeforeCommitCallback const* callback) {
    TRI_ASSERT(callback != nullptr);
    TRI_ASSERT(*callback != nullptr);
    _beforeCommitCallbacks.push_back(callback);
  }

  void addAfterCommitCallback(AfterCommitCallback const* callback) {
    TRI_ASSERT(callback != nullptr);
    TRI_ASSERT(*callback != nullptr);
    _afterCommitCallbacks.push_back(callback);
  }

  void applyBeforeCommitCallbacks() noexcept {
    return applyCallbackImpl(_beforeCommitCallbacks);
  }

  void applyAfterCommitCallbacks() noexcept {
    return applyCallbackImpl(_afterCommitCallbacks);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // only used in maintainer mode for testing
  void setHistoryEntry(std::shared_ptr<transaction::HistoryEntry> const& entry);
  void clearHistoryEntry() noexcept;
  void adjustMemoryUsage(std::int64_t value) noexcept;
#endif

  /// @brief acquire a database snapshot if we do not yet have one.
  /// Returns true if a snapshot was acquired, otherwise false (i.e., if we
  /// already had a snapshot)
  [[nodiscard]] virtual bool ensureSnapshot() = 0;

  /// @brief begin a transaction
  virtual futures::Future<Result> beginTransaction(
      transaction::Hints hints) = 0;

  /// @brief commit a transaction
  virtual futures::Future<Result> commitTransaction(
      transaction::Methods* trx) = 0;

  /// @brief abort a transaction
  virtual Result abortTransaction(transaction::Methods* trx) = 0;

  virtual Result triggerIntermediateCommit() = 0;

  virtual futures::Future<Result> performIntermediateCommitIfRequired(
      DataSourceId cid) = 0;

  /// @returns number of insertions/removals in a transaction, update takes 2
  /// primitive operations
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  [[nodiscard]] virtual uint64_t numPrimitiveOperations() const noexcept = 0;

  /// @brief return number of commits.
  /// for cluster transactions on coordinator, this either returns 0 or 1.
  /// for leader, follower or single-server transactions, this can include any
  /// number, because it will also include intermediate commits.
  virtual uint64_t numCommits() const noexcept = 0;

  virtual uint64_t numIntermediateCommits() const noexcept = 0;

  virtual void addIntermediateCommits(uint64_t value) = 0;

  virtual bool hasFailedOperations() const noexcept = 0;

  virtual void beginQuery(std::shared_ptr<ResourceMonitor> resourceMonitor,
                          bool /*isModificationQuery*/) {}
  virtual void endQuery(bool /*isModificationQuery*/) noexcept {}

  [[nodiscard]] TransactionCollection* findCollection(DataSourceId cid) const;

  /// @brief make a exclusive transaction, only valid before begin
  void setExclusiveAccessType();

  /// @brief whether or not a transaction is read-only
  [[nodiscard]] bool isReadOnlyTransaction() const noexcept;

  /// @brief whether or not a transaction is a follower transaction
  [[nodiscard]] bool isFollowerTransaction() const noexcept;

  /// @brief servers already contacted
  [[nodiscard]] containers::FlatHashSet<ServerID> const& knownServers()
      const noexcept;

  [[nodiscard]] bool knowsServer(std::string_view uuid) const noexcept;

  /// @brief add a server to the known set
  void addKnownServer(std::string_view uuid);

  /// @brief remove a server from the known set
  void removeKnownServer(std::string_view uuid);

  void clearKnownServers();

  void chooseReplicas(containers::FlatHashSet<ShardID> const& shards);

  /// @brief lookup a replica choice for some shard, this basically looks
  /// up things in `_chosenReplicas`, but if the shard is not found there,
  /// it uses `chooseReplicas` to make a choice.
  ServerID whichReplica(ShardID const& shard);
  containers::FlatHashMap<ShardID, ServerID> whichReplicas(
      containers::FlatHashSet<ShardID> const& shardIds);

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  [[nodiscard]] virtual TRI_voc_tick_t lastOperationTick() const noexcept = 0;

  void acceptAnalyzersRevision(
      QueryAnalyzerRevisions const& analyzersRevsion) noexcept;

  [[nodiscard]] transaction::OperationOrigin operationOrigin() const noexcept;

  [[nodiscard]] QueryAnalyzerRevisions const& analyzersRevision()
      const noexcept {
    return _analyzersRevision;
  }

#ifdef USE_ENTERPRISE
  void addInaccessibleCollection(DataSourceId cid, std::string const& cname);
  [[nodiscard]] bool isInaccessibleCollection(DataSourceId cid);
  [[nodiscard]] bool isInaccessibleCollection(std::string_view cname);
#endif

  /// @brief roll a new transaction ID on the coordintor. Use this method
  /// with care, it should only be used when retrying in a synchronized
  /// fashion after a fast-path locking detected a dead-lock situation.
  /// Only allowed on coordinators.
  void coordinatorRerollTransactionId();

  std::shared_ptr<transaction::CounterGuard> counterGuard();

  /// @brief set name of user who originated the transaction. will
  /// only be set if no user has been registered with the transaction yet.
  /// this user name is informational only and can be used for logging,
  /// metrics etc. it should not be used for permission checks.
  void setUsername(std::string const& name);

  /// @brief return name of user who originated the transaction. may be
  /// empty. this user name is informational only and can be used for logging,
  /// metrics etc. it should not be used for permission checks.
  std::string_view username() const noexcept;

  void trackShardRequest(CollectionNameResolver const& resolver,
                         std::string_view database, std::string_view shard,
                         std::string_view user, AccessMode::Type accessMode,
                         std::string_view context) noexcept;

  void trackShardUsage(CollectionNameResolver const& resolver,
                       std::string_view database, std::string_view shard,
                       std::string_view user, AccessMode::Type accessMode,
                       std::string_view context, size_t nBytes) noexcept;

 protected:
  virtual std::unique_ptr<TransactionCollection> createTransactionCollection(
      DataSourceId cid, AccessMode::Type accessType) = 0;

  /// @brief clear the query cache for all collections that were modified by
  /// the transaction
  void clearQueryCache() const;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // reset the internal Transaction ID to none.
  // Only used in the Transaction Mock for internal reasons.
  void resetTransactionId();
#endif

 private:
  /// @brief add the choice of replica for some more shards to the map
  /// _chosenReplicas. Please note that the choice of replicas is not
  /// arbitrary! If two collections have the same `distributeShardsLike`
  /// (or one has the other as `distributeShardsLike`), then the choices for
  /// corresponding shards must be made in a coherent fashion. Therefore:
  /// Do not fill in this map yourself, always use this method for this.
  /// The Nolock version does not acquire the _replicaMutex and is only
  /// called from other, public methods in this class.
  void chooseReplicasNolock(containers::FlatHashSet<ShardID> const& shards);

  template<typename Callbacks>
  void applyCallbackImpl(Callbacks& callbacks) noexcept {
    for (auto& callback : callbacks) {
      try {
        (*callback)(*this);
      } catch (...) {
      }
    }
    callbacks.clear();
  }

  /// @brief check if current user can access this collection
  Result checkCollectionPermission(DataSourceId cid, std::string_view cname,
                                   AccessMode::Type);

  /// @brief helper function for addCollection
  futures::Future<Result> addCollectionInternal(DataSourceId cid,
                                                std::string_view cname,
                                                AccessMode::Type accessType,
                                                bool lockUsage);

  /// @brief find a collection in the transaction's list of collections
  struct CollectionNotFound {
    std::size_t lowerBound;
  };
  struct CollectionFound {
    TransactionCollection* collection;
  };

  [[nodiscard]] auto findCollectionOrPos(DataSourceId cid) const
      -> std::variant<CollectionNotFound, CollectionFound>;

  void publishShardMetrics(CollectionNameResolver const& resolver);

 protected:
  TRI_vocbase_t& _vocbase;  /// @brief vocbase for this transaction

  /// @brief access type (read|write)
  AccessMode::Type _type = AccessMode::Type::READ;
  /// @brief current status
  transaction::Status _status = transaction::Status::CREATED;

  // in case of read-only transactions collections can be added lazily.
  // this can happen concurrently, so for this we need to protect the list
  mutable basics::ReadWriteLock _collectionsLock;
  mutable std::atomic<std::thread::id> _collectionsLockOwner;
  containers::SmallVector<TransactionCollection*, 8> _collections;

  transaction::Hints _hints{};  // hints; set on _nestingLevel == 0

  ServerState::RoleEnum const _serverRole;  /// role of the server

  transaction::Options _options;

  std::vector<BeforeCommitCallback const*> _beforeCommitCallbacks;
  std::vector<AfterCommitCallback const*> _afterCommitCallbacks;

  std::shared_ptr<transaction::CounterGuard> _counterGuard;

 private:
  TransactionId _id;  /// @brief local trx id

  /// a collection of stored cookies
  containers::FlatHashMap<void const*, Cookie::ptr> _cookies;

  /// @brief servers we already talked to for this transactions
  containers::FlatHashSet<ServerID> _knownServers;

  /// @brief current choice of replica to read from for read-from-followers
  /// (aka allowDirtyReads). Please note that the choice of replicas is not
  /// arbitrary! If two collections have the same `distributeShardsLike`
  /// (or one has the other as `distributeShardsLike`), then the choices for
  /// corresponding shards must be made in a coherent fashion. We call these
  /// shards the "shard group" and the shard of the collection, of which all
  /// others are `distributeShardsLike`, the "shard group leader".
  /// The principle is the following: Whenever a shard appears first in a
  /// read-from-follower read-only transaction, then we determine its shard
  /// group leader and decide for it the replica, from which we read for
  /// the whole shard group in this transaction. Note that this needs to
  /// take into account **all** shards of the shard group, since we must be
  /// sure that the server chosen is in sync **for all shards in the group**!
  /// We store this choice in this map here for the shard group leader as well
  /// as for the shard which just occurred.
  /// Do not fill in this map yourself, always use the method `chooseReplicas`
  /// for this. If you use `whichReplica(<shardID>)`, then this happens
  /// automatically. This member is only relevant (and != nullptr) if the
  /// transaction option allowDirtyReads is set.
  std::mutex _replicaMutex;  // protects access to _chosenReplicas
  std::unique_ptr<containers::FlatHashMap<ShardID, ServerID>> _chosenReplicas;

  QueryAnalyzerRevisions _analyzersRevision;

  transaction::OperationOrigin const _operationOrigin;

  metrics::MetricsFeature::UsageTrackingMode _usageTrackingMode;

  /// @brief name of user who originated the transaction. may be empty.
  /// this user name is informational only and can be used for logging,
  /// metrics etc.
  /// it should not be used for permission checks.
  std::shared_mutex mutable _usernameLock;
  std::string _username;

  // protects _shardsBytesWritten and _shardsBytesRead
  std::mutex mutable _shardsMetricsMutex;
  // map from collection name (shard name) to number of bytes written
  containers::FlatHashMap<std::string, size_t> _shardBytesWritten;
  // map from collection name (shard name) to number of bytes read
  containers::FlatHashMap<std::string, size_t> _shardBytesRead;
  // number of times the metrics have been increased since the metrics
  // were last published
  size_t _shardBytesUnpublishedEvents = 0;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::shared_ptr<transaction::HistoryEntry> _historyEntry;
#endif
};

}  // namespace arangodb
