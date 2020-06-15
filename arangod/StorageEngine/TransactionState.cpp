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
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace arangodb;

/// @brief transaction type
TransactionState::TransactionState(TRI_vocbase_t& vocbase,
                                   TRI_voc_tid_t tid,
                                   transaction::Options const& options)
    : _vocbase(vocbase),
      _id(tid),
      _lastWrittenOperationTick(0),
      _type(AccessMode::Type::READ),
      _status(transaction::Status::CREATED),
      _arena(),
      _collections{_arena},  // assign arena to vector
      _hints(),
      _options(options),
      _serverRole(ServerState::instance()->getRole()),
      _registeredTransaction(false) {}

/// @brief free a transaction container
TransactionState::~TransactionState() {
  TRI_ASSERT(_status != transaction::Status::RUNNING);
  
  // process collections in reverse order, free all collections
  for (auto it = _collections.rbegin(); it != _collections.rend(); ++it) {
    (*it)->releaseUsage();
    delete (*it);
  }
}

/// @brief return the collection from a transaction
TransactionCollection* TransactionState::collection(TRI_voc_cid_t cid,
                                                    AccessMode::Type accessType) const {
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

/// @brief return the collection from a transaction
TransactionCollection* TransactionState::collection(std::string const& name,
                                                    AccessMode::Type accessType) const {
  TRI_ASSERT(_status == transaction::Status::CREATED ||
             _status == transaction::Status::RUNNING);

  auto it = std::find_if(_collections.begin(), _collections.end(), [&name](TransactionCollection const* trxColl) {
    return trxColl->collectionName() == name;
  });

  if (it == _collections.end() || !(*it)->canAccess(accessType)) {
    // not found or not accessible in the requested mode
    return nullptr;
  }

  return (*it);
}

TransactionState::Cookie* TransactionState::cookie(void const* key) noexcept {
  auto itr = _cookies.find(key);

  return itr == _cookies.end() ? nullptr : itr->second.get();
}

TransactionState::Cookie::ptr TransactionState::cookie(void const* key,
                                                       TransactionState::Cookie::ptr&& cookie) {
  _cookies[key].swap(cookie);

  return std::move(cookie);
}

/// @brief add a collection to a transaction
Result TransactionState::addCollection(TRI_voc_cid_t cid, std::string const& cname,
                                       AccessMode::Type accessType, bool lockUsage) {
  Result res;

  // upgrade transaction type if required
  if (_status == transaction::Status::CREATED) {
    if (AccessMode::isWriteOrExclusive(accessType) &&
        !AccessMode::isWriteOrExclusive(_type)) {
      // if one collection is written to, the whole transaction becomes a
      // write-transaction
      _type = AccessMode::Type::WRITE;
    }
  }

  // check if we already got this collection in the _collections vector
  size_t position = 0;
  TransactionCollection* trxColl = findCollection(cid, position);

  if (trxColl != nullptr) {
    static_assert(AccessMode::Type::NONE < AccessMode::Type::READ &&
                      AccessMode::Type::READ < AccessMode::Type::WRITE &&
                      AccessMode::Type::WRITE < AccessMode::Type::EXCLUSIVE,
                  "AccessMode::Type total order fail");
    LOG_TRX("ad6d0", TRACE, this) << "updating collection usage " << cid << ": '" << cname << "'";
   
    // we may need to recheck permissions here
    if (trxColl->accessType() < accessType) {
      res.reset(checkCollectionPermission(cid, cname, accessType));

      if (res.fail()) {
        return res;
      }
    }
    // collection is already contained in vector
    return res.reset(trxColl->updateUsage(accessType));
  }

  // collection not found.
  
  LOG_TRX("ad6e1", TRACE, this) << "adding new collection " << cid << ": '" << cname << "'";
      
  if (_status != transaction::Status::CREATED &&
      AccessMode::isWriteOrExclusive(accessType) &&
      !_options.allowImplicitCollectionsForWrite) {
    // trying to write access a collection that was not declared at start.
    // this is only supported internally for replication transactions.
    return res.reset(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION, 
                     std::string(TRI_errno_string(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION)) + ": " + cname + 
                     " [" + AccessMode::typeString(accessType) + "]");
  }

  if (!AccessMode::isWriteOrExclusive(accessType) &&
      (isRunning() && !_options.allowImplicitCollectionsForRead)) {
    return res.reset(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION, 
                     std::string(TRI_errno_string(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION)) + ": " + cname +
                     " [" + AccessMode::typeString(accessType) + "]");
  }
    
  // now check the permissions
  res = checkCollectionPermission(cid, cname, accessType);

  if (res.fail()) {
    return res;
  }

  // collection was not contained. now create and insert it
  TRI_ASSERT(trxColl == nullptr);

  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  trxColl = engine->createTransactionCollection(*this, cid, accessType).release();

  TRI_ASSERT(trxColl != nullptr);

  // insert collection at the correct position
  try {
    _collections.insert(_collections.begin() + position, trxColl);
  } catch (...) {
    delete trxColl;
    return res.reset(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  if (lockUsage) {
    TRI_ASSERT(!isRunning() || !AccessMode::isWriteOrExclusive(accessType) || _options.allowImplicitCollectionsForWrite);
    res = trxColl->lockUsage();
  }

  return res;
}

/// @brief run a callback on all collections
void TransactionState::allCollections(                     // iterate
    std::function<bool(TransactionCollection&)> const& cb  // callback to invoke
) {
  for (auto& trxCollection : _collections) {
    TRI_ASSERT(trxCollection);  // ensured by addCollection(...)
    if (!cb(*trxCollection)) {
      // abort early
      return;
    }
  }
}
  
/// @brief use all participating collections of a transaction
Result TransactionState::useCollections() {
  Result res;
  // process collections in forward order
  for (TransactionCollection* trxCollection : _collections) {
    res = trxCollection->lockUsage();
    if (!res.ok()) {
      break;
    }
  }
  return res;
}

/// @brief find a collection in the transaction's list of collections
TransactionCollection* TransactionState::findCollection(TRI_voc_cid_t cid) const {
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
TransactionCollection* TransactionState::findCollection(TRI_voc_cid_t cid,
                                                        size_t& position) const {
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

void TransactionState::setExclusiveAccessType() {
  if (_status != transaction::Status::CREATED) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "cannot change the type of a running transaction");
  }
  _type = AccessMode::Type::EXCLUSIVE;
}

void TransactionState::acceptAnalyzersRevision(AnalyzersRevision::Revision analyzersRevision) noexcept {
  // only init from default allowed! Or we have problem -> different analyzersRevision in one transaction
  LOG_TOPIC_IF("9127a", ERR, Logger::AQL, (_analyzersRevision != analyzersRevision && _analyzersRevision != AnalyzersRevision::MIN))
    << " Changing analyzers revision for transaction from " << _analyzersRevision << " to " << analyzersRevision;
  TRI_ASSERT(_analyzersRevision == analyzersRevision || _analyzersRevision == AnalyzersRevision::MIN);
  _analyzersRevision = analyzersRevision;
}

Result TransactionState::checkCollectionPermission(TRI_voc_cid_t cid,
                                                   std::string const& cname,
                                                   AccessMode::Type accessType) {
  TRI_ASSERT(!cname.empty());
  ExecContext const& exec = ExecContext::current();

  // no need to check for superuser, cluster_sync tests break otherwise
  if (exec.isSuperuser()) {
    return Result{};
  }
  
  auto level = exec.collectionAuthLevel(_vocbase.name(), cname);
  TRI_ASSERT(level != auth::Level::UNDEFINED);  // not allowed here

  if (level == auth::Level::NONE) {
    LOG_TOPIC("24971", TRACE, Logger::AUTHORIZATION)
        << "User " << exec.user() << " has collection auth::Level::NONE";
    
#ifdef USE_ENTERPRISE
    if (accessType == AccessMode::Type::READ && _options.skipInaccessibleCollections) {
      addInaccessibleCollection(cid, cname);
      return Result();
    }
#endif

    return Result(TRI_ERROR_FORBIDDEN,
              std::string(TRI_errno_string(TRI_ERROR_FORBIDDEN)) + ": " + cname +
              " [" + AccessMode::typeString(accessType) + "]");
  } else {
    bool collectionWillWrite = AccessMode::isWriteOrExclusive(accessType);

    if (level == auth::Level::RO && collectionWillWrite) {
      LOG_TOPIC("d3e61", TRACE, Logger::AUTHORIZATION)
          << "User " << exec.user() << " has no write right for collection " << cname;

      return Result(TRI_ERROR_ARANGO_READ_ONLY,
                    std::string(TRI_errno_string(TRI_ERROR_ARANGO_READ_ONLY)) + ": " + cname +
                    " [" + AccessMode::typeString(accessType) + "]");
    }
  }

  return Result{};
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
      if (trxCollection                      // valid instance
          && trxCollection->collection()     // has a valid collection
          && trxCollection->hasOperations()  // may have been modified
      ) {
        // we're only interested in collections that may have been modified
        collections.emplace_back(trxCollection->collection()->guid());
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
  if (_status != transaction::Status::CREATED && _status != transaction::Status::RUNNING) {
    LOG_TOPIC("257ea", ERR, Logger::FIXME) << "trying to update transaction status with "
                                     "an invalid state. current: "
                                  << _status << ", future: " << status;
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
