////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/ServerState.h"
#include "Containers/HashSet.h"
#include "Containers/SmallVector.h"
#include "StorageEngine/ITransactionable.h"
#include "Transaction/Hints.h"
#include "Transaction/LogMacros.h"
#include "Transaction/Options.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/voc-types.h"

#include <map>
#include <variant>

struct TRI_vocbase_t;

namespace arangodb {

namespace transaction {
class Methods;
struct Options;
}  // namespace transaction

class TransactionCollection;
struct TransactionStatistics;

/// @brief transaction type
class TransactionState {
 public:
  /// @brief an implementation-dependent structure for storing runtime data
  struct Cookie {
    typedef std::unique_ptr<Cookie> ptr;
    virtual ~Cookie() = default;
  };

  [[nodiscard]] static bool ServerIdLessThan(ServerID const& lhs, ServerID const& rhs) {
    return lhs < rhs;
  }

  typedef std::function<void(TransactionState& state)> StatusChangeCallback;

  TransactionState() = delete;
  TransactionState(TransactionState const&) = delete;
  TransactionState& operator=(TransactionState const&) = delete;

  virtual ~TransactionState();

  // named constructors:
  static auto createRocksDBTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                                            transaction::Options const& options)
      -> std::shared_ptr<TransactionState>;
  static auto createClusterTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                                            transaction::Options const& options)
      -> std::shared_ptr<TransactionState>;

 protected:
  TransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                   transaction::Options const& options,
                   std::unique_ptr<Transactionable> transactionable);

 public:
  /// @return a cookie associated with the specified key, nullptr if none
  [[nodiscard]] Cookie* cookie(void const* key) const noexcept;

  /// @brief associate the specified cookie with the specified key
  /// @return the previously associated cookie, if any
  Cookie::ptr cookie(void const* key, Cookie::ptr&& cookie);

  [[nodiscard]] bool isRunningInCluster() const {
    return ServerState::isRunningInCluster(_serverRole);
  }
  [[nodiscard]] bool isDBServer() const {
    return ServerState::isDBServer(_serverRole);
  }
  [[nodiscard]] bool isCoordinator() const {
    return ServerState::isCoordinator(_serverRole);
  }
  [[nodiscard]] ServerState::RoleEnum serverRole() const { return _serverRole; }

  [[nodiscard]] transaction::Options& options() { return _options; }
  [[nodiscard]] transaction::Options const& options() const {
    return _options;
  }
  [[nodiscard]] TRI_vocbase_t& vocbase() const { return _vocbase; }
  [[nodiscard]] TransactionId id() const { return _id; }
  [[nodiscard]] transaction::Status status() const noexcept { return _transactionable->status(); }
  [[nodiscard]] bool isRunning() const {
    return status() == transaction::Status::RUNNING;
  }
  void setRegistered() noexcept { _registeredTransaction = true; }
  [[nodiscard]] bool wasRegistered() const noexcept {
    return _registeredTransaction;
  }

  /// @brief returns the name of the actor the transaction runs on:
  /// - leader
  /// - follower
  /// - coordinator
  /// - single
  [[nodiscard]] char const* actorName() const noexcept;

  /// @brief return a reference to the global transaction statistics/counters
  TransactionStatistics& statistics() noexcept;

  [[nodiscard]] double lockTimeout() const { return _options.lockTimeout; }
  void lockTimeout(double value) {
    if (value > 0.0) {
      _options.lockTimeout = value;
    }
  }

  [[nodiscard]] bool waitForSync() const { return _options.waitForSync; }
  void waitForSync(bool value) { _options.waitForSync = value; }

  [[nodiscard]] bool allowImplicitCollectionsForRead() const {
    return _options.allowImplicitCollectionsForRead;
  }
  void allowImplicitCollectionsForRead(bool value) {
    _options.allowImplicitCollectionsForRead = value;
  }

  /// @brief return the collection from a transaction
  [[nodiscard]] TransactionCollection* collection(DataSourceId cid,
                                                  AccessMode::Type accessType) const;

  /// @brief return the collection from a transaction
  [[nodiscard]] TransactionCollection* collection(std::string const& name,
                                                  AccessMode::Type accessType) const;

  /// @brief add a collection to a transaction
  [[nodiscard]] Result addCollection(DataSourceId cid, std::string const& cname,
                                     AccessMode::Type accessType, bool lockUsage);

  /// @brief use all participating collections of a transaction
  [[nodiscard]] Result useCollections();

  /// @brief run a callback on all collections of the transaction
  template <typename F>
  void allCollections(F&& cb) {
    for (auto& trxCollection : _collections) {
      TRI_ASSERT(trxCollection);  // ensured by addCollection(...)
      if (!std::forward<F>(cb)(*trxCollection)) {  // abort early
        return;
      }
    }
  }

  /// @brief return the number of collections in the transaction
  [[nodiscard]] size_t numCollections() const { return _collections.size(); }

  /// @brief whether or not a transaction consists of a single operation
  [[nodiscard]] bool isSingleOperation() const {
    return hasHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  /// @brief whether or not a specific hint is set for the transaction
  [[nodiscard]] bool hasHint(transaction::Hints::Hint hint) const {
    return _hints.has(hint);
  }

  /// @brief begin a transaction
  [[nodiscard]] arangodb::Result beginTransaction(transaction::Hints hints) {
    LOG_TRX("e6a2b", TRACE, this)
        << "beginning " << AccessMode::typeString(_type) << " transaction";
    _hints = hints;
    return _transactionable->beginTransaction(hints);
  }

  /// @brief commit a transaction
  [[nodiscard]] arangodb::Result commitTransaction(transaction::Methods* trx) {
    return _transactionable->commitTransaction(trx);
  }

  /// @brief abort a transaction
  [[nodiscard]] arangodb::Result abortTransaction(transaction::Methods* trx) {
    return _transactionable->abortTransaction(trx);
  }

  /// @brief return number of commits.
  /// for cluster transactions on coordinator, this either returns 0 or 1.
  /// for leader, follower or single-server transactions, this can include any
  /// number, because it will also include intermediate commits.
  [[nodiscard]] uint64_t numCommits() const {
    return _transactionable->numCommits();
  }

  [[nodiscard]] bool hasFailedOperations() const {
    return _transactionable->hasFailedOperations();
  }

  void beginQuery(bool isModificationQuery) {
    return _transactionable->beginQuery(isModificationQuery);
  }
  void endQuery(bool isModificationQuery) noexcept {
    return _transactionable->endQuery(isModificationQuery);
  }

  [[nodiscard]] TransactionCollection* findCollection(DataSourceId cid) const;

  /// @brief make a exclusive transaction, only valid before begin
  void setExclusiveAccessType();

  /// @brief whether or not a transaction is read-only
  [[nodiscard]] bool isReadOnlyTransaction() const noexcept {
    return _transactionable->isReadOnlyTransaction();
  }

  /// @brief whether or not a transaction is a follower transaction
  [[nodiscard]] bool isFollowerTransaction() const {
    return hasHint(transaction::Hints::Hint::IS_FOLLOWER_TRX);
  }

  /// @brief servers already contacted
  [[nodiscard]] ::arangodb::containers::HashSet<std::string> const& knownServers() const {
    return _knownServers;
  }

  [[nodiscard]] bool knowsServer(std::string const& uuid) const {
    return _knownServers.find(uuid) != _knownServers.end();
  }

  /// @brief add a server to the known set
  void addKnownServer(std::string const& uuid) { _knownServers.emplace(uuid); }

  /// @brief remove a server from the known set
  void removeKnownServer(std::string const& uuid) { _knownServers.erase(uuid); }

  void clearKnownServers() { _knownServers.clear(); }

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  [[nodiscard]] TRI_voc_tick_t lastOperationTick() const noexcept {
      return _transactionable->lastOperationTick();
  };

  void acceptAnalyzersRevision(QueryAnalyzerRevisions const& analyzersRevsion) noexcept;

  [[nodiscard]] QueryAnalyzerRevisions const& analyzersRevision() const noexcept {
    return _analyzersRevision;
  }

#ifdef USE_ENTERPRISE
  void addInaccessibleCollection(DataSourceId cid, std::string const& cname);
  [[nodiscard]] bool isInaccessibleCollection(DataSourceId cid);
  [[nodiscard]] bool isInaccessibleCollection(std::string const& cname);
#endif

  /// @brief roll a new transaction ID on the coordintor. Use this method
  /// with care, it should only be used when retrying in a synchronized
  /// fashion after a fast-path locking detected a dead-lock situation.
  /// Only allowed on coordinators.
  void coordinatorRerollTransactionId();

 protected:
  /// @brief find a collection in the transaction's list of collections
  struct CollectionNotFound {
    std::size_t lowerBound;
  };
  struct CollectionFound {
    TransactionCollection* collection;
  };
  [[nodiscard]] auto findCollectionOrPos(DataSourceId cid) const
      -> std::variant<CollectionNotFound, CollectionFound>;

  /// @brief clear the query cache for all collections that were modified by
  /// the transaction
  void clearQueryCache();

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // reset the internal Transaction ID to none.
  // Only used in the Transaction Mock for internal reasons.
  void resetTransactionId();
#endif

 private:
  /// @brief check if current user can access this collection
  [[nodiscard]] Result checkCollectionPermission(DataSourceId cid, std::string const& cname,
                                                 AccessMode::Type);

  /// @brief helper function for addCollection
  [[nodiscard]] Result addCollectionInternal(DataSourceId cid, std::string const& cname,
                                             AccessMode::Type accessType, bool lockUsage);

 protected:
  TRI_vocbase_t& _vocbase;  /// @brief vocbase for this transaction

  /// @brief access type (read|write)
  AccessMode::Type _type;

  using ListType = arangodb::containers::SmallVector<TransactionCollection*>;
  ListType::allocator_type::arena_type _arena;  // memory for collections
  ListType _collections;  // list of participating collections

  transaction::Hints _hints;  // hints; set on _nestingLevel == 0

  ServerState::RoleEnum const _serverRole;  /// role of the server

  transaction::Options _options;

 private:
  TransactionId _id;  /// @brief local trx id

  /// a collection of stored cookies
  std::map<void const*, Cookie::ptr> _cookies;

  /// @brief servers we already talked to for this transactions
  ::arangodb::containers::HashSet<std::string> _knownServers;

  QueryAnalyzerRevisions _analyzersRevision;
  bool _registeredTransaction;

  std::unique_ptr<arangodb::Transactionable> _transactionable;
};

class RunningTransactionState;
class PreTransactionState;
class PostTransactionState;
class PreTransactionState {
 public:
  [[nodiscard]] auto beginTransaction() && -> ResultT<RunningTransactionState>;
  [[nodiscard]] auto id() const noexcept -> TransactionId;
  [[nodiscard]] Result addCollection(DataSourceId cid, std::string const& cname,
                                     AccessMode::Type accessType, bool lockUsage);
};

class RunningTransactionState {
 public:

  [[nodiscard]] auto commitTransaction() && -> ResultT<PostTransactionState>;

  [[nodiscard]] auto abortTransaction() && -> ResultT<PostTransactionState>;
};

class PostTransactionState {
 public:

};

namespace transaction {
auto createRocksDBTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                                   transaction::Options const& options)
    -> std::shared_ptr<PreTransactionState>;
auto createClusterTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                                   transaction::Options const& options)
    -> std::shared_ptr<PreTransactionState>;
}  // namespace transaction

}  // namespace arangodb
