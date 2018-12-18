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

#include "TransactionState.h"
#include "Aql/QueryCache.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Utils/ExecContext.h"
#include "VocBase/ticks.h"

using namespace arangodb;

/// @brief transaction type
TransactionState::TransactionState(
    TRI_vocbase_t& vocbase,
    TRI_voc_tid_t tid,
    transaction::Options const& options
):
      _vocbase(vocbase),
      _id(tid),
      _type(AccessMode::Type::READ),
      _status(transaction::Status::CREATED),
      _arena(),
      _collections{_arena},  // assign arena to vector
      _serverRole(ServerState::instance()->getRole()),
      _hints(),
      _nestingLevel(0),
      _options(options) {}

/// @brief free a transaction container
TransactionState::~TransactionState() {
  TRI_ASSERT(_status != transaction::Status::RUNNING);

  releaseCollections();

  // free all collections
  for (auto it = _collections.rbegin(); it != _collections.rend(); ++it) {
    delete (*it);
  }
}

std::vector<std::string> TransactionState::collectionNames(std::unordered_set<std::string> const& initial) const {
  std::vector<std::string> result;
  result.reserve(_collections.size() + initial.size());
  for (auto const& it : initial) {
    result.emplace_back(it);
  }
  for (auto const& trxCollection : _collections) {
    if (trxCollection->collection() != nullptr) {
      result.emplace_back(trxCollection->collectionName());
    }
  }

  return result;
}

/// @brief return the collection from a transaction
TransactionCollection* TransactionState::collection(
    TRI_voc_cid_t cid, AccessMode::Type accessType) {
  TRI_ASSERT(_status == transaction::Status::CREATED ||
             _status == transaction::Status::RUNNING);

  size_t unused;
  TransactionCollection* trxCollection = findCollection(cid, unused);

  if (trxCollection == nullptr || !trxCollection->canAccess(accessType)) {
    // not found or not accessible in the requested mode
    return nullptr;
  }

  return trxCollection;
}

TransactionState::Cookie* TransactionState::cookie(
    void const* key
) noexcept {
  auto itr = _cookies.find(key);

  return itr == _cookies.end() ? nullptr : itr->second.get();
}

TransactionState::Cookie::ptr TransactionState::cookie(
    void const* key,
    TransactionState::Cookie::ptr&& cookie
) {
  _cookies[key].swap(cookie);

  return std::move(cookie);
}

/// @brief add a collection to a transaction
int TransactionState::addCollection(TRI_voc_cid_t cid,
                                    std::string const& cname,
                                    AccessMode::Type accessType,
                                    int nestingLevel, bool force) {
  LOG_TRX(this, nestingLevel) << "adding collection " << cid;
  
  // upgrade transaction type if required
  if (nestingLevel == 0) {
    if (!force) {
      TRI_ASSERT(_status == transaction::Status::CREATED);
    }

    if (AccessMode::isWriteOrExclusive(accessType) &&
        !AccessMode::isWriteOrExclusive(_type)) {
      // if one collection is written to, the whole transaction becomes a
      // write-transaction
      _type = AccessMode::Type::WRITE;
    }
  }

  // check if we already got this collection in the _collections vector
  size_t position = 0;
  TransactionCollection* trxCollection = findCollection(cid, position);

  if (trxCollection != nullptr) {
    static_assert(AccessMode::Type::NONE < AccessMode::Type::READ &&
                  AccessMode::Type::READ < AccessMode::Type::WRITE &&
                  AccessMode::Type::WRITE < AccessMode::Type::EXCLUSIVE,
                  "AccessMode::Type total order fail");
    // we may need to recheck permissions here
    if (trxCollection->accessType() < accessType) {
      int res = checkCollectionPermission(cid, cname, accessType);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
    // collection is already contained in vector
    return trxCollection->updateUsage(accessType, nestingLevel);
  }

  // collection not found.

  if (nestingLevel > 0 && AccessMode::isWriteOrExclusive(accessType)) {
    // trying to write access a collection in an embedded transaction
    return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
  }

  if (!AccessMode::isWriteOrExclusive(accessType) &&
      (isRunning() && !_options.allowImplicitCollections)) {
    return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
  }
  
  // now check the permissions
  int res = checkCollectionPermission(cid, cname, accessType);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  // collection was not contained. now create and insert it
  TRI_ASSERT(trxCollection == nullptr);

  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  trxCollection = engine->createTransactionCollection(
    *this, cid, accessType, nestingLevel
  ).release();

  TRI_ASSERT(trxCollection != nullptr);

  // insert collection at the correct position
  try {
    _collections.insert(_collections.begin() + position, trxCollection);
  } catch (...) {
    delete trxCollection;

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief make sure all declared collections are used & locked
Result TransactionState::ensureCollections(int nestingLevel) {
  return useCollections(nestingLevel);
}

/// @brief run a callback on all collections
void TransactionState::allCollections(std::function<bool(TransactionCollection*)> const& cb) {
  for (auto& trxCollection : _collections) {
    if (!cb(trxCollection)) {
      // abort early
      return;
    }
  }
}

/// @brief use all participating collections of a transaction
Result TransactionState::useCollections(int nestingLevel) {
  Result res;
  // process collections in forward order
  for (auto& trxCollection : _collections) {
    res = trxCollection->use(nestingLevel);
    if (!res.ok()) {
      break;
    }
  }
  return res;
}

/// @brief release collection locks for a transaction
int TransactionState::unuseCollections(int nestingLevel) {
  // process collections in reverse order
  for (auto it = _collections.rbegin(); it != _collections.rend(); ++it) {
    (*it)->unuse(nestingLevel);
  }

  return TRI_ERROR_NO_ERROR;
}

int TransactionState::lockCollections() {
  for (auto& trxCollection : _collections) {
    int res = trxCollection->lockRecursive();

    if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_LOCKED) {
      return res;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief find a collection in the transaction's list of collections
TransactionCollection* TransactionState::findCollection(
    TRI_voc_cid_t cid) const {
  for (auto* trxCollection : _collections) {
    if (cid == trxCollection->id()) {
      // found
      return trxCollection;
    }
    if (cid < trxCollection->id()) {
      // collection not found
      break;
    }
  }

  return nullptr;
}

/// @brief find a collection in the transaction's list of collections
///        The idea is if a collection is found it will be returned.
///        In this case the position is not used.
///        In case the collection is not found. It will return a
///        nullptr and the position will be set. The position
///        defines where the collection should be inserted,
///        so whenever we want to insert the collection we
///        have to use this position for insert.
TransactionCollection* TransactionState::findCollection(
    TRI_voc_cid_t cid, size_t& position) const {
  size_t const n = _collections.size();
  size_t i;

  for (i = 0; i < n; ++i) {
    auto trxCollection = _collections[i];
    
    if (cid == trxCollection->id()) {
      // found
      return trxCollection;
    }

    if (cid < trxCollection->id()) {
      // collection not found
      break;
    }
    // next
  }

  // update the insert position if required
  position = i;

  return nullptr;
}

void TransactionState::setType(AccessMode::Type type) {
  if (AccessMode::isWriteOrExclusive(type) &&
      AccessMode::isWriteOrExclusive(_type)) {
    // type already correct. do nothing
    return;
  }

  if (AccessMode::isRead(type) && AccessMode::isWriteOrExclusive(_type)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot make a write transaction read-only");
  }
  if (AccessMode::isWriteOrExclusive(type) && AccessMode::isRead(_type) &&
      _status != transaction::Status::CREATED) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "cannot make a running read transaction a write transaction");
  }
  // all right
  _type = type;
}

bool TransactionState::isLockedShard(std::string const& shard) const {
    auto it = _lockedShards.find(shard);
    return it != _lockedShards.end();
}

void TransactionState::setLockedShard(std::string const& shard) {
  _lockedShards.emplace(shard);
}

void TransactionState::setLockedShards(std::unordered_set<std::string> const& lockedShards) {
  // Explicitly copy!
  _lockedShards = lockedShards;
}
   
bool TransactionState::isOnlyExclusiveTransaction() const {
  if (!AccessMode::isWriteOrExclusive(_type)) {
    return false;
  }
  for (TransactionCollection* coll : _collections) {
    if (AccessMode::isWrite(coll->accessType())) {
      return false;
    }
  }
  return true;
}

int TransactionState::checkCollectionPermission(TRI_voc_cid_t cid,
                                                std::string const& cname,
                                                AccessMode::Type accessType) const {
  ExecContext const* exec = ExecContext::CURRENT;

  // no need to check for superuser, cluster_sync tests break otherwise
  if (exec != nullptr && !exec->isSuperuser() && ExecContext::isAuthEnabled()) {
    auto level = exec->collectionAuthLevel(_vocbase.name(), cname);
    TRI_ASSERT(level != auth::Level::UNDEFINED); // not allowed here

    if (level == auth::Level::NONE) {
      LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "User " << exec->user()
      << " has collection auth::Level::NONE";

      return TRI_ERROR_FORBIDDEN;
    }

    bool collectionWillWrite = AccessMode::isWriteOrExclusive(accessType);

    if (level == auth::Level::RO && collectionWillWrite) {
      LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "User " << exec->user()
      << " has no write right for collection " << cname;

      return TRI_ERROR_ARANGO_READ_ONLY;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief release collection locks for a transaction
int TransactionState::releaseCollections() {
  if (hasHint(transaction::Hints::Hint::LOCK_NEVER) ||
      hasHint(transaction::Hints::Hint::NO_USAGE_LOCK)) {
    return TRI_ERROR_NO_ERROR;
  }

  // process collections in reverse order
  for (auto it = _collections.rbegin(); it != _collections.rend(); ++it) {
    (*it)->release();
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief clear the query cache for all collections that were modified by
/// the transaction
void TransactionState::clearQueryCache() {
  if (_collections.empty()) {
    return;
  }

  try {
    std::vector<std::string> collections;

    for (auto& trxCollection : _collections) {
      if (trxCollection->hasOperations()) {
        // we're only interested in collections that may have been modified
        collections.emplace_back(trxCollection->collectionName());
      }
    }

    if (!collections.empty()) {
      arangodb::aql::QueryCache::instance()->invalidate(&_vocbase, collections);
    }
  } catch (...) {
    // in case something goes wrong, we have to remove all queries from the
    // cache
    arangodb::aql::QueryCache::instance()->invalidate(&_vocbase);
  }
}

/// @brief update the status of a transaction
void TransactionState::updateStatus(transaction::Status status) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_status != transaction::Status::CREATED && 
      _status != transaction::Status::RUNNING) {
    LOG_TOPIC(ERR, Logger::FIXME) << "trying to update transaction status with an invalid state. current: " << _status << ", future: " << status;
  }
#endif

  TRI_ASSERT(_status == transaction::Status::CREATED ||
             _status == transaction::Status::RUNNING);

  if (_status == transaction::Status::CREATED) {
    TRI_ASSERT(status == transaction::Status::RUNNING ||
               status == transaction::Status::ABORTED);
  } else if (_status == transaction::Status::RUNNING) {
    TRI_ASSERT(status == transaction::Status::COMMITTED ||
               status == transaction::Status::ABORTED);
  }

  _status = status;
}
