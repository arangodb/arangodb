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

#ifndef ARANGOD_STORAGE_ENGINE_TRANSACTION_STATE_H
#define ARANGOD_STORAGE_ENGINE_TRANSACTION_STATE_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/HashSet.h"
#include "Basics/SmallVector.h"
#include "Cluster/ServerState.h"
#include "Transaction/Hints.h"
#include "Transaction/Options.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#define LOG_TRX(logid, llevel, trx, tlevel)                \
  LOG_TOPIC(logid, llevel, arangodb::Logger::TRANSACTIONS) \
      << "#" << trx->id() << "." << tlevel << " ("         \
      << transaction::statusString(trx->status()) << "): "

#else

#define LOG_TRX(logid, llevel, ...) \
  while (0) LOG_TOPIC(logid, llevel,  arangodb::Logger::TRANSACTIONS)
#endif

struct TRI_vocbase_t;

namespace arangodb {

namespace transaction {
class Context;
class Methods;
struct Options;
}  // namespace transaction

class ExecContext;
class TransactionCollection;

/// @brief transaction type
class TransactionState {
 public:
  /// @brief an implementation-dependent structure for storing runtime data
  struct Cookie {
    typedef std::unique_ptr<Cookie> ptr;
    virtual ~Cookie() {}
  };

  typedef std::function<void(TransactionState& state)> StatusChangeCallback;

  TransactionState() = delete;
  TransactionState(TransactionState const&) = delete;
  TransactionState& operator=(TransactionState const&) = delete;

  TransactionState(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
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

  inline transaction::Options& options() { return _options; }
  inline transaction::Options const& options() const { return _options; }
  inline TRI_vocbase_t& vocbase() const { return _vocbase; }
  inline TRI_voc_tid_t id() const { return _id; }
  inline transaction::Status status() const { return _status; }
  inline bool isRunning() const {
    return _status == transaction::Status::RUNNING;
  }
  void setRegistered() noexcept { _registeredTransaction = true; }
  bool wasRegistered() const noexcept { return _registeredTransaction; }

  int increaseNesting() {
    return _nestingLevel.fetch_add(1, std::memory_order_relaxed) + 1;
  }
  int decreaseNesting() {
    TRI_ASSERT(nestingLevel() > 0);
    return _nestingLevel.fetch_sub(1, std::memory_order_relaxed) - 1;
  }
  int nestingLevel() const {
    return _nestingLevel.load(std::memory_order_relaxed);
  }
  bool isTopLevelTransaction() const { return nestingLevel() == 0; }
  bool isEmbeddedTransaction() const { return !isTopLevelTransaction(); }

  double timeout() const { return _options.lockTimeout; }
  void timeout(double value) {
    if (value > 0.0) {
      _options.lockTimeout = value;
    }
  }

  bool waitForSync() const { return _options.waitForSync; }
  void waitForSync(bool value) { _options.waitForSync = value; }

  bool allowImplicitCollections() const {
    return _options.allowImplicitCollections;
  }
  void allowImplicitCollections(bool value) {
    _options.allowImplicitCollections = value;
  }

  /// @brief return the collection from a transaction
  TransactionCollection* collection(TRI_voc_cid_t cid,
                                    AccessMode::Type accessType) const;

  /// @brief add a collection to a transaction
  Result addCollection(TRI_voc_cid_t cid, std::string const& cname,
                       AccessMode::Type accessType, int nestingLevel, bool force);

  /// @brief make sure all declared collections are used & locked
  Result ensureCollections(int nestingLevel = 0);

  /// @brief use all participating collections of a transaction
  Result useCollections(int nestingLevel);

  /// @brief run a callback on all collections of the transaction
  void allCollections(std::function<bool(TransactionCollection&)> const& cb);

  /// @brief return the number of collections in the transaction
  size_t numCollections() const { return _collections.size(); }

  /// @brief release collection locks for a transaction
  int unuseCollections(int nestingLevel);

  /// FIXME delete, server-based locking should take care of this
  int lockCollections();

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

  virtual bool hasFailedOperations() const = 0;

  TransactionCollection* findCollection(TRI_voc_cid_t cid) const;

  /// @brief make a exclusive transaction, only valid before begin
  void setExclusiveAccessType();
  
  /// @brief whether or not a transaction is read-only
  bool isReadOnlyTransaction() const {
    return (_type == AccessMode::Type::READ);
  }

  /// @brief whether or not a transaction only has exculsive or read accesses
  bool isOnlyExclusiveTransaction() const;
  
  /// @brief servers already contacted
  arangodb::HashSet<std::string> const& knownServers() const {
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

 protected:
  /// @brief find a collection in the transaction's list of collections
  TransactionCollection* findCollection(TRI_voc_cid_t cid, size_t& position) const;

  /// @brief release collection locks for a transaction
  void releaseCollections();

  /// @brief clear the query cache for all collections that were modified by
  /// the transaction
  void clearQueryCache();

 private:
  /// @brief check if current user can access this collection
  Result checkCollectionPermission(std::string const& cname, AccessMode::Type) const;

  
 protected:
  TRI_vocbase_t& _vocbase;  /// @brief vocbase for this transaction
  TRI_voc_tid_t const _id;  /// @brief local trx id

  /// @brief access type (read|write)
  AccessMode::Type _type;
  /// @brief current status
  transaction::Status _status;

  SmallVector<TransactionCollection*>::allocator_type::arena_type _arena;  // memory for collections
  SmallVector<TransactionCollection*> _collections;  // list of participating collections

  ServerState::RoleEnum const _serverRole;  /// role of the server

  transaction::Hints _hints;  // hints; set on _nestingLevel == 0

  transaction::Options _options;

 private:
  /// a collection of stored cookies
  std::map<void const*, Cookie::ptr> _cookies;
  
  /// @brief servers we already talked to for this transactions
  arangodb::HashSet<std::string> _knownServers;
  
  /// @brief reference counter of # of 'Methods' instances using this object
  std::atomic<int> _nestingLevel;
  
  bool _registeredTransaction;
};

}  // namespace arangodb

#endif
