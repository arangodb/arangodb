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

#ifndef ARANGOD_STORAGE_ENGINE_TRANSACTION_STATE_H
#define ARANGOD_STORAGE_ENGINE_TRANSACTION_STATE_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/ServerState.h"
#include "Containers/HashSet.h"
#include "Containers/SmallVector.h"
#include "Transaction/Hints.h"
#include "Transaction/Options.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/voc-types.h"

#include <map>

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#define LOG_TRX(logid, llevel, trx)                \
  LOG_TOPIC(logid, llevel, arangodb::Logger::TRANSACTIONS) \
      << "#" << trx->id().id() << " ("         \
      << transaction::statusString(trx->status()) << "): "

#else

#define LOG_TRX(logid, llevel, ...) \
  while (0) LOG_TOPIC(logid, llevel,  arangodb::Logger::TRANSACTIONS)
#endif

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

  typedef std::function<void(TransactionState& state)> StatusChangeCallback;

  TransactionState() = delete;
  TransactionState(TransactionState const&) = delete;
  TransactionState& operator=(TransactionState const&) = delete;

  TransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                   transaction::Options const& options);
  virtual ~TransactionState();

  /// @return a cookie associated with the specified key, nullptr if none
  Cookie* cookie(void const* key) noexcept;

  /// @brief associate the specified cookie with the specified key
  /// @return the previously associated cookie, if any
  Cookie::ptr cookie(void const* key, Cookie::ptr&& cookie);

  bool isRunningInCluster() const {
    return ServerState::isRunningInCluster(_serverRole);
  }
  bool isDBServer() const { return ServerState::isDBServer(_serverRole); }
  bool isCoordinator() const { return ServerState::isCoordinator(_serverRole); }
  ServerState::RoleEnum serverRole() const { return _serverRole; }

  inline transaction::Options& options() { return _options; }
  inline transaction::Options const& options() const { return _options; }
  inline TRI_vocbase_t& vocbase() const { return _vocbase; }
  inline TransactionId id() const { return _id; }
  inline transaction::Status status() const { return _status; }
  inline bool isRunning() const {
    return _status == transaction::Status::RUNNING;
  }
  void setRegistered() noexcept { _registeredTransaction = true; }
  bool wasRegistered() const noexcept { return _registeredTransaction; }

  /// @brief returns the name of the actor the transaction runs on:
  /// - leader
  /// - follower
  /// - coordinator
  /// - single
  char const* actorName() const noexcept; 
  
  /// @brief return a reference to the global transaction statistics/counters
  TransactionStatistics& statistics() noexcept;

  double lockTimeout() const { return _options.lockTimeout; }
  void lockTimeout(double value) {
    if (value > 0.0) {
      _options.lockTimeout = value;
    }
  }

  bool waitForSync() const { return _options.waitForSync; }
  void waitForSync(bool value) { _options.waitForSync = value; }

  bool allowImplicitCollectionsForRead() const {
    return _options.allowImplicitCollectionsForRead;
  }
  void allowImplicitCollectionsForRead(bool value) {
    _options.allowImplicitCollectionsForRead = value;
  }

  /// @brief return the collection from a transaction
  TransactionCollection* collection(DataSourceId cid, AccessMode::Type accessType) const;

  /// @brief return the collection from a transaction
  TransactionCollection* collection(std::string const& name,
                                    AccessMode::Type accessType) const;

  /// @brief add a collection to a transaction
  Result addCollection(DataSourceId cid, std::string const& cname,
                       AccessMode::Type accessType, bool lockUsage);

  /// @brief use all participating collections of a transaction
  Result useCollections();

  /// @brief run a callback on all collections of the transaction
  template<typename F>
  void allCollections(F&& cb) {
    for (auto& trxCollection : _collections) {
      TRI_ASSERT(trxCollection);  // ensured by addCollection(...)
      if (!std::forward<F>(cb)(*trxCollection)) { // abort early
        return;
      }
    }
  }
  
  /// @brief return the number of collections in the transaction
  size_t numCollections() const { return _collections.size(); }

  /// @brief whether or not a transaction consists of a single operation
  bool isSingleOperation() const {
    return hasHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  /// @brief update the status of a transaction
  void updateStatus(transaction::Status status);

  /// @brief whether or not a specific hint is set for the transaction
  bool hasHint(transaction::Hints::Hint hint) const { return _hints.has(hint); }

  /// @brief begin a transaction
  virtual arangodb::Result beginTransaction(transaction::Hints hints) = 0;

  /// @brief commit a transaction
  virtual arangodb::Result commitTransaction(transaction::Methods* trx) = 0;

  /// @brief abort a transaction
  virtual arangodb::Result abortTransaction(transaction::Methods* trx) = 0;
  
  /// @brief return number of commits.
  /// for cluster transactions on coordinator, this either returns 0 or 1.
  /// for leader, follower or single-server transactions, this can include any
  /// number, because it will also include intermediate commits.
  virtual uint64_t numCommits() const = 0;

  virtual bool hasFailedOperations() const = 0;

  TransactionCollection* findCollection(DataSourceId cid) const;

  /// @brief make a exclusive transaction, only valid before begin
  void setExclusiveAccessType();

  /// @brief whether or not a transaction is read-only
  bool isReadOnlyTransaction() const {
    return (_type == AccessMode::Type::READ);
  }

  /// @brief whether or not a transaction is a follower transaction
  bool isFollowerTransaction() const {
    return hasHint(transaction::Hints::Hint::IS_FOLLOWER_TRX);
  }

  /// @brief whether or not a transaction only has exculsive or read accesses
  bool isOnlyExclusiveTransaction() const;

  /// @brief servers already contacted
  ::arangodb::containers::HashSet<std::string> const& knownServers() const {
    return _knownServers;
  }

  bool knowsServer(std::string const& uuid) const {
    return _knownServers.find(uuid) != _knownServers.end();
  }

  /// @brief add a server to the known set
  void addKnownServer(std::string const& uuid) {
    _knownServers.emplace(uuid);
  }

  /// @brief remove a server from the known set
  void removeKnownServer(std::string const& uuid) {
    _knownServers.erase(uuid);
  }

  void clearKnownServers() {
    _knownServers.clear();
  }

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  TRI_voc_tick_t lastOperationTick() const noexcept {
    return _lastWrittenOperationTick;
  }


  void acceptAnalyzersRevision(
      QueryAnalyzerRevisions const& analyzersRevsion) noexcept;

  const QueryAnalyzerRevisions& analyzersRevision() const noexcept {
    return _analyzersRevision;
  }
  
  #ifdef USE_ENTERPRISE
  void addInaccessibleCollection(DataSourceId cid, std::string const& cname);
  bool isInaccessibleCollection(DataSourceId cid);
  bool isInaccessibleCollection(std::string const& cname);
  #endif

 protected:
  /// @brief find a collection in the transaction's list of collections
  TransactionCollection* findCollection(DataSourceId cid, size_t& position) const;

  /// @brief clear the query cache for all collections that were modified by
  /// the transaction
  void clearQueryCache();

 private:
  /// @brief check if current user can access this collection
  Result checkCollectionPermission(DataSourceId cid, std::string const& cname,
                                   AccessMode::Type);

 protected:
  TRI_vocbase_t& _vocbase;  /// @brief vocbase for this transaction
  TransactionId const _id;  /// @brief local trx id

  /// @brief tick of last added & written operation
  TRI_voc_tick_t _lastWrittenOperationTick;

  /// @brief access type (read|write)
  AccessMode::Type _type;
  /// @brief current status
  transaction::Status _status;

  using ListType = arangodb::containers::SmallVector<TransactionCollection*>;
  ListType::allocator_type::arena_type _arena;  // memory for collections
  ListType _collections;  // list of participating collections

  transaction::Hints _hints;  // hints; set on _nestingLevel == 0
  
  ServerState::RoleEnum const _serverRole;  /// role of the server

  transaction::Options _options;

 private:
  /// a collection of stored cookies
  std::map<void const*, Cookie::ptr> _cookies;

  /// @brief servers we already talked to for this transactions
  ::arangodb::containers::HashSet<std::string> _knownServers;

  QueryAnalyzerRevisions _analyzersRevision;
  bool _registeredTransaction;
};

}  // namespace arangodb

#endif
