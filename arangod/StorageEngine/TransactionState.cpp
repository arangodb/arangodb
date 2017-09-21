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
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "RestServer/FeatureCacheFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Utils/ExecContext.h"
#include "VocBase/ticks.h"

using namespace arangodb;

/// @brief transaction type
TransactionState::TransactionState(TRI_vocbase_t* vocbase,
                                   transaction::Options const& options)
    : _vocbase(vocbase),
      _id(0),
      _type(AccessMode::Type::READ),
      _status(transaction::Status::CREATED),
      _arena(),
      _collections{_arena},  // assign arena to vector
      _serverRole(ServerState::instance()->getRole()),
      _resolver(new CollectionNameResolver(vocbase)),
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

  delete _resolver;
}

std::vector<std::string> TransactionState::collectionNames() const {
  std::vector<std::string> result;
  result.reserve(_collections.size());

  for (auto& trxCollection : _collections) {
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

/// @brief add a collection to a transaction
int TransactionState::addCollection(TRI_voc_cid_t cid,
                                    AccessMode::Type accessType,
                                    int nestingLevel, bool force) {
  LOG_TRX(this, nestingLevel) << "adding collection " << cid;

  AuthenticationFeature* auth =
      FeatureCacheFeature::instance()->authenticationFeature();
  if (auth->isActive() && ExecContext::CURRENT != nullptr) {
    std::string const colName = _resolver->getCollectionNameCluster(cid);

    // only valid on coordinator or single server
    TRI_ASSERT(ServerState::instance()->isCoordinator() ||
               !ServerState::instance()->isRunningInCluster());
    // avoid extra lookups of auth context, if we use the same db as stored
    // in the execution context initialized by RestServer/VocbaseContext
    AuthLevel level = auth->canUseCollection(ExecContext::CURRENT->user(),
                                     _vocbase->name(), colName);
    
    if (level == AuthLevel::NONE) {
      LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "User " << ExecContext::CURRENT->user()
                                             << " has collection AuthLevel::NONE";
      return TRI_ERROR_FORBIDDEN;
    }
    bool collectionWillWrite = AccessMode::isWriteOrExclusive(accessType);
    if (level == AuthLevel::RO && collectionWillWrite) {
      LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "User " << ExecContext::CURRENT->user()
                                              << "has no write right for collection " << colName;
      return TRI_ERROR_ARANGO_READ_ONLY;
    }
  }

  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "cid: " << cid
  //            << ", accessType: " << accessType
  //            << ", nestingLevel: " << nestingLevel
  //            << ", force: " << force
  //            << ", allowImplicitCollections: " <<
  //            _options.allowImplicitCollections;

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

  // check if we already have got this collection in the _collections vector
  size_t position = 0;
  TransactionCollection* trxCollection = findCollection(cid, position);

  if (trxCollection != nullptr) {
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

  // collection was not contained. now create and insert it
  TRI_ASSERT(trxCollection == nullptr);

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  trxCollection =
      engine->createTransactionCollection(this, cid, accessType, nestingLevel);

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
    int res = trxCollection->lock();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief find a collection in the transaction's list of collections
TransactionCollection* TransactionState::findCollection(
    TRI_voc_cid_t cid) const {
  size_t unused = 0;
  return findCollection(cid, unused);
}

/// @brief find a collection in the transaction's list of collections
TransactionCollection* TransactionState::findCollection(
    TRI_voc_cid_t cid, size_t& position) const {
  size_t const n = _collections.size();
  size_t i;

  for (i = 0; i < n; ++i) {
    auto trxCollection = _collections.at(i);

    if (cid < trxCollection->id()) {
      // collection not found
      break;
    }

    if (cid == trxCollection->id()) {
      // found
      return trxCollection;
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
   
bool TransactionState::isExclusiveTransactionOnSingleCollection() const {
  return ((numCollections() == 1) && (_collections[0]->accessType() == AccessMode::Type::EXCLUSIVE));
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
      arangodb::aql::QueryCache::instance()->invalidate(_vocbase, collections);
    }
  } catch (...) {
    // in case something goes wrong, we have to remove all queries from the
    // cache
    arangodb::aql::QueryCache::instance()->invalidate(_vocbase);
  }
}

/// @brief update the status of a transaction
void TransactionState::updateStatus(transaction::Status status) {
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
